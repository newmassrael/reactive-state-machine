#pragma once

#include "IDataModelItem.h"
#include "IInvokeNode.h"
#include "IStateNode.h"
#include "ITransitionNode.h"
#include "actions/IActionNode.h"
#include "model/DoneData.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Implementation class for state node
 *
 * This class implements state nodes in a state chart.
 * Corresponds to <state>, <parallel>, <final> elements in SCXML documents.
 */

namespace RSM {

class StateNode : public IStateNode {
public:
    /**
     * @brief Constructor
     * @param id State identifier
     * @param type State type
     */
    StateNode(const std::string &id, Type type);

    /**
     * @brief Destructor
     */
    virtual ~StateNode();

    /**
     * @brief Return state ID
     * @return State ID
     */
    virtual const std::string &getId() const override;

    /**
     * @brief Return state type
     * @return State type
     */
    virtual Type getType() const override;

    /**
     * @brief Set parent state
     * @param parent Parent state pointer
     */
    virtual void setParent(IStateNode *parent) override;

    /**
     * @brief Return parent state
     * @return Parent state pointer
     */
    virtual IStateNode *getParent() const override;

    /**
     * @brief Add child state
     * @param child Child state
     */
    virtual void addChild(std::shared_ptr<IStateNode> child) override;

    /**
     * @brief Return list of child states
     * @return List of child states
     */
    virtual const std::vector<std::shared_ptr<IStateNode>> &getChildren() const override;

    /**
     * @brief Add transition
     * @param transition Transition node
     */
    virtual void addTransition(std::shared_ptr<ITransitionNode> transition) override;

    /**
     * @brief Return list of transitions
     * @return List of transitions
     */
    virtual const std::vector<std::shared_ptr<ITransitionNode>> &getTransitions() const override;

    /**
     * @brief Add data model item
     * @param dataItem Data model item
     */
    virtual void addDataItem(std::shared_ptr<IDataModelItem> dataItem) override;

    /**
     * @brief Return list of data model items
     * @return List of data model items
     */
    virtual const std::vector<std::shared_ptr<IDataModelItem>> &getDataItems() const override;

    /**
     * @brief Set initial state ID
     * @param initialState Initial state ID
     */
    virtual void setInitialState(const std::string &initialState) override;

    /**
     * @brief Return initial state ID
     * @return Initial state ID
     */
    virtual const std::string &getInitialState() const override;

    /**
     * @brief Set entry callback
     * @param callback Entry callback name
     */
    virtual void setOnEntry(const std::string &callback) override;

    /**
     * @brief Return entry callback
     * @return Entry callback name
     */
    virtual const std::string &getOnEntry() const override;

    /**
     * @brief Set exit callback
     * @param callback Exit callback name
     */
    virtual void setOnExit(const std::string &callback) override;

    /**
     * @brief Return exit callback
     * @return Exit callback name
     */
    virtual const std::string &getOnExit() const override;

    /**
     * @brief W3C SCXML 3.8: Add entry action block (each onentry handler is a separate block)
     * @param block Action node block
     */
    void addEntryActionBlock(std::vector<std::shared_ptr<RSM::IActionNode>> block) override;

    /**
     * @brief W3C SCXML 3.8: Get entry action blocks
     * @return Entry action blocks
     */
    const std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> &getEntryActionBlocks() const override;

    /**
     * @brief W3C SCXML 3.9: Add exit action block (each onexit handler is a separate block)
     * @param block Action node block
     */
    void addExitActionBlock(std::vector<std::shared_ptr<RSM::IActionNode>> block) override;

    /**
     * @brief W3C SCXML 3.9: Get exit action blocks
     * @return Exit action blocks
     */
    const std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> &getExitActionBlocks() const override;

    /**
     * @brief Add invoke node
     * @param invoke Invoke node
     */
    virtual void addInvoke(std::shared_ptr<IInvokeNode> invoke) override;

    /**
     * @brief Return list of invoke nodes
     * @return List of invoke nodes
     */
    virtual const std::vector<std::shared_ptr<IInvokeNode>> &getInvoke() const override;

    // Set history type
    void setHistoryType(HistoryType type) {
        historyType_ = type;
    }

    // IStateNode interface implementation
    void setHistoryType(bool isDeep) override {
        historyType_ = isDeep ? HistoryType::DEEP : HistoryType::SHALLOW;
    }

    HistoryType getHistoryType() const override {
        return historyType_;
    }

    bool isShallowHistory() const override {
        return historyType_ == HistoryType::SHALLOW;
    }

    bool isDeepHistory() const override {
        return historyType_ == HistoryType::DEEP;
    }

    // Method to add reactive guard ID
    void addReactiveGuard(const std::string &guardId) override;

    // IStateNode interface implementation
    const std::vector<std::string> &getReactiveGuards() const override;

    bool isFinalState() const override;

    /**
     * @brief Return DoneData object reference (const)
     * @return DoneData object reference
     */
    const DoneData &getDoneData() const override;

    /**
     * @brief Return DoneData object reference (mutable)
     * @return DoneData object reference
     */
    DoneData &getDoneData() override;

    /**
     * @brief Set <content> element of <donedata>
     * @param content Content string
     */
    void setDoneDataContent(const std::string &content) override;

    /**
     * @brief Add <param> element to <donedata>
     * @param name Parameter name
     * @param location Data model location path
     */
    void addDoneDataParam(const std::string &name, const std::string &location) override;

    void clearDoneDataParams() override;

    /**
     * @brief Return transition object of initial element
     * @return Pointer to initial transition object, nullptr if no initial element
     */
    virtual std::shared_ptr<ITransitionNode> getInitialTransition() const override;

    /**
     * @brief Set transition object of initial element
     * @param transition Initial transition object
     */
    virtual void setInitialTransition(std::shared_ptr<ITransitionNode> transition) override;

private:
    std::string id_;
    Type type_;
    IStateNode *parent_;
    HistoryType historyType_ = HistoryType::NONE;
    std::vector<std::shared_ptr<IStateNode>> children_;
    std::vector<std::shared_ptr<ITransitionNode>> transitions_;
    std::vector<std::shared_ptr<IDataModelItem>> dataItems_;
    std::string initialState_;
    std::string onEntry_;
    std::string onExit_;

    // New action system (IActionNode-based)

    // W3C SCXML 3.8/3.9: Block-based action storage for proper handler isolation
    std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> entryActionBlocks_;
    std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> exitActionBlocks_;

    std::vector<std::shared_ptr<IInvokeNode>> invokes_;
    DoneData doneData_;
    std::vector<std::string> reactiveGuards_;
    std::shared_ptr<ITransitionNode> initialTransition_;
};

}  // namespace RSM