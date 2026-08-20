/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ShapeOwner.h>

#include <Jolt/SystemInternal.h>

#include <AzCore/Math/MathUtils.h>

namespace Jolt::Internal
{
    bool CreateShapeSet(
        RuntimeImplementation& system,
        const WorldHandle worldHandle,
        const AZStd::span<const ColliderShapeConfiguration> configurations,
        const float uniformScale,
        ShapeSet& shapeSet)
    {
        if (configurations.empty()
            || !AZ::IsFiniteFloat(uniformScale)
            || uniformScale <= 0.0f)
        {
            return false;
        }

        shapeSet.m_shapeHandles.reserve(configurations.size());
        for (const ColliderShapeConfiguration& configuration : configurations)
        {
            if (!configuration.m_localTransform.IsFinite())
            {
                DestroyShapeSet(system, worldHandle, shapeSet);
                return false;
            }

            ShapeConfiguration runtimeConfiguration = configuration.m_shape;
            for (const MaterialConfiguration& materialConfiguration : configuration.m_materials)
            {
                const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
                if (!materialHandle)
                {
                    DestroyShapeSet(system, worldHandle, shapeSet);
                    return false;
                }
                shapeSet.m_ownedMaterials.push_back(materialHandle);
                runtimeConfiguration.m_materials.push_back(materialHandle);
            }

            ShapeHandle shapeHandle = system.CreateShape(worldHandle, runtimeConfiguration);
            if (!shapeHandle)
            {
                DestroyShapeSet(system, worldHandle, shapeSet);
                return false;
            }
            shapeSet.m_ownedShapes.push_back(shapeHandle);

            const float shapeScale = uniformScale * configuration.m_localTransform.GetUniformScale();
            if (!AZ::IsFiniteFloat(shapeScale) || shapeScale <= 0.0f)
            {
                DestroyShapeSet(system, worldHandle, shapeSet);
                return false;
            }
            if (!AZ::IsClose(shapeScale, 1.0f, AZ::Constants::Tolerance))
            {
                DecoratedShapeConfiguration scaledConfiguration;
                scaledConfiguration.m_geometry = ScaledShapeConfiguration{
                    .m_shapeHandle = shapeHandle,
                    .m_scale = AZ::Vector3(shapeScale),
                };
                scaledConfiguration.m_userData = runtimeConfiguration.m_userData;
                shapeHandle = system.CreateShape(worldHandle, scaledConfiguration);
                if (!shapeHandle)
                {
                    DestroyShapeSet(system, worldHandle, shapeSet);
                    return false;
                }
                shapeSet.m_ownedShapes.push_back(shapeHandle);
            }

            if (configurations.size() == 1)
            {
                const AZ::Vector3 localPosition = configuration.m_localTransform.GetTranslation() * uniformScale;
                const AZ::Quaternion localRotation = configuration.m_localTransform.GetRotation();
                if (!localPosition.IsZero(AZ::Constants::Tolerance)
                    || !localRotation.IsClose(AZ::Quaternion::CreateIdentity(), AZ::Constants::Tolerance))
                {
                    DecoratedShapeConfiguration transformedConfiguration;
                    transformedConfiguration.m_geometry = RotatedTranslatedShapeConfiguration{
                        .m_shapeHandle = shapeHandle,
                        .m_position = localPosition,
                        .m_rotation = localRotation,
                    };
                    transformedConfiguration.m_userData = runtimeConfiguration.m_userData;
                    shapeHandle = system.CreateShape(worldHandle, transformedConfiguration);
                    if (!shapeHandle)
                    {
                        DestroyShapeSet(system, worldHandle, shapeSet);
                        return false;
                    }
                    shapeSet.m_ownedShapes.push_back(shapeHandle);
                }
            }
            shapeSet.m_shapeHandles.push_back(shapeHandle);
        }

        if (shapeSet.m_shapeHandles.size() == 1)
        {
            shapeSet.m_rootShapeHandle = shapeSet.m_shapeHandles.front();
            return true;
        }

        CompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children.reserve(shapeSet.m_shapeHandles.size());
        for (size_t shapeIndex = 0; shapeIndex < shapeSet.m_shapeHandles.size(); ++shapeIndex)
        {
            const ColliderShapeConfiguration& configuration = configurations[shapeIndex];
            compoundConfiguration.m_children.push_back({
                .m_position = configuration.m_localTransform.GetTranslation() * uniformScale,
                .m_rotation = configuration.m_localTransform.GetRotation(),
                .m_shapeHandle = shapeSet.m_shapeHandles[shapeIndex],
                .m_userData = configuration.m_compoundUserData,
            });
        }
        const ShapeHandle compoundHandle = system.CreateShape(worldHandle, compoundConfiguration);
        if (!compoundHandle)
        {
            DestroyShapeSet(system, worldHandle, shapeSet);
            return false;
        }
        shapeSet.m_ownedShapes.push_back(compoundHandle);
        shapeSet.m_rootShapeHandle = compoundHandle;
        return true;
    }

    void DestroyShapeSet(
        RuntimeImplementation& system,
        const WorldHandle worldHandle,
        ShapeSet& shapeSet)
    {
        for (
            auto shapeIterator = shapeSet.m_ownedShapes.rbegin();
            shapeIterator != shapeSet.m_ownedShapes.rend();
            ++shapeIterator)
        {
            [[maybe_unused]] const bool destroyed = system.DestroyShape(worldHandle, *shapeIterator);
        }
        for (const MaterialHandle materialHandle : shapeSet.m_ownedMaterials)
        {
            [[maybe_unused]] const bool destroyed = system.DestroyMaterial(materialHandle);
        }
        shapeSet = {};
    }
} // namespace Jolt::Internal
