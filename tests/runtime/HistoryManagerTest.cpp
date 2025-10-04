#include "runtime/HistoryManager.h"
#include "actions/IActionNode.h"
#include "model/DoneData.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/DeepHistoryFilter.h"
#include "runtime/HistoryManager.h"
#include "runtime/HistoryValidator.h"
#include "runtime/ShallowHistoryFilter.h"
#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace RSM;

/**
 * @brief Mock StateNode implementation for History States testing
 *
 * SCXML W3C Specification Section 3.6 - History States
 * This mock provides the necessary state hierarchy for testing history behavior
 */
class MockStateNode : public IStateNode {
public:
    MockStateNode(const std::string &id, Type type, IStateNode *parent = nullptr)
        : id_(id), type_(type), parent_(parent) {}

    // Basic state identification
    const std::string &getId() const override {
        return id_;
    }

    Type getType() const override {
        return type_;
    }

    // Parent-child relationships
    void setParent(IStateNode *parent) override {
        parent_ = parent;
    }

    IStateNode *getParent() const override {
        return parent_;
    }

    void addChild(std::shared_ptr<IStateNode> child) override {
        children_.push_back(child);
        child->setParent(this);
    }

    const std::vector<std::shared_ptr<IStateNode>> &getChildren() const override {
        return children_;
    }

    // Transitions (mock implementation)
    void addTransition(std::shared_ptr<ITransitionNode> transition) override {
        transitions_.push_back(transition);
    }

    const std::vector<std::shared_ptr<ITransitionNode>> &getTransitions() const override {
        return transitions_;
    }

    // Data model (mock implementation)
    void addDataItem(std::shared_ptr<IDataModelItem> dataItem) override {
        dataItems_.push_back(dataItem);
    }

    const std::vector<std::shared_ptr<IDataModelItem>> &getDataItems() const override {
        return dataItems_;
    }

    // Entry/Exit callbacks
    void setOnEntry(const std::string &callback) override {
        onEntry_ = callback;
    }

    const std::string &getOnEntry() const override {
        return onEntry_;
    }

    void setOnExit(const std::string &callback) override {
        onExit_ = callback;
    }

    const std::string &getOnExit() const override {
        return onExit_;
    }

    // Initial state
    void setInitialState(const std::string &state) override {
        initialState_ = state;
    }

    const std::string &getInitialState() const override {
        return initialState_;
    }

    // W3C SCXML 3.8/3.9: Block-based action methods
    void addEntryActionBlock(std::vector<std::shared_ptr<IActionNode>> block) override {
        entryActionBlocks_.push_back(std::move(block));
    }

    const std::vector<std::vector<std::shared_ptr<IActionNode>>> &getEntryActionBlocks() const override {
        return entryActionBlocks_;
    }

    void addExitActionBlock(std::vector<std::shared_ptr<IActionNode>> block) override {
        exitActionBlocks_.push_back(std::move(block));
    }

    const std::vector<std::vector<std::shared_ptr<IActionNode>>> &getExitActionBlocks() const override {
        return exitActionBlocks_;
    }

    // History support
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

    // Other required methods (mock implementation)
    void addInvoke(std::shared_ptr<IInvokeNode> invoke) override {
        invokes_.push_back(invoke);
    }

    const std::vector<std::shared_ptr<IInvokeNode>> &getInvoke() const override {
        return invokes_;
    }

    void addReactiveGuard(const std::string &guardId) override {
        reactiveGuards_.push_back(guardId);
    }

    const std::vector<std::string> &getReactiveGuards() const override {
        return reactiveGuards_;
    }

    bool isFinalState() const override {
        return type_ == Type::FINAL;
    }

    // DoneData support
    const DoneData &getDoneData() const override {
        return doneData_;
    }

    DoneData &getDoneData() override {
        return doneData_;
    }

    void setDoneDataContent(const std::string &content) override {
        doneData_.setContent(content);
    }

    void addDoneDataParam(const std::string &name, const std::string &location) override {
        doneData_.addParam(name, location);
    }

    void clearDoneDataParams() override {
        doneData_.clearParams();
    }

    // Initial transition
    std::shared_ptr<ITransitionNode> getInitialTransition() const override {
        return initialTransition_;
    }

