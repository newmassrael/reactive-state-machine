#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace SCXML {
namespace Common {
namespace Constants {

// State Types
namespace StateTypes {
constexpr const char *ATOMIC = "atomic";
constexpr const char *COMPOUND = "compound";
constexpr const char *PARALLEL = "parallel";
constexpr const char *FINAL = "final";
constexpr const char *HISTORY_SHALLOW = "history_shallow";
constexpr const char *HISTORY_DEEP = "history_deep";
}  // namespace StateTypes

// Transition Types
namespace TransitionTypes {
constexpr const char *INTERNAL = "internal";
constexpr const char *EXTERNAL = "external";
}  // namespace TransitionTypes

// Default Values
namespace Defaults {
constexpr int DEFAULT_PRIORITY = 0;
constexpr int MAX_HIERARCHY_DEPTH = 32;
constexpr int MAX_EVENT_QUEUE_SIZE = 1000;
constexpr int MAX_TRANSITION_STACK_SIZE = 100;
constexpr size_t DEFAULT_STATE_CACHE_SIZE = 512;

// Timeouts
constexpr auto EVENT_PROCESSING_TIMEOUT = std::chrono::milliseconds(5000);
constexpr auto STATE_TRANSITION_TIMEOUT = std::chrono::milliseconds(1000);
}  // namespace Defaults

// Error Messages
namespace ErrorMessages {
constexpr const char *NULL_POINTER = "Null pointer encountered";
constexpr const char *INVALID_STATE_ID = "Invalid state identifier";
constexpr const char *INVALID_TRANSITION = "Invalid transition";
constexpr const char *HIERARCHY_TOO_DEEP = "State hierarchy exceeds maximum depth";
constexpr const char *EVENT_QUEUE_FULL = "Event queue is full";
constexpr const char *PARSING_FAILED = "XML parsing failed";
constexpr const char *EVALUATION_FAILED = "Expression evaluation failed";
constexpr const char *ACTION_EXECUTION_FAILED = "Action execution failed";
constexpr const char *GUARD_EVALUATION_FAILED = "Guard evaluation failed";
}  // namespace ErrorMessages

// XML Element Names
namespace XmlElements {
constexpr const char *SCXML = "scxml";
constexpr const char *STATE = "state";
constexpr const char *PARALLEL = "parallel";
constexpr const char *TRANSITION = "transition";
constexpr const char *FINAL = "final";
constexpr const char *HISTORY = "history";
constexpr const char *ONENTRY = "onentry";
constexpr const char *ONEXIT = "onexit";
constexpr const char *RAISE = "raise";
constexpr const char *SEND = "send";
constexpr const char *ASSIGN = "assign";
constexpr const char *IF = "if";
constexpr const char *ELSEIF = "elseif";
constexpr const char *ELSE = "else";
constexpr const char *FOREACH = "foreach";
constexpr const char *LOG = "log";
constexpr const char *SCRIPT = "script";
constexpr const char *DATA = "data";
constexpr const char *DATAMODEL = "datamodel";
constexpr const char *INVOKE = "invoke";
constexpr const char *FINALIZE = "finalize";
constexpr const char *CONTENT = "content";
constexpr const char *PARAM = "param";
constexpr const char *DONEDATA = "donedata";
}  // namespace XmlElements

// XML Attribute Names
namespace XmlAttributes {
constexpr const char *ID = "id";
constexpr const char *INITIAL = "initial";
constexpr const char *EVENT = "event";
constexpr const char *TARGET = "target";
constexpr const char *COND = "cond";
constexpr const char *TYPE = "type";
constexpr const char *SRC = "src";
constexpr const char *EXPR = "expr";
constexpr const char *LOCATION = "location";
constexpr const char *NAME = "name";
constexpr const char *VALUE = "value";
constexpr const char *LABEL = "label";
constexpr const char *DELAY = "delay";
constexpr const char *DELAYEXPR = "delayexpr";
constexpr const char *NAMELIST = "namelist";
constexpr const char *ARRAY = "array";
constexpr const char *ITEM = "item";
constexpr const char *INDEX = "index";
constexpr const char *AUTOFORWARD = "autoforward";
constexpr const char *TYPEHINT = "typehint";
constexpr const char *SRCEXPR = "srcexpr";
constexpr const char *TYPEEXPR = "typeexpr";
constexpr const char *IDLOCATION = "idlocation";
}  // namespace XmlAttributes

// Event System
namespace Events {
constexpr const char *ERROR_PREFIX = "error.";
constexpr const char *DONE_PREFIX = "done.";
constexpr const char *INTERNAL_PREFIX = "#_internal";

// System Events
constexpr const char *ERROR_EXECUTION = "error.execution";
constexpr const char *ERROR_COMMUNICATION = "error.communication";
constexpr const char *ERROR_PLATFORM = "error.platform";
constexpr const char *DONE_INVOKE = "done.invoke";
constexpr const char *DONE_STATE = "done.state";
}  // namespace Events

// Regular Expressions
namespace Patterns {
constexpr const char *WILDCARD_ANY = ".*";
constexpr const char *EVENT_NAME_PATTERN = R"([a-zA-Z][a-zA-Z0-9_\.\-]*)";
constexpr const char *STATE_ID_PATTERN = R"([a-zA-Z][a-zA-Z0-9_\-]*)";
constexpr const char *XPATH_PATTERN = R"(/[^/\s]+(?:/[^/\s]+)*)";
}  // namespace Patterns

// Performance Tuning
namespace Performance {
constexpr size_t SMALL_VECTOR_RESERVE = 4;
constexpr size_t MEDIUM_VECTOR_RESERVE = 16;
constexpr size_t LARGE_VECTOR_RESERVE = 64;
constexpr size_t HASH_MAP_BUCKET_COUNT = 64;
constexpr double HASH_MAP_LOAD_FACTOR = 0.75;
}  // namespace Performance

// Logging
namespace Logging {
constexpr const char *DEBUG_PREFIX = "DEBUG";
constexpr const char *INFO_PREFIX = "INFO ";
constexpr const char *WARN_PREFIX = "WARN ";
constexpr const char *ERROR_PREFIX = "ERROR";
constexpr const char *UNKNOWN_PREFIX = "?????";
}  // namespace Logging

// Validation
namespace Validation {
constexpr size_t MAX_STATE_ID_LENGTH = 128;
constexpr size_t MAX_EVENT_NAME_LENGTH = 256;
constexpr size_t MAX_EXPRESSION_LENGTH = 4096;
constexpr size_t MAX_XML_CONTENT_SIZE = 1024 * 1024;  // 1MB
constexpr size_t BUFFER_SIZE = 1024;                  // Standard buffer size for file operations
constexpr size_t JS_MEMORY_LIMIT = 16 * 1024 * 1024;  // 16MB JavaScript memory limit
constexpr int MAX_RECURSION_DEPTH = 100;
}  // namespace Validation

}  // namespace Constants
}  // namespace Common
}  // namespace SCXML

// Compatibility support
namespace SCXMLConstants = SCXML::Common::Constants;