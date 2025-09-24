#include "scripting/JSResult.h"
#include <cfloat>
#include <climits>
#include <cmath>
#include <gtest/gtest.h>

namespace RSM {

/**
 * @brief Comprehensive test suite for JSResult type conversion logic
 *
 * Tests all possible type conversions in JSResult::getValue<T>() method:
 * - Direct type matches (no conversion needed)
 * - Numeric type conversions (int64_t ↔ double)
 * - Edge cases: overflow, underflow, precision loss
 * - Boundary values: LLONG_MIN, LLONG_MAX, DBL_MIN, DBL_MAX
 * - Invalid conversions (should return default values)
 */
class JSResultTypeConversionTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

// ========================================
// Direct Type Match Tests (No Conversion)
// ========================================

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_String) {
    auto result = JSResult::createSuccess(std::string("test_string"));

    EXPECT_EQ(result.getValue<std::string>(), "test_string");
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_Bool) {
    auto resultTrue = JSResult::createSuccess(true);
    auto resultFalse = JSResult::createSuccess(false);

    EXPECT_EQ(resultTrue.getValue<bool>(), true);
    EXPECT_EQ(resultFalse.getValue<bool>(), false);
    EXPECT_TRUE(resultTrue.isSuccess());
    EXPECT_TRUE(resultFalse.isSuccess());
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_Int64) {
    int64_t testValue = 12345678901234LL;
    auto result = JSResult::createSuccess(testValue);

    EXPECT_EQ(result.getValue<int64_t>(), testValue);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_Double) {
    double testValue = 3.141592653589793;
    auto result = JSResult::createSuccess(testValue);

    EXPECT_DOUBLE_EQ(result.getValue<double>(), testValue);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_ScriptUndefined) {
    auto result = JSResult::createSuccess(ScriptUndefined{});

    // Should work as ScriptUndefined
    EXPECT_TRUE(std::holds_alternative<ScriptUndefined>(result.getInternalValue()));
    EXPECT_TRUE(result.isSuccess());

    // All other type requests should return default values
    EXPECT_EQ(result.getValue<int64_t>(), 0);
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 0.0);
    EXPECT_EQ(result.getValue<bool>(), false);
    EXPECT_EQ(result.getValue<std::string>(), "");
    EXPECT_EQ(result.getArray(), nullptr);
    EXPECT_EQ(result.getObject(), nullptr);
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_ScriptNull) {
    auto result = JSResult::createSuccess(ScriptNull{});

    // Should work as ScriptNull
    EXPECT_TRUE(std::holds_alternative<ScriptNull>(result.getInternalValue()));
    EXPECT_TRUE(result.isSuccess());

    // All other type requests should return default values
    EXPECT_EQ(result.getValue<int64_t>(), 0);
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 0.0);
    EXPECT_EQ(result.getValue<bool>(), false);
    EXPECT_EQ(result.getValue<std::string>(), "");
    EXPECT_EQ(result.getArray(), nullptr);
    EXPECT_EQ(result.getObject(), nullptr);
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_ScriptArray) {
    auto array = std::make_shared<ScriptArray>();
    array->elements = {int64_t(1), std::string("test"), true};
    auto result = JSResult::createSuccess(array);

    // Should work as array
    EXPECT_TRUE(result.isArray());
    EXPECT_NE(result.getArray(), nullptr);
    EXPECT_EQ(result.getArray()->elements.size(), 3u);

    // All basic type requests should return default values
    EXPECT_EQ(result.getValue<int64_t>(), 0);
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 0.0);
    EXPECT_EQ(result.getValue<bool>(), false);
    EXPECT_EQ(result.getValue<std::string>(), "");
    EXPECT_EQ(result.getObject(), nullptr);
}

TEST_F(JSResultTypeConversionTest, DirectTypeMatch_ScriptObject) {
    auto object = std::make_shared<ScriptObject>();
    object->properties["name"] = std::string("test");
    object->properties["value"] = int64_t(42);
    auto result = JSResult::createSuccess(object);

    // Should work as object
    EXPECT_TRUE(result.isObject());
    EXPECT_NE(result.getObject(), nullptr);
    EXPECT_EQ(result.getObject()->properties.size(), 2u);

    // All basic type requests should return default values
    EXPECT_EQ(result.getValue<int64_t>(), 0);
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 0.0);
    EXPECT_EQ(result.getValue<bool>(), false);
    EXPECT_EQ(result.getValue<std::string>(), "");
    EXPECT_EQ(result.getArray(), nullptr);
}

// ========================================
// Numeric Type Conversion Tests
// ========================================

