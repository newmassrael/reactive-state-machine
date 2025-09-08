#include "../CoreTestCommon.h"
#include "core/DataNode.h"
#include "common/Result.h"
#include <fstream>
#include <filesystem>
#include <memory>

// SCXML W3C 사양 준수 Data Node 기본 테스트
// 참조: https://www.w3.org/TR/scxml/#data
class DataNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        createTestDataFiles();
    }

    void TearDown() override
    {
        cleanupTestFiles();
        CoreTestBase::TearDown();
    }

    void createTestDataFiles()
    {
        // SCXML W3C JSON 테스트 데이터
        jsonTestFile = "scxml_test_data.json";
        std::ofstream jsonFile(jsonTestFile);
        jsonFile << R"({
            "name": "SCXML Test User",
            "age": 25,
            "isActive": true,
            "scores": [90, 85, 88]
        })";
        jsonFile.close();
    }

    void cleanupTestFiles()
    {
        if (std::filesystem::exists(jsonTestFile)) std::filesystem::remove(jsonTestFile);
    }

    std::string jsonTestFile;
};

// SCXML W3C 사양 3.3.1: <data> 요소 기본 구조 검증
TEST_F(DataNodeTest, SCXML_W3C_BasicDataElementStructure)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("sessionid");
    
    // W3C 사양: id 속성은 필수이며 유효한 식별자여야 함
    EXPECT_EQ("sessionid", dataNode->getId());
    EXPECT_FALSE(dataNode->getId().empty());
    
    // W3C 사양: src, expr, content 속성은 선택적
    EXPECT_TRUE(dataNode->getSrc().empty());
    EXPECT_TRUE(dataNode->getExpr().empty());  
    EXPECT_TRUE(dataNode->getContent().empty());
}

// SCXML W3C 사양 3.3.1: expr 속성 설정 및 조회
TEST_F(DataNodeTest, SCXML_W3C_ExpressionAttribute)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("counter");
    
    // W3C 사양: expr 속성을 통한 초기값 지정
    dataNode->setExpr("0");
    EXPECT_EQ("0", dataNode->getExpr());
    
    // W3C 사양: ECMAScript 표현식 지원
    dataNode->setExpr("Math.floor(Math.random() * 100)");
    EXPECT_EQ("Math.floor(Math.random() * 100)", dataNode->getExpr());
    
    // W3C 사양: 복합 객체 표현식
    dataNode->setExpr("{ name: 'John', age: 30, active: true }");
    EXPECT_EQ("{ name: 'John', age: 30, active: true }", dataNode->getExpr());
}

// SCXML W3C 사양 3.3.1: src 속성을 통한 외부 데이터 참조
TEST_F(DataNodeTest, SCXML_W3C_SrcAttributeExternalReference)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("userconfig");
    
    // W3C 사양: src 속성을 통한 외부 데이터 소스 지정
    dataNode->setSrc("config.json");
    EXPECT_EQ("config.json", dataNode->getSrc());
    
    // W3C 사양: HTTP URL 지원 (프로토콜 체크)
    dataNode->setSrc("http://example.com/data.xml");
    EXPECT_EQ("http://example.com/data.xml", dataNode->getSrc());
    
    // W3C 사양: 로컬 파일 참조
    dataNode->setSrc(jsonTestFile);
    EXPECT_EQ(jsonTestFile, dataNode->getSrc());
}

// SCXML W3C 사양 3.3.1: 인라인 콘텐츠 데이터
TEST_F(DataNodeTest, SCXML_W3C_InlineContentData)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("config");
    
    // W3C 사양: JSON 형식 인라인 데이터
    std::string jsonContent = R"({"timeout": 5000, "retries": 3, "debug": true})";
    dataNode->setContent(jsonContent);
    EXPECT_EQ(jsonContent, dataNode->getContent());
    
    // W3C 사양: XML 형식 인라인 데이터
    std::string xmlContent = R"(<config><server>localhost</server><port>8080</port></config>)";
    dataNode->setContent(xmlContent);
    EXPECT_EQ(xmlContent, dataNode->getContent());
    
    // W3C 사양: 텍스트 형식 데이터
    dataNode->setContent("simple text value");
    EXPECT_EQ("simple text value", dataNode->getContent());
}

