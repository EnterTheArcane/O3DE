/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Symbol/SymbolSerializer.h>
#include <AzCore/Symbol/Internal/SymbolStorageBudget.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <limits>

namespace UnitTest
{
    namespace
    {
        constexpr char UppercaseHexDigits[] = "0123456789ABCDEF";

        void AppendPercentEscape(
            AZStd::string& output,
            const unsigned char byte)
        {
            output.push_back('%');
            output.push_back(UppercaseHexDigits[byte >> 4]);
            output.push_back(UppercaseHexDigits[byte & 0x0F]);
        }

        AZStd::string MakeXmlUnsafeSymbolValue()
        {
            AZStd::string value{"Text<>&\"'%"};
            for (unsigned char control = 0x01; control <= 0x1F; ++control)
            {
                value.push_back(static_cast<char>(control));
            }
            value.append("\xEF\xBF\xBE", 3);
            value.append("\xEF\xBF\xBF", 3);
            value.append("\xF0\x9F\x99\x82", 4);
            return value;
        }

        AZStd::string MakeCanonicalXmlSafeText()
        {
            AZStd::string value{"Text<>&\"'%25"};
            for (unsigned char control = 0x01; control <= 0x1F; ++control)
            {
                AppendPercentEscape(value, control);
            }
            value.append("%EF%BF%BE%EF%BF%BF");
            value.append("\xF0\x9F\x99\x82", 4);
            return value;
        }

        class ReportedLengthStream final
            : public AZ::IO::MemoryStream
        {
        public:
            explicit ReportedLengthStream(AZ::IO::SizeType length)
                : MemoryStream("x", 2)
                , m_length{length}
            {
            }

            AZ::IO::SizeType GetLength() const override
            {
                return m_length;
            }

            AZ::IO::SizeType Read(AZ::IO::SizeType bytes, void* output) override
            {
                m_readCalled = true;
                return MemoryStream::Read(bytes, output);
            }

            bool m_readCalled = false;

        private:
            AZ::IO::SizeType m_length;
        };

        class ShortReadStream final
            : public AZ::IO::GenericStream
        {
        public:
            ShortReadStream(
                const void* buffer,
                const size_t bufferSize)
                : m_stream{buffer, bufferSize}
            {
            }

            bool IsOpen() const override
            {
                return m_stream.IsOpen();
            }

            bool CanSeek() const override
            {
                return m_stream.CanSeek();
            }

            bool CanRead() const override
            {
                return m_stream.CanRead();
            }

            bool CanWrite() const override
            {
                return false;
            }

            void Seek(
                const AZ::IO::OffsetType bytes,
                const SeekMode mode) override
            {
                m_stream.Seek(bytes, mode);
            }

            AZ::IO::SizeType Read(
                AZ::IO::SizeType bytes,
                void* outputBuffer) override
            {
                if (!m_returnedShortRead && m_stream.GetCurPos() != 0 && bytes != 0)
                {
                    m_returnedShortRead = true;
                    --bytes;
                }
                return m_stream.Read(bytes, outputBuffer);
            }

            AZ::IO::SizeType Write(
                [[maybe_unused]] AZ::IO::SizeType bytes,
                [[maybe_unused]] const void* inputBuffer) override
            {
                return 0;
            }

            AZ::IO::SizeType GetCurPos() const override
            {
                return m_stream.GetCurPos();
            }

            AZ::IO::SizeType GetLength() const override
            {
                return m_stream.GetLength();
            }

        private:
            AZ::IO::MemoryStream m_stream;
            bool m_returnedShortRead = false;
        };

