#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/IfActionNode.h"
#include "core/actions/LogActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 If Action Node 테스트
class IfActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        ifAction = std::make_shared<SCXML::Core::IfActionNode>("if1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::IfActionNode> ifAction;
};

// SCXML 사양 3.12.1: <if> 기본 조건 테스트
TEST_F(IfActionNodeTest, BasicIfCondition)
{
    // SCXML 사양: if 요소는 cond 속성이 필수
    ifAction->setIfCondition("x > 0");
    
    EXPECT_EQ("x > 0", ifAction->getIfCondition());
    EXPECT_EQ("if", ifAction->getActionType());
    EXPECT_EQ(1, ifAction->getBranchCount()); // if branch만 있음
    EXPECT_FALSE(ifAction->hasElseBranch());
}

// SCXML 사양 3.12.1: <if>-<elseif>-<else> 구조 테스트
TEST_F(IfActionNodeTest, CompleteConditionalStructure)
{
    // if branch 설정
    ifAction->setIfCondition("state == 'ready'");
    
    // elseif branch 추가
    auto& elseif1 = ifAction->addElseIfBranch("state == 'waiting'");
    auto& elseif2 = ifAction->addElseIfBranch("state == 'error'");
    (void)elseif1; // suppress unused warning
    (void)elseif2; // suppress unused warning
    
    // else branch 추가
    auto& elseBranch = ifAction->addElseBranch();
    (void)elseBranch; // suppress unused warning
    
    // 구조 검증
    EXPECT_EQ(4, ifAction->getBranchCount()); // if + 2 elseif + else
    EXPECT_TRUE(ifAction->hasElseBranch());
    
    const auto& branches = ifAction->getBranches();
    EXPECT_EQ("state == 'ready'", branches[0].condition);
    EXPECT_FALSE(branches[0].isElseBranch);
    
    EXPECT_EQ("state == 'waiting'", branches[1].condition);
    EXPECT_FALSE(branches[1].isElseBranch);
    
    EXPECT_EQ("state == 'error'", branches[2].condition);
    EXPECT_FALSE(branches[2].isElseBranch);
    
    EXPECT_EQ("", branches[3].condition); // else branch has no condition
    EXPECT_TRUE(branches[3].isElseBranch);
}

// SCXML 사양 3.12.1: 조건부 실행 콘텐츠
TEST_F(IfActionNodeTest, ConditionalExecutableContent)
{
    // if branch에 액션 추가
    ifAction->setIfCondition("x > 0");
    auto logAction1 = std::make_shared<SCXML::Core::LogActionNode>("log1");
    logAction1->setExpr("'x is positive'");
    ifAction->addIfAction(logAction1);
    
    // elseif branch에 액션 추가
    auto& elseifBranch = ifAction->addElseIfBranch("x < 0");
    auto logAction2 = std::make_shared<SCXML::Core::LogActionNode>("log2");
    logAction2->setExpr("'x is negative'");
    elseifBranch.actions.push_back(logAction2);
    
    // else branch에 액션 추가
    auto& elseBranch = ifAction->addElseBranch();
    auto logAction3 = std::make_shared<SCXML::Core::LogActionNode>("log3");
    logAction3->setExpr("'x is zero'");
    elseBranch.actions.push_back(logAction3);
    
    // 검증
    const auto& branches = ifAction->getBranches();
    EXPECT_EQ(1, branches[0].actions.size()); // if branch
    EXPECT_EQ(1, branches[1].actions.size()); // elseif branch
    EXPECT_EQ(1, branches[2].actions.size()); // else branch
}

// SCXML 사양 3.12.1: 조건 표현식 형식
TEST_F(IfActionNodeTest, ConditionExpressionFormats)
{
    // Boolean 리터럴
    ifAction->setIfCondition("true");
    EXPECT_EQ("true", ifAction->getIfCondition());
    
    ifAction->setIfCondition("false");
    EXPECT_EQ("false", ifAction->getIfCondition());
    
    // 변수 비교
    ifAction->setIfCondition("currentState == 'active'");
    EXPECT_EQ("currentState == 'active'", ifAction->getIfCondition());
    
    // 수치 비교
    ifAction->setIfCondition("count >= 10");
    EXPECT_EQ("count >= 10", ifAction->getIfCondition());
    
    // 복합 조건
    ifAction->setIfCondition("(x > 0) && (y < 100)");
    EXPECT_EQ("(x > 0) && (y < 100)", ifAction->getIfCondition());
    
    // 함수 호출
    ifAction->setIfCondition("In('stateA')");
    EXPECT_EQ("In('stateA')", ifAction->getIfCondition());
}

