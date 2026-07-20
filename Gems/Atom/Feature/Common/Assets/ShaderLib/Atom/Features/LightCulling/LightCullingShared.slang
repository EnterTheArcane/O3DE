/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#include <Atom/Features/LightCulling/NVLC.slang>

// These should match the numbers in LightCullingConstants.h

public static const int TILE_DIM_X = 16;
public static const int TILE_DIM_Y = 16;

// Simple point, simple spot, point(sphere), spot (disk), capsule, quad lights, decals
public static const int NUM_LIGHT_TYPES = 7;


public uint GetLightListIndex(uint3 groupID, uint gridWidth, int offset)
{
    return groupID.y * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN * gridWidth + groupID.x * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN + offset;
} 

public uint2 ComputeTileId(float2 svPosition)
{
    uint2 pixelId = uint2(svPosition);
    // Assumes the screen is divided into 16x16 tiles
    uint2 tileId = pixelId >> 4;
    return tileId;
}
