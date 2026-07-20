/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifndef PIPELINE_HAS_OBJECT_SRG
// assume we have an object-srg by default
#define PIPELINE_HAS_OBJECT_SRG 1
#endif 

#if PIPELINE_HAS_OBJECT_SRG
public uint GetLightingChannelMask(VsSystemValues SV)
{
    return ObjectSrg.m_lightingChannelMask;
}
#else
public uint GetLightingChannelMask(VsSystemValues SV)
{
    return SV.m_lightingChannelMask;
}
#endif // PIPELINE_HAS_PASS_SRG


