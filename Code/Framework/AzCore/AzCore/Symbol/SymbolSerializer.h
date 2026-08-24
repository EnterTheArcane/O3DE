/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Symbol/Symbol.h>

namespace AZ
{
    //! Serializes a Symbol as value bytes followed by one mandatory zero marker.
    class AZCORE_API SymbolSerializer final
        : public SerializeContext::IDataSerializer
    {
    public:
        size_t DataToText(
            IO::GenericStream& input,
            IO::GenericStream& output,
            bool isDataBigEndian) override;

        size_t TextToData(
            const char* text,
            unsigned int textVersion,
            IO::GenericStream& stream,
            bool isDataBigEndian) override;

        size_t Save(
            const void* classPtr,
            IO::GenericStream& stream,
            bool isDataBigEndian) override;

        bool Load(
            void* classPtr,
            IO::GenericStream& stream,
            unsigned int version,
            bool isDataBigEndian) override;

        bool CompareValueData(
            const void* lhs,
            const void* rhs) override;

    };
} // namespace AZ
