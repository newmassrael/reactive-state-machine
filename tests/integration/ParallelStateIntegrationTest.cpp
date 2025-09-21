#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>

namespace RSM {
namespace Tests {

class ParallelStateIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        sessionId_ = "parallel_integration_test_session";
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
            engine_->shutdown();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::string sessionId_;
};

// W3C SCXML parallel 상태 기본 파싱 테스트
TEST_F(ParallelStateIntegrationTest, BasicParallelStateParsing) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parallel1">
        <parallel id="parallel1">
            <state id="region1"/>
            <state id="region2"/>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "parallel1");
}

// parallel 상태의 최종 상태 테스트
TEST_F(ParallelStateIntegrationTest, ParallelStateWithFinalStates) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parallel1">
        <parallel id="parallel1">
            <state id="region1">
                <transition event="done.state.region1" target="final1"/>
                <final id="final1"/>
            </state>
            <state id="region2">
                <transition event="done.state.region2" target="final2"/>
                <final id="final2"/>
            </state>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "parallel1");
}

// 중첩된 parallel 상태 테스트
TEST_F(ParallelStateIntegrationTest, NestedParallelStates) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="outer">
        <parallel id="outer">
            <state id="region1">
                <parallel id="inner1">
                    <state id="inner1_region1"/>
                    <state id="inner1_region2"/>
                </parallel>
            </state>
            <state id="region2"/>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "outer");
}

// 데이터 모델이 포함된 parallel 상태 테스트
TEST_F(ParallelStateIntegrationTest, ParallelStateWithDataModel) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <datamodel>
            <data id="region1_status" expr="'inactive'"/>
            <data id="region2_status" expr="'inactive'"/>
        </datamodel>
        <parallel id="parallel1">
            <state id="region1">
                <onentry>
                    <script>region1_status = 'active';</script>
                </onentry>
            </state>
            <state id="region2">
                <onentry>
                    <script>region2_status = 'active';</script>
                </onentry>
            </state>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "parallel1");
    EXPECT_EQ(model->getDatamodel(), "ecmascript");
}

// 잘못된 parallel 상태 구성 테스트
TEST_F(ParallelStateIntegrationTest, InvalidParallelStateConfiguration) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parallel1">
        <parallel id="parallel1">
            <!-- 잘못된 구성: parallel은 자식 상태가 있어야 함 -->
        </parallel>
    </scxml>)";

    // 파싱이 실패하거나 경고와 함께 성공할 수 있음
    // 중요한 것은 크래시가 발생하지 않는 것
    auto model = parser_->parseContent(scxmlContent);
    if (model) {
        EXPECT_EQ(model->getInitialState(), "parallel1");
    }
}

}  // namespace Tests
}  // namespace RSM