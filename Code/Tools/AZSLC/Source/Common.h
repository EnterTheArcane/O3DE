/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <cstdint>

namespace AZ::ShaderCompiler
{
    namespace Packing
    {
        enum class MatrixMajor : uint32_t
        {
            // If unspecified, it uses the default for the code emission (specified by -Zpc or -Zpr at build time)
            Default,
            // Use column_major explicitly
            ColumnMajor,
            // Use row_major explicitly
            RowMajor,
        };
    }

    struct SamplerStateDesc
    {
        enum class FilterMode : uint32_t
        {
            Point = 0, Linear,
        };

        enum class ReductionType : uint32_t
        {
            /// Performs filtering on samples.
            Filter = 0,
            /// Performs comparison of samples using the supplied comparison function.
            Comparison,
            /// Returns minimum of samples.
            Minimum,
            /// Returns maximum of samples.
            Maximum,
        };

        enum class AddressMode : uint32_t
        {
            Wrap = 0,
            Mirror,
            Clamp,
            Border,
            MirrorOnce,
        };

        enum class ComparisonFunc : uint32_t
        {
            Never = 0,
            Less,
            Equal,
            LessEqual,
            Greater,
            NotEqual,
            GreaterEqual,
            Always,
        };

        enum class BorderColor : uint32_t
        {
            OpaqueBlack = 0, TransparentBlack, OpaqueWhite,
        };

        uint32_t m_anisotropyMax = 0; /// Range [1 .. 16]
        bool m_anisotropyEnable = false;
        FilterMode m_filterMin = FilterMode::Point;
        FilterMode m_filterMag = FilterMode::Point;
        FilterMode m_filterMip = FilterMode::Point;
        ComparisonFunc m_comparisonFunc = ComparisonFunc::Always;
        ReductionType m_reductionType = ReductionType::Filter;
        AddressMode m_addressU = AddressMode::Wrap;
        AddressMode m_addressV = AddressMode::Wrap;
        AddressMode m_addressW = AddressMode::Wrap;
        float m_mipLodMin = 0.0f;
        float m_mipLodMax = 15.0f; //On RHI side limit is set by Limits::Image::MipCountMax
        float m_mipLodBias = 0.0f;
        BorderColor m_borderColor = BorderColor::TransparentBlack;

        // Metadata for emission
        bool m_isComparison = false;
        bool m_isDynamic = false;
    };
}
