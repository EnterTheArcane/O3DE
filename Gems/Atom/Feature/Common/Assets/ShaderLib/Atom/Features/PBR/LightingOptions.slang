/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// --- Note: About this file ---
// This file uses #defines to completely strip out certain lighting features from shaders
// This enables us to avoid code duplication and write an uber-esque shader code and then
// be able to adjust the code for different use cases (for example basic materials can strip
// away transimission related code used for foliage). These are different from shader options
// in that shader options allow more user flexibility to adjust materials at runtime, whereas
// these #define options are for customizing and optimizing material types (like BasePBR)

// --- Light Defines ---

#ifndef ENABLE_SHADOWS
#define ENABLE_SHADOWS              1
#endif

#ifndef ENABLE_LIGHT_CULLING
#define ENABLE_LIGHT_CULLING              1
#endif

#ifndef ENABLE_DECALS
#define ENABLE_DECALS              1
#endif

#ifndef ENABLE_AREA_LIGHT_VALIDATION
#define ENABLE_AREA_LIGHT_VALIDATION    1
#endif

#ifndef ENABLE_AREA_LIGHTS
#define ENABLE_AREA_LIGHTS              1
#endif

#ifndef ENABLE_SPHERE_LIGHTS
#define ENABLE_SPHERE_LIGHTS            ENABLE_AREA_LIGHTS
#endif

#ifndef ENABLE_DISK_LIGHTS
#define ENABLE_DISK_LIGHTS              ENABLE_AREA_LIGHTS
#endif

#ifndef ENABLE_CAPSULE_LIGHTS
#define ENABLE_CAPSULE_LIGHTS           ENABLE_AREA_LIGHTS
#endif

#ifndef ENABLE_QUAD_LIGHTS
#define ENABLE_QUAD_LIGHTS              ENABLE_AREA_LIGHTS
#endif

#ifndef ENABLE_POLYGON_LTC_LIGHTS
#define ENABLE_POLYGON_LTC_LIGHTS       ENABLE_AREA_LIGHTS
#endif


// --- Material defines ---

#ifndef ENABLE_CLEAR_COAT
#define ENABLE_CLEAR_COAT               1
#endif

#ifndef ENABLE_TRANSMISSION
#define ENABLE_TRANSMISSION             1
#endif

#ifndef ENABLE_PARALLAX
#define ENABLE_PARALLAX					1
#endif

#ifndef ENABLE_MOBILEBRDF
#define ENABLE_MOBILEBRDF               0
#endif

// --- Shader Options ---

[AtomOption(true)]
public extern bool o_specularF0_enableMultiScatterCompensation();
[AtomOption(true)]
public extern bool o_enableShadows();
[AtomOption(true)]
public extern bool o_enableDirectionalLights();
[AtomOption(true)]
public extern bool o_enablePunctualLights();

[AtomOption(true)]
public extern bool o_enableAreaLights();
[AtomOption(true)]
public extern bool o_enableSphereLights();
[AtomOption(true)]
public extern bool o_enableSphereLightShadows();
[AtomOption(true)]
public extern bool o_enableDiskLights();
[AtomOption(true)]
public extern bool o_enableDiskLightShadows();
[AtomOption(true)]
public extern bool o_enableCapsuleLights();
[AtomOption(true)]
public extern bool o_enableQuadLightLTC();
[AtomOption(true)]
public extern bool o_enableQuadLightApprox();
[AtomOption(true)]
public extern bool o_enablePolygonLights();
[AtomOption(true)]
public extern bool o_enableSimplePointLights();
[AtomOption(true)]
public extern bool o_enableSimpleSpotLights();
[AtomOption(true)]
public extern bool o_enableSimpleSpotLightShadows();
[AtomOption(true)]
public extern bool o_enableDecals();

[AtomOption(true)]
public extern bool o_enableIBL();
[AtomOption(false)]
public extern bool o_enableSubsurfaceScattering();
[AtomOption(false)]
public extern bool o_area_light_validation();

#if ENABLE_CLEAR_COAT
[AtomOption(false)]
public extern bool o_clearCoat_feature_enabled();
#endif

#if ENABLE_TRANSMISSION
public enum TransmissionMode
{
    None,
    ThickObject,
    ThinObject,
};

[AtomOption(TransmissionMode.None)]
public extern TransmissionMode o_transmission_mode();
#endif