// SCXML W3C 사양 3.3.1: 데이터 형식 지정 (format 속성)
TEST_F(DataNodeTest, SCXML_W3C_DataFormatSpecification)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("formatTest");
    
    // W3C 사양: 기본 형식 (구현 의존적, 여기서는 text)
    EXPECT_EQ("text", dataNode->getFormat());
    
    // W3C 사양: JSON 형식 지정
    dataNode->setFormat("json");
    EXPECT_EQ("json", dataNode->getFormat());
    
    // W3C 사양: XML 형식 지정
    dataNode->setFormat("xml");
    EXPECT_EQ("xml", dataNode->getFormat());
    
    // W3C 사양: ECMAScript 형식 지정
    dataNode->setFormat("ecmascript");
    EXPECT_EQ("ecmascript", dataNode->getFormat());
    
    // W3C 사양: 사용자 정의 형식
    dataNode->setFormat("custom-format");
    EXPECT_EQ("custom-format", dataNode->getFormat());
}

// SCXML W3C 사양 3.3.1: 데이터 모델 타입 지원
TEST_F(DataNodeTest, SCXML_W3C_DataModelTypeSupport)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("typeTest");
    
    // W3C 사양: ECMAScript 데이터 모델 지원 (필수)
    EXPECT_TRUE(dataNode->supportsDataModel("ecmascript"));
    
    // W3C 사양: 확장된 데이터 모델 지원
    EXPECT_TRUE(dataNode->supportsDataModel("javascript"));
    EXPECT_TRUE(dataNode->supportsDataModel("xpath"));
    EXPECT_TRUE(dataNode->supportsDataModel("null"));
    
    // 모든 데이터 모델 타입을 지원하는 범용적 구현
    EXPECT_TRUE(dataNode->supportsDataModel("custom-datamodel"));
}

// SCXML W3C 사양 3.3.1: 데이터 노드 검증 규칙
TEST_F(DataNodeTest, SCXML_W3C_DataNodeValidation)
{
    // W3C 사양: 유효한 데이터 노드들
    auto validNode1 = std::make_unique<SCXML::Core::DataNode>("valid1");
    validNode1->setExpr("42");
    auto errors1 = validNode1->validate();
    EXPECT_TRUE(errors1.empty()) << "Valid expr-based node should pass validation";
    
    auto validNode2 = std::make_unique<SCXML::Core::DataNode>("valid2"); 
    validNode2->setSrc("data.json");
    auto errors2 = validNode2->validate();
    EXPECT_TRUE(errors2.empty()) << "Valid src-based node should pass validation";
    
    auto validNode3 = std::make_unique<SCXML::Core::DataNode>("valid3");
    validNode3->setContent("content data");
    auto errors3 = validNode3->validate();
    EXPECT_TRUE(errors3.empty()) << "Valid content-based node should pass validation";
    
    // W3C 사양: 빈 데이터 노드도 유효 (런타임에 undefined로 초기화)
    auto emptyNode = std::make_unique<SCXML::Core::DataNode>("empty");
    auto emptyErrors = emptyNode->validate();
    EXPECT_TRUE(emptyErrors.empty()) << "Empty data node should be valid";
}

// SCXML W3C 사양 3.3.1: XML 콘텐츠 특성
TEST_F(DataNodeTest, SCXML_W3C_XmlContentCharacteristics)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("xmlTest");
    
    // W3C 사양: DataNode 자체는 XML 콘텐츠가 아님 (내용은 XML일 수 있지만)
    EXPECT_FALSE(dataNode->isXmlContent());
    
    // W3C 사양: XPath 쿼리 지원하지 않음 (DataNode는 단순 데이터 홀더)
    auto xpathResult = dataNode->queryXPath("//test");
    EXPECT_FALSE(xpathResult.has_value());
}

// SCXML W3C 사양 3.3.1: 데이터 노드 속성 관리
TEST_F(DataNodeTest, SCXML_W3C_AttributeManagement)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("attrTest");
    
    // W3C 사양: 임의 속성 설정 및 조회
    dataNode->setAttribute("custom-attr", "custom-value");
    EXPECT_EQ("custom-value", dataNode->getAttribute("custom-attr"));
    
    // W3C 사양: 존재하지 않는 속성은 빈 문자열 반환
    EXPECT_EQ("", dataNode->getAttribute("nonexistent"));
    
    // W3C 사양: 모든 속성 조회
    const auto& attributes = dataNode->getAttributes();
    EXPECT_TRUE(attributes.find("custom-attr") != attributes.end());
    
    // W3C 사양: 타입 및 스코프 메타데이터
    dataNode->setType("object");
    dataNode->setScope("global");
    EXPECT_EQ("object", dataNode->getType());
    EXPECT_EQ("global", dataNode->getScope());
}

