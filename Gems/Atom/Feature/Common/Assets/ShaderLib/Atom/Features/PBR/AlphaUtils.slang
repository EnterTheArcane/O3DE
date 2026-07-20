/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


public enum OpacityMode
{
    Opaque,
    Cutout,
    Blended,
    TintedTransparent,
};

[AtomOption(OpacityMode.Opaque)]
public extern OpacityMode o_opacity_mode();

public void CheckClipping(real alpha, real opacityFactor)
{
#ifdef CS_SAMPLERS
    // In compute shaders the clip() intrinsic is not allowed.
#else
    switch(o_opacity_mode())
    {
        case OpacityMode::Cutout:
        {
            if(alpha)
            {
                clip(alpha - (1.0 - opacityFactor));
            }
            else
            {
                clip(-1);
            }
            break;
        }
        default:
            break;
    }
#endif //ifdef CS_SAMPLERS
}
