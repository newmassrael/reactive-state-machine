#pragma once

#include "factory/NodeFactory.h"
#include "model/SCXMLContext.h"
#include "model/SCXMLModel.h"
#include "parsing/ActionParser.h"
#include "parsing/DataModelParser.h"
#include "parsing/DoneDataParser.h"
#include "parsing/GuardParser.h"
#include "parsing/InvokeParser.h"
#include "parsing/StateNodeParser.h"
#include "parsing/TransitionParser.h"
#include "parsing/XIncludeProcessor.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Class that orchestrates SCXML parsing
 *
 * This class manages and coordinates the parsing of SCXML documents.
 * It utilizes various element-specific parsers to convert documents
 * into complete object models.
 */

namespace RSM {

class SCXMLParser {
public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory instance for node creation
     * @param xincludeProcessor Processor instance for XInclude processing (optional)
     */
    explicit SCXMLParser(std::shared_ptr<NodeFactory> nodeFactory,
                         std::shared_ptr<IXIncludeProcessor> xincludeProcessor = nullptr);

    /**
     * @brief Destructor
     */
    ~SCXMLParser();

    /**
     * @brief Parse SCXML file
     * @param filename File path to parse
     * @return Parsed SCXML model, nullptr on failure
     */
    std::shared_ptr<SCXMLModel> parseFile(const std::string &filename);

    /**
     * @brief Parse SCXML string
     * @param content SCXML string to parse
     * @return Parsed SCXML model, nullptr on failure
     */
    std::shared_ptr<SCXMLModel> parseContent(const std::string &content);

    /**
     * @brief Check for parsing errors
     * @return Whether errors occurred
     */
    bool hasErrors() const;

    /**
     * @brief Return parsing error messages
     * @return List of error messages
     */
    const std::vector<std::string> &getErrorMessages() const;

    /**
     * @brief Return parsing warning messages
     * @return List of warning messages
     */
    const std::vector<std::string> &getWarningMessages() const;

    /**
     * @brief Return state node parser
     * @return State node parser
     */
    std::shared_ptr<StateNodeParser> getStateNodeParser() const {
        return stateNodeParser_;
    }

    /**
     * @brief Return transition parser
     * @return Transition parser
     */
    std::shared_ptr<TransitionParser> getTransitionParser() const {
        return transitionParser_;
    }

    /**
     * @brief Return action parser
     * @return Action parser
     */
    std::shared_ptr<ActionParser> getActionParser() const {
        return actionParser_;
    }

    /**
     * @brief Return guard parser
     * @return Guard parser
     */
    std::shared_ptr<GuardParser> getGuardParser() const {
        return guardParser_;
    }

    /**
     * @brief Return data model parser
     * @return Data model parser
     */
    std::shared_ptr<DataModelParser> getDataModelParser() const {
        return dataModelParser_;
    }

    /**
     * @brief Return InvokeParser
     * @return Invoke parser
     */
    std::shared_ptr<InvokeParser> getInvokeParser() const {
        return invokeParser_;
    }

    /**
     * @brief Return DoneData parser
     * @return DoneData parser
     */
    std::shared_ptr<DoneDataParser> getDoneDataParser() const {
        return doneDataParser_;
    }

    /**
     * @brief Return XInclude processor
     * @return XInclude processor
     */
    std::shared_ptr<IXIncludeProcessor> getXIncludeProcessor() const {
        return xincludeProcessor_;
    }

private:
    /**
     * @brief Execute XML document parsing
     * @param doc libxml++ document object
     * @return Parsed SCXML model, nullptr on failure
     */
    std::shared_ptr<SCXMLModel> parseDocument(xmlpp::Document *doc);

    /**
     * @brief Parse SCXML root node
     * @param scxmlNode Root node
     * @param model Target model
     * @return Success status
     */
    bool parseScxmlNode(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief Parse context properties
     * @param scxmlNode SCXML node
     * @param model Target model
     */
    void parseContextProperties(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief Parse dependency injection points
     * @param scxmlNode SCXML node
     * @param model Target model
     */
    void parseInjectPoints(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief Initialize parsing task
     */
    void initParsing();

    /**
     * @brief Add error message
     * @param message Error message
     */
    void addError(const std::string &message);

    /**
     * @brief Add warning message
     * @param message Warning message
     */
    void addWarning(const std::string &message);

    /**
     * @brief Validate model
     * @param model Model to validate
     * @return Whether it is valid
     */
    bool validateModel(std::shared_ptr<SCXMLModel> model);

    void addSystemVariables(std::shared_ptr<SCXMLModel> model);

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::shared_ptr<StateNodeParser> stateNodeParser_;
    std::shared_ptr<TransitionParser> transitionParser_;
    std::shared_ptr<ActionParser> actionParser_;
    std::shared_ptr<GuardParser> guardParser_;
    std::shared_ptr<DataModelParser> dataModelParser_;
    std::shared_ptr<InvokeParser> invokeParser_;
    std::shared_ptr<DoneDataParser> doneDataParser_;
    std::shared_ptr<IXIncludeProcessor> xincludeProcessor_;
    std::vector<std::string> errorMessages_;
    std::vector<std::string> warningMessages_;
};

}  // namespace RSM