/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PostProcessing.AcesColorSpaceConversion;

public real3 AcesCg_To_LinearSrgb(real3 color)
{
    return mul(mul(XYZToSRGBMat, mul(D60ToD65Cat, AP1ToXYZMat)), color);
}
