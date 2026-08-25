/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/BodyBus.h>
#include <Jolt/CharacterBus.h>
#include <Jolt/ColliderBus.h>
#include <Jolt/ConstraintBus.h>
#include <Jolt/HairBus.h>
#include <Jolt/PathBus.h>
#include <Jolt/RagdollBus.h>
#include <Jolt/RigidBodyBus.h>
#include <Jolt/SceneBus.h>
#include <Jolt/SkeletonBus.h>
#include <Jolt/SkeletonComponentBus.h>
#include <Jolt/SoftBodyBus.h>
#include <Jolt/StaticRigidBodyBus.h>
#include <Jolt/VehicleBus.h>
#include <Jolt/VirtualCharacterBus.h>
#include <Jolt/WorldBus.h>
#include <Jolt/WorldDiagnosticsBus.h>
#include <Jolt/WorldQueryBus.h>
#include <Jolt/WorldRollbackBus.h>
#include <Jolt/WorldSimulationBus.h>

#include <AzCore/std/typetraits/is_same.h>

namespace Jolt
{
    static_assert(BodyRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(CharacterRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(ColliderRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(ConstraintRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(HairRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(MotorcycleRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(PathRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(RagdollRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(RigidBodyRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(SceneRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(SkeletonRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(SkeletonRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(SkeletonComponentRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(SoftBodyRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(StaticRigidBodyRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(TrackedVehicleRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(VirtualCharacterRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(WheeledVehicleRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);

    static_assert(BodyNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(ConstraintNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(HairNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(PathNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(RagdollNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(SceneNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(SkeletonComponentNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(VehicleNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(VirtualCharacterNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);
    static_assert(WorldNotificationBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::ById);
    static_assert(WorldNotificationBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Multiple);

    static_assert(WorldRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(WorldRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(WorldSimulationRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(WorldSimulationRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(WorldQueryRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(WorldQueryRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(WorldRollbackRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(WorldRollbackRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);
    static_assert(WorldDiagnosticsRequestBus::Traits::AddressPolicy == AZ::EBusAddressPolicy::Single);
    static_assert(WorldDiagnosticsRequestBus::Traits::HandlerPolicy == AZ::EBusHandlerPolicy::Single);

    static_assert(AZStd::is_same_v<BodyRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<CharacterRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<ColliderRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<ConstraintRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<HairRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<MotorcycleRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<PathRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<RagdollRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<RigidBodyRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<SceneRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<SkeletonComponentRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<SoftBodyRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<StaticRigidBodyRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<TrackedVehicleRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<VirtualCharacterRequestBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<WheeledVehicleRequestBus::BusIdType, AZ::EntityId>);

    static_assert(AZStd::is_same_v<BodyNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<ConstraintNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<HairNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<PathNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<RagdollNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<SceneNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<SkeletonComponentNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<VehicleNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<VirtualCharacterNotificationBus::BusIdType, AZ::EntityId>);
    static_assert(AZStd::is_same_v<WorldNotificationBus::BusIdType, WorldHandle>);
} // namespace Jolt
