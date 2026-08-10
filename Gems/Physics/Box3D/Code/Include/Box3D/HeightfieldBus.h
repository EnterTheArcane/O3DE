/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>
#include <Box3D/ShapeConfiguration.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <cstddef>

namespace Box3D
{
    //! Mutable heightfield owned by one component. Returned views expire on the next update or deactivation.
    class HeightfieldRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetBodyHandle() const = 0;

        [[nodiscard]]
        virtual ShapeHandle GetShapeHandle() const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetColumnCount() const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetRowCount() const = 0;

        [[nodiscard]]
        virtual AZStd::span<const float> GetHeights() const = 0;

        [[nodiscard]]
        virtual AZStd::span<const AZ::u8> GetMaterialIndices() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<float> GetHeightsCopy() const
        {
            const AZStd::span<const float> heights = GetHeights();
            return {heights.begin(), heights.end()};
        }

        [[nodiscard]]
        virtual AZStd::vector<AZ::u8> GetMaterialIndicesCopy() const
        {
            const AZStd::span<const AZ::u8> materialIndices = GetMaterialIndices();
            return {materialIndices.begin(), materialIndices.end()};
        }

        virtual bool ReplaceHeightfield(const HeightfieldShapeConfiguration& configuration) = 0;

        virtual bool UpdateHeights(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            AZStd::span<const float> heights) = 0;

        virtual bool UpdateHeightsFromList(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            const AZStd::vector<float>& heights)
        {
            return UpdateHeights(startColumn, startRow, columnCount, rowCount, heights);
        }

        virtual bool UpdateMaterials(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            AZStd::span<const AZ::u8> materialIndices) = 0;

        virtual bool UpdateMaterialsFromList(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            const AZStd::vector<AZ::u8>& materialIndices)
        {
            return UpdateMaterials(startColumn, startRow, columnCount, rowCount, materialIndices);
        }
    };

    using HeightfieldRequestBus = AZ::EBus<HeightfieldRequests>;
} // namespace Box3D
