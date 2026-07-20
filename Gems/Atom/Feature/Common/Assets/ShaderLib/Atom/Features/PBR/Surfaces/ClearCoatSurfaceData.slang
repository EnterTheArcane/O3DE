/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#if ENABLE_CLEAR_COAT

public struct ClearCoatSurfaceData
{
    public real factor;           //!< clear coat strength factor
    public real roughness;        //!< clear coat linear roughness (not base layer one)
    public real3 normal;          //!< normal used for top layer clear coat

    [mutating] public void InitializeToZero()
    {
        factor = 0.0;
        roughness = 0.0;
        normal = real3(0.0, 0.0, 0.0);
    }
};



#endif