    void setInitialTransition(std::shared_ptr<ITransitionNode> transition) override {
        initialTransition_ = transition;
    }

private:
    std::string id_;
    Type type_;
    IStateNode *parent_ = nullptr;
    std::vector<std::shared_ptr<IStateNode>> children_;
    std::vector<std::shared_ptr<ITransitionNode>> transitions_;
    std::vector<std::shared_ptr<IDataModelItem>> dataItems_;
    std::vector<std::shared_ptr<IInvokeNode>> invokes_;
    std::string onEntry_;
    std::string onExit_;
    std::string initialState_;
    std::vector<std::vector<std::shared_ptr<IActionNode>>> entryActionBlocks_;
    std::vector<std::vector<std::shared_ptr<IActionNode>>> exitActionBlocks_;
    std::vector<std::string> reactiveGuards_;
    HistoryType historyType_ = HistoryType::NONE;
    DoneData doneData_;
    std::shared_ptr<ITransitionNode> initialTransition_;
};

/**
 * @brief Comprehensive History Manager Test Suite
 *
 * Tests SOLID architecture implementation of W3C SCXML History States
 * Covers both shallow and deep history behaviors according to specification
 */
class HistoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock state provider function
        stateProvider_ = [this](const std::string &stateId) -> std::shared_ptr<IStateNode> {
            auto it = mockStates_.find(stateId);
            return (it != mockStates_.end()) ? it->second : nullptr;
        };

        // Create SOLID dependency injection components
        auto shallowFilter = std::make_unique<ShallowHistoryFilter>(stateProvider_);
        auto deepFilter = std::make_unique<DeepHistoryFilter>(stateProvider_);
        auto validator = std::make_unique<HistoryValidator>(stateProvider_);

        // Initialize History Manager with dependency injection
        historyManager_ = std::make_unique<HistoryManager>(stateProvider_, std::move(shallowFilter),
                                                           std::move(deepFilter), std::move(validator));

        setupMockStateHierarchy();
    }

    void TearDown() override {
        mockStates_.clear();
        historyManager_.reset();
    }

    /**
     * @brief Setup W3C SCXML compliant state hierarchy for testing
     *
     * Creates the following hierarchy:
     * root (compound)
     *   ├── stateA (compound)
     *   │   ├── stateA1 (atomic)
     *   │   ├── stateA2 (atomic)
     *   │   └── historyA (shallow history)
     *   ├── stateB (compound)
     *   │   ├── stateB1 (compound)
     *   │   │   ├── stateB1a (atomic)
     *   │   │   └── stateB1b (atomic)
     *   │   ├── stateB2 (atomic)
     *   │   └── historyB (deep history)
     *   └── historyRoot (deep history)
     */
    void setupMockStateHierarchy() {
        // Root compound state
        auto root = std::make_shared<MockStateNode>("root", Type::COMPOUND);
        mockStates_["root"] = root;

        // State A - compound with shallow history
        auto stateA = std::make_shared<MockStateNode>("stateA", Type::COMPOUND, root.get());
        auto stateA1 = std::make_shared<MockStateNode>("stateA1", Type::ATOMIC, stateA.get());
        auto stateA2 = std::make_shared<MockStateNode>("stateA2", Type::ATOMIC, stateA.get());
        auto historyA = std::make_shared<MockStateNode>("historyA", Type::HISTORY, stateA.get());

        root->addChild(stateA);
        root->addChild(std::make_shared<MockStateNode>("historyRoot", Type::HISTORY, root.get()));
        stateA->addChild(stateA1);
        stateA->addChild(stateA2);
        stateA->addChild(historyA);

        mockStates_["stateA"] = stateA;
        mockStates_["stateA1"] = stateA1;
        mockStates_["stateA2"] = stateA2;
        mockStates_["historyA"] = historyA;

        // State B - compound with deep history and nested states
        auto stateB = std::make_shared<MockStateNode>("stateB", Type::COMPOUND, root.get());
        auto stateB1 = std::make_shared<MockStateNode>("stateB1", Type::COMPOUND, stateB.get());
        auto stateB1a = std::make_shared<MockStateNode>("stateB1a", Type::ATOMIC, stateB1.get());
        auto stateB1b = std::make_shared<MockStateNode>("stateB1b", Type::ATOMIC, stateB1.get());
        auto stateB2 = std::make_shared<MockStateNode>("stateB2", Type::ATOMIC, stateB.get());
        auto historyB = std::make_shared<MockStateNode>("historyB", Type::HISTORY, stateB.get());

        root->addChild(stateB);
        stateB->addChild(stateB1);
        stateB->addChild(stateB2);
        stateB->addChild(historyB);
        stateB1->addChild(stateB1a);
        stateB1->addChild(stateB1b);

        mockStates_["stateB"] = stateB;
        mockStates_["stateB1"] = stateB1;
        mockStates_["stateB1a"] = stateB1a;
        mockStates_["stateB1b"] = stateB1b;
        mockStates_["stateB2"] = stateB2;
        mockStates_["historyB"] = historyB;

        // Root level deep history
        auto historyRoot = std::make_shared<MockStateNode>("historyRoot", Type::HISTORY, root.get());
        mockStates_["historyRoot"] = historyRoot;
    }

    std::unique_ptr<HistoryManager> historyManager_;
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;
    std::unordered_map<std::string, std::shared_ptr<MockStateNode>> mockStates_;
};

