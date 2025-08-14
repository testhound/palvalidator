#include <catch2/catch_test_macros.hpp>
#include "BinaryPatternTemplateSerializer.h"
#include "BinaryPatternTemplateDeserializer.h"
#include "PatternTemplate.h"
#include <sstream>
#include <iostream>

TEST_CASE("PatternTemplate Serialization and Deserialization Round Trip", "[serialization]")
{
    std::cout << "DEBUG: Testing patternTemplate serialization/deserialization" << std::endl;
    // 1. ARRANGE: Create a complex PatternTemplate object to be tested.
    PatternTemplate originalPattern("H[1]>L[3]_AND_C[0]>O[5]");

    originalPattern.addCondition(
        PatternCondition(
            PriceComponentDescriptor(PriceComponentType::High, 1),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Low, 3)
        )
    );

    originalPattern.addCondition(
        PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Open, 5)
        )
    );

    BinaryPatternTemplateSerializer serializer;
    BinaryPatternTemplateDeserializer deserializer;
    std::stringstream ss; // Use an in-memory stream

    // 2. ACT: Serialize the object to the stream, then deserialize it back.
    serializer.serialize(ss, originalPattern);
    PatternTemplate deserializedPattern = deserializer.deserialize(ss);

    // 3. ASSERT: Verify that the deserialized object is identical to the original.
    REQUIRE(deserializedPattern.getName() == originalPattern.getName());
    REQUIRE(deserializedPattern.getMaxBarOffset() == originalPattern.getMaxBarOffset());
    
    const auto& originalConditions = originalPattern.getConditions();
    const auto& deserializedConditions = deserializedPattern.getConditions();
    
    REQUIRE(deserializedConditions.size() == originalConditions.size());
    REQUIRE(deserializedConditions.size() == 2);

    // Check condition 1
    const auto& origCond1 = originalConditions[0];
    const auto& deserCond1 = deserializedConditions[0];
    REQUIRE(deserCond1.getLhs().getComponentType() == origCond1.getLhs().getComponentType());
    REQUIRE(deserCond1.getLhs().getBarOffset() == origCond1.getLhs().getBarOffset());
    REQUIRE(deserCond1.getOperator() == origCond1.getOperator());
    REQUIRE(deserCond1.getRhs().getComponentType() == origCond1.getRhs().getComponentType());
    REQUIRE(deserCond1.getRhs().getBarOffset() == origCond1.getRhs().getBarOffset());

    // Check condition 2
    const auto& origCond2 = originalConditions[1];
    const auto& deserCond2 = deserializedConditions[1];
    REQUIRE(deserCond2.getLhs().getComponentType() == origCond2.getLhs().getComponentType());
    REQUIRE(deserCond2.getLhs().getBarOffset() == origCond2.getLhs().getBarOffset());
    REQUIRE(deserCond2.getOperator() == origCond2.getOperator());
    REQUIRE(deserCond2.getRhs().getComponentType() == origCond2.getRhs().getComponentType());
    REQUIRE(deserCond2.getRhs().getBarOffset() == origCond2.getRhs().getBarOffset());
}

TEST_CASE("Serialization with a single condition", "[serialization]")
{
    // ARRANGE
    PatternTemplate originalPattern("C[0]>C[1]");
    originalPattern.addCondition(
        PatternCondition(
            PriceComponentDescriptor(PriceComponentType::Close, 0),
            ComparisonOperator::GreaterThan,
            PriceComponentDescriptor(PriceComponentType::Close, 1)
        )
    );

    BinaryPatternTemplateSerializer serializer;
    BinaryPatternTemplateDeserializer deserializer;
    std::stringstream ss;

    // ACT
    serializer.serialize(ss, originalPattern);
    PatternTemplate deserializedPattern = deserializer.deserialize(ss);

    // ASSERT
    REQUIRE(deserializedPattern.getName() == "C[0]>C[1]");
    REQUIRE(deserializedPattern.getConditions().size() == 1);
    REQUIRE(deserializedPattern.getMaxBarOffset() == 1);
    const auto& condition = deserializedPattern.getConditions()[0];
    REQUIRE(condition.getLhs().getComponentType() == PriceComponentType::Close);
    REQUIRE(condition.getLhs().getBarOffset() == 0);
    REQUIRE(condition.getRhs().getComponentType() == PriceComponentType::Close);
    REQUIRE(condition.getRhs().getBarOffset() == 1);
}

TEST_CASE("Serialization of an empty pattern throws no errors", "[serialization]")
{
    // ARRANGE
    PatternTemplate originalPattern("EmptyPattern");
    // No conditions added

    BinaryPatternTemplateSerializer serializer;
    BinaryPatternTemplateDeserializer deserializer;
    std::stringstream ss;

    // ACT & ASSERT
    REQUIRE_NOTHROW(serializer.serialize(ss, originalPattern));
    PatternTemplate deserializedPattern = deserializer.deserialize(ss);

    REQUIRE(deserializedPattern.getName() == "EmptyPattern");
    REQUIRE(deserializedPattern.getConditions().empty());
    REQUIRE(deserializedPattern.getMaxBarOffset() == 0);
}
