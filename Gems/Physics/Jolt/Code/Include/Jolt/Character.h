/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Rollback.h>

#include <Jolt/Collision.h>
#include <Jolt/BodyConfiguration.h>
#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace Jolt
{
    enum class GroundState : AZ::u8
    {
        None = 0,
        InAir,
        NotSupported,
        OnGround,
        OnSteepGround,
    };

    struct CharacterConfiguration final
    {
        AZ_TYPE_INFO(CharacterConfiguration, CharacterConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        WorldTransform m_transform;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        CollisionGroupConfiguration m_collisionGroup;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        AllowedDofs m_allowedDofs =
            AllowedDofs::TranslationX
            | AllowedDofs::TranslationY
            | AllowedDofs::TranslationZ;

        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_friction = 0.2f;
        float m_gravityFactor = 1.0f;
        float m_mass = 80.0f;
        float m_maximumSeparationDistance = 0.05f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_supportingPlaneDistance = -1.0e10f;

        bool m_activate = true;
        bool m_enhancedInternalEdgeRemoval = false;
    };

    struct CharacterState final
    {
        AZ_TYPE_INFO(CharacterState, CharacterStateTypeId);

        WorldTransform m_transform;
        WorldPosition m_groundPosition;

        AZ::Vector3 m_groundNormal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_groundVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();

        BodyHandle m_bodyHandle;
        BodyHandle m_groundBodyHandle;
        MaterialHandle m_groundMaterialHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_groundSubShapeId;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        GroundState m_groundState = GroundState::None;

        bool m_isInSimulation = false;
        bool m_isSupported = false;
    };

    struct CharacterRuntimeConfiguration final
    {
        AZ_TYPE_INFO(CharacterRuntimeConfiguration, CharacterRuntimeConfigurationTypeId);

        ObjectLayer m_objectLayer = DefaultLayers::Moving;

        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_maximumSeparationDistance = 0.05f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_supportingPlaneDistance = -1.0e10f;
    };

    struct VirtualCharacterConfiguration final
    {
        AZ_TYPE_INFO(VirtualCharacterConfiguration, VirtualCharacterConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        ShapeHandle m_innerBodyShapeHandle;
        WorldTransform m_transform;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        ObjectLayer m_innerBodyObjectLayer = DefaultLayers::Moving;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;

        AZ::Vector3 m_shapeOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_characterPadding = 0.02f;
        float m_collisionTolerance = 1.0e-3f;
        float m_hitReductionCosMaximumAngle = 0.999f;
        float m_mass = 70.0f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_maximumStrength = 100.0f;
        float m_minimumTimeRemaining = 1.0e-4f;
        float m_penetrationRecoverySpeed = 1.0f;
        float m_predictiveContactDistance = 0.1f;
        float m_supportingPlaneDistance = -1.0e10f;

        AZ::u32 m_maximumCollisionIterations = 5;
        AZ::u32 m_maximumConstraintIterations = 15;
        AZ::u32 m_maximumHitCount = 256;

        bool m_collideWithBackFaces = true;
        bool m_enhancedInternalEdgeRemoval = false;
    };

    struct VirtualCharacterUpdateConfiguration final
    {
        AZ_TYPE_INFO(VirtualCharacterUpdateConfiguration, VirtualCharacterUpdateConfigurationTypeId);

        AZ::Vector3 m_gravity = AZ::Vector3(0.0f, 0.0f, -9.81f);
        AZ::Vector3 m_stickToFloorStepDown = AZ::Vector3(0.0f, 0.0f, -0.5f);
        AZ::Vector3 m_walkStairsStepDownExtra = AZ::Vector3::CreateZero();
        AZ::Vector3 m_walkStairsStepUp = AZ::Vector3(0.0f, 0.0f, 0.4f);

        float m_walkStairsCosAngleForwardContact = 0.25881905f;
        float m_walkStairsMinimumStepForward = 0.02f;
        float m_walkStairsStepForwardTest = 0.15f;

        bool m_extended = true;
    };

    struct VirtualCharacterRuntimeConfiguration final
    {
        AZ_TYPE_INFO(VirtualCharacterRuntimeConfiguration, VirtualCharacterRuntimeConfigurationTypeId);

        AZ::Vector3 m_shapeOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_hitReductionCosMaximumAngle = 0.999f;
        float m_mass = 70.0f;
        float m_maximumSlopeAngle = 0.87266463f;
        float m_maximumStrength = 100.0f;
        float m_penetrationRecoverySpeed = 1.0f;
        float m_supportingPlaneDistance = -1.0e10f;

        AZ::u32 m_maximumHitCount = 256;

        bool m_enhancedInternalEdgeRemoval = false;
    };

    struct CharacterCollisionRequest final
    {
        AZ_TYPE_INFO(CharacterCollisionRequest, CharacterCollisionRequestTypeId);

        WorldTransform m_transform;
        AZ::Vector3 m_movementDirection = AZ::Vector3::CreateZero();
        ShapeHandle m_shapeHandle;
        float m_maximumSeparationDistance = 0.0f;
    };

    struct CharacterCollisionHit final
    {
        AZ_TYPE_INFO(CharacterCollisionHit, CharacterCollisionHitTypeId);

        WorldPosition m_queryContactPosition;
        WorldPosition m_targetContactPosition;

        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();

        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        CharacterHandle m_characterHandle;
        VirtualCharacterHandle m_virtualCharacterHandle;
        SubShapeId m_querySubShapeId;
        SubShapeId m_targetSubShapeId;

        float m_penetrationDepth = 0.0f;
    };

    class ICharacterCollisionFilter
        : public IQueryFilter
    {
    public:
        ~ICharacterCollisionFilter() override = default;

        //! Called synchronously while the character operation holds world locks. Do not call runtime capabilities.

        [[nodiscard]]
        virtual bool ShouldIncludeCharacter(
            [[maybe_unused]] CharacterHandle characterHandle) const
        {
            return true;
        }

        [[nodiscard]]
        virtual bool ShouldIncludeVirtualCharacter(
            [[maybe_unused]] VirtualCharacterHandle characterHandle) const
        {
            return true;
        }
    };

    struct VirtualCharacterStairConfiguration final
    {
        AZ_TYPE_INFO(VirtualCharacterStairConfiguration, VirtualCharacterStairConfigurationTypeId);

        AZ::Vector3 m_stepDownExtra = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stepForward = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stepForwardTest = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stepUp = AZ::Vector3::CreateZero();

        float m_deltaTime = 0.0f;
    };

    struct VirtualCharacterContact final
    {
        AZ_TYPE_INFO(VirtualCharacterContact, VirtualCharacterContactTypeId);

        WorldPosition m_position;

        AZ::Vector3 m_contactNormal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_surfaceNormal = AZ::Vector3::CreateZero();

        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        VirtualCharacterHandle m_characterHandle;
        SubShapeId m_subShapeId;

        float m_distance = 0.0f;
        float m_fraction = 0.0f;
        MotionType m_motionType = MotionType::None;

        bool m_canPushCharacter = true;
        bool m_hadCollision = false;
        bool m_isBackFacing = false;
        bool m_isSensor = false;
        bool m_wasDiscarded = false;
    };

    struct VirtualCharacterContactSettings final
    {
        bool m_canPushCharacter = true;
        bool m_canReceiveImpulses = true;
    };

    struct VirtualCharacterContactSolve final
    {
        WorldPosition m_position;

        AZ::Vector3 m_characterVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_contactNormal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_contactVelocity = AZ::Vector3::CreateZero();

        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        VirtualCharacterHandle m_characterHandle;
        SubShapeId m_subShapeId;
    };

    class IVirtualCharacterContactCallbacks
        : public IRollbackParticipant
    {
    public:
        virtual ~IVirtualCharacterContactCallbacks() = default;

        //! Called synchronously while the character operation holds world locks. Do not call runtime capabilities.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual void OnAdjustBodyVelocity(
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(bodyHandle);
            AZ_UNUSED(linearVelocity);
            AZ_UNUSED(angularVelocity);
        }

        [[nodiscard]]
        virtual bool OnContactValidate(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            return true;
        }

        virtual void OnContactAdded(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact,
            VirtualCharacterContactSettings& settings)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(settings);
        }

        virtual void OnContactPersisted(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact,
            VirtualCharacterContactSettings& settings)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(settings);
        }

        virtual void OnContactRemoved(
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(bodyHandle);
            AZ_UNUSED(subShapeId);
        }

        virtual void OnContactSolve(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContactSolve& contact,
            AZ::Vector3& newCharacterVelocity)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(newCharacterVelocity);
        }

        [[nodiscard]]
        virtual bool OnCharacterContactValidate(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            return true;
        }

        virtual void OnCharacterContactAdded(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact,
            VirtualCharacterContactSettings& settings)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(settings);
        }

        virtual void OnCharacterContactPersisted(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContact& contact,
            VirtualCharacterContactSettings& settings)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(settings);
        }

        virtual void OnCharacterContactRemoved(
            VirtualCharacterHandle characterHandle,
            VirtualCharacterHandle otherCharacterHandle,
            SubShapeId subShapeId)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(otherCharacterHandle);
            AZ_UNUSED(subShapeId);
        }

        virtual void OnCharacterContactSolve(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterContactSolve& contact,
            AZ::Vector3& newCharacterVelocity)
        {
            AZ_UNUSED(characterHandle);
            AZ_UNUSED(contact);
            AZ_UNUSED(newCharacterVelocity);
        }
    };

    struct VirtualCharacterState final
    {
        AZ_TYPE_INFO(VirtualCharacterState, VirtualCharacterStateTypeId);

        WorldTransform m_transform;
        WorldPosition m_groundPosition;

        AZ::Vector3 m_groundNormal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_groundVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();

        BodyHandle m_groundBodyHandle;
        BodyHandle m_innerBodyHandle;
        MaterialHandle m_groundMaterialHandle;
        ShapeHandle m_innerBodyShapeHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_groundSubShapeId;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        GroundState m_groundState = GroundState::None;

        bool m_isSupported = false;
        bool m_maximumHitCountExceeded = false;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::GroundState, "{69C3D746-A660-4286-A17F-7D7FE2B4475C}");
