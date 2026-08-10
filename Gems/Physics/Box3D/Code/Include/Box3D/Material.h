/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <cstddef>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    //! Optional renderer-facing appearance used by diagnostic geometry.
    enum class DebugMaterialPreset : AZ::u8
    {
        Default,
        Matte,
        Soft,
        Dead,
        Glossy,
        Metallic,
    };

    //! Physical and diagnostic properties shared by shapes through a MaterialHandle.
    struct MaterialConfiguration final
    {
        AZ_TYPE_INFO(MaterialConfiguration, MaterialConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZ::Name m_name;

        AZ::Vector3 m_tangentVelocity = AZ::Vector3::CreateZero();
        AZ::Color m_debugColor = AZ::Colors::White;

        AZ::u64 m_surfaceTypeId = 0;

        float m_friction = 0.6f;
        float m_restitution = 0.0f;
        float m_rollingResistance = 0.0f;
        float m_density = 1000.0f;
        float m_explosionScale = 1.0f;

        DebugMaterialPreset m_debugMaterialPreset = DebugMaterialPreset::Default;
        bool m_debugAppearanceEnabled = false;
    };

    struct MaterialHandleCollection final
    {
        AZ_TYPE_INFO(MaterialHandleCollection, MaterialHandleCollectionTypeId);

        MaterialHandleCollection() = default;

        explicit MaterialHandleCollection(
            AZStd::span<const MaterialHandle> handles)
            : m_handles(handles.begin(), handles.end())
        {
        }

        void Add(
            MaterialHandle handle)
        {
            m_handles.push_back(handle);
        }

        void Clear()
        {
            m_handles.clear();
        }

        [[nodiscard]]
        size_t GetCount() const
        {
            return m_handles.size();
        }

        [[nodiscard]]
        MaterialHandle GetAt(
            size_t index) const
        {
            if (index < m_handles.size())
            {
                return m_handles[index];
            }

            return {};
        }

        [[nodiscard]]
        AZStd::span<const MaterialHandle> GetHandles() const
        {
            return m_handles;
        }

    private:
        AZStd::vector<MaterialHandle> m_handles;
    };

    struct MaterialConfigurationCollection final
    {
        AZ_TYPE_INFO(MaterialConfigurationCollection, MaterialConfigurationCollectionTypeId);

        MaterialConfigurationCollection() = default;

        explicit MaterialConfigurationCollection(
            AZStd::span<const MaterialConfiguration> configurations)
            : m_configurations(configurations.begin(), configurations.end())
        {
        }

        void Add(
            const MaterialConfiguration& configuration)
        {
            m_configurations.push_back(configuration);
        }

        void Clear()
        {
            m_configurations.clear();
        }

        [[nodiscard]]
        size_t GetCount() const
        {
            return m_configurations.size();
        }

        [[nodiscard]]
        MaterialConfiguration GetAt(
            size_t index) const
        {
            if (index < m_configurations.size())
            {
                return m_configurations[index];
            }

            return {};
        }

        [[nodiscard]]
        AZStd::span<const MaterialConfiguration> GetConfigurations() const
        {
            return m_configurations;
        }

    private:
        AZStd::vector<MaterialConfiguration> m_configurations;
    };
} // namespace Box3D

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Box3D::DebugMaterialPreset, Box3D::DebugMaterialPresetTypeId);
}