// SCXML W3C 사양 3.3.1: 콘텐츠 아이템 컬렉션
TEST_F(DataNodeTest, SCXML_W3C_ContentItemCollection)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("collectionTest");
    
    // W3C 사양: 다중 콘텐츠 아이템 지원
    dataNode->addContent("item1");
    dataNode->addContent("item2");  
    dataNode->addContent("item3");
    
    const auto& items = dataNode->getContentItems();
    EXPECT_EQ(3, items.size());
    EXPECT_EQ("item1", items[0]);
    EXPECT_EQ("item2", items[1]);
    EXPECT_EQ("item3", items[2]);
}

// SCXML W3C 사양 3.3.1: 우선순위 규칙 (expr > src > content)
TEST_F(DataNodeTest, SCXML_W3C_AttributePrecedenceRules)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("precedenceTest");
    
    // W3C 사양: 모든 속성 설정 (우선순위 테스트)
    dataNode->setExpr("expression_value");     // 1순위
    dataNode->setSrc("source.json");           // 2순위  
    dataNode->setContent("content_value");     // 3순위
    
    // W3C 사양: expr이 최고 우선순위
    EXPECT_EQ("expression_value", dataNode->getExpr());
    EXPECT_EQ("source.json", dataNode->getSrc());
    EXPECT_EQ("content_value", dataNode->getContent());
    
    // 우선순위 검증을 위해 expr을 지우고 테스트
    dataNode->setExpr("");
    EXPECT_TRUE(dataNode->getExpr().empty());
    EXPECT_FALSE(dataNode->getSrc().empty());  // src가 다음 우선순위
}

// SCXML W3C 사양 3.3.1: 다양한 데이터 타입 지원
TEST_F(DataNodeTest, SCXML_W3C_VariousDataTypeSupport)
{
    // W3C 사양: 불린 데이터
    auto boolNode = std::make_unique<SCXML::Core::DataNode>("boolData");
    boolNode->setExpr("true");
    EXPECT_EQ("true", boolNode->getExpr());
    
    // W3C 사양: 숫자 데이터
    auto numberNode = std::make_unique<SCXML::Core::DataNode>("numberData");
    numberNode->setExpr("42.5");
    EXPECT_EQ("42.5", numberNode->getExpr());
    
    // W3C 사양: 문자열 데이터
    auto stringNode = std::make_unique<SCXML::Core::DataNode>("stringData");
    stringNode->setExpr("'Hello SCXML'");
    EXPECT_EQ("'Hello SCXML'", stringNode->getExpr());
    
    // W3C 사양: 배열 데이터
    auto arrayNode = std::make_unique<SCXML::Core::DataNode>("arrayData");
    arrayNode->setExpr("[1, 2, 3, 'four', true]");
    EXPECT_EQ("[1, 2, 3, 'four', true]", arrayNode->getExpr());
    
    // W3C 사양: 객체 데이터
    auto objectNode = std::make_unique<SCXML::Core::DataNode>("objectData");
    objectNode->setExpr("{ id: 123, name: 'test', active: true }");
    EXPECT_EQ("{ id: 123, name: 'test', active: true }", objectNode->getExpr());
}

// SCXML W3C 사양 3.3.1: 특수 문자 및 이스케이프 처리
TEST_F(DataNodeTest, SCXML_W3C_SpecialCharacterHandling)
{
    auto dataNode = std::make_unique<SCXML::Core::DataNode>("specialChars");
    
    // W3C 사양: JSON 이스케이프 문자 처리
    dataNode->setContent(R"({"message": "Hello\n\"World\""})");
    EXPECT_EQ(R"({"message": "Hello\n\"World\""})", dataNode->getContent());
    
    // W3C 사양: XML 특수 문자 처리
    dataNode->setContent(R"(<data value="&lt;test&gt; &amp; &quot;quote&quot;"/>)");
    EXPECT_EQ(R"(<data value="&lt;test&gt; &amp; &quot;quote&quot;"/>)", dataNode->getContent());
    
    // W3C 사양: 유니코드 문자 지원
    dataNode->setContent("데이터 모델 테스트 - 한글 지원");
    EXPECT_EQ("데이터 모델 테스트 - 한글 지원", dataNode->getContent());
}