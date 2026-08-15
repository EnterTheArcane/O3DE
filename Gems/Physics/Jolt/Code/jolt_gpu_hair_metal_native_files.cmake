#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(JOLT_GPU_HAIR_METAL_NATIVE_FILES
    Jolt/Compute/MTL/ComputeBufferMTL.h
    Jolt/Compute/MTL/ComputeBufferMTL.mm
    Jolt/Compute/MTL/ComputeQueueMTL.h
    Jolt/Compute/MTL/ComputeQueueMTL.mm
    Jolt/Compute/MTL/ComputeShaderMTL.h
    Jolt/Compute/MTL/ComputeShaderMTL.mm
    Jolt/Compute/MTL/ComputeSystemMTL.h
    Jolt/Compute/MTL/ComputeSystemMTL.mm
    Jolt/Compute/MTL/ComputeSystemMTLImpl.h
    Jolt/Compute/MTL/ComputeSystemMTLImpl.mm
)

set(FILES)
foreach(jolt_gpu_hair_native_file IN LISTS JOLT_GPU_HAIR_METAL_NATIVE_FILES)
    list(APPEND FILES "${jolt_source_dir}/${jolt_gpu_hair_native_file}")
endforeach()
