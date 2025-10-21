#include "parsing/TransitionParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"
#include <algorithm>
#include <sstream>

RSM::TransitionParser::TransitionParser(std::shared_ptr<RSM::NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating transition parser");
}

RSM::TransitionParser::~TransitionParser() {
    LOG_DEBUG("Destroying transition parser");
}

void RSM::TransitionParser::setActionParser(std::shared_ptr<RSM::ActionParser> actionParser) {
    actionParser_ = actionParser;
    LOG_DEBUG("Action parser set");
}

std::shared_ptr<RSM::ITransitionNode> RSM::TransitionParser::parseTransitionNode(const xmlpp::Element *transElement,
                                                                                 RSM::IStateNode *stateNode) {
    if (!transElement || !stateNode) {
        LOG_WARN("Null transition element or state node");
        return nullptr;
    }

    auto eventAttr = transElement->get_attribute("event");
    auto targetAttr = transElement->get_attribute("target");

    std::string event = eventAttr ? eventAttr->get_value() : "";
    std::string target = targetAttr ? targetAttr->get_value() : "";

    LOG_DEBUG("Parsing transition: {} -> {}", (event.empty() ? "<no event>" : event),
              (target.empty() ? "<internal>" : target));

    // Treat as internal transition if target is empty
    bool isInternal = target.empty();

    // Create transition node
    std::shared_ptr<RSM::ITransitionNode> transition;

    if (isInternal) {
        LOG_DEBUG("Internal transition detected (no target)");

        // Create transition with empty target
        transition = nodeFactory_->createTransitionNode(event, "");

        // Explicitly clear target list
        transition->clearTargets();

        LOG_DEBUG("After clearTargets() - targets count: {}", transition->getTargets().size());
    } else {
        // Create with empty string on initialization
        transition = nodeFactory_->createTransitionNode(event, "");

        // Clear existing target list and start fresh
        transition->clearTargets();

        // Parse space-separated target string
        std::stringstream ss(target);
        std::string targetId;

        // Add individual targets
        while (ss >> targetId) {
            if (!targetId.empty()) {
                transition->addTarget(targetId);
                LOG_DEBUG("Added target: {}", targetId);
            }
        }
    }

    // Set internal transition
    transition->setInternal(isInternal);

    // Process type attribute
    auto typeAttr = transElement->get_attribute("type");
    if (typeAttr) {
        std::string type = typeAttr->get_value();
        transition->setAttribute("type", type);
        LOG_DEBUG("Type: {}", type);

        // Set as internal transition if type is "internal"
        if (type == "internal") {
            transition->setInternal(true);
            isInternal = true;  // Update isInternal variable
        }
    }

    // Process condition attribute
    auto condAttr = transElement->get_attribute("cond");
    if (condAttr) {
        std::string cond = condAttr->get_value();
        transition->setAttribute("cond", cond);
        transition->setGuard(cond);
        LOG_DEBUG("Condition: {}", cond);
    }

    // Process guard attribute
    std::string guard = ParsingCommon::getAttributeValue(transElement, {"guard"});
    if (!guard.empty()) {
        transition->setGuard(guard);
        LOG_DEBUG("Guard: {}", guard);
    }

    // Parse event list
    if (!event.empty()) {
        auto events = parseEventList(event);
        for (const auto &eventName : events) {
            transition->addEvent(eventName);
            LOG_DEBUG("Added event: {}", eventName);
        }
    }

    // Parse actions
    parseActions(transElement, transition);

    LOG_DEBUG("Transition parsed successfully with {} ActionNodes", transition->getActionNodes().size());
    return transition;
}

std::shared_ptr<RSM::ITransitionNode>
RSM::TransitionParser::parseInitialTransition(const xmlpp::Element *initialElement) {
    if (!initialElement) {
        LOG_WARN("Null initial element");
        return nullptr;
    }

    LOG_DEBUG("Parsing initial transition");

    // Find transition element within initial element
    auto transElement = ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (!transElement) {
        LOG_WARN("No transition element found in initial");
        return nullptr;
    }

    auto targetAttr = transElement->get_attribute("target");
    if (!targetAttr) {
        LOG_WARN("Initial transition missing target attribute");
        return nullptr;
    }

    std::string target = targetAttr->get_value();
    LOG_DEBUG("Initial transition target: {}", target);

    // Create initial transition - no event
    auto transition = nodeFactory_->createTransitionNode("", target);

    // Set special attribute
    transition->setAttribute("initial", "true");

    // Parse actions
    parseActions(transElement, transition);

    LOG_DEBUG("Initial transition parsed successfully");
    return transition;
}

std::vector<std::shared_ptr<RSM::ITransitionNode>>
RSM::TransitionParser::parseTransitionsInState(const xmlpp::Element *stateElement, RSM::IStateNode *stateNode) {
    std::vector<std::shared_ptr<RSM::ITransitionNode>> transitions;

    if (!stateElement || !stateNode) {
        LOG_WARN("Null state element or node");
        return transitions;
    }

    LOG_DEBUG("Parsing transitions in state: {}", stateNode->getId());

    // Find all transition elements
    auto transElements = ParsingCommon::findChildElements(stateElement, "transition");
    for (auto *transElement : transElements) {
        auto transition = parseTransitionNode(transElement, stateNode);
        if (transition) {
            transitions.push_back(transition);
        }
    }

    LOG_DEBUG("Found {} transitions", transitions.size());
    return transitions;
}

bool RSM::TransitionParser::isTransitionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "transition");
}

void RSM::TransitionParser::parseActions(const xmlpp::Element *transElement,
                                         std::shared_ptr<RSM::ITransitionNode> transition) {
    if (!transElement || !transition) {
        return;
    }

    // ActionParser is required for SCXML parsing
    if (!actionParser_) {
        assert(false && "ActionParser is required for SCXML compliance");
        return;
    }

    {
        // SCXML specification compliance: Store ActionNode objects directly
        auto actionNodes = actionParser_->parseActionsInElement(transElement);
        for (const auto &actionNode : actionNodes) {
            if (actionNode) {
                transition->addActionNode(actionNode);
                LOG_DEBUG("Added ActionNode: {}", actionNode->getActionType());
            }
        }
    }
}

std::vector<std::string> RSM::TransitionParser::parseEventList(const std::string &eventStr) const {
    std::vector<std::string> events;
    std::stringstream ss(eventStr);
    std::string event;

    // Parse space-separated event list
    while (std::getline(ss, event, ' ')) {
        // Remove empty strings
        if (!event.empty()) {
            events.push_back(event);
        }
    }

    return events;
}

bool RSM::TransitionParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    return ParsingCommon::matchNodeName(nodeName, searchName);
}

std::vector<std::string> RSM::TransitionParser::parseTargetList(const std::string &targetStr) const {
    std::vector<std::string> targets;
    std::stringstream ss(targetStr);
    std::string target;

    // Parse space-separated target list
    while (std::getline(ss, target, ' ')) {
        // Remove empty strings
        if (!target.empty()) {
            targets.push_back(target);
        }
    }

    return targets;
}