TEST_F(JSResultTypeConversionTest, Conversion_Int64ToDouble) {
    // Test conversion from int64_t to double
    int64_t intValue = 42;
    auto result = JSResult::createSuccess(intValue);

    // Should be able to get as double
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 42.0);
    // Original int64_t access should still work
    EXPECT_EQ(result.getValue<int64_t>(), 42);
}

TEST_F(JSResultTypeConversionTest, Conversion_DoubleToInt64_WholeNumber) {
    // Test conversion from double to int64_t (whole number)
    double doubleValue = 123.0;
    auto result = JSResult::createSuccess(doubleValue);

    // Should be able to get as int64_t since it's a whole number
    EXPECT_EQ(result.getValue<int64_t>(), 123);
    // Original double access should still work
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 123.0);
}

TEST_F(JSResultTypeConversionTest, Conversion_DoubleToInt64_FractionalNumber) {
    // Test conversion from double to int64_t (fractional number - should fail)
    double doubleValue = 123.456;
    auto result = JSResult::createSuccess(doubleValue);

    // Should NOT be able to get as int64_t since it's not a whole number
    EXPECT_EQ(result.getValue<int64_t>(), 0);  // Default value
    // Original double access should still work
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 123.456);
}

TEST_F(JSResultTypeConversionTest, Conversion_NegativeNumbers) {
    // Test negative number conversions
    int64_t negativeInt = -12345;
    double negativeDouble = -678.0;

    auto intResult = JSResult::createSuccess(negativeInt);
    auto doubleResult = JSResult::createSuccess(negativeDouble);

    // int64_t to double
    EXPECT_DOUBLE_EQ(intResult.getValue<double>(), -12345.0);

    // double to int64_t (whole number)
    EXPECT_EQ(doubleResult.getValue<int64_t>(), -678);
}

// ========================================
// DATA LOSS PREVENTION TESTS
// ========================================

TEST_F(JSResultTypeConversionTest, DataLossPrevention_NoValueLoss) {
    // Critical requirement: No data loss during C++ and JavaScript type conversions

    // 1. int64_t values must be recoverable after double conversion
    int64_t originalInt = 9007199254740992LL;  // 2^53 (double precision 경계)
    auto intResult = JSResult::createSuccess(originalInt);

    double asDouble = intResult.getValue<double>();
    int64_t backToInt = static_cast<int64_t>(asDouble);  // 수동 역변환
    EXPECT_EQ(originalInt, backToInt) << "Data loss in int64_t → double → int64_t conversion!";

    // 2. double values (whole numbers) must be recoverable after int64_t conversion
    double originalDouble = 42.0;
    auto doubleResult = JSResult::createSuccess(originalDouble);

    int64_t asInt = doubleResult.getValue<int64_t>();
    double backToDouble = static_cast<double>(asInt);  // 수동 역변환
    EXPECT_DOUBLE_EQ(originalDouble, backToDouble) << "Data loss in double → int64_t → double conversion!";

    // 3. Fractional values should fail int64_t conversion (prevent data loss)
    double fractionalDouble = 42.7;
    auto fractionalResult = JSResult::createSuccess(fractionalDouble);

    EXPECT_EQ(fractionalResult.getValue<int64_t>(), 0)
        << "Fractional value incorrectly converted to int64_t - data loss risk!";
    EXPECT_DOUBLE_EQ(fractionalResult.getValue<double>(), 42.7) << "Original double value lost!";
}

TEST_F(JSResultTypeConversionTest, DataLossPrevention_PrecisionBoundaries) {
    // Value preservation test at IEEE 754 double precision boundaries

    // All integers ≤ 2^53 can be exactly represented in double
    int64_t safePrecisionInt = (1LL << 53);  // 2^53
    auto safeResult = JSResult::createSuccess(safePrecisionInt);

    double asDouble = safeResult.getValue<double>();
    EXPECT_EQ(static_cast<int64_t>(asDouble), safePrecisionInt) << "Data loss in safe precision range!";

    // Integers larger than 2^53 may have precision loss,
    // but conversion should still work (no crashes)
    int64_t largePrecisionInt = (1LL << 53) + 1;  // 2^53 + 1
    auto largeResult = JSResult::createSuccess(largePrecisionInt);

    double largeDbl = largeResult.getValue<double>();
    EXPECT_TRUE(largeDbl > 0) << "Crash during large integer conversion!";
    EXPECT_EQ(largeResult.getValue<int64_t>(), largePrecisionInt) << "Original int64_t value lost!";
}

// ========================================
// Boundary Value Tests
// ========================================