// ============================================================================
// SOLID Architecture Tests
// ============================================================================

TEST_F(HistoryManagerTest, SOLID_DependencyInjection_InitializationSuccess) {
    // Test that SOLID dependency injection works correctly
    EXPECT_TRUE(historyManager_ != nullptr);

    // Verify that all injected dependencies are working
    EXPECT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));
    EXPECT_TRUE(historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1"));
}

TEST_F(HistoryManagerTest, SOLID_InterfaceSegregation_ComponentIsolation) {
    // Test that components work independently (Interface Segregation Principle)
    EXPECT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW));
    EXPECT_TRUE(historyManager_->recordHistory("stateA", {"stateA2"}));

    auto result = historyManager_->restoreHistory("historyA");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.targetStateIds.size(), 1);
    EXPECT_EQ(result.targetStateIds[0], "stateA2");
}

// ============================================================================
// W3C SCXML History State Registration Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_HistoryState_ShallowRegistration_ShouldSucceed) {
    // SCXML W3C Specification: History states must be registered with parent compound state
    bool result = historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1");

    EXPECT_TRUE(result);
    EXPECT_TRUE(historyManager_->isHistoryState("historyA"));
}

TEST_F(HistoryManagerTest, W3C_HistoryState_DeepRegistration_ShouldSucceed) {
    // Test deep history registration with nested state hierarchy
    bool result = historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1");

    EXPECT_TRUE(result);
    EXPECT_TRUE(historyManager_->isHistoryState("historyB"));
}

TEST_F(HistoryManagerTest, W3C_HistoryState_InvalidParent_ShouldFail) {
    // W3C Specification: History states must have valid parent compound states
    bool result = historyManager_->registerHistoryState("invalidHistory", "nonexistentParent", HistoryType::SHALLOW);

    EXPECT_FALSE(result);
    EXPECT_FALSE(historyManager_->isHistoryState("invalidHistory"));
}

TEST_F(HistoryManagerTest, W3C_HistoryState_DuplicateRegistration_ShouldFail) {
    // Test duplicate registration prevention
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW));

    // Second registration should fail
    bool result = historyManager_->registerHistoryState("historyA", "stateA", HistoryType::DEEP);
    EXPECT_FALSE(result);
}

// ============================================================================
// W3C SCXML History Recording Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_HistoryRecording_ShallowHistory_ShouldRecordDirectChildren) {
    // Setup shallow history state
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));

    // Record history - should only capture direct children of stateA
    std::vector<std::string> activeStates = {"stateA2"};
    bool result = historyManager_->recordHistory("stateA", activeStates);

    EXPECT_TRUE(result);
}

