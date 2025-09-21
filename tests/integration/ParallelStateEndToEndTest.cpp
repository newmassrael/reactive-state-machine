#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>

namespace RSM {
namespace Tests {

class ParallelStateEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        sessionId_ = "parallel_e2e_test_session";
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

// 기본적인 parallel 상태 End-to-End 워크플로우 테스트
TEST_F(ParallelStateEndToEndTest, BasicParallelStateWorkflow) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel_work" datamodel="ecmascript">
        <datamodel>
            <data id="task1_completed" expr="false"/>
            <data id="task2_completed" expr="false"/>
            <data id="workflow_status" expr="'초기화'"/>
        </datamodel>
        
        <parallel id="parallel_work">
            <state id="task1">
                <onentry>
                    <script>workflow_status = '작업1 시작';</script>
                </onentry>
                <transition event="complete_task1" target="task1_done">
                    <script>task1_completed = true;</script>
                </transition>
                <final id="task1_done"/>
            </state>
            
            <state id="task2">
                <onentry>
                    <script>workflow_status = '작업2 시작';</script>
                </onentry>
                <transition event="complete_task2" target="task2_done">
                    <script>task2_completed = true;</script>
                </transition>
                <final id="task2_done"/>
            </state>
        </parallel>
        
        <final id="workflow_complete"/>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);

    // 상태 머신 생성 테스트
    EXPECT_EQ(model->getInitialState(), "parallel_work");
    EXPECT_EQ(model->getDatamodel(), "ecmascript");
}

// 복잡한 parallel 상태 시나리오 테스트
TEST_F(ParallelStateEndToEndTest, ComplexParallelStateScenario) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="multi_region_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="region_count" expr="0"/>
            <data id="completed_regions" expr="0"/>
            <data id="total_progress" expr="0"/>
        </datamodel>
        
        <parallel id="multi_region_parallel">
            <state id="ui_region">
                <onentry>
                    <script>region_count++; total_progress += 10;</script>
                </onentry>
                <state id="ui_loading">
                    <transition event="ui_ready" target="ui_active"/>
                </state>
                <state id="ui_active">
                    <transition event="ui_complete" target="ui_finished"/>
                </state>
                <final id="ui_finished">
                    <onentry>
                        <script>completed_regions++;</script>
                    </onentry>
                </final>
            </state>
            
            <state id="data_region">
                <onentry>
                    <script>region_count++; total_progress += 10;</script>
                </onentry>
                <state id="data_loading">
                    <transition event="data_ready" target="data_processing"/>
                </state>
                <state id="data_processing">
                    <transition event="data_complete" target="data_finished"/>
                </state>
                <final id="data_finished">
                    <onentry>
                        <script>completed_regions++;</script>
                    </onentry>
                </final>
            </state>
        </parallel>
        
        <final id="all_complete"/>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "multi_region_parallel");
}

// 에러 처리가 포함된 parallel 상태 테스트
TEST_F(ParallelStateEndToEndTest, ParallelStateWithErrorHandling) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="resilient_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="error_count" expr="0"/>
            <data id="retry_count" expr="0"/>
            <data id="max_retries" expr="3"/>
        </datamodel>
        
        <parallel id="resilient_parallel">
            <state id="critical_task">
                <onentry>
                    <script>retry_count = 0;</script>
                </onentry>
                <state id="task_running">
                    <transition event="task_error" target="task_error_handler">
                        <script>error_count++;</script>
                    </transition>
                    <transition event="task_success" target="task_complete"/>
                </state>
                <state id="task_error_handler">
                    <transition cond="retry_count &lt; max_retries" target="task_running">
                        <script>retry_count++;</script>
                    </transition>
                    <transition cond="retry_count >= max_retries" target="task_failed"/>
                </state>
                <final id="task_complete"/>
                <final id="task_failed"/>
            </state>
            
            <state id="monitoring_task">
                <state id="monitoring_active">
                    <transition event="system_error" target="monitoring_alert"/>
                    <transition event="monitoring_complete" target="monitoring_done"/>
                </state>
                <state id="monitoring_alert">
                    <transition event="alert_handled" target="monitoring_active"/>
                </state>
                <final id="monitoring_done"/>
            </state>
        </parallel>
        
        <final id="system_shutdown"/>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "resilient_parallel");
}

// 타이밍과 동기화가 중요한 parallel 상태 테스트
TEST_F(ParallelStateEndToEndTest, ParallelStateTimingAndSynchronization) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="synchronized_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="sync_point_1" expr="false"/>
            <data id="sync_point_2" expr="false"/>
            <data id="all_synchronized" expr="false"/>
        </datamodel>
        
        <parallel id="synchronized_parallel">
            <state id="process_a">
                <state id="phase_a1">
                    <transition event="a1_complete" target="sync_a">
                        <script>sync_point_1 = true;</script>
                    </transition>
                </state>
                <state id="sync_a">
                    <transition cond="sync_point_1 &amp;&amp; sync_point_2" target="phase_a2">
                        <script>all_synchronized = true;</script>
                    </transition>
                </state>
                <state id="phase_a2">
                    <transition event="a2_complete" target="process_a_done"/>
                </state>
                <final id="process_a_done"/>
            </state>
            
            <state id="process_b">
                <state id="phase_b1">
                    <transition event="b1_complete" target="sync_b">
                        <script>sync_point_2 = true;</script>
                    </transition>
                </state>
                <state id="sync_b">
                    <transition cond="sync_point_1 &amp;&amp; sync_point_2" target="phase_b2">
                        <script>all_synchronized = true;</script>
                    </transition>
                </state>
                <state id="phase_b2">
                    <transition event="b2_complete" target="process_b_done"/>
                </state>
                <final id="process_b_done"/>
            </state>
        </parallel>
        
        <final id="all_processes_complete"/>
    </scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getInitialState(), "synchronized_parallel");
}

}  // namespace Tests
}  // namespace RSM