TEST_F(JSResultTypeConversionTest, BoundaryValues_Int64Max) {
    int64_t maxInt = LLONG_MAX;
    auto result = JSResult::createSuccess(maxInt);

    // Should work as int64_t
    EXPECT_EQ(result.getValue<int64_t>(), LLONG_MAX);

    // Conversion to double (may lose precision but should work)
    double convertedDouble = result.getValue<double>();
    EXPECT_TRUE(convertedDouble > 0);  // Should be positive large number
}

TEST_F(JSResultTypeConversionTest, BoundaryValues_Int64Min) {
    int64_t minInt = LLONG_MIN;
    auto result = JSResult::createSuccess(minInt);

    // Should work as int64_t
    EXPECT_EQ(result.getValue<int64_t>(), LLONG_MIN);

    // Conversion to double (may lose precision but should work)
    double convertedDouble = result.getValue<double>();
    EXPECT_TRUE(convertedDouble < 0);  // Should be negative large number
}

TEST_F(JSResultTypeConversionTest, BoundaryValues_DoubleMax) {
    double maxDouble = DBL_MAX;
    auto result = JSResult::createSuccess(maxDouble);

    // Should work as double
    EXPECT_DOUBLE_EQ(result.getValue<double>(), DBL_MAX);

    // Conversion to int64_t should fail (out of range)
    EXPECT_EQ(result.getValue<int64_t>(), 0);  // Default value
}

TEST_F(JSResultTypeConversionTest, BoundaryValues_DoubleMin) {
    double minDouble = -DBL_MAX;  // Most negative finite value
    auto result = JSResult::createSuccess(minDouble);

    // Should work as double
    EXPECT_DOUBLE_EQ(result.getValue<double>(), -DBL_MAX);

    // Conversion to int64_t should fail (out of range)
    EXPECT_EQ(result.getValue<int64_t>(), 0);  // Default value
}

// ========================================
// Cross-Type Conversion Matrix - All 8 Types
// ========================================

TEST_F(JSResultTypeConversionTest, CrossTypeConversionMatrix_AllTypesToInt64) {
    // Test all 8 ScriptValue types trying to convert to int64_t

    // 1. ScriptUndefined → int64_t
    auto undefinedResult = JSResult::createSuccess(ScriptUndefined{});
    EXPECT_EQ(undefinedResult.getValue<int64_t>(), 0);

    // 2. ScriptNull → int64_t
    auto nullResult = JSResult::createSuccess(ScriptNull{});
    EXPECT_EQ(nullResult.getValue<int64_t>(), 0);

    // 3. bool → int64_t
    auto boolResult = JSResult::createSuccess(true);
    EXPECT_EQ(boolResult.getValue<int64_t>(), 0);  // No automatic conversion

    // 4. int64_t → int64_t (direct match)
    auto intResult = JSResult::createSuccess(int64_t(42));
    EXPECT_EQ(intResult.getValue<int64_t>(), 42);

    // 5. double → int64_t (conditional conversion)
    auto doubleResult = JSResult::createSuccess(42.0);
    EXPECT_EQ(doubleResult.getValue<int64_t>(), 42);

    // 6. string → int64_t
    auto stringResult = JSResult::createSuccess(std::string("123"));
    EXPECT_EQ(stringResult.getValue<int64_t>(), 0);  // No automatic conversion

    // 7. Array → int64_t
    auto arrayResult = JSResult::createSuccess(std::make_shared<ScriptArray>());
    EXPECT_EQ(arrayResult.getValue<int64_t>(), 0);

    // 8. Object → int64_t
    auto objectResult = JSResult::createSuccess(std::make_shared<ScriptObject>());
    EXPECT_EQ(objectResult.getValue<int64_t>(), 0);
}

TEST_F(JSResultTypeConversionTest, CrossTypeConversionMatrix_AllTypesToDouble) {
    // Test all 8 ScriptValue types trying to convert to double

    // 1. ScriptUndefined → double
    auto undefinedResult = JSResult::createSuccess(ScriptUndefined{});
    EXPECT_DOUBLE_EQ(undefinedResult.getValue<double>(), 0.0);

    // 2. ScriptNull → double
    auto nullResult = JSResult::createSuccess(ScriptNull{});
    EXPECT_DOUBLE_EQ(nullResult.getValue<double>(), 0.0);

    // 3. bool → double
    auto boolResult = JSResult::createSuccess(true);
    EXPECT_DOUBLE_EQ(boolResult.getValue<double>(), 0.0);  // No automatic conversion

    // 4. int64_t → double (automatic conversion)
    auto intResult = JSResult::createSuccess(int64_t(42));
    EXPECT_DOUBLE_EQ(intResult.getValue<double>(), 42.0);

    // 5. double → double (direct match)
    auto doubleResult = JSResult::createSuccess(42.5);
    EXPECT_DOUBLE_EQ(doubleResult.getValue<double>(), 42.5);

    // 6. string → double
    auto stringResult = JSResult::createSuccess(std::string("123.456"));
    EXPECT_DOUBLE_EQ(stringResult.getValue<double>(), 0.0);  // No automatic conversion

    // 7. Array → double
    auto arrayResult = JSResult::createSuccess(std::make_shared<ScriptArray>());
    EXPECT_DOUBLE_EQ(arrayResult.getValue<double>(), 0.0);

    // 8. Object → double
    auto objectResult = JSResult::createSuccess(std::make_shared<ScriptObject>());
    EXPECT_DOUBLE_EQ(objectResult.getValue<double>(), 0.0);
}

