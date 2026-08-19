/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/Internal/SymbolAdmissionPolicy.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Utils/NoDestructor.h>
#include <AzCore/std/parallel/mutex.h>

namespace AzNetworking::Internal
{
    namespace
    {
        class NetworkOriginSymbolRegistry final
        {
        public:
            [[nodiscard]]
            bool FindOrAdmit(
                const AZStd::string_view spelling,
                SymbolAdmissionPolicy& policy,
                AZ::Symbol& symbol)
            {
                if (spelling.empty())
                {
                    symbol = AZ::Symbol{};
                    return true;
                }

                AZStd::lock_guard lock{m_mutex};
                if (AZ::Internal::FindSymbol(symbol, spelling))
                {
                    return true;
                }

                const AZ::u64 storageCost = GetNetworkSymbolStorageCost(spelling.size());
                if (!policy.HasCapacity()
                    || m_symbolCount >= MaxNetworkOriginSymbols
                    || storageCost > MaxNetworkOriginSymbolBytes - m_allocatedBytes)
                {
                    AZ_WarningOnce(
                        "AzNetworking",
                        false,
                        "Rejected a new network-origin Symbol because a permanent admission limit was reached");
                    return false;
                }

                symbol = AZ::Symbol{spelling};
                policy.CommitAdmission();
                ++m_symbolCount;
                m_allocatedBytes += storageCost;
                return true;
            }

        private:
            AZStd::mutex m_mutex;
            AZ::u64 m_allocatedBytes = 0;
            AZ::u32 m_symbolCount = 0;
        };

        NetworkOriginSymbolRegistry& GetNetworkOriginSymbolRegistry()
        {
            static AZ::NoDestructor<NetworkOriginSymbolRegistry> registry;
            return registry.Get();
        }
    } // namespace

    bool AdmitNetworkOriginSymbol(
        const AZStd::string_view spelling,
        SymbolAdmissionPolicy& policy,
        AZ::Symbol& symbol)
    {
        return GetNetworkOriginSymbolRegistry().FindOrAdmit(spelling, policy, symbol);
    }
} // namespace AzNetworking::Internal
