/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

// This #define magic lets you use the PixelGeometryData class in this file without making it the final PixelGeometryData
// used in your shader. Simply #define PixelGeometryData to your custom definition before including this file
//
#ifndef PixelGeometryData
#define PixelGeometryData       PixelGeometryData_EnhancedPBR
#endif

#include "../StandardPBR/StandardPBR_PixelGeometryData.slang"

// Class inheritance doesn't seem to be working
#define DOES_CLASS_INHERITANCE_WORK_IN_ASZL 0
#if DOES_CLASS_INHERITANCE_WORK_IN_ASZL

    public struct PixelGeometryData_EnhancedPBR : PixelGeometryData_StandardPBR 
    {
        public float2 detailUvs[UvSetCount];
    };

#else

    public struct PixelGeometryData_EnhancedPBR 
    {
        public float3 positionWS;
        public real3 vertexNormal;
        public float2 uvs[UvSetCount];
        public float2 detailUvs[UvSetCount];
        public float3 tangents[UvSetCount];
        public float3 bitangents[UvSetCount];
        public bool isFrontFace;
        public bool isDisplacementClipped;
    };

#endif
