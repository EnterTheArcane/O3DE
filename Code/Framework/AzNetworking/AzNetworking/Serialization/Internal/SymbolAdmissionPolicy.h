/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Symbol.h>

namespace AzNetworking
{
    class IConnection;
}

namespace AzNetworking::Internal
{
    inline constexpr AZ::u32 MaxNewNetworkOriginSymbolsPerConnection = 1024;
    inline constexpr AZ::u32 MaxNetworkOriginSymbols = 262144;
    inline constexpr AZ::u64 MaxNetworkOriginSymbolBytes = 64 * 1024 * 1024;
    inline constexpr AZ::u64 NetworkSymbolEntryOverheadBytes = 64;

    class SymbolAdmissionPolicy final
    {
    public:
        explicit SymbolAdmissionPolicy(
            const AZ::u32 maxNewSymbolCount = MaxNewNetworkOriginSymbolsPerConnection)
            : m_maxNewSymbolCount{maxNewSymbolCount}
        {
        }

        [[nodiscard]]
        bool HasCapacity() const
        {
            return m_newSymbolCount < m_maxNewSymbolCount;
        }

        void CommitAdmission()
        {
            ++m_newSymbolCount;
        }

    private:
        AZ::u32 m_maxNewSymbolCount;
        AZ::u32 m_newSymbolCount = 0;
    };

    //! Resolves an existing Symbol or admits a bounded new network-origin spelling.
    //! All process-wide and per-connection quota checks are committed atomically
    //! with respect to other network-origin admissions.
    [[nodiscard]]
    bool AdmitNetworkOriginSymbol(
        AZStd::string_view spelling,
        SymbolAdmissionPolicy& policy,
        AZ::Symbol& symbol);

    [[nodiscard]]
    SymbolAdmissionPolicy& GetSymbolAdmissionPolicy(
        IConnection& connection);

    [[nodiscard]]
    constexpr AZ::u64 GetNetworkSymbolStorageCost(const size_t spellingSize)
    {
        return NetworkSymbolEntryOverheadBytes + spellingSize;
    }
} // namespace AzNetworking::Internal
