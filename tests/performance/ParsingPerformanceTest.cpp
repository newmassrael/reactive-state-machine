#include "PerformanceTestCommon.h"

// Parsing performance tests
class ParsingPerformanceTest : public PerformanceTestBase
{
};

// Test parsing performance with large SCXML documents
TEST_F(ParsingPerformanceTest, LargeDocumentParsing)
{
    // Generate a large SCXML document with many states
    std::string largeScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="state0">)";
    
    // Add 500 states with transitions (reduced for more consistent performance)
    for (int i = 0; i < 500; i++) {
        largeScxml += R"(
      <state id="state)" + std::to_string(i) + R"(">
        <transition event="next" target="state)" + std::to_string((i + 1) % 500) + R"("/>
      </state>)";
    }
    largeScxml += "\n    </scxml>";
    
    measureParsingTime(largeScxml, "LargeDocument_500States");
    
    auto model = parser->parseContent(largeScxml);
    EXPECT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());
}

// Test memory usage during parsing
TEST_F(ParsingPerformanceTest, MemoryUsageDuringParsing)
{
    recordMemoryUsage();
    
    std::string complexScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="complex">
      <datamodel>
        <data id="counter" expr="0"/>
        <data id="result" expr="{}"/>
      </datamodel>
      <state id="complex">
        <onentry>
          <assign location="counter" expr="100"/>
          <assign location="result" expr="{ processed: true }"/>
        </onentry>
        <transition event="next" target="final"/>
      </state>
      <final id="final"/>
    </scxml>)";
    
    auto model = parser->parseContent(complexScxml);
    
    recordMemoryUsage();
    
    EXPECT_TRUE(model != nullptr);
}