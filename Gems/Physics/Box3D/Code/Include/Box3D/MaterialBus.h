/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>
#include <Box3D/Material.h>

#include <AzCore/EBus/EBus.h>

namespace Box3D
{
    struct MaterialResult final
    {
        AZ_TYPE_INFO(MaterialResult, MaterialResultTypeId);

        MaterialConfiguration m_configuration;
        bool m_found = false;
    };

    class MaterialRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        [[nodiscard]]
        virtual MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) = 0;

        virtual bool UpdateMaterial(
            MaterialHandle materialHandle,
            const MaterialConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual MaterialResult GetMaterial(MaterialHandle materialHandle) const = 0;

        virtual bool DestroyMaterial(MaterialHandle materialHandle) = 0;
    };

    using MaterialRequestBus = AZ::EBus<MaterialRequests>;
} // namespace Box3D
