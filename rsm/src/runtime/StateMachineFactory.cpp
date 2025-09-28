#include "runtime/StateMachineFactory.h"
#include "common/Logger.h"
#include "scripting/IScriptEngine.h"
#include "scripting/JSEngine.h"
#include <random>

namespace RSM {

// === StateMachineFactory Implementation ===

StateMachineFactory::CreationResult StateMachineFactory::createProduction() {
    return createInternal("", true);
}

StateMachineFactory::CreationResult StateMachineFactory::createWithSCXML(const std::string &scxmlContent) {
    if (scxmlContent.empty()) {
        return CreationResult("SCXML content cannot be empty");
    }

    return createInternal(scxmlContent, true);
}

StateMachineFactory::CreationResult StateMachineFactory::createInternal(const std::string &scxmlContent,
                                                                        bool autoInitialize) {
    try {
        // Create StateMachine - uses JSEngine::instance() internally
        auto stateMachine = std::make_unique<StateMachine>();

        // Load SCXML if provided
        if (!scxmlContent.empty()) {
            if (!stateMachine->loadSCXMLFromString(scxmlContent)) {
                return CreationResult("Failed to load SCXML content");
            }
        }

        // Initialize if requested
        if (autoInitialize) {
            if (!stateMachine->start()) {
                return CreationResult("Failed to start StateMachine");
            }
        }

        LOG_DEBUG("StateMachineFactory: Successfully created StateMachine instance");
        return CreationResult(std::move(stateMachine));

    } catch (const std::exception &e) {
        return CreationResult("StateMachine creation failed: " + std::string(e.what()));
    }
}

// === Builder Implementation ===

StateMachineFactory::CreationResult StateMachineFactory::Builder::build() {
    return StateMachineFactory::createInternal(scxmlContent_, autoInitialize_);
}

}  // namespace RSM