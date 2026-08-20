/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Constraint.h>
#include <Jolt/Handle.h>
#include <Jolt/SoftBody.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    inline constexpr AZ::u32 InvalidSceneIndex = AZStd::numeric_limits<AZ::u32>::max();

    struct SceneRigidBodyConfiguration final
    {
        AZ_TYPE_INFO(SceneRigidBodyConfiguration, SceneRigidBodyConfigurationTypeId);

        CookedShapeHandle m_cookedShapeHandle;
        BodyConfiguration m_body;
    };

    struct SceneConstraintConfiguration final
    {
        AZ_TYPE_INFO(SceneConstraintConfiguration, SceneConstraintConfigurationTypeId);

        ConstraintConfiguration m_constraint;
        AZ::u32 m_firstBodyIndex = InvalidSceneIndex;
        AZ::u32 m_secondBodyIndex = InvalidSceneIndex;
        AZ::u32 m_firstDependencyIndex = InvalidSceneIndex;
        AZ::u32 m_secondDependencyIndex = InvalidSceneIndex;
    };

    using SceneBodyConfiguration = AZStd::variant<SceneRigidBodyConfiguration, SoftBodyConfiguration>;

    struct SceneConfiguration final
    {
        AZ_TYPE_INFO(SceneConfiguration, SceneConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<SceneBodyConfiguration> m_bodies;
        AZStd::vector<SceneConstraintConfiguration> m_constraints;
        AZ::Name m_name;
    };

    struct SceneDefinitionState final
    {
        AZ_TYPE_INFO(SceneDefinitionState, SceneDefinitionStateTypeId);

        AZ::Name m_name;
        AZ::u32 m_bodyCount = 0;
        AZ::u32 m_rigidBodyCount = 0;
        AZ::u32 m_softBodyCount = 0;
        AZ::u32 m_constraintCount = 0;
        AZ::u32 m_instanceCount = 0;
    };

    struct SceneInstanceState final
    {
        AZ_TYPE_INFO(SceneInstanceState, SceneInstanceStateTypeId);

        SceneDefinitionHandle m_definitionHandle;
        AZ::u32 m_bodyCount = 0;
        AZ::u32 m_rigidBodyCount = 0;
        AZ::u32 m_softBodyCount = 0;
        AZ::u32 m_constraintCount = 0;
    };
} // namespace Jolt
