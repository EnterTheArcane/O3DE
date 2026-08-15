#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(JOLT_GPU_HAIR_VULKAN_NATIVE_FILES
    Jolt/Compute/VK/BufferVK.h
    Jolt/Compute/VK/ComputeBufferVK.cpp
    Jolt/Compute/VK/ComputeBufferVK.h
    Jolt/Compute/VK/ComputeQueueVK.cpp
    Jolt/Compute/VK/ComputeQueueVK.h
    Jolt/Compute/VK/ComputeShaderVK.cpp
    Jolt/Compute/VK/ComputeShaderVK.h
    Jolt/Compute/VK/ComputeSystemVK.cpp
    Jolt/Compute/VK/ComputeSystemVK.h
    Jolt/Compute/VK/ComputeSystemVKImpl.cpp
    Jolt/Compute/VK/ComputeSystemVKImpl.h
    Jolt/Compute/VK/ComputeSystemVKWithAllocator.cpp
    Jolt/Compute/VK/ComputeSystemVKWithAllocator.h
    Jolt/Compute/VK/IncludeVK.h
)

set(FILES)
foreach(jolt_gpu_hair_native_file IN LISTS JOLT_GPU_HAIR_VULKAN_NATIVE_FILES)
    list(APPEND FILES "${jolt_source_dir}/${jolt_gpu_hair_native_file}")
endforeach()
