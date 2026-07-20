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
#define PixelGeometryData       PixelGeometryData_Eye
#endif

#include "../BasePBR/BasePBR_PixelGeometryData.slang"

public struct PixelGeometryData_Eye
{
    // BasePBR
    public float3 positionWS;
    public float2 uvs[UvSetCount];
    public float3 tangents[UvSetCount];
    public float3 bitangents[UvSetCount];
    public real3 vertexNormal;
    public bool isFrontFace;

    // Eye
    public float4x4 objectToWorld;
    public float3x3 objectToWorldIT;
    public float3 localPosition;
};