TEST_F(HistoryManagerTest, W3C_HistoryRecording_DeepHistory_ShouldRecordAllDescendants) {
    // Setup deep history state
    ASSERT_TRUE(historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1"));

    // Record history with nested active states
    std::vector<std::string> activeStates = {"stateB1", "stateB1a"};
    bool result = historyManager_->recordHistory("stateB", activeStates);

    EXPECT_TRUE(result);
}

TEST_F(HistoryManagerTest, W3C_HistoryRecording_InvalidParent_ShouldFail) {
    // Test recording for non-existent parent state
    std::vector<std::string> activeStates = {"someState"};
    bool result = historyManager_->recordHistory("nonexistentState", activeStates);

    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerTest, W3C_HistoryRecording_EmptyActiveStates_ShouldSucceed) {
    // W3C allows recording empty history (no active children)
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW));

    std::vector<std::string> emptyStates = {};
    bool result = historyManager_->recordHistory("stateA", emptyStates);

    EXPECT_TRUE(result);
}

// ============================================================================
// W3C SCXML History Restoration Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_HistoryRestoration_ShallowHistory_WithPreviousRecord) {
    // Setup and record shallow history
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));
    ASSERT_TRUE(historyManager_->recordHistory("stateA", {"stateA2"}));

    // Restore history
    auto result = historyManager_->restoreHistory("historyA");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.targetStateIds.size(), 1);
    EXPECT_EQ(result.targetStateIds[0], "stateA2");
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(HistoryManagerTest, W3C_HistoryRestoration_ShallowHistory_WithoutPreviousRecord_ShouldUseDefault) {
    // Setup shallow history with default state
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));

    // Restore without previous recording - should use default
    auto result = historyManager_->restoreHistory("historyA");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.targetStateIds.size(), 1);
    EXPECT_EQ(result.targetStateIds[0], "stateA1");
}

TEST_F(HistoryManagerTest, W3C_HistoryRestoration_DeepHistory_WithNestedStates) {
    // W3C SCXML 3.11: Deep history records ONLY active atomic descendants (leaf states)
    // If stateB1 (compound) and stateB1a (atomic) are active, only stateB1a is recorded
    ASSERT_TRUE(historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1"));
    ASSERT_TRUE(historyManager_->recordHistory("stateB", {"stateB1", "stateB1a"}));

    // Restore deep history
    auto result = historyManager_->restoreHistory("historyB");

    EXPECT_TRUE(result.success);
    // W3C Spec: Deep history filters out intermediate compound states, keeps only atomic descendants
    EXPECT_EQ(result.targetStateIds.size(), 1);

    // Should restore only the deepest atomic state (stateB1a), not intermediate compound (stateB1)
    std::vector<std::string> expected = {"stateB1a"};
    EXPECT_EQ(result.targetStateIds, expected);
}

TEST_F(HistoryManagerTest, W3C_HistoryRestoration_NonexistentHistory_ShouldFail) {
    // Test restoration of unregistered history state
    auto result = historyManager_->restoreHistory("nonexistentHistory");

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.targetStateIds.empty());
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// W3C SCXML History Type Differentiation Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_HistoryTypes_ShallowVsDeep_FilteringBehavior) {
    // Setup both shallow and deep history states
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));
    ASSERT_TRUE(historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1"));

    // Record complex nested state configuration
    std::vector<std::string> complexActiveStates = {"stateA2", "stateB1", "stateB1a"};

    ASSERT_TRUE(historyManager_->recordHistory("stateA", complexActiveStates));
    ASSERT_TRUE(historyManager_->recordHistory("stateB", complexActiveStates));

    // Restore shallow history - should only get direct children of stateA
    auto shallowResult = historyManager_->restoreHistory("historyA");
    EXPECT_TRUE(shallowResult.success);
    EXPECT_EQ(shallowResult.targetStateIds.size(), 1);
    EXPECT_EQ(shallowResult.targetStateIds[0], "stateA2");

    // W3C SCXML 3.11: Deep history records ONLY active atomic descendants
    // If stateB1 (compound) and stateB1a (atomic) are active, only stateB1a is recorded
    auto deepResult = historyManager_->restoreHistory("historyB");
    EXPECT_TRUE(deepResult.success);
    EXPECT_EQ(deepResult.targetStateIds.size(), 1);  // Only atomic state

    // Check that deep history contains ONLY the atomic descendant (stateB1a), not compound (stateB1)
    bool hasStateB1 = std::find(deepResult.targetStateIds.begin(), deepResult.targetStateIds.end(), "stateB1") !=
                      deepResult.targetStateIds.end();
    bool hasStateB1a = std::find(deepResult.targetStateIds.begin(), deepResult.targetStateIds.end(), "stateB1a") !=
                       deepResult.targetStateIds.end();
    EXPECT_FALSE(hasStateB1);  // Compound state should NOT be recorded
    EXPECT_TRUE(hasStateB1a);  // Atomic state should be recorded
}

