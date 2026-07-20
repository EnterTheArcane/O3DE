/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PostProcessing.AcesColorSpaceConversion;

public real3 LinearSrgb_To_AcesCg(real3 color)
{
    return mul(mul(XYZToAP1Mat, mul(D65ToD60Cat, SRGBToXYZMat)), color);
}
