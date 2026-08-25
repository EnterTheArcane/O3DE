/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/span.h>

#include <cstddef>

namespace Jolt
{
    //! Owns deterministic behavior state used by a simulation callback.
    //! Mutable state capture and restore run under the owning world lock and never concurrently with that world's callback.
    class IRollbackParticipant
    {
    public:
        virtual ~IRollbackParticipant() = default;

        //! Identifies the participant implementation and state family.
        [[nodiscard]]
        virtual AZ::TypeId GetStateTypeId() const = 0;

        //! Identifies compatible revisions of a state schema.
        [[nodiscard]]
        virtual AZ::u32 GetStateVersion() const
        {
            return 0;
        }

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        //! A zero byte count declares that this participant has no mutable behavior state and may be
        //! invoked concurrently by independent worlds. Such participants must be thread-safe and deterministic.
        //! A nonzero byte count binds the registration to one world at a time. The count must remain
        //! constant from registration through unregistration.
        [[nodiscard]]
        virtual size_t GetStateByteCount() const
        {
            return 0;
        }

        virtual bool CaptureState(const AZStd::span<AZ::u8> state) const
        {
            return state.empty();
        }

        //! Validates and prepares a restore without changing observable state.
        virtual bool PrepareRestoreState(const AZStd::span<const AZ::u8> state) const
        {
            return state.empty();
        }

        //! Commits state accepted by PrepareRestoreState. This operation must not fail.
        virtual void CommitRestoreState(
            [[maybe_unused]] const AZStd::span<const AZ::u8> state) const
        {
        }
    };
} // namespace Jolt
