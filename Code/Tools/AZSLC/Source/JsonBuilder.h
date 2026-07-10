/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "StreamableInterface.h"

#include <AzCore/JSON/document.h>
#include <AzCore/JSON/prettywriter.h>
#include <AzCore/JSON/stringbuffer.h>

#include <concepts>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace AZ::ShaderCompiler
{
    using JsonAllocator = rapidjson::Document::AllocatorType;

    template <typename T>
    concept JsonStreamable = requires(Streamable& stream, const T& value)
    {
        { stream << value } -> std::same_as<Streamable&>;
    };

    class JsonBuilder
    {
    public:
        explicit JsonBuilder(rapidjson::Document& document)
            : m_allocator(document.GetAllocator())
        {
        }

        explicit JsonBuilder(JsonAllocator& allocator)
            : m_allocator(allocator)
        {
        }

        JsonAllocator& GetAllocator() const
        {
            return m_allocator;
        }

        rapidjson::Value MakeArray() const
        {
            return rapidjson::Value(rapidjson::kArrayType);
        }

        rapidjson::Value MakeObject() const
        {
            return rapidjson::Value(rapidjson::kObjectType);
        }

        rapidjson::Value MakeString(std::string_view value) const
        {
            return rapidjson::Value(value.data(), static_cast<rapidjson::SizeType>(value.size()), m_allocator);
        }

        void AddString(rapidjson::Value& object, std::string_view name, std::string_view value) const
        {
            object.AddMember(MakeString(name).Move(), MakeString(value), m_allocator);
        }

        template <JsonStreamable T>
        void AddStreamableString(rapidjson::Value& object, std::string_view name, const T& value) const
        {
            std::ostringstream baseStr;
            Streamable&& stream{MakeOStreamStreamable{baseStr}};
            stream << value;
            AddString(object, name, baseStr.str());
        }

        void AddValue(rapidjson::Value& object, std::string_view name, rapidjson::Value value) const
        {
            object.AddMember(MakeString(name).Move(), value.Move(), m_allocator);
        }

        template <typename T> requires std::is_arithmetic_v<std::decay_t<T>>
        void AddValue(rapidjson::Value& object, std::string_view name, T value) const
        {
            object.AddMember(MakeString(name).Move(), value, m_allocator);
        }

        void PushString(rapidjson::Value& array, std::string_view value) const
        {
            array.PushBack(MakeString(value), m_allocator);
        }

        void PushValue(rapidjson::Value& array, rapidjson::Value value) const
        {
            array.PushBack(value.Move(), m_allocator);
        }

        template <typename T> requires std::is_arithmetic_v<std::decay_t<T>>
        void PushValue(rapidjson::Value& array, T value) const
        {
            array.PushBack(value, m_allocator);
        }

        std::string Serialize(const rapidjson::Value& value) const
        {
            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter writer(buffer);
            value.Accept(writer);
            return buffer.GetString();
        }

    private:
        JsonAllocator& m_allocator;
    };
}