        template<class T>
        void ExpectBinaryObjectStreamTruncationRejected(
            const T& input,
            const T& unchangedValue,
            AZ::SerializeContext& serializeContext)
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                outputStream,
                AZ::ObjectStream::ST_BINARY,
                &input,
                &serializeContext));
            ASSERT_GT(buffer.size(), 1);

            T completeOutput = unchangedValue;
            AZ::IO::MemoryStream completeStream(buffer.data(), buffer.size());
            ASSERT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(completeStream, completeOutput, &serializeContext));
            EXPECT_EQ(completeOutput, input);

            AZ::ObjectStream::FilterDescriptor strictFilter;
            strictFilter.m_flags = AZ::ObjectStream::FILTERFLAG_STRICT;
            AZ_TEST_START_TRACE_SUPPRESSION;
            for (size_t truncatedSize = 0; truncatedSize < buffer.size(); ++truncatedSize)
            {
                T truncatedOutput = unchangedValue;
                AZ::IO::MemoryStream truncatedStream(buffer.data(), truncatedSize);
                EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(
                    truncatedStream,
                    truncatedOutput,
                    &serializeContext,
                    strictFilter)) << "Truncated size " << truncatedSize;

                // ObjectStream may commit a complete leaf before detecting a missing structural end tag.
                // Truncation within the leaf must preserve its destination.
                if (truncatedSize < buffer.size() - 2)
                {
                    EXPECT_EQ(truncatedOutput, unchangedValue) << "Truncated size " << truncatedSize;
                }
            }
            AZ_TEST_STOP_TRACE_SUPPRESSION(buffer.size() + 1);
        }
    } // namespace

    class SymbolSerializerTests
        : public LeakDetectionFixture
    {
    };

    TEST_F(SymbolSerializerTests, BinarySerializationPreservesLongValuesAndTerminalNul)
    {
        AZ::SymbolSerializer serializer;

        const AZ::Symbol empty;
        AZStd::vector<AZ::u8> emptyBytes;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> emptyStream(&emptyBytes);
        EXPECT_EQ(serializer.Save(&empty, emptyStream, false), 1);
        ASSERT_EQ(emptyBytes.size(), 1);
        EXPECT_EQ(emptyBytes.front(), 0);

        const AZStd::string longValue(32768, 'm');
        const AZ::Symbol longSymbol{longValue};
        AZStd::vector<AZ::u8> longBytes;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> longStream(&longBytes);
        EXPECT_EQ(serializer.Save(&longSymbol, longStream, false), longValue.size() + 1);
        ASSERT_EQ(longBytes.size(), longValue.size() + 1);
        EXPECT_EQ(longBytes.back(), 0);
        const AZStd::string_view serializedValue{
            reinterpret_cast<const char*>(longBytes.data()),
            longValue.size(),
        };
        EXPECT_EQ(serializedValue, longValue);

        AZ::Symbol loaded;
        AZ::IO::MemoryStream inputStream(longBytes.data(), longBytes.size());
        EXPECT_TRUE(serializer.Load(&loaded, inputStream, 0, false));
        EXPECT_EQ(loaded, longSymbol);
    }

    TEST_F(SymbolSerializerTests, BinaryLoadRejectsMalformedAndShortInputWithoutAssignment)
    {
        AZ::SymbolSerializer serializer;
        const AZ::Symbol existing{AZStd::string_view{"ExistingBinarySymbol"}};

        AZStd::vector<AZStd::vector<AZ::u8>> invalidValues{
            {},
            {'n', 'o', 'm', 'a', 'r', 'k', 'e', 'r'},
            {'a', 0, 'b', 0},
            {static_cast<AZ::u8>(0xC0), static_cast<AZ::u8>(0xAF), 0},
            AZStd::vector<AZ::u8>(65537 + 1, 'x'),
        };
        invalidValues.back().back() = 0;
        invalidValues.back()[65536] = 0xFF;

        for (const AZStd::vector<AZ::u8>& invalidValue : invalidValues)
        {
            AZ::Symbol output = existing;
            AZ::IO::MemoryStream inputStream(invalidValue.data(), invalidValue.size());
            EXPECT_FALSE(serializer.Load(&output, inputStream, 0, false));
            EXPECT_EQ(output, existing);
        }

        const AZStd::array<AZ::u8, 2> validBytes{'a', 0};
        AZ::IO::MemoryStream shortInput(validBytes.data(), validBytes.size());
        shortInput.Seek(1, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZ::Symbol output = existing;
        EXPECT_FALSE(serializer.Load(&output, shortInput, 0, false));
        EXPECT_EQ(output, existing);
    }

    TEST_F(SymbolSerializerTests, ImpossibleStreamLengthsFailBeforeReadingOrAssigning)
    {
        constexpr AZ::IO::SizeType Lengths[] = {
            0,
            AZ::Internal::SymbolStorageBudgetBytes + 1,
            AZ::IO::SizeType{(std::numeric_limits<AZ::u32>::max)()} + 1,
            (std::numeric_limits<AZ::IO::SizeType>::max)(),
        };
        const AZ::Symbol original{"LengthRejectionPreservesValue"};
        AZ::SymbolSerializer serializer;
        for (const AZ::IO::SizeType length : Lengths)
        {
            SCOPED_TRACE(length);
            ReportedLengthStream input{length};
            AZ::Symbol output = original;
            EXPECT_FALSE(serializer.Load(&output, input, 0, false));
            EXPECT_EQ(output, original);
            EXPECT_FALSE(input.m_readCalled);
            AZStd::vector<char> text{'x'};
            AZ::IO::ByteContainerStream<AZStd::vector<char>> textOutput{&text};
            EXPECT_EQ(serializer.DataToText(input, textOutput, false), 0);
            EXPECT_FALSE(input.m_readCalled);
            EXPECT_EQ(textOutput.GetCurPos(), 0);
            EXPECT_EQ(text, AZStd::vector<char>{'x'});
        }
    }

    TEST_F(SymbolSerializerTests, TextAndBinaryRoundTripAcrossInlineAndLongValueBoundaries)
    {
        constexpr size_t Lengths[] = {1022, 1023, 1024, 1025, 32767, 32768};
        AZ::SymbolSerializer serializer;
        for (const size_t length : Lengths)
        {
            for (const char fill : {'v', '%'})
            {
                SCOPED_TRACE(length);
                SCOPED_TRACE(fill);
                AZStd::string value(length, fill);
                size_t utf8Offset = 1021;
                if (utf8Offset > length - 4)
                {
                    utf8Offset = length - 4;
                }
                value.replace(utf8Offset, 4, "\xF0\x9F\x99\x82", 4);
                const AZ::Symbol input{value};
                AZStd::vector<char> binary;
                AZ::IO::ByteContainerStream<AZStd::vector<char>> binaryOutput{&binary};
                ASSERT_EQ(serializer.Save(&input, binaryOutput, false), value.size() + 1);
                AZ::IO::MemoryStream binaryInput{binary.data(), binary.size()};
                AZStd::vector<char> text;
                AZ::IO::ByteContainerStream<AZStd::vector<char>> textOutput{&text};
                ASSERT_GT(serializer.DataToText(binaryInput, textOutput, false), 0);
                text.push_back('\0');
                AZStd::vector<char> decoded;
                AZ::IO::ByteContainerStream<AZStd::vector<char>> decodedOutput{&decoded};
                ASSERT_EQ(serializer.TextToData(text.data(), 0, decodedOutput, false), value.size() + 1);
                EXPECT_EQ(decoded, binary);
                AZ::IO::MemoryStream decodedInput{decoded.data(), decoded.size()};
                AZ::Symbol output;
                ASSERT_TRUE(serializer.Load(&output, decodedInput, 0, false));
                EXPECT_EQ(output, input);
            }
        }
    }

    TEST_F(SymbolSerializerTests, BinaryLoadPreservesDestinationWhenValueExceedsPolicy)
    {
        if constexpr (AZ::Internal::SymbolValueSizeLimit != 0)
        {
            AZStd::string value(AZ::Internal::SymbolValueSizeLimit + 1, 'p');
            const AZ::Symbol original{"BinaryPolicyOriginal"};
            AZ::Symbol output = original;
            AZ::IO::MemoryStream stream(value.c_str(), value.size() + 1);
            AZ::SymbolSerializer serializer;
            EXPECT_FALSE(serializer.Load(&output, stream, 0, false));
            EXPECT_EQ(output, original);
            EXPECT_FALSE(AZ::Symbol::Find(value));
        }
    }

    TEST_F(SymbolSerializerTests, TextEncodingIsCanonicalForEveryXmlUnsafeValue)
    {
        AZ::SymbolSerializer serializer;
        const AZStd::string value = MakeXmlUnsafeSymbolValue();
        const AZ::Symbol symbol{value};

        AZStd::vector<AZ::u8> binary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> binaryOutput(&binary);
        ASSERT_EQ(serializer.Save(&symbol, binaryOutput, false), value.size() + 1);

        AZ::IO::MemoryStream binaryInput(binary.data(), binary.size());
        AZStd::vector<char> text;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> textOutput(&text);
        const size_t encodedSize = serializer.DataToText(binaryInput, textOutput, false);
        const AZStd::string expected = MakeCanonicalXmlSafeText();

        EXPECT_EQ(encodedSize, expected.size());
        const AZStd::string_view encodedText{text.data(), text.size()};
        EXPECT_EQ(encodedText, expected);
    }

    TEST_F(SymbolSerializerTests, TextDecodingAcceptsLowercaseAndRedundantEscapesAndIsOnePass)
    {
        AZ::SymbolSerializer serializer;
        constexpr char EncodedValue[] = "%2f%41%25%01%ef%bf%be%EF%BF%BF";

        AZStd::vector<AZ::u8> binary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> binaryOutput(&binary);
        ASSERT_NE(serializer.TextToData(EncodedValue, 0, binaryOutput, false), 0);

        AZStd::string expected{"/A%"};
        expected.push_back('\x01');
        expected.append("\xEF\xBF\xBE", 3);
        expected.append("\xEF\xBF\xBF", 3);

        AZ::Symbol decoded;
        AZ::IO::MemoryStream binaryInput(binary.data(), binary.size());
        ASSERT_TRUE(serializer.Load(&decoded, binaryInput, 0, false));
        EXPECT_EQ(decoded.GetStringView(), expected);

        AZStd::vector<AZ::u8> onePassBinary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> onePassOutput(&onePassBinary);
        ASSERT_EQ(serializer.TextToData("%2525", 0, onePassOutput, false), 4);

        AZ::Symbol onePassDecoded;
        AZ::IO::MemoryStream onePassInput(onePassBinary.data(), onePassBinary.size());
        ASSERT_TRUE(serializer.Load(&onePassDecoded, onePassInput, 0, false));
        EXPECT_EQ(onePassDecoded.GetStringView(), "%25");

        AZ::IO::MemoryStream reencodeInput(onePassBinary.data(), onePassBinary.size());
        AZStd::vector<char> reencoded;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> reencodeOutput(&reencoded);
        ASSERT_EQ(serializer.DataToText(reencodeInput, reencodeOutput, false), 5);
        const AZStd::string_view reencodedText{reencoded.data(), reencoded.size()};
        EXPECT_EQ(reencodedText, "%2525");
    }

    TEST_F(SymbolSerializerTests, TextCodecRoundTripsLongValues)
    {
        AZ::SymbolSerializer serializer;
        const AZStd::string longValue(32768, '%');
        const AZ::Symbol longSymbol{longValue};

        AZStd::vector<AZ::u8> binary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> binaryOutput(&binary);
        ASSERT_EQ(serializer.Save(&longSymbol, binaryOutput, false), longValue.size() + 1);

        AZ::IO::MemoryStream binaryInput(binary.data(), binary.size());
        AZStd::vector<char> encoded;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> textOutput(&encoded);
        constexpr size_t EncodedTextSize = 32768 * 3;
        ASSERT_EQ(serializer.DataToText(binaryInput, textOutput, false), EncodedTextSize);
        ASSERT_EQ(encoded.size(), EncodedTextSize);
        for (size_t index = 0; index < encoded.size(); index += 3)
        {
            EXPECT_EQ(encoded[index], '%');
            EXPECT_EQ(encoded[index + 1], '2');
            EXPECT_EQ(encoded[index + 2], '5');
        }

        encoded.push_back('\0');
        AZStd::vector<AZ::u8> decodedBinary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> decodedOutput(&decodedBinary);
        EXPECT_EQ(serializer.TextToData(encoded.data(), 0, decodedOutput, false), longValue.size() + 1);

        AZ::Symbol decoded;
        AZ::IO::MemoryStream decodedInput(decodedBinary.data(), decodedBinary.size());
        ASSERT_TRUE(serializer.Load(&decoded, decodedInput, 0, false));
        EXPECT_EQ(decoded, longSymbol);

        AZStd::string invalidEncoded(EncodedTextSize + 1, 'e');
        invalidEncoded.append("%GG");
        AZStd::vector<AZ::u8> invalidEncodedOutput;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> invalidEncodedStream(&invalidEncodedOutput);
        EXPECT_EQ(serializer.TextToData(invalidEncoded.c_str(), 0, invalidEncodedStream, false), 0);
        EXPECT_EQ(invalidEncodedStream.GetCurPos(), 0);

        AZStd::string invalidDecoded(65536 + 1, 'd');
        invalidDecoded.append("%00");
        AZStd::vector<AZ::u8> invalidDecodedOutput;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> invalidDecodedStream(&invalidDecodedOutput);
        EXPECT_EQ(serializer.TextToData(invalidDecoded.c_str(), 0, invalidDecodedStream, false), 0);
        EXPECT_EQ(invalidDecodedStream.GetCurPos(), 0);
    }

    TEST_F(SymbolSerializerTests, TextCodecRejectsMalformedInputBeforeWriting)
    {
        AZ::SymbolSerializer serializer;

        AZStd::vector<AZ::u8> nullOutput{0xA5, 0x5A};
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> nullOutputStream(&nullOutput);
        EXPECT_EQ(serializer.TextToData(nullptr, 0, nullOutputStream, false), 0);
        EXPECT_EQ(nullOutputStream.GetCurPos(), 0);

        AZStd::vector<AZStd::string> invalidValues{
            "%",
            "%0",
            "%GG",
            "%00",
            "%C0%AF",
            "%ED%A0%80",
            AZStd::string(1024, 'v') + "%",
            AZStd::string(1024, 'v') + "%0",
            AZStd::string(1024, 'v') + "%GG",
            AZStd::string(1024, 'v') + "%C0%AF",
        };
        AZStd::string malformedUtf8;
        malformedUtf8.push_back(static_cast<char>(0xC0));
        malformedUtf8.push_back(static_cast<char>(0xAF));
        invalidValues.push_back(malformedUtf8);

        for (const AZStd::string& invalidValue : invalidValues)
        {
            AZStd::vector<AZ::u8> output{0xA5, 0x5A};
            AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> outputStream(&output);
            EXPECT_EQ(serializer.TextToData(invalidValue.c_str(), 0, outputStream, false), 0);
            EXPECT_EQ(outputStream.GetCurPos(), 0);
            ASSERT_EQ(output.size(), 2);
            EXPECT_EQ(output[0], 0xA5);
            EXPECT_EQ(output[1], 0x5A);
        }

        AZStd::vector<AZStd::vector<AZ::u8>> invalidBinaryValues{
            {},
            {'n', 'o', 'm', 'a', 'r', 'k', 'e', 'r'},
            {'a', 0, 'b', 0},
            {static_cast<AZ::u8>(0xC0), static_cast<AZ::u8>(0xAF), 0},
            AZStd::vector<AZ::u8>(65537 + 1, 'x'),
        };
        invalidBinaryValues.back().back() = 0;
        invalidBinaryValues.back()[65536] = 0xFF;
        for (const AZStd::vector<AZ::u8>& invalidValue : invalidBinaryValues)
        {
            AZ::IO::MemoryStream inputStream(invalidValue.data(), invalidValue.size());
            AZStd::vector<char> output{'x'};
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&output);
            EXPECT_EQ(serializer.DataToText(inputStream, outputStream, false), 0);
            EXPECT_EQ(outputStream.GetCurPos(), 0);
            ASSERT_EQ(output.size(), 1);
            EXPECT_EQ(output.front(), 'x');
        }
    }

    TEST_F(SymbolSerializerTests, ShortWritesReturnFailureAfterLeavingBoundedPartialOutput)
    {
        AZ::SymbolSerializer serializer;
        const AZ::Symbol symbol{AZStd::string_view{"ShortWrite"}};

        AZStd::array<char, 5> shortBinaryBuffer{};
        AZ::IO::MemoryStream shortBinaryOutput(shortBinaryBuffer.data(), shortBinaryBuffer.size(), 0);
        EXPECT_EQ(serializer.Save(&symbol, shortBinaryOutput, false), 0);
        EXPECT_EQ(shortBinaryOutput.GetCurPos(), shortBinaryBuffer.size());
        const AZStd::string_view shortBinaryPrefix{shortBinaryBuffer.data(), shortBinaryBuffer.size()};
        EXPECT_EQ(shortBinaryPrefix, "Short");

        const AZStd::array<AZ::u8, 2> validBinary{'a', 0};
        AZ::IO::MemoryStream shortTextInput(validBinary.data(), validBinary.size());
        shortTextInput.Seek(1, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::array<char, 4> untouchedText{};
        AZ::IO::MemoryStream untouchedTextOutput(untouchedText.data(), untouchedText.size(), 0);
        EXPECT_EQ(serializer.DataToText(shortTextInput, untouchedTextOutput, false), 0);
        EXPECT_EQ(untouchedTextOutput.GetCurPos(), 0);

        AZStd::vector<AZ::u8> longBinary(301, 'x');
        longBinary.back() = 0;
        AZ::IO::MemoryStream longBinaryInput(longBinary.data(), longBinary.size());
        AZStd::array<char, 10> shortTextBuffer{};
        AZ::IO::MemoryStream shortTextOutput(shortTextBuffer.data(), shortTextBuffer.size(), 0);
        EXPECT_EQ(serializer.DataToText(longBinaryInput, shortTextOutput, false), 0);
        EXPECT_EQ(shortTextOutput.GetCurPos(), shortTextBuffer.size());
        const AZStd::string_view shortTextPrefix{shortTextBuffer.data(), shortTextBuffer.size()};
        EXPECT_EQ(shortTextPrefix, "xxxxxxxxxx");

        AZStd::array<char, 4> shortDecodedBuffer{};
        AZ::IO::MemoryStream shortDecodedOutput(shortDecodedBuffer.data(), shortDecodedBuffer.size(), 0);
        EXPECT_EQ(serializer.TextToData("four", 0, shortDecodedOutput, false), 0);
        EXPECT_EQ(shortDecodedOutput.GetCurPos(), shortDecodedBuffer.size());
        const AZStd::string_view shortDecodedPrefix{shortDecodedBuffer.data(), shortDecodedBuffer.size()};
        EXPECT_EQ(shortDecodedPrefix, "four");
    }

    TEST_F(SymbolSerializerTests, EmptyTextRepresentationRoundTripsThroughBinaryFormat)
    {
        AZ::SymbolSerializer serializer;
        const AZ::Symbol empty;

        AZStd::vector<AZ::u8> binary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> binaryOutput(&binary);
        ASSERT_EQ(serializer.Save(&empty, binaryOutput, false), 1);

        AZ::IO::MemoryStream binaryInput(binary.data(), binary.size());
        AZStd::vector<char> text;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> textOutput(&text);
        EXPECT_EQ(serializer.DataToText(binaryInput, textOutput, false), 0);
        EXPECT_TRUE(text.empty());

        AZStd::vector<AZ::u8> decodedBinary;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> decodedOutput(&decodedBinary);
        EXPECT_EQ(serializer.TextToData("", 0, decodedOutput, false), 1);
        ASSERT_EQ(decodedBinary.size(), 1);
        EXPECT_EQ(decodedBinary.front(), 0);
    }

    TEST_F(SymbolSerializerTests, EmptySymbolRoundTripsThroughXmlObjectStream)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input;

        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
            outputStream,
            AZ::ObjectStream::ST_XML,
            &input,
            &serializeContext));

        AZ::Symbol output{AZStd::string_view{"ExistingXmlSymbol"}};
        AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
        EXPECT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
        EXPECT_TRUE(output.IsEmpty());
    }

    TEST_F(SymbolSerializerTests, JsonObjectStreamRejectsEmbeddedNulAndNonStringValues)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{AZStd::string_view{"good"}};

        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
            outputStream,
            AZ::ObjectStream::ST_JSON,
            &input,
            &serializeContext));

        const AZStd::string validJson(buffer.begin(), buffer.end());
        const size_t valuePosition = validJson.find("\"good\"");
        ASSERT_NE(valuePosition, AZStd::string::npos);

        AZ::ObjectStream::FilterDescriptor strictFilter;
        strictFilter.m_flags = AZ::ObjectStream::FILTERFLAG_STRICT;

        AZStd::string embeddedNulJson = validJson;
        embeddedNulJson.replace(valuePosition + 1, 4, "good\\u0000evil");
        AZ::Symbol embeddedNulOutput{AZStd::string_view{"UnchangedEmbeddedNulSymbol"}};
        AZ::IO::MemoryStream embeddedNulStream(embeddedNulJson.data(), embeddedNulJson.size());
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(
            embeddedNulStream,
            embeddedNulOutput,
            &serializeContext,
            strictFilter));
        AZ_TEST_STOP_TRACE_SUPPRESSION(2);
        EXPECT_EQ(embeddedNulOutput.GetStringView(), "UnchangedEmbeddedNulSymbol");

        AZStd::string nonStringJson = validJson;
        nonStringJson.replace(valuePosition, 6, "17");
        AZ::Symbol nonStringOutput{AZStd::string_view{"UnchangedNonStringSymbol"}};
        AZ::IO::MemoryStream nonStringStream(nonStringJson.data(), nonStringJson.size());
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(
            nonStringStream,
            nonStringOutput,
            &serializeContext,
            strictFilter));
        AZ_TEST_STOP_TRACE_SUPPRESSION(2);
        EXPECT_EQ(nonStringOutput.GetStringView(), "UnchangedNonStringSymbol");
    }

    TEST_F(SymbolSerializerTests, ObjectStreamFormatsRoundTripCanonicalTextCodec)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        AZStd::string value(16384, 's');
        value.append(MakeXmlUnsafeSymbolValue());
        const AZ::Symbol input{value};

        constexpr AZ::ObjectStream::StreamType StreamTypes[] = {
            AZ::ObjectStream::ST_BINARY,
            AZ::ObjectStream::ST_XML,
            AZ::ObjectStream::ST_JSON,
        };
        for (const AZ::ObjectStream::StreamType streamType : StreamTypes)
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, streamType, &input, &serializeContext));

            if (streamType != AZ::ObjectStream::ST_BINARY)
            {
                const AZStd::string serialized(buffer.begin(), buffer.end());
                EXPECT_NE(serialized.find("%25"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%01"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%09"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%0A"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%0D"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%EF%BF%BE"), AZStd::string::npos);
                EXPECT_NE(serialized.find("%EF%BF%BF"), AZStd::string::npos);
                EXPECT_EQ(serialized.find('\x01'), AZStd::string::npos);
                EXPECT_EQ(serialized.find("\xEF\xBF\xBE"), AZStd::string::npos);
                EXPECT_EQ(serialized.find("\xEF\xBF\xBF"), AZStd::string::npos);
            }

            AZ::Symbol output;
            AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
            EXPECT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
            EXPECT_EQ(output, input);
        }
    }

    TEST_F(SymbolSerializerTests, TextObjectStreamsRejectShortReadsWithoutAssignment)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{AZStd::string_view{"ShortReadInput"}};

        constexpr AZ::ObjectStream::StreamType StreamTypes[] = {
            AZ::ObjectStream::ST_XML,
            AZ::ObjectStream::ST_JSON,
        };
        for (const AZ::ObjectStream::StreamType streamType : StreamTypes)
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, streamType, &input, &serializeContext));

            AZ::Symbol output{AZStd::string_view{"UnchangedShortReadSymbol"}};
            ShortReadStream inputStream(buffer.data(), buffer.size());
            AZ_TEST_START_TRACE_SUPPRESSION;
            EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
            EXPECT_EQ(output.GetStringView(), "UnchangedShortReadSymbol");
        }
    }

    TEST_F(SymbolSerializerTests, BinaryObjectStreamRejectsEverySymbolTruncationBoundary)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);

        const AZStd::string value(2048, 's');
        const AZ::Symbol input{value};
        const AZ::Symbol unchanged{AZStd::string_view{"UnchangedTruncatedSymbol"}};
        ExpectBinaryObjectStreamTruncationRejected(input, unchanged, serializeContext);
    }

    TEST_F(SymbolSerializerTests, BinaryObjectStreamRejectsEveryPrimitiveTruncationBoundary)
    {
        AZ::SerializeContext serializeContext;
        constexpr AZ::u32 Input = 0x12345678;
        constexpr AZ::u32 Unchanged = 0xABCDEF01;
        ExpectBinaryObjectStreamTruncationRejected(Input, Unchanged, serializeContext);
    }
} // namespace UnitTest
