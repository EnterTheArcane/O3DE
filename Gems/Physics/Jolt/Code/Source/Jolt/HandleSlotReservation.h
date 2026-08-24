/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/HandleEncoding.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/limits.h>

namespace Jolt::Internal
{
    template<typename SlotType>
    void ResetHandleSlot(SlotType& slot)
    {
        AZStd::destroy_at(&slot);
        AZStd::construct_at(&slot);
    }

    enum class HandleSlotReservationStatus : AZ::u8
    {
        None = 0,
        Success,
        GenerationExhausted,
        IndexCapacityExhausted,
    };

    struct HandleSlotReservation final
    {
        constexpr explicit operator bool() const noexcept
        {
            return m_status == HandleSlotReservationStatus::Success;
        }

        AZ::u32 m_index = 0;
        HandleSlotReservationStatus m_status = HandleSlotReservationStatus::None;
        bool m_appended = false;
    };

    template<typename HandleType, typename SlotContainer, typename FreeSlotContainer, typename AcquireGeneration>
    [[nodiscard]]
    HandleSlotReservation ReserveHandleSlot(
        SlotContainer& slots,
        FreeSlotContainer& freeSlots,
        const AZ::u32 maximumIndex,
        AcquireGeneration acquireGeneration)
    {
        const bool appended = freeSlots.empty();
        if (appended && slots.size() > maximumIndex)
        {
            return {.m_status = HandleSlotReservationStatus::IndexCapacityExhausted};
        }

        const AZ::u32 generation = acquireGeneration();
        if (generation == 0)
        {
            return {.m_status = HandleSlotReservationStatus::GenerationExhausted};
        }

        AZ::u32 slotIndex = 0;
        if (appended)
        {
            slotIndex = static_cast<AZ::u32>(slots.size());
            slots.emplace_back();
        }
        else
        {
            slotIndex = freeSlots.back();
            freeSlots.pop_back();
        }

        slots[slotIndex].m_generation = generation;
        return {
            .m_index = slotIndex,
            .m_status = HandleSlotReservationStatus::Success,
            .m_appended = appended,
        };
    }

    template<typename HandleType, typename SlotContainer, typename FreeSlotContainer>
    [[nodiscard]]
    HandleSlotReservation ReserveHandleSlot(
        SlotContainer& slots,
        FreeSlotContainer& freeSlots,
        const AZ::u32 maximumIndex)
    {
        return ReserveHandleSlot<HandleType>(
            slots,
            freeSlots,
            maximumIndex,
            []
            {
                return AcquireHandleGeneration<HandleType>();
            });
    }

    template<typename HandleType, typename SlotContainer, typename FreeSlotContainer>
    [[nodiscard]]
    HandleSlotReservation ReserveHandleSlot(
        SlotContainer& slots,
        FreeSlotContainer& freeSlots,
        const AZ::u32 maximumIndex,
        AtomicGenerationSources& generationSources)
    {
        return ReserveHandleSlot<HandleType>(
            slots,
            freeSlots,
            maximumIndex,
            [&generationSources]
            {
                return AcquireHandleGeneration<HandleType>(generationSources);
            });
    }

    template<typename SlotContainer, typename FreeSlotContainer>
    void RollbackHandleSlot(
        SlotContainer& slots,
        FreeSlotContainer& freeSlots,
        const HandleSlotReservation& reservation)
    {
        AZ_Assert(reservation, "Only a successful handle slot reservation can be rolled back.");
        if (!reservation || reservation.m_index >= slots.size())
        {
            return;
        }
        AZ_Assert(slots[reservation.m_index].m_generation != 0, "A handle slot reservation can only be rolled back once.");
        if (slots[reservation.m_index].m_generation == 0)
        {
            return;
        }

        if (reservation.m_appended)
        {
            AZ_Assert(
                reservation.m_index + 1 == slots.size(), "Appended handle slots must be rolled back in reverse reservation order.");
            if (reservation.m_index + 1 != slots.size())
            {
                return;
            }

            slots.pop_back();
            return;
        }

        ResetHandleSlot(slots[reservation.m_index]);
        freeSlots.push_back(reservation.m_index);
    }

    template<typename SlotContainer, typename FreeSlotContainer>
    void ReleaseHandleSlot(
        SlotContainer& slots,
        FreeSlotContainer& freeSlots,
        const AZ::u32 slotIndex)
    {
        AZ_Assert(slotIndex < slots.size(), "A handle slot release index must be in range.");
        if (slotIndex >= slots.size())
        {
            return;
        }
        AZ_Assert(slots[slotIndex].m_generation != 0, "A live handle slot can only be released once.");
        if (slots[slotIndex].m_generation == 0)
        {
            return;
        }

        ResetHandleSlot(slots[slotIndex]);
        freeSlots.push_back(slotIndex);
    }
} // namespace Jolt::Internal
