/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


public float3 GetImageSize(Texture2DArray<float> image)
{
    float3 size = float3(0., 0., 0.);
    image.GetDimensions(size.x, size.y, size.z);
    return size;
}

public float3 GetImageSize(RWTexture2DArray<float> image)
{
    float3 size;
    image.GetDimensions(size.x, size.y, size.z);
    return size;
}

public bool IsInsideOfImageSize(float3 coord, float3 imageSize)
{
    return 
       (coord.x >= 0. &&
        coord.y >= 0. &&
        coord.z >= 0. &&
        coord.x < imageSize.x &&
        coord.y < imageSize.y &&
        coord.z < imageSize.z);
}

public bool IsInsideOfImageSize(
    float3 coord,
    float3 inputImageSize,
    float3 outputImageSize)
{
    return IsInsideOfImageSize(coord, inputImageSize) &&
        IsInsideOfImageSize(coord, outputImageSize);
}
