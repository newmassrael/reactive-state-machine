#include "parsing/GuardParser.h"
#include "GuardUtils.h"
#include "ParsingCommon.h"
#include "common/Logger.h"
#include <algorithm>

RSM::GuardParser::GuardParser(std::shared_ptr<RSM::NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating guard parser");
}

RSM::GuardParser::~GuardParser() {
    LOG_DEBUG("Destroying guard parser");
}

std::shared_ptr<RSM::IGuardNode> RSM::GuardParser::parseGuardNode(const xmlpp::Element *guardNode) {
    if (!guardNode) {
        LOG_WARN("Null guard node");
        return nullptr;
    }

    auto idAttr = guardNode->get_attribute("id");
    auto targetAttr = guardNode->get_attribute("target");
    auto conditionAttr = guardNode->get_attribute("condition");

    // Try alternative attribute names if id and target/condition are missing
    if (!idAttr) {
        idAttr = guardNode->get_attribute("name");
    }

    if (!targetAttr && !conditionAttr) {
        targetAttr = guardNode->get_attribute("to");
    }

    if (!idAttr || (!targetAttr && !conditionAttr)) {
        LOG_WARN("Guard node missing required attributes");
        LOG_DEBUG("Node name: {}", guardNode->get_name());
        // Print all available attributes for debugging
        auto attrs = guardNode->get_attributes();
        for (auto *attr : attrs) {
            auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
            if (xmlAttr) {
                LOG_DEBUG("Attribute: {} = {}", xmlAttr->get_name(), xmlAttr->get_value());
            }
        }
        return nullptr;
    }

    std::string id = idAttr->get_value();

    // Create basic guard node - initialized with empty string
    auto guard = nodeFactory_->createGuardNode(id, "");

    // Process target attribute
    if (targetAttr) {
        std::string target = targetAttr->get_value();
        LOG_DEBUG("Guard: {} with target attribute: {}", id, target);

        if (GuardUtils::isConditionExpression(target)) {
            // When target is a condition expression
            guard->setCondition(target);
            LOG_DEBUG("Set condition from target: {}", target);
        } else {
            // When target is a state ID
            guard->setTargetState(target);
            LOG_DEBUG("Set target state: {}", target);
        }
    }

    // Process condition attribute
    if (conditionAttr) {
        std::string condition = conditionAttr->get_value();
        guard->setCondition(condition);
        LOG_DEBUG("Set condition from attribute: {}", condition);
    }

    // Process <code:condition> or <condition> element
    auto conditionNode = guardNode->get_first_child("code:condition");
    if (!conditionNode) {
        conditionNode = guardNode->get_first_child("condition");
    }

    if (conditionNode) {
        // Process <code:condition> or <condition> element
        auto conditionElement = RSM::ParsingCommon::findFirstChildElement(guardNode, "condition");
        if (conditionElement) {
            LOG_DEBUG("Found condition element");

            // Process both CDATA sections and plain text
            std::string conditionText = RSM::ParsingCommon::extractTextContent(conditionElement, true);
            LOG_DEBUG("Raw condition content: '{}'", conditionText);

            if (!conditionText.empty()) {
                guard->setCondition(conditionText);
                LOG_DEBUG("Set condition from element: {}", conditionText);
            }
        }
    }

    // Parse dependencies
    parseDependencies(guardNode, guard);

    // Parse external implementation
    parseExternalImplementation(guardNode, guard);

    LOG_DEBUG("Guard parsed successfully");
    return guard;
}

std::shared_ptr<RSM::IGuardNode> RSM::GuardParser::parseGuardFromTransition(const xmlpp::Element *transitionNode,
                                                                            const std::string &targetState) {
    if (!transitionNode) {
        LOG_WARN("Null transition node");
        return nullptr;
    }

    // Find guard attribute considering namespace prefix
    auto guardAttr = transitionNode->get_attribute("guard", "code");
    if (!guardAttr) {
        // Try without namespace
        guardAttr = transitionNode->get_attribute("guard");
    }

    if (!guardAttr) {
        // No guard attribute found
        return nullptr;
    }

    std::string guardId = guardAttr->get_value();

    LOG_DEBUG("Parsing guard from transition: {} for state: {}", guardId, targetState);

    // Create basic guard node - initialized with empty string
    auto guard = nodeFactory_->createGuardNode(guardId, "");

    // Set target state explicitly
    guard->setTargetState(targetState);

    // Check if cond attribute exists
    auto condAttr = transitionNode->get_attribute("cond");
    if (condAttr) {
        std::string condition = condAttr->get_value();
        guard->setCondition(condition);
        LOG_DEBUG("Set condition from cond attribute: {}", condition);
    }

    LOG_DEBUG("Guard from transition parsed successfully");
    return guard;
}

