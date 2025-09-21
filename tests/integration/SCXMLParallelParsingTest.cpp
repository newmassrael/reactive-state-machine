#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>

namespace RSM {
namespace Tests {

class SCXMLParallelParsingTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        sessionId_ = "scxml_parallel_parsing_test_session";
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

// 최소한의 parallel 상태 파싱 테스트
TEST_F(SCXMLParallelParsingTest, MinimalParallelStateParsing) {
    const std::string minimalParallelSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1"/>
            <state id="region2"/>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(minimalParallelSCXML);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "parallel1");
    EXPECT_EQ(model->getDatamodel(), "ecmascript");
}

// 복잡한 parallel 상태 구조 파싱 테스트
TEST_F(SCXMLParallelParsingTest, ComplexParallelStructureParsing) {
    const std::string complexParallelSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="main_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="counter" expr="0"/>
        </datamodel>
        
        <parallel id="main_parallel">
            <state id="worker1">
                <onentry>
                    <script>counter++;</script>
                </onentry>
                <transition event="finish" target="done1"/>
                <final id="done1"/>
            </state>
            
            <state id="worker2">
                <onentry>
                    <script>counter++;</script>
                </onentry>
                <transition event="finish" target="done2"/>
                <final id="done2"/>
            </state>
            
            <state id="monitor">
                <transition event="timeout" target="timeout_final"/>
                <final id="timeout_final"/>
            </state>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(complexParallelSCXML);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "main_parallel");
    EXPECT_EQ(model->getDatamodel(), "ecmascript");
}

// 중첩된 parallel 상태 파싱 테스트
TEST_F(SCXMLParallelParsingTest, NestedParallelStateParsing) {
    const std::string nestedParallelSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="outer_parallel" datamodel="ecmascript">
        <parallel id="outer_parallel">
            <state id="outer_region1">
                <parallel id="inner_parallel1">
                    <state id="inner1_region1"/>
                    <state id="inner1_region2"/>
                </parallel>
            </state>
            
            <state id="outer_region2">
                <parallel id="inner_parallel2">
                    <state id="inner2_region1"/>
                    <state id="inner2_region2"/>
                </parallel>
            </state>
        </parallel>
    </scxml>)";

    auto model = parser_->parseContent(nestedParallelSCXML);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "outer_parallel");
}

// 잘못된 parallel 상태 구조 파싱 테스트
TEST_F(SCXMLParallelParsingTest, InvalidParallelStateParsing) {
    const std::string invalidParallelSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="invalid_parallel">
        <parallel id="invalid_parallel">
            <!-- parallel 상태에는 최소 하나의 자식 상태가 있어야 함 -->
        </parallel>
    </scxml>)";

    // 파싱이 실패하거나 경고와 함께 성공할 수 있음
    // 중요한 것은 크래시가 발생하지 않는 것
    try {
        auto model = parser_->parseContent(invalidParallelSCXML);
        // 파싱이 성공하면 검증 계속 진행
        if (model) {
            EXPECT_EQ(model->getInitialState(), "invalid_parallel");
        }
    } catch (const std::exception &e) {
        // 파싱 실패는 예상되는 결과
        EXPECT_FALSE(std::string(e.what()).empty());
    }
}

}  // namespace Tests
}  // namespace RSM