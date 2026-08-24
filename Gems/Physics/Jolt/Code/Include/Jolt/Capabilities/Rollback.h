/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/Operation.h>
#include <Jolt/Rollback.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Rollback
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Rollback* Get();

        [[nodiscard]]
        StateSnapshotHandle CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        Operation<StateSnapshotHandle> CaptureBodyStateAsync(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool CaptureBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateRestoreResult RestoreBodyState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        Operation<StateRestoreResult> RestoreBodyStateAsync(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle);

        [[nodiscard]]
        Operation<StateSnapshotHandle> CaptureWorldStateAsync(WorldHandle worldHandle);

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureWorldState(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles,
            AZStd::span<const AZ::u32> partitionBodyCounts,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        //! Exports one snapshot or a complete multipart batch for a matching native build.
        //! Import additionally requires matching logical topology and effective simulation configuration.
        bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles,
            StateSnapshotArchive& archive);

        //! Imports into matching topology without publishing partial results on failure.
        bool ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const;

        [[nodiscard]]
        StateRestoreResult RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        Operation<StateRestoreResult> RestoreWorldStateAsync(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle);

        //! Prevalidates a batch returned by CaptureWorldStateParts before beginning restore.
        [[nodiscard]]
        StateRestoreResult RestoreWorldStateParts(
            WorldHandle worldHandle,
            AZStd::span<const StateSnapshotHandle> snapshotHandles);

        bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result);

        [[nodiscard]]
        bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const;

    private:
        friend class Runtime;

        Rollback() = default;
        ~Rollback() = default;
    };
} // namespace Jolt
