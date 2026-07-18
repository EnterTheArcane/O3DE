/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/typetraits/underlying_type.h>
#include <AzCore/std/typetraits/conditional.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/RTTI/TypeSafeIntegral.h>

namespace AzNetworking
{
    template <typename C>
    concept IterableContainer = requires(C& value)
    {
        typename C::value_type;
        typename C::const_iterator;
        typename C::size_type;
        value.begin();
        value.end();
        value.size();
    };

    // Identifies AZStd containers
    struct AzContainerHelper
    {
        template <typename TYPE>
            requires requires(TYPE& value, typename TYPE::size_type size) { value.reserve(size); }
        static void ReserveContainer(TYPE& value, typename TYPE::size_type size)
        {
            value.reserve(size);
        }

        template<typename TYPE>
            requires (!requires(TYPE& value, typename TYPE::size_type size) { value.reserve(size); })
        static void ReserveContainer(TYPE&, typename TYPE::size_type)
        {
            ;
        }
    };

    template <typename OBJECT_TYPE>
    struct SerializeType
    {
        static bool Serialize(ISerializer& serializer, OBJECT_TYPE& value)
        {
            return value.Serialize(serializer);
        }
    };

    // Base template
    template <typename TYPE, typename = void>
    struct SerializeObjectHelper
    {
        static bool SerializeObject(ISerializer& serializer, TYPE& value)
        {
            return SerializeType<TYPE>::Serialize(serializer, value);
        }
    };

    // Non-containers
    template <typename TYPE>
        requires (!IterableContainer<TYPE>)
    struct SerializeObjectHelper<TYPE, void>
    {
        static bool SerializeObject(ISerializer& serializer, TYPE& value)
        {
            return SerializeType<TYPE>::Serialize(serializer, value);
        }
    };

    inline bool ISerializer::IsValid() const
    {
        return m_serializerValid;
    }

    inline void ISerializer::Invalidate()
    {
        m_serializerValid = false;
    }

    inline bool ISerializer::Serialize(char& value, const char* name, uint8_t minValue, uint8_t maxValue)
    {
        return Serialize(reinterpret_cast<uint8_t&>(value), name, minValue, maxValue);
    }

    template <typename TYPE>
    inline bool ISerializer::Serialize(TYPE& value, const char* name)
    {
        enum { IsEnum = AZStd::is_enum<TYPE>::value };
        enum { IsTypeSafeIntegral = AZStd::is_type_safe_integral<TYPE>::value };
        return SerializeHelper<IsEnum, IsTypeSafeIntegral>::Serialize(*this, value, name);
    }

    // Helper for objects and structures
    template <>
    struct ISerializer::SerializeHelper<false, false>
    {
        template <typename TYPE>
        static bool Serialize(ISerializer& serializer, TYPE& value, const char* name)
        {
            if (serializer.BeginObject(name))
            {
                if (SerializeObjectHelper<TYPE>::SerializeObject(serializer, value))
                {
                    return serializer.EndObject(name);
                }
            }
            return false;
        }
    };

    // Helper for enums
    template <>
    struct ISerializer::SerializeHelper<true, false>
    {
        template <typename TYPE>
        static bool Serialize(ISerializer& serializer, TYPE& value, const char* name)
        {
            using SizeType = typename AZStd::underlying_type<TYPE>::type;

            SizeType& integralValue = reinterpret_cast<SizeType&>(value);

            if (!serializer.Serialize(integralValue, name))
            {
                return false;
            }

            //auto enumMembers = AzEnumTraits<TYPE>::Members;
            //if (AZStd::find(enumMembers.begin(), enumMembers.end(), static_cast<Type>(integralValue)) == enumMembers.end())
            //{
            //    return false;
            //}

            return true;
        }
    };

    // Helper for type-safe integrals
    template <>
    struct ISerializer::SerializeHelper<true, true>
    {
        template <typename TYPE>
        static bool Serialize(ISerializer& serializer, TYPE& value, const char* name)
        {
            using RawType = typename AZStd::underlying_type<TYPE>::type;

            RawType& rawValue = reinterpret_cast<RawType&>(value);

            if (!serializer.Serialize(rawValue, name))
            {
                return false;
            }

            return true;
        }
    };
}

#include <AzNetworking/Serialization/AzContainerSerializers.h>
