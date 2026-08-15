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
    //! Capture and restore run under the world lock and never concurrently with the callback.
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

        [[nodiscard]]
        virtual size_t GetStateByteCount() const
        {
            return 0;
        }

        virtual bool CaptureState(const AZStd::span<AZ::u8> state) const
        {
            return state.empty();
        }

        virtual bool RestoreState(const AZStd::span<const AZ::u8> state) const
        {
            return state.empty();
        }
    };
} // namespace Jolt
