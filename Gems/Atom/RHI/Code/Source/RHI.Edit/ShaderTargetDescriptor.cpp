/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI.Edit/ShaderTargetDescriptor.h>

namespace AZ::RHI
{
    AZStd::string_view ShaderTargetDescriptor::GetStageProfile(ShaderHardwareStage stage) const
    {
        const auto profileIterator = m_stageProfiles.find(stage);
        if (profileIterator != m_stageProfiles.end())
        {
            return profileIterator->second;
        }
        return {};
    }

    HashValue64 ShaderTargetDescriptor::GetHash() const
    {
        HashValue64 hash = TypeHash64(m_format, HashValue64{0});

        // Stage-profile hashing must be order independent: iterate stages in enum order.
        constexpr ShaderHardwareStage stagesInEnumOrder[] = {
            ShaderHardwareStage::Vertex,
            ShaderHardwareStage::Geometry,
            ShaderHardwareStage::Fragment,
            ShaderHardwareStage::Compute,
            ShaderHardwareStage::RayTracing,
        };
        for (const ShaderHardwareStage stage : stagesInEnumOrder)
        {
            const AZStd::string_view profile = GetStageProfile(stage);
            if (!profile.empty())
            {
                hash = TypeHash64(stage, hash);
                hash = TypeHash64(reinterpret_cast<const uint8_t*>(profile.data()), profile.size(), hash);
            }
        }

        hash = TypeHash64(m_conventions.m_invertY, hash);
        hash = TypeHash64(m_conventions.m_useDxPositionW, hash);
        hash = TypeHash64(m_conventions.m_useDxMemoryLayout, hash);
        hash = TypeHash64(m_conventions.m_uniqueBindingIndicesPerSet, hash);
        hash = TypeHash64(m_conventions.m_enable16BitTypes, hash);
        hash = TypeHash64(m_rootConstantCapacityInBytes, hash);
        hash = TypeHash64(m_constantBufferAlignmentInBytes, hash);

        for (const AZStd::string_view stepName : m_postProcessSteps)
        {
            hash = TypeHash64(reinterpret_cast<const uint8_t*>(stepName.data()), stepName.size(), hash);
        }

        return hash;
    }
}
