/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Diagnostics.h>
#include <Jolt/Rollback.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    JOLT_API void ReflectWorldRollback(AZ::ReflectContext* context);

    class IWorldRollbackRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual ~IWorldRollbackRequests() = default;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldStateConfigured(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) = 0;

        [[nodiscard]]
        virtual AZStd::vector<StateSnapshotHandle> CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles,
            const AZStd::vector<AZ::u32>& partitionBodyCounts) = 0;

        virtual bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles,
            StateSnapshotArchive& archive) = 0;

        [[nodiscard]]
        virtual AZStd::vector<StateSnapshotHandle> ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive) = 0;

        virtual bool RecaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        virtual bool RecaptureWorldStateConfigured(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) = 0;

        virtual bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual bool IsStateSnapshotValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const = 0;

        virtual StateRestoreResult RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        //! Prevalidates a batch returned by CaptureWorldStateParts before beginning restore.
        virtual StateRestoreResult RestoreWorldStateParts(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles) = 0;

        virtual bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result) = 0;

        [[nodiscard]]
        virtual bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const = 0;
    };

    using WorldRollbackRequestBus = AZ::EBus<IWorldRollbackRequests>;
} // namespace Jolt