// ========================================
// Complete Type Coverage Summary Test
// ========================================

TEST_F(JSResultTypeConversionTest, CompleteCoverage_AllScriptValueTypes) {
    // This test verifies that we can create JSResult with all 8 ScriptValue types
    // and that they are correctly identified

    // 1. ScriptUndefined
    auto undefinedResult = JSResult::createSuccess(ScriptUndefined{});
    EXPECT_TRUE(std::holds_alternative<ScriptUndefined>(undefinedResult.getInternalValue()));

    // 2. ScriptNull
    auto nullResult = JSResult::createSuccess(ScriptNull{});
    EXPECT_TRUE(std::holds_alternative<ScriptNull>(nullResult.getInternalValue()));

    // 3. bool
    auto boolResult = JSResult::createSuccess(true);
    EXPECT_TRUE(std::holds_alternative<bool>(boolResult.getInternalValue()));

    // 4. int64_t
    auto intResult = JSResult::createSuccess(int64_t(42));
    EXPECT_TRUE(std::holds_alternative<int64_t>(intResult.getInternalValue()));

    // 5. double
    auto doubleResult = JSResult::createSuccess(42.5);
    EXPECT_TRUE(std::holds_alternative<double>(doubleResult.getInternalValue()));

    // 6. string
    auto stringResult = JSResult::createSuccess(std::string("test"));
    EXPECT_TRUE(std::holds_alternative<std::string>(stringResult.getInternalValue()));

    // 7. ScriptArray
    auto arrayResult = JSResult::createSuccess(std::make_shared<ScriptArray>());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<ScriptArray>>(arrayResult.getInternalValue()));

    // 8. ScriptObject
    auto objectResult = JSResult::createSuccess(std::make_shared<ScriptObject>());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<ScriptObject>>(objectResult.getInternalValue()));
}

// ========================================
// W3C SCXML Compliance Tests
// ========================================

TEST_F(JSResultTypeConversionTest, W3C_JavaScript_NumberTypeFlexibility) {
    // W3C SCXML Section 5.9: JavaScript numbers should be accessible as both int and double

    // Case 1: Whole number stored as double should be accessible as int64_t
    double wholeDouble = 42.0;
    auto result1 = JSResult::createSuccess(wholeDouble);

    EXPECT_EQ(result1.getValue<int64_t>(), 42);
    EXPECT_DOUBLE_EQ(result1.getValue<double>(), 42.0);

    // Case 2: Integer should be accessible as double
    int64_t wholeInt = 42;
    auto result2 = JSResult::createSuccess(wholeInt);

    EXPECT_EQ(result2.getValue<int64_t>(), 42);
    EXPECT_DOUBLE_EQ(result2.getValue<double>(), 42.0);

    // Case 3: Fractional number should NOT be accessible as int64_t
    double fractional = 42.5;
    auto result3 = JSResult::createSuccess(fractional);

    EXPECT_EQ(result3.getValue<int64_t>(), 0);  // Conversion failed
    EXPECT_DOUBLE_EQ(result3.getValue<double>(), 42.5);
}

TEST_F(JSResultTypeConversionTest, W3C_IEEE754_Compliance) {
    // W3C SCXML: JavaScript numbers follow IEEE 754 standard

    // Test IEEE 754 special values
    auto infResult = JSResult::createSuccess(std::numeric_limits<double>::infinity());
    auto nanResult = JSResult::createSuccess(std::numeric_limits<double>::quiet_NaN());

    EXPECT_TRUE(std::isinf(infResult.getValue<double>()));
    EXPECT_TRUE(std::isnan(nanResult.getValue<double>()));

    // These should not convert to int64_t
    EXPECT_EQ(infResult.getValue<int64_t>(), 0);
    EXPECT_EQ(nanResult.getValue<int64_t>(), 0);
}

}  // namespace RSM