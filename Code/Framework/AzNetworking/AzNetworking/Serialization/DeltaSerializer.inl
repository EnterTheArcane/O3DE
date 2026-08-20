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
            Invalidate();
            return false;
        }
        m_hasCreatedDelta = true;

        // Gather value records from the base object
        m_gatheringRecords = true;
        if (!base.Serialize(*this))
        {
            return false;
        }

        if (!m_delta.SetBufferSize(0))
        {
            Invalidate();
            return false;
        }
        m_deltaByteOffset = 0;

        m_objectCounter = 0;
        // Compile deltas from the new object
        m_gatheringRecords = false;
        if (!current.Serialize(*this)
            || m_objectCounter != m_records.size()
            || m_delta.GetNumDirtyBits() != m_records.size()
            || m_delta.GetBufferSize() != m_deltaByteOffset)
        {
            Invalidate();
            return false;
        }

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
            || !m_dataSerializer.IsValid())
        {
            return false;
        }
        if (m_nextDirtyBit != m_delta.GetNumDirtyBits()
            || m_dataSerializer.GetSize() != m_delta.GetBufferSize())
        {
            Invalidate();
            return false;
        }
        return true;
    }
}