std::shared_ptr<RSM::IGuardNode> RSM::GuardParser::parseReactiveGuard(const xmlpp::Element *reactiveGuardNode) {
    if (!reactiveGuardNode) {
        LOG_WARN("Null reactive guard node");
        return nullptr;
    }

    auto idAttr = reactiveGuardNode->get_attribute("id");
    auto targetAttr = reactiveGuardNode->get_attribute("target");
    auto conditionAttr = reactiveGuardNode->get_attribute("condition");

    if (!idAttr || (!targetAttr && !conditionAttr)) {
        LOG_WARN("Reactive guard node missing required attributes");
        return nullptr;
    }

    std::string id = idAttr->get_value();

    // Create basic guard node - initialized with empty string
    auto guard = nodeFactory_->createGuardNode(id, "");

    // Set reactive attributes
    guard->setReactive(true);
    guard->setAttribute("reactive", "true");

    // Process target attribute
    if (targetAttr) {
        std::string target = targetAttr->get_value();
        LOG_DEBUG("Reactive guard: {} with target: {}", id, target);

        if (GuardUtils::isConditionExpression(target)) {
            // When target is a condition expression
            guard->setCondition(target);
            LOG_DEBUG("Set condition from target: {}", target);
        } else {
            // When target is a state ID
            guard->setTargetState(target);
            LOG_DEBUG("Set target state: {}", target);
        }
    }

    // Process condition attribute
    if (conditionAttr) {
        std::string condition = conditionAttr->get_value();
        guard->setCondition(condition);
        LOG_DEBUG("Set condition from attribute: {}", condition);
    }

    // Parse dependencies
    parseDependencies(reactiveGuardNode, guard);

    // Parse external implementation
    parseExternalImplementation(reactiveGuardNode, guard);

    LOG_DEBUG("Reactive guard parsed successfully");
    return guard;
}

std::vector<std::shared_ptr<RSM::IGuardNode>> RSM::GuardParser::parseGuardsElement(const xmlpp::Element *guardsNode) {
    std::vector<std::shared_ptr<RSM::IGuardNode>> guards;

    if (!guardsNode) {
        LOG_WARN("Null guards node");
        return guards;
    }

    LOG_DEBUG("Parsing guards element");

    // Parse guard nodes
    auto guardNodes = guardsNode->get_children("code:guard");
    if (guardNodes.empty()) {
        // Try without namespace
        guardNodes = guardsNode->get_children("guard");
    }

    for (auto *node : guardNodes) {
        auto *guardElement = dynamic_cast<const xmlpp::Element *>(node);
        if (guardElement) {
            auto guard = parseGuardNode(guardElement);
            if (guard) {
                guards.push_back(guard);
                LOG_DEBUG("Added guard: {}", guard->getId());
            }
        }
    }

    LOG_DEBUG("Parsed {} guards", guards.size());
    return guards;
}

