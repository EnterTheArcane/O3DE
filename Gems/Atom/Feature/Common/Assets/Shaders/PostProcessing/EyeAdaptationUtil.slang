/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

 public struct EyeAdaptation
{
    public float m_exposureValue;      // current frame's exposure value in stops (logarithmic space)
    public float m_setValueTime;   // the time when the m_exposureValue was set
};