// SCXML 사양: Branch 인덱싱과 액션 추가
TEST_F(IfActionNodeTest, BranchIndexingAndActions)
{
    // 구조 생성
    ifAction->setIfCondition("branch == 0");
    ifAction->addElseIfBranch("branch == 1");
    ifAction->addElseIfBranch("branch == 2");
    ifAction->addElseBranch();
    
    // 각 branch에 액션 추가
    auto action0 = std::make_shared<SCXML::Core::LogActionNode>("action0");
    auto action1 = std::make_shared<SCXML::Core::LogActionNode>("action1");
    auto action2 = std::make_shared<SCXML::Core::LogActionNode>("action2");
    auto action3 = std::make_shared<SCXML::Core::LogActionNode>("action3");
    
    ifAction->addActionToBranch(0, action0); // if
    ifAction->addActionToBranch(1, action1); // elseif1
    ifAction->addActionToBranch(2, action2); // elseif2  
    ifAction->addActionToBranch(3, action3); // else
    
    // 검증
    const auto& branches = ifAction->getBranches();
    EXPECT_EQ(4, branches.size());
    EXPECT_EQ(1, branches[0].actions.size());
    EXPECT_EQ(1, branches[1].actions.size());
    EXPECT_EQ(1, branches[2].actions.size());
    EXPECT_EQ(1, branches[3].actions.size());
}

// SCXML 사양: clone 기능 테스트
TEST_F(IfActionNodeTest, CloneFunctionality)
{
    // 복잡한 if 구조 생성
    ifAction->setIfCondition("original == true");
    auto logAction = std::make_shared<SCXML::Core::LogActionNode>("original_log");
    logAction->setExpr("'original branch'");
    ifAction->addIfAction(logAction);
    
    auto& elseifBranch = ifAction->addElseIfBranch("original == false");
    auto elseifAction = std::make_shared<SCXML::Core::LogActionNode>("elseif_log");
    elseifAction->setExpr("'elseif branch'");
    elseifBranch.actions.push_back(elseifAction);
    
    auto& elseBranch = ifAction->addElseBranch();
    auto elseAction = std::make_shared<SCXML::Core::LogActionNode>("else_log");
    elseAction->setExpr("'else branch'");
    elseBranch.actions.push_back(elseAction);
    
    // Clone 생성
    auto cloned = ifAction->clone();
    auto clonedIf = std::dynamic_pointer_cast<SCXML::Core::IfActionNode>(cloned);
    
    ASSERT_TRUE(clonedIf != nullptr);
    
    // 구조 비교
    EXPECT_EQ(ifAction->getBranchCount(), clonedIf->getBranchCount());
    EXPECT_EQ(ifAction->hasElseBranch(), clonedIf->hasElseBranch());
    EXPECT_EQ(ifAction->getIfCondition(), clonedIf->getIfCondition());
    
    const auto& originalBranches = ifAction->getBranches();
    const auto& clonedBranches = clonedIf->getBranches();
    
    for (size_t i = 0; i < originalBranches.size(); ++i) {
        EXPECT_EQ(originalBranches[i].condition, clonedBranches[i].condition);
        EXPECT_EQ(originalBranches[i].isElseBranch, clonedBranches[i].isElseBranch);
        EXPECT_EQ(originalBranches[i].actions.size(), clonedBranches[i].actions.size());
    }
    
    // 서로 다른 객체임을 확인
    EXPECT_NE(ifAction.get(), clonedIf.get());
}

