/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

namespace AzNetworking
{
    template <typename TYPE>
    bool DeltaSerializerCreate::CreateDelta(TYPE& base, TYPE& current)
    {
        if (m_hasCreatedDelta)
        {
            return FailCreate();
        }
        m_hasCreatedDelta = true;
        m_delta.Reset();
        ClearRecordState();

        // Gather value records from the base object
        m_gatheringRecords = true;
        if (!base.Serialize(*this) || !IsValid())
        {
            return FailCreate();
        }

        m_objectCounter = 0;
        // Compile deltas from the new object
        m_gatheringRecords = false;
        if (!current.Serialize(*this)
            || !IsValid()
            || !m_dataSerializer.IsValid()
            || m_objectCounter != m_recordCount
            || m_delta.GetNumDirtyBits() != m_recordCount)
        {
            return FailCreate();
        }

        // Update the delta buffer size based on how much data was serialized
        m_delta.SetBufferSize(m_dataSerializer.GetSize());
        return true;
    }

    template <typename TYPE>
    bool DeltaSerializerApply::ApplyDelta(TYPE& output)
    {
        if (m_hasAppliedDelta)
        {
            Invalidate();
            return false;
        }
        m_hasAppliedDelta = true;

        if (!output.Serialize(*this)
            || !IsValid()
            || !m_dataSerializer.IsValid()
            || m_nextDirtyBit != m_delta.GetNumDirtyBits()
            || m_dataSerializer.GetReadSize() != m_delta.GetBufferSize())
        {
            Invalidate();
            return false;
        }
        return true;
    }
}
