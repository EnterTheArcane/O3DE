/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AzNetworking
{
    enum class SymbolAdmission : AZ::u8
    {
        TrustedLocal,
        NetworkOrigin,
        ExistingOnly,
    };

    namespace Internal
    {
        class SymbolAdmissionPolicy;

        //! Narrow capability used while decoding raw Symbol spellings.
        //! The policy is borrowed and must outlive the serializer using this context.
        struct SymbolSerializationContext final
        {
            SymbolAdmission m_admission = SymbolAdmission::TrustedLocal;
            SymbolAdmissionPolicy* m_policy = nullptr;
        };
    } // namespace Internal
} // namespace AzNetworking
