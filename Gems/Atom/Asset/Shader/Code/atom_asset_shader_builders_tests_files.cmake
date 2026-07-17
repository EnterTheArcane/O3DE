#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(FILES
    Tests/Common/ShaderBuilderTestFixture.h
    Tests/Common/ShaderBuilderTestFixture.cpp
    Tests/McppBinderTests.cpp
    Tests/ShaderBuilderUtilityTests.cpp
    Tests/ShaderBuildArgumentsTests.cpp
    Tests/ShaderReflectionDataTests.cpp
    Tests/Slang/DualLanguageParityTests.cpp
    Tests/Slang/SlangBackendTests.cpp
    Tests/Slang/SlangCompilerServiceTests.cpp
    Tests/Slang/SlangOptionsModuleGeneratorTests.cpp
    Tests/Slang/SlangModuleResolverTests.cpp
    Tests/Slang/Gates/Gate1_SharedShaderResourceGroupAbiParityTests.cpp
    Tests/Slang/Gates/Gate2_OptionLoweringTests.cpp
    Tests/Slang/Gates/Gate3_DxilSigningTests.cpp
    Tests/Slang/Gates/Gate4_ModuleClosureReloadTests.cpp
    Tests/Slang/Gates/Gate5_DependencyCrossCheckTests.cpp
    Tests/Slang/Gates/Gate6_ThreadSafetyCharacterizationTests.cpp
    Tests/Slang/Gates/Gate7_RootConstantsAndStaticSamplerTests.cpp
)
