#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(JOLT_GPU_HAIR_DX12_NATIVE_FILES
    Jolt/Compute/DX12/ComputeBufferDX12.cpp
    Jolt/Compute/DX12/ComputeBufferDX12.h
    Jolt/Compute/DX12/ComputeQueueDX12.cpp
    Jolt/Compute/DX12/ComputeQueueDX12.h
    Jolt/Compute/DX12/ComputeShaderDX12.h
    Jolt/Compute/DX12/ComputeSystemDX12.cpp
    Jolt/Compute/DX12/ComputeSystemDX12.h
    Jolt/Compute/DX12/ComputeSystemDX12Impl.cpp
    Jolt/Compute/DX12/ComputeSystemDX12Impl.h
    Jolt/Compute/DX12/IncludeDX12.h
)

set(FILES)
foreach(jolt_gpu_hair_native_file IN LISTS JOLT_GPU_HAIR_DX12_NATIVE_FILES)
    list(APPEND FILES "${jolt_source_dir}/${jolt_gpu_hair_native_file}")
endforeach()