std::vector<std::shared_ptr<RSM::IGuardNode>> RSM::GuardParser::parseAllGuards(const xmlpp::Element *scxmlNode) {
    std::vector<std::shared_ptr<RSM::IGuardNode>> allGuards;

    if (!scxmlNode) {
        LOG_WARN("Null SCXML node");
        return allGuards;
    }

    LOG_DEBUG("Parsing all guards in SCXML document");

    // 1. Parse guards within code:guards element
    auto guardsNode = scxmlNode->get_first_child("code:guards");
    if (!guardsNode) {
        // Try without namespace
        guardsNode = scxmlNode->get_first_child("guards");
    }

    if (guardsNode) {
        auto *element = dynamic_cast<const xmlpp::Element *>(guardsNode);
        if (element) {
            auto guards = parseGuardsElement(element);
            allGuards.insert(allGuards.end(), guards.begin(), guards.end());
        }
    }

    // 2. Find guard attributes in transitions of all states
    std::vector<const xmlpp::Node *> stateNodes;

    // Collect all state nodes (state, parallel, final)
    auto states = scxmlNode->get_children("state");
    stateNodes.insert(stateNodes.end(), states.begin(), states.end());

    auto parallels = scxmlNode->get_children("parallel");
    stateNodes.insert(stateNodes.end(), parallels.begin(), parallels.end());

    auto finals = scxmlNode->get_children("final");
    stateNodes.insert(stateNodes.end(), finals.begin(), finals.end());

    // Check guard attributes in transition elements of each state
    for (auto *stateNode : stateNodes) {
        auto *stateElement = dynamic_cast<const xmlpp::Element *>(stateNode);
        if (stateElement) {
            // Get state ID
            auto idAttr = stateElement->get_attribute("id");
            if (!idAttr) {
                continue;
            }

            std::string stateId = idAttr->get_value();

            // Process transition elements
            auto transNodes = stateElement->get_children("transition");
            for (auto *transNode : transNodes) {
                auto *transElement = dynamic_cast<const xmlpp::Element *>(transNode);
                if (transElement) {
                    auto targetAttr = transElement->get_attribute("target");
                    if (targetAttr) {
                        std::string target = targetAttr->get_value();
                        auto guard = parseGuardFromTransition(transElement, target);
                        if (guard) {
                            allGuards.push_back(guard);
                            LOG_DEBUG("Added guard from transition in state {}", stateId);
                        }
                    }
                }
            }

            // Process reactive guards
            auto reactiveGuardNodes = stateElement->get_children("code:reactive-guard");
            if (reactiveGuardNodes.empty()) {
                // Try without namespace
                reactiveGuardNodes = stateElement->get_children("reactive-guard");
            }

            for (auto *node : reactiveGuardNodes) {
                auto *guardElement = dynamic_cast<const xmlpp::Element *>(node);
                if (guardElement) {
                    auto guard = parseReactiveGuard(guardElement);
                    if (guard) {
                        allGuards.push_back(guard);
                        LOG_DEBUG("Added reactive guard from state {}", stateId);
                    }
                }
            }
        }
    }

    // 3. Remove duplicates (based on ID)
    std::sort(allGuards.begin(), allGuards.end(),
              [](const std::shared_ptr<RSM::IGuardNode> &a, const std::shared_ptr<RSM::IGuardNode> &b) {
                  return a->getId() < b->getId();
              });

    allGuards.erase(std::unique(allGuards.begin(), allGuards.end(),
                                [](const std::shared_ptr<RSM::IGuardNode> &a,
                                   const std::shared_ptr<RSM::IGuardNode> &b) { return a->getId() == b->getId(); }),
                    allGuards.end());

    LOG_DEBUG("Found {} unique guards", allGuards.size());
    return allGuards;
}

bool RSM::GuardParser::isGuardNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return RSM::ParsingCommon::matchNodeName(nodeName, "guard");
}

bool RSM::GuardParser::isReactiveGuardNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return RSM::ParsingCommon::matchNodeName(nodeName, "reactive-guard");
}

void RSM::GuardParser::parseDependencies(const xmlpp::Element *guardNode,
                                         std::shared_ptr<RSM::IGuardNode> guardObject) {
    if (!guardNode || !guardObject) {
        return;
    }

    // Parse dependencies
    auto depNodes = guardNode->get_children("code:dependency");
    if (depNodes.empty()) {
        // Try without namespace
        depNodes = guardNode->get_children("dependency");
    }

    for (auto *node : depNodes) {
        auto *element = dynamic_cast<const xmlpp::Element *>(node);
        if (element) {
            auto propAttr = element->get_attribute("property");
            if (!propAttr) {
                propAttr = element->get_attribute("prop");  // Try alternative attribute name
            }

            if (propAttr) {
                std::string property = propAttr->get_value();
                guardObject->addDependency(property);
                LOG_DEBUG("Added dependency: {}", property);
            }
        }
    }
}

void RSM::GuardParser::parseExternalImplementation(const xmlpp::Element *guardNode,
                                                   std::shared_ptr<RSM::IGuardNode> guardObject) {
    if (!guardNode || !guardObject) {
        return;
    }

    auto implNode = guardNode->get_first_child("code:external-implementation");
    if (!implNode) {
        // Try without namespace
        implNode = guardNode->get_first_child("external-implementation");
    }

    if (implNode) {
        auto *element = dynamic_cast<const xmlpp::Element *>(implNode);
        if (element) {
            auto classAttr = element->get_attribute("class");
            auto factoryAttr = element->get_attribute("factory");

            if (classAttr) {
                std::string className = classAttr->get_value();
                guardObject->setExternalClass(className);
                LOG_DEBUG("External class: {}", className);
            }

            if (factoryAttr) {
                std::string factory = factoryAttr->get_value();
                guardObject->setExternalFactory(factory);
                LOG_DEBUG("External factory: {}", factory);
            }
        }
    }
}

bool RSM::GuardParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    // Exact match
    if (nodeName == searchName) {
        return true;
    }

    // With namespace (e.g., "code:guard")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == searchName;
    }

    return false;
}
