/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/SymbolSerializer.h>

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AZ
{
    size_t SymbolSerializer::DataToText(
        IO::GenericStream& input,
        IO::GenericStream& output,
        bool)
    {
        const size_t dataSize = static_cast<size_t>(input.GetLength());
        if (dataSize == 0 || dataSize > Symbol::MaxLength + 1)
        {
            return TextConversionFailure;
        }

        AZStd::array<char, Symbol::MaxLength + 1> encodedValue;
        if (input.Read(dataSize, encodedValue.data()) != dataSize
            || encodedValue[dataSize - 1] != '\0')
        {
            return TextConversionFailure;
        }

        const AZStd::string_view value{encodedValue.data(), dataSize - 1};
        if (Internal::ValidateSymbolSpelling(value) != Internal::SymbolValidationError::None)
        {
            return TextConversionFailure;
        }
        if (output.Write(value.size(), value.data()) != value.size())
        {
            return TextConversionFailure;
        }
        return value.size();
    }

    size_t SymbolSerializer::TextToData(
        const char* text,
        unsigned int,
        IO::GenericStream& stream,
        bool)
    {
        if (!text)
        {
            return TextConversionFailure;
        }

        const size_t size = strnlen(text, Symbol::MaxLength + 1);
        if (size > Symbol::MaxLength)
        {
            return TextConversionFailure;
        }
        const AZStd::string_view value{text, size};
        if (Internal::ValidateSymbolSpelling(value) != Internal::SymbolValidationError::None)
        {
            return TextConversionFailure;
        }
        if (stream.Write(size, text) != size
            || stream.Write(1, "") != 1)
        {
            return TextConversionFailure;
        }
        return size + 1;
    }

    size_t SymbolSerializer::Save(
        const void* classPtr,
        IO::GenericStream& stream,
        bool)
    {
        const Symbol* symbol = reinterpret_cast<const Symbol*>(classPtr);
        const AZStd::string_view value = symbol->GetStringView();
        if (stream.Write(value.size(), symbol->GetCStr()) != value.size()
            || stream.Write(1, "") != 1)
        {
            return 0;
        }
        return value.size() + 1;
    }

    bool SymbolSerializer::Load(
        void* classPtr,
        IO::GenericStream& stream,
        unsigned int,
        bool)
    {
        const size_t size = static_cast<size_t>(stream.GetLength());
        if (size == 0 || size > Symbol::MaxLength + 1)
        {
            return false;
        }

        AZStd::array<char, Symbol::MaxLength + 1> encodedValue;
        if (stream.Read(size, encodedValue.data()) != size
            || encodedValue[size - 1] != '\0')
        {
            return false;
        }

        const AZStd::string_view spelling{encodedValue.data(), size - 1};
        if (Internal::ValidateSymbolSpelling(spelling) != Internal::SymbolValidationError::None)
        {
            return false;
        }

        Symbol* symbol = reinterpret_cast<Symbol*>(classPtr);
        *symbol = Internal::InternValidatedSymbol(spelling);
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
