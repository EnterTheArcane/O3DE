/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


public enum BlendMode
{
    Opaque,
    Cutout,
    Blended,
    Additive,
    Subtractive,
    Modulate,
};

[AtomOption(BlendMode.Opaque)]
public extern BlendMode o_blendMode();

public void CheckClipping(float alpha, float opacityFactor)
{
    switch(o_blendMode())
    {
    case BlendMode::Cutout:
        {
            if(alpha)
            {
                clip(alpha - opacityFactor);
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
}

