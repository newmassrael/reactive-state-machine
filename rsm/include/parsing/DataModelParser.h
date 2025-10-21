#pragma once

#include "factory/NodeFactory.h"
#include "model/IDataModelItem.h"
#include "model/SCXMLContext.h"
#include <fstream>
#include <libxml++/libxml++.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Class responsible for parsing data model elements
 *
 * This class provides functionality to parse data model-related elements
 * in SCXML documents. It parses datamodel elements and data elements within
 * them and converts them into IDataModelItem objects.
 */

namespace RSM {

class DataModelParser {
public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory instance for node creation
     */
    explicit DataModelParser(std::shared_ptr<NodeFactory> nodeFactory);

    /**
     * @brief Destructor
     */
    ~DataModelParser();

    /**
     * @brief Parse datamodel element
     * @param datamodelNode XML datamodel node
     * @return List of parsed data model items
     */
    std::vector<std::shared_ptr<IDataModelItem>> parseDataModelNode(const xmlpp::Element *datamodelNode,
                                                                    const SCXMLContext &context);

    /**
     * @brief Parse individual data element
     * @param dataNode XML data node
     * @return Created data model item
     */
    std::shared_ptr<IDataModelItem> parseDataModelItem(const xmlpp::Element *dataNode, const SCXMLContext &context);

    /**
     * @brief Parse all data model elements within a state node
     * @param stateNode State node
     * @return List of parsed data model items
     */
    std::vector<std::shared_ptr<IDataModelItem>> parseDataModelInState(const xmlpp::Element *stateNode,
                                                                       const SCXMLContext &context);

    /**
     * @brief Check if element is a data model item
     * @param element XML element
     * @return Whether it is a data model item
     */
    bool isDataModelItem(const xmlpp::Element *element) const;

    /**
     * @brief Extract data model type
     * @param datamodelNode XML datamodel node
     * @return Data model type (default: "")
     */
    std::string extractDataModelType(const xmlpp::Element *datamodelNode) const;

private:
    /**
     * @brief Parse content of data model item
     * @param dataNode XML data node
     * @param dataItem Data model item
     */
    void parseDataContent(const xmlpp::Element *dataNode, std::shared_ptr<IDataModelItem> dataItem);

    /**
     * @brief Handle namespace matching
     * @param nodeName Node name
     * @param searchName Name to search for
     * @return Whether node name matches search name (considering namespace)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief Load content from external data source
     * @param src External data source URL
     * @param dataItem Item to store data in
     */
    void loadExternalContent(const std::string &src, std::shared_ptr<IDataModelItem> dataItem);

    std::shared_ptr<NodeFactory> nodeFactory_;
};

}  // namespace RSM