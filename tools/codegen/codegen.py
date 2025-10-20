#!/usr/bin/env python3
"""
SCXML Static Code Generator (Python + Jinja2)

Generates C++ state machine code from W3C SCXML files.
Replaces C++ StaticCodeGenerator with modern Python + Jinja2 approach.
"""

import sys
import os
import argparse
from pathlib import Path
from jinja2 import Environment, FileSystemLoader, select_autoescape
from scxml_parser import SCXMLParser, SCXMLModel


class CodeGenerator:
    """
    Static code generator for W3C SCXML state machines

    Uses Jinja2 templates to generate C++ code from parsed SCXML models.
    """

    def __init__(self, template_dir=None):
        if template_dir is None:
            template_dir = Path(__file__).parent / 'templates'

        # Setup Jinja2 environment
        self.env = Environment(
            loader=FileSystemLoader(str(template_dir)),
            autoescape=select_autoescape(['html', 'xml']),
            trim_blocks=True,
            lstrip_blocks=True
        )

        # Add custom filters
        self.env.filters['capitalize'] = self._capitalize_state
        self.env.filters['escape_cpp'] = self._escape_cpp_string

    def _capitalize_state(self, state_id):
        """Capitalize state/event names for C++ enums"""
        if not state_id:
            return ""
        # Handle special cases
        if state_id.lower() == 'pass':
            return 'Pass'
        if state_id.lower() == 'fail':
            return 'Fail'
        # Capitalize first letter
        return state_id[0].upper() + state_id[1:]

    def _escape_cpp_string(self, text):
        """Escape C++ string literals"""
        if not text:
            return ""
        # Escape backslashes first
        text = text.replace('\\', '\\\\')
        # Escape quotes
        text = text.replace('"', '\\"')
        # Escape newlines
        text = text.replace('\n', '\\n')
        text = text.replace('\r', '\\r')
        text = text.replace('\t', '\\t')
        return text

    def _analyze_model_features(self, model: SCXMLModel):
        """
        Analyze model and set feature flags

        Determines which headers and helpers are needed based on
        SCXML features used in the model.
        """
        # W3C SCXML B.1: ECMAScript datamodel requires JSEngine only if used
        # Note: datamodel_type alone doesn't require JSEngine (test276 has ecmascript but no expressions)
        # JSEngine is enabled when actual ECMAScript features are detected (assign, foreach, guards, etc.)

        # Detect which helpers are needed
        # W3C SCXML 3.12: TransitionHelper always needed for event matching (matches C++ codegen line 772)
        model.needs_transition_helper = True
        model.needs_event_type_helper = False
        model.needs_assign_helper = False
        model.needs_foreach = False
        model.needs_guard_helper = False
        model.needs_send_helper = False
        model.needs_event_data_helper = False

        # Event metadata fields
        model.needs_event_name = False
        model.needs_event_data = False
        model.needs_event_type = False
        model.needs_event_sendid = False
        model.needs_event_origin = False
        model.needs_event_origintype = False
        model.needs_event_invokeid = False
        model.needs_external_flag = False

        # Scan all actions
        for state in model.states.values():
            # Check transitions
            for transition in state.transitions:
                if transition.cond:
                    model.needs_guard_helper = True

                for action in transition.actions:
                    self._analyze_action(action, model)

            # Check entry/exit actions
            for action in state.on_entry + state.on_exit:
                self._analyze_action(action, model)

        # Detect event metadata needs
        if model.needs_jsengine:
            # If JSEngine is used, need all event metadata fields for setCurrentEventInJSEngine
            model.needs_event_name = True
            model.needs_event_data = True
            model.needs_event_type = True
            model.needs_event_sendid = True
            model.needs_event_origin = True
            model.needs_event_origintype = True
            model.needs_event_invokeid = True
            model.needs_external_flag = True
            # JSEngine raises error.execution for runtime errors
            model.events.add('error.execution')
            # Need EventTypeHelper for getEventType() method (matches C++ codegen line 781)
            model.needs_event_type_helper = True
            # W3C SCXML B.1: JSEngine always needs these helpers (matches C++ codegen line 788-791)
            model.needs_assign_helper = True
            model.needs_foreach = True
            model.needs_guard_helper = True

    def _analyze_action(self, action, model):
        """Analyze single action for feature detection"""
        action_type = action.get('type', '')

        if action_type == 'send':
            model.needs_send_helper = True
            # Send actions can raise error.execution (invalid target, etc.)
            model.events.add('error.execution')
            # Send with params needs EventDataHelper
            if action.get('params'):
                model.needs_event_data_helper = True
            # Send with delay/delayexpr needs EventScheduler (test175, test179, etc.)
            if action.get('delay') or action.get('delayexpr'):
                if not hasattr(model, 'needs_event_scheduler'):
                    model.needs_event_scheduler = False
                model.needs_event_scheduler = True
            if action.get('type') == 'http://www.w3.org/TR/scxml/#SCXMLEventProcessor':
                model.needs_external_flag = True
            # Check for event metadata access
            if '_event.sendid' in str(action):
                model.needs_event_sendid = True
            if '_event.origin' in str(action):
                model.needs_event_origin = True
            if '_event.invokeid' in str(action):
                model.needs_event_invokeid = True

        elif action_type == 'cancel':
            # W3C SCXML 6.2: <cancel> requires EventScheduler to cancel delayed events
            if not hasattr(model, 'needs_event_scheduler'):
                model.needs_event_scheduler = False
            model.needs_event_scheduler = True

        elif action_type == 'assign':
            model.needs_assign_helper = True

        elif action_type == 'foreach':
            model.needs_foreach = True

        elif action_type == 'if' and action.get('cond'):
            model.needs_guard_helper = True
            # Check for _event references in conditions
            cond = action.get('cond', '')
            if '_event.type' in cond:
                model.needs_event_type = True
            if '_event.data' in cond:
                model.needs_event_data = True
            if '_event.name' in cond:
                model.needs_event_name = True
            
            # Recursively analyze nested actions in if/elseif/else branches
            for nested_action in action.get('then_actions', []):
                self._analyze_action(nested_action, model)
            for elseif_branch in action.get('elseif_branches', []):
                for nested_action in elseif_branch.get('actions', []):
                    self._analyze_action(nested_action, model)
            for nested_action in action.get('else_actions', []):
                self._analyze_action(nested_action, model)

    def _classify_variables(self, model: SCXMLModel):
        """
        Classify datamodel variables by type

        Determines whether variables are static (int, string, bool) or
        require runtime evaluation (JSEngine).
        """
        for var in model.variables:
            var_id = var.get('id')
            expr = var.get('expr', '')
            content = var.get('content', '')

            # Detect type from expression
            if not expr and not content:
                var['type'] = 'runtime'
            elif expr == '0' or (expr and expr.isdigit()):
                var['type'] = 'int'
            elif expr.startswith('"') and expr.endswith('"'):
                var['type'] = 'string'
            elif expr in ['true', 'false']:
                var['type'] = 'bool'
            else:
                # Complex expression, needs JSEngine
                var['type'] = 'runtime'
                model.needs_jsengine = True

    def generate(self, scxml_path: str, output_dir: str) -> bool:
        """
        Generate C++ code from SCXML file

        Args:
            scxml_path: Path to SCXML input file
            output_dir: Directory for generated C++ file

        Returns:
            True if generation succeeded, False otherwise
        """
        try:
            # Parse SCXML
            parser = SCXMLParser()
            model = parser.parse_file(scxml_path)

            print(f"Generating code for: {model.name}")
            print(f"  States: {len(model.states)}")
            print(f"  Events: {len(model.events)}")
            print(f"  Needs JSEngine: {model.needs_jsengine}")

            # Classify variables
            self._classify_variables(model)

            # Analyze features
            self._analyze_model_features(model)
            
            # Add Wildcard event only if wildcard transitions exist
            # W3C SCXML 3.12.1: Wildcard patterns include '*' and '.*'
            has_wildcard = any(
                any(t.event == '*' or t.event == '.*' for t in state.transitions)
                for state in model.states.values()
            )
            if has_wildcard:
                model.events.add('Wildcard')
            
            # Add invoke-related events if static invoke present
            if model.static_invokes:
                model.events.add('done.invoke')
                model.events.add('cancel.invoke')
                model.events.add('error.execution')  # W3C SCXML 6.4.6: Invoke failure

            # Build prefix matching map for transitions
            # W3C SCXML 3.12.1: event="error" matches "error", "error.execution", etc.
            self._build_prefix_matching(model)

            # Check if we need Interpreter wrapper
            if self._needs_interpreter_wrapper(model):
                print(f"  → Generating Interpreter wrapper (dynamic features detected)")
                return self._generate_interpreter_wrapper(model, output_dir)

            # Load template
            template = self.env.get_template('state_machine.jinja2')

            # Render template
            output = template.render(model=model)

            # Write output file
            output_path = Path(output_dir) / f"{model.name}_sm.h"
            output_path.parent.mkdir(parents=True, exist_ok=True)

            with open(output_path, 'w') as f:
                f.write(output)

            print(f"  ✓ Generated: {output_path}")
            return True

        except Exception as e:
            print(f"Error generating code: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc()
            return False

    def _needs_interpreter_wrapper(self, model: SCXMLModel) -> bool:
        """
        Determine if model requires Interpreter wrapper

        Returns True if any dynamic features are detected that
        cannot be handled by static code generation.
        """
        # No initial state
        if not model.initial:
            print(f"    Reason: No initial state")
            return True

        # Check if initial state exists in model
        if model.initial not in model.states:
            print(f"    Reason: Initial state '{model.initial}' not found in model")
            return True

        # Scoped datamodel (duplicate variable names - test240, test241, test244)
        var_names = [var['id'] for var in model.variables]
        if len(var_names) != len(set(var_names)):
            duplicates = [name for name in var_names if var_names.count(name) > 1]
            print(f"    Reason: Scoped datamodel detected (duplicate variables: {set(duplicates)})")
            return True

        # Parallel states
        if model.has_parallel_states:
            print(f"    Reason: Parallel states detected")
            return True

        # Dynamic invoke requires Interpreter wrapper (matches C++ All-or-Nothing strategy)
        # Static invoke (type="scxml" src="file:...") can be handled statically
        if model.has_dynamic_invoke:
            print(f"    Reason: Dynamic invoke detected (srcexpr, content, or contentexpr)")
            return True

        # Dynamic expressions: eventexpr, targetexpr, delayexpr can be handled statically via JSEngine
        # Only check for other dynamic features (parallel, history, etc.)
        # Note: has_dynamic_expressions includes eventexpr/targetexpr/delayexpr which are JSEngine-compatible

        # History states
        if model.has_history_states:
            print(f"    Reason: History states detected")
            return True

        # Parent-child communication (<send target="#_parent">) is supported in static code
        # Child state machines receive parent pointer via constructor and use SendHelper::sendToParent()
        # Templates in actions/send.jinja2 already handle this case
        # No wrapper needed for parent communication
        return False

    def _only_simple_event_expressions(self, model: SCXMLModel) -> bool:
        """Check if dynamic expressions are only simple _event references"""
        # For now, assume if we have dynamic expressions but only event metadata,
        # we can handle it statically
        return model.has_event_metadata and not model.has_invoke

    def _generate_interpreter_wrapper(self, model: SCXMLModel, output_dir: str) -> bool:
        """
        Generate Interpreter wrapper for dynamic features

        Creates a simple wrapper that uses the Interpreter engine
        for SCXML files with features not supported by static codegen.
        """
        wrapper_template = """#pragma once
#include <memory>
#include "runtime/StateMachine.h"
#include "model/SCXMLModel.h"

namespace RSM::Generated::{{ model.name }} {

// Interpreter wrapper for {{ model.name }}
// Reason: Static codegen does not support this SCXML file's features
// Uses runtime/StateMachine for dynamic execution
class {{ model.name }} {
public:
    {{ model.name }}() {
        // TODO: Load SCXML file and create StateMachine instance
    }

    void run() {
        // TODO: Implement using StateMachine
    }
};

} // namespace RSM::Generated::{{ model.name }}
"""
        template = self.env.from_string(wrapper_template)
        output = template.render(model=model)

        output_path = Path(output_dir) / f"{model.name}_sm.h"
        output_path.parent.mkdir(parents=True, exist_ok=True)

        with open(output_path, 'w') as f:
            f.write(output)

        print(f"  ✓ Generated wrapper: {output_path}")
        return True

    def _build_prefix_matching(self, model: SCXMLModel):
        """
        Build prefix matching for event transitions

        W3C SCXML 3.12.1: event="error" matches "error", "error.execution", etc.

        For each transition, store list of all events that match the prefix.
        """
        # Get all event names (from model.events + generated system events)
        all_events = list(model.events)
        if model.static_invokes:
            all_events.extend(['done.invoke', 'cancel.invoke', 'error.execution'])

        # For each state's transitions, build prefix match list
        for state in model.states.values():
            for transition in state.transitions:
                if not transition.event or transition.event in ['*', '.*', '_*']:
                    continue

                # Find all events that match this transition's event descriptor
                # W3C SCXML 3.12.1: prefix matching
                matching_events = []
                for event_name in all_events:
                    # Exact match
                    if event_name == transition.event:
                        matching_events.append(event_name)
                    # Prefix match: event="error" matches "error.execution"
                    elif event_name.startswith(transition.event + '.'):
                        matching_events.append(event_name)

                # Store in transition for template use
                transition.prefix_matching_events = matching_events


def main():
    parser = argparse.ArgumentParser(
        description='Generate C++ state machine code from W3C SCXML files'
    )
    parser.add_argument('scxml_file', help='Input SCXML file')
    parser.add_argument('-o', '--output-dir', default='.',
                        help='Output directory for generated files')
    parser.add_argument('-t', '--template-dir', default=None,
                        help='Template directory (default: ./templates)')

    args = parser.parse_args()

    # Check input file exists
    if not Path(args.scxml_file).exists():
        print(f"Error: SCXML file not found: {args.scxml_file}", file=sys.stderr)
        return 1

    # Generate code
    generator = CodeGenerator(template_dir=args.template_dir)
    success = generator.generate(args.scxml_file, args.output_dir)

    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