// ============================================================================
// Error Handling and Edge Cases Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_ErrorHandling_InvalidDefaultState_ShouldFail) {
    // Test registration with invalid default state
    bool result =
        historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "nonexistentDefault");

    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerTest, W3C_ErrorHandling_HistoryOfAtomicState_ShouldFail) {
    // W3C: History states only make sense for compound states
    bool result = historyManager_->registerHistoryState("historyAtomic", "stateA1", HistoryType::SHALLOW);

    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerTest, W3C_ThreadSafety_ConcurrentOperations) {
    // Test thread safety of history operations
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple threads performing concurrent operations
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            std::vector<std::string> activeStates = {(i % 2 == 0) ? "stateA1" : "stateA2"};
            if (historyManager_->recordHistory("stateA", activeStates)) {
                auto result = historyManager_->restoreHistory("historyA");
                if (result.success) {
                    successCount++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
        thread.join();
    }

    // All operations should succeed without race conditions
    EXPECT_EQ(successCount.load(), 10);
}

// ============================================================================
// Integration with StateMachine Lifecycle Tests
// ============================================================================

TEST_F(HistoryManagerTest, W3C_StateMachineIntegration_HistoryStateQuery) {
    // Test isHistoryState method for integration
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW));

    EXPECT_TRUE(historyManager_->isHistoryState("historyA"));
    EXPECT_FALSE(historyManager_->isHistoryState("stateA"));
    EXPECT_FALSE(historyManager_->isHistoryState("nonexistent"));
}

TEST_F(HistoryManagerTest, W3C_StateMachineIntegration_MultipleHistoryStates) {
    // Test management of multiple history states simultaneously
    ASSERT_TRUE(historyManager_->registerHistoryState("historyA", "stateA", HistoryType::SHALLOW, "stateA1"));
    ASSERT_TRUE(historyManager_->registerHistoryState("historyB", "stateB", HistoryType::DEEP, "stateB1"));
    ASSERT_TRUE(historyManager_->registerHistoryState("historyRoot", "root", HistoryType::DEEP, "stateA"));

    // Record different histories
    ASSERT_TRUE(historyManager_->recordHistory("stateA", {"stateA2"}));
    // W3C SCXML 3.11: stateB1 (compound) will be filtered out, only stateB1b (atomic) recorded
    ASSERT_TRUE(historyManager_->recordHistory("stateB", {"stateB1", "stateB1b"}));
    // W3C SCXML 3.11: stateA and stateB (compound) filtered out, only stateA1 and stateB2 (atomic) recorded
    ASSERT_TRUE(historyManager_->recordHistory("root", {"stateA", "stateA1", "stateB", "stateB2"}));

    // Verify independent restoration
    auto resultA = historyManager_->restoreHistory("historyA");
    auto resultB = historyManager_->restoreHistory("historyB");
    auto resultRoot = historyManager_->restoreHistory("historyRoot");

    EXPECT_TRUE(resultA.success);
    EXPECT_TRUE(resultB.success);
    EXPECT_TRUE(resultRoot.success);

    // Shallow history: direct child only
    EXPECT_EQ(resultA.targetStateIds, std::vector<std::string>{"stateA2"});

    // Deep history for stateB: only atomic descendant (stateB1b)
    EXPECT_EQ(resultB.targetStateIds.size(), 1);
    EXPECT_EQ(resultB.targetStateIds[0], "stateB1b");

    // Deep history for root: only atomic descendants (stateA1, stateB2)
    EXPECT_EQ(resultRoot.targetStateIds.size(), 2);
    EXPECT_TRUE(std::find(resultRoot.targetStateIds.begin(), resultRoot.targetStateIds.end(), "stateA1") !=
                resultRoot.targetStateIds.end());
    EXPECT_TRUE(std::find(resultRoot.targetStateIds.begin(), resultRoot.targetStateIds.end(), "stateB2") !=
                resultRoot.targetStateIds.end());
}