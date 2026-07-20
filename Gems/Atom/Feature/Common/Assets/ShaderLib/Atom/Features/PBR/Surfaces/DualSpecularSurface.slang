/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PBR.Surfaces.AnisotropicSurfaceData;
#include <Atom/Features/PBR/Surfaces/BasePbrSurfaceData.slang>
__exported import Atom.Features.PBR.Surfaces.ClearCoatSurfaceData;
__exported import Atom.Features.PBR.Surfaces.TransmissionSurfaceData;

public struct Surface
{
    public BasePbrSurfaceData pbr;
    //AnisotropicSurfaceData anisotropy;
    public TransmissionSurfaceData transmission;    
};

