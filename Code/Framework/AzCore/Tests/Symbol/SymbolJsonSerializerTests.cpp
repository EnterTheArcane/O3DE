/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Symbol/SymbolJsonSerializer.h>
#include <AzCore/std/string/string.h>

#include <Tests/Serialization/Json/BaseJsonSerializerFixture.h>

namespace JsonSerializationTests
{
    class SymbolJsonSerializerTests
        : public BaseJsonSerializerFixture
    {
    public:
        void SetUp() override
        {
            BaseJsonSerializerFixture::SetUp();
            m_serializer = AZStd::make_unique<AZ::SymbolJsonSerializer>();
        }

        void TearDown() override
        {
            m_serializer.reset();
            BaseJsonSerializerFixture::TearDown();
        }

        AZStd::unique_ptr<AZ::SymbolJsonSerializer> m_serializer;
    };

    TEST_F(SymbolJsonSerializerTests, StringRoundTrips)
    {
        const AZ::Symbol input{AZStd::string_view{"JsonSymbol"}};
        rapidjson::Value jsonValue;

        const auto storeResult = m_serializer->Store(
            jsonValue,
            &input,
            nullptr,
            azrtti_typeid<AZ::Symbol>(),
            *m_jsonSerializationContext);
        ASSERT_EQ(storeResult.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Success);
        ASSERT_TRUE(jsonValue.IsString());

        AZ::Symbol output;
        const auto loadResult = m_serializer->Load(
            &output,
            azrtti_typeid<AZ::Symbol>(),
            jsonValue,
            *m_jsonDeserializationContext);
        EXPECT_EQ(loadResult.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Success);
        EXPECT_EQ(output, input);
    }

    TEST_F(SymbolJsonSerializerTests, EmptyStringLoadsEmptySymbol)
    {
        rapidjson::Value jsonValue;
        jsonValue.SetString("", 0);
        AZ::Symbol output{AZStd::string_view{"Existing"}};

        const auto result = m_serializer->Load(
            &output,
            azrtti_typeid<AZ::Symbol>(),
            jsonValue,
            *m_jsonDeserializationContext);

        EXPECT_EQ(result.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Success);
        EXPECT_TRUE(output.IsEmpty());
    }

    TEST_F(SymbolJsonSerializerTests, RawLengthAwareValueBypassesObjectStreamTextCodec)
    {
        AZStd::string rawValue{"Json%"};
        rawValue.push_back('\x01');
        rawValue.append("\xEF\xBF\xBE", 3);
        rawValue.append("\xEF\xBF\xBF", 3);
        const AZ::Symbol input{rawValue};
        rapidjson::Value jsonValue;

        const auto storeResult = m_serializer->Store(
            jsonValue,
            &input,
            nullptr,
            azrtti_typeid<AZ::Symbol>(),
            *m_jsonSerializationContext);
        ASSERT_EQ(storeResult.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Success);
        ASSERT_TRUE(jsonValue.IsString());
        const AZStd::string_view storedValue{jsonValue.GetString(), jsonValue.GetStringLength()};
        EXPECT_EQ(storedValue, rawValue);

        AZ::Symbol output;
        const auto loadResult = m_serializer->Load(
            &output,
            azrtti_typeid<AZ::Symbol>(),
            jsonValue,
            *m_jsonDeserializationContext);
        EXPECT_EQ(loadResult.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Success);
        EXPECT_EQ(output, input);
    }

    TEST_F(SymbolJsonSerializerTests, InvalidInputsPreserveDestination)
    {
        const AZ::Symbol existing{AZStd::string_view{"Existing"}};
        rapidjson::Document document;
        rapidjson::Value objectValue{rapidjson::kObjectType};
        rapidjson::Value embeddedNull;
        constexpr char embedded[] = {'a', '\0', 'b'};
        embeddedNull.SetString(embedded, sizeof(embedded), document.GetAllocator());
        rapidjson::Value malformed;
        constexpr char invalidUtf8[] = {static_cast<char>(0xC0), static_cast<char>(0xAF)};
        malformed.SetString(invalidUtf8, sizeof(invalidUtf8), document.GetAllocator());
        const AZStd::string oversizedValue(AZ::Symbol::MaxStringSize + 1, 'x');
        rapidjson::Value oversized;
        oversized.SetString(
            oversizedValue.data(),
            static_cast<rapidjson::SizeType>(oversizedValue.size()),
            document.GetAllocator());

        rapidjson::Value* invalidValues[] = {&objectValue, &embeddedNull, &malformed, &oversized};
        for (rapidjson::Value* invalidValue : invalidValues)
        {
            AZ::Symbol output = existing;
            const auto result = m_serializer->Load(
                &output,
                azrtti_typeid<AZ::Symbol>(),
                *invalidValue,
                *m_jsonDeserializationContext);
            EXPECT_EQ(result.GetResultCode().GetOutcome(), AZ::JsonSerializationResult::Outcomes::Invalid);
            EXPECT_EQ(output, existing);
        }
    }

    TEST_F(SymbolJsonSerializerTests, ExplicitDefaultObjectCannotBypassSerializer)
    {
        EXPECT_EQ(
            m_serializer->GetOperationsFlags(),
            AZ::BaseJsonSerializer::OperationFlags::ManualDefault);
    }
} // namespace JsonSerializationTests