// SCXML 사양: 검증 기능 테스트
TEST_F(IfActionNodeTest, ValidationFunctionality)
{
    // 유효한 구조
    ifAction->setIfCondition("valid == true");
    auto validationErrors = ifAction->validate();
    EXPECT_TRUE(validationErrors.empty());
    
    // if 조건이 없는 경우 (유효하지 않음)
    auto invalidIf = std::make_shared<SCXML::Core::IfActionNode>("invalid");
    auto invalidErrors = invalidIf->validate();
    EXPECT_FALSE(invalidErrors.empty()); // 오류가 있어야 함
}

// SCXML 사양 3.12.1: 중첩된 조건부 구조
TEST_F(IfActionNodeTest, NestedConditionalStructures)
{
    // 외부 if 구조
    ifAction->setIfCondition("outerCondition == true");
    
    // 중첩된 if를 외부 if의 액션으로 추가
    auto nestedIf = std::make_shared<SCXML::Core::IfActionNode>("nested");
    nestedIf->setIfCondition("innerCondition == true");
    
    auto innerLog = std::make_shared<SCXML::Core::LogActionNode>("inner");
    innerLog->setExpr("'nested condition met'");
    nestedIf->addIfAction(innerLog);
    
    ifAction->addIfAction(nestedIf);
    
    // 검증
    const auto& branches = ifAction->getBranches();
    EXPECT_EQ(1, branches[0].actions.size());
    
    // 중첩된 if가 올바르게 추가되었는지 확인
    auto nestedAction = branches[0].actions[0];
    auto retrievedNestedIf = std::dynamic_pointer_cast<SCXML::Core::IfActionNode>(nestedAction);
    EXPECT_TRUE(retrievedNestedIf != nullptr);
    if (retrievedNestedIf) {
        EXPECT_EQ("innerCondition == true", retrievedNestedIf->getIfCondition());
    }
}

// SCXML 사양: else branch 중복 방지
TEST_F(IfActionNodeTest, ElseBranchUniqueness)
{
    // 먼저 if 조건 설정 (SCXML 사양 준수)
    ifAction->setIfCondition("test == true");
    
    // 첫 번째 else branch 추가
    auto& elseBranch1 = ifAction->addElseBranch();
    (void)elseBranch1; // suppress unused warning
    EXPECT_TRUE(ifAction->hasElseBranch());
    EXPECT_EQ(2, ifAction->getBranchCount()); // if + else
    
    // 두 번째 else branch 추가 시도 - 기존 else 반환되어야 함
    auto& elseBranch2 = ifAction->addElseBranch();
    (void)elseBranch2; // suppress unused warning
    
    // else branch는 여전히 하나만 있어야 함
    const auto& branches = ifAction->getBranches();
    int elseCount = 0;
    for (const auto& branch : branches) {
        if (branch.isElseBranch) {
            elseCount++;
        }
    }
    EXPECT_EQ(elseCount, 1); // else branch는 정확히 1개
    EXPECT_EQ(2, ifAction->getBranchCount()); // 여전히 if + else = 2개
}

// SCXML 사양 위반: else without if should throw exception
TEST_F(IfActionNodeTest, ElseWithoutIfShouldThrow)
{
    // if 조건 설정 없이 바로 else 추가 시도 - 예외 발생해야 함
    EXPECT_THROW(ifAction->addElseBranch(), std::invalid_argument);
    
    // 예외 발생 후에도 브랜치는 여전히 비어있어야 함
    EXPECT_EQ(0, ifAction->getBranchCount());
    EXPECT_FALSE(ifAction->hasElseBranch());
}

// SCXML 사양: 빈 조건부 블록 처리
TEST_F(IfActionNodeTest, EmptyConditionalBlocks)
{
    // 액션이 없는 branch들
    ifAction->setIfCondition("emptyCondition == true");
    ifAction->addElseIfBranch("anotherEmpty == true");
    ifAction->addElseBranch();
    
    // 구조는 유효하지만 액션이 없음
    const auto& branches = ifAction->getBranches();
    EXPECT_EQ(3, branches.size());
    EXPECT_TRUE(branches[0].actions.empty());
    EXPECT_TRUE(branches[1].actions.empty());  
    EXPECT_TRUE(branches[2].actions.empty());
    
    // 검증에서 경고는 있을 수 있지만 오류는 아님
    auto warnings = ifAction->validate();
    // 빈 블록은 기술적으로 유효함 (구현에 따라 경고 처리)
}