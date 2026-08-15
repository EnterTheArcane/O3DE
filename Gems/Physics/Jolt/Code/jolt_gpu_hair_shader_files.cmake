#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(JOLT_GPU_HAIR_SHADER_NAMES
    HairApplyDeltaTransform
    HairApplyGlobalPose
    HairCalculateCollisionPlanes
    HairCalculateRenderPositions
    HairGridAccumulate
    HairGridClear
    HairGridNormalize
    HairIntegrate
    HairSkinRoots
    HairSkinVertices
    HairTeleport
    HairUpdateRoots
    HairUpdateStrands
    HairUpdateVelocity
    HairUpdateVelocityIntegrate
)

set(JOLT_GPU_HAIR_SHADER_HEADERS
    Jolt/Shaders/HairApplyDeltaTransformBindings.h
    Jolt/Shaders/HairApplyGlobalPose.h
    Jolt/Shaders/HairApplyGlobalPoseBindings.h
    Jolt/Shaders/HairCalculateCollisionPlanesBindings.h
    Jolt/Shaders/HairCalculateRenderPositions.h
    Jolt/Shaders/HairCalculateRenderPositionsBindings.h
    Jolt/Shaders/HairCommon.h
    Jolt/Shaders/HairGridAccumulateBindings.h
    Jolt/Shaders/HairGridClearBindings.h
    Jolt/Shaders/HairGridNormalizeBindings.h
    Jolt/Shaders/HairIntegrate.h
    Jolt/Shaders/HairIntegrateBindings.h
    Jolt/Shaders/HairSkinRootsBindings.h
    Jolt/Shaders/HairSkinVerticesBindings.h
    Jolt/Shaders/HairStructs.h
    Jolt/Shaders/HairTeleportBindings.h
    Jolt/Shaders/HairUpdateRootsBindings.h
    Jolt/Shaders/HairUpdateStrandsBindings.h
    Jolt/Shaders/HairUpdateVelocity.h
    Jolt/Shaders/HairUpdateVelocityBindings.h
    Jolt/Shaders/HairUpdateVelocityIntegrateBindings.h
    Jolt/Shaders/ShaderCore.h
    Jolt/Shaders/ShaderMat44.h
    Jolt/Shaders/ShaderMath.h
    Jolt/Shaders/ShaderPlane.h
    Jolt/Shaders/ShaderQuat.h
    Jolt/Shaders/ShaderVec3.h
)

set(JOLT_GPU_HAIR_SHADER_FILES ${JOLT_GPU_HAIR_SHADER_HEADERS})
foreach(jolt_gpu_hair_shader_name IN LISTS JOLT_GPU_HAIR_SHADER_NAMES)
    list(APPEND JOLT_GPU_HAIR_SHADER_FILES "Jolt/Shaders/${jolt_gpu_hair_shader_name}.hlsl")
endforeach()
