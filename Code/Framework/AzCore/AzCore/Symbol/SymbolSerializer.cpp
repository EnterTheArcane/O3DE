/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/SymbolSerializer.h>

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AZ
{
    namespace
    {
        constexpr size_t MaxEncodedTextSize = Symbol::MaxStringSize * 3;
        constexpr size_t TextOutputChunkSize = 256;
        constexpr char UppercaseHexDigits[] = "0123456789ABCDEF";

        [[nodiscard]]
        bool TryDecodeHexDigit(
            const char digit,
            unsigned char& value)
        {
            if (digit >= '0' && digit <= '9')
            {
                value = static_cast<unsigned char>(digit - '0');
                return true;
            }
            if (digit >= 'A' && digit <= 'F')
            {
                value = static_cast<unsigned char>(digit - 'A' + 10);
                return true;
            }
            if (digit >= 'a' && digit <= 'f')
            {
                value = static_cast<unsigned char>(digit - 'a' + 10);
                return true;
            }
            return false;
        }
    } // namespace

    size_t SymbolSerializer::DataToText(
        IO::GenericStream& input,
        IO::GenericStream& output,
        bool)
    {
        const size_t dataSize = static_cast<size_t>(input.GetLength());
        if (dataSize == 0 || dataSize > Symbol::MaxStringBufferSize)
        {
            return 0;
        }

        AZStd::array<char, Symbol::MaxStringBufferSize> serializedValue;
        if (input.Read(dataSize, serializedValue.data()) != dataSize
            || serializedValue[dataSize - 1] != '\0')
        {
            return 0;
        }

        const AZStd::string_view value{serializedValue.data(), dataSize - 1};
        if (!Symbol::IsValid(value))
        {
            return 0;
        }

        AZStd::array<char, TextOutputChunkSize> textOutput;
        size_t bufferedSize = 0;
        size_t encodedSize = 0;

        const auto flushOutput = [&output, &textOutput, &bufferedSize]() -> bool
        {
            if (bufferedSize == 0)
            {
                return true;
            }
            if (output.Write(bufferedSize, textOutput.data()) != bufferedSize)
            {
                return false;
            }
            bufferedSize = 0;
            return true;
        };

        const auto appendRawByte = [&flushOutput, &textOutput, &bufferedSize, &encodedSize](const char byte) -> bool
        {
            if (bufferedSize == textOutput.size() && !flushOutput())
            {
                return false;
            }
            textOutput[bufferedSize++] = byte;
            ++encodedSize;
            return true;
        };

        const auto appendEscapedByte = [&flushOutput, &textOutput, &bufferedSize, &encodedSize](const unsigned char byte) -> bool
        {
            constexpr size_t EscapeSize = 3;
            if (textOutput.size() - bufferedSize < EscapeSize && !flushOutput())
            {
                return false;
            }
            textOutput[bufferedSize++] = '%';
            textOutput[bufferedSize++] = UppercaseHexDigits[byte >> 4];
            textOutput[bufferedSize++] = UppercaseHexDigits[byte & 0x0F];
            encodedSize += EscapeSize;
            return true;
        };

        size_t index = 0;
        while (index < value.size())
        {
            const unsigned char byte = static_cast<unsigned char>(value[index]);
            size_t escapeByteCount = 0;
            if (byte == '%'
                || (byte >= 0x01 && byte <= 0x1F))
            {
                escapeByteCount = 1;
            }
            else if (byte == 0xEF
                && index + 2 < value.size()
                && static_cast<unsigned char>(value[index + 1]) == 0xBF
                && (static_cast<unsigned char>(value[index + 2]) == 0xBE
                    || static_cast<unsigned char>(value[index + 2]) == 0xBF))
            {
                escapeByteCount = 3;
            }

            if (escapeByteCount > 0)
            {
                for (size_t escapedIndex = 0; escapedIndex < escapeByteCount; ++escapedIndex)
                {
                    if (!appendEscapedByte(static_cast<unsigned char>(value[index + escapedIndex])))
                    {
                        return 0;
                    }
                }
                index += escapeByteCount;
            }
            else
            {
                if (!appendRawByte(value[index]))
                {
                    return 0;
                }
                ++index;
            }
        }

        if (!flushOutput())
        {
            return 0;
        }
        return encodedSize;
    }

    size_t SymbolSerializer::TextToData(
        const char* text,
        unsigned int,
        IO::GenericStream& stream,
        bool)
    {
        if (!text)
        {
            return 0;
        }

        const size_t encodedSize = strnlen(text, MaxEncodedTextSize + 1);
        if (encodedSize > MaxEncodedTextSize)
        {
            return 0;
        }

        AZStd::array<char, Symbol::MaxStringBufferSize> decodedValue;
        size_t decodedSize = 0;
        size_t index = 0;
        while (index < encodedSize)
        {
            unsigned char decodedByte = 0;
            if (text[index] == '%')
            {
                if (index + 2 >= encodedSize)
                {
                    return 0;
                }

                unsigned char highNibble = 0;
                unsigned char lowNibble = 0;
                if (!TryDecodeHexDigit(text[index + 1], highNibble)
                    || !TryDecodeHexDigit(text[index + 2], lowNibble))
                {
                    return 0;
                }
                decodedByte = static_cast<unsigned char>((highNibble << 4) | lowNibble);
                index += 3;
            }
            else
            {
                decodedByte = static_cast<unsigned char>(text[index]);
                ++index;
            }

            if (decodedByte == 0 || decodedSize == Symbol::MaxStringSize)
            {
                return 0;
            }
            decodedValue[decodedSize++] = static_cast<char>(decodedByte);
        }

        const AZStd::string_view value{decodedValue.data(), decodedSize};
        if (!Symbol::IsValid(value))
        {
            return 0;
        }

        decodedValue[decodedSize] = '\0';
        const size_t serializedSize = decodedSize + 1;
        if (stream.Write(serializedSize, decodedValue.data()) != serializedSize)
        {
            return 0;
        }
        return serializedSize;
    }

    size_t SymbolSerializer::Save(
        const void* classPtr,
        IO::GenericStream& stream,
        bool)
    {
        const Symbol* symbol = reinterpret_cast<const Symbol*>(classPtr);
        const AZStd::string_view value = symbol->GetStringView();
        const size_t serializedSize = value.size() + 1;
        if (stream.Write(serializedSize, symbol->GetCStr()) != serializedSize)
        {
            return 0;
        }
        return serializedSize;
    }

    bool SymbolSerializer::Load(
        void* classPtr,
        IO::GenericStream& stream,
        unsigned int,
        bool)
    {
        const size_t size = static_cast<size_t>(stream.GetLength());
        if (size == 0 || size > Symbol::MaxStringBufferSize)
        {
            return false;
        }

        AZStd::array<char, Symbol::MaxStringBufferSize> encodedValue;
        if (stream.Read(size, encodedValue.data()) != size
            || encodedValue[size - 1] != '\0')
        {
            return false;
        }

        const AZStd::string_view value{encodedValue.data(), size - 1};
        const AZStd::optional<Symbol> symbol = Symbol::TryCreate(value);
        if (!symbol)
        {
            return false;
        }

        Symbol* output = reinterpret_cast<Symbol*>(classPtr);
        *output = *symbol;
        return true;
    }

    bool SymbolSerializer::CompareValueData(
        const void* lhs,
        const void* rhs)
    {
        const Symbol* lhsSymbol = reinterpret_cast<const Symbol*>(lhs);
        const Symbol* rhsSymbol = reinterpret_cast<const Symbol*>(rhs);
        return *lhsSymbol == *rhsSymbol;
    }
} // namespace AZ
