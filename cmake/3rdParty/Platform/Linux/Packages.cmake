#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

o3de_declare_package(RapidJSON
    PACKAGE RapidJSON-1.1.0-rev1-multiplatform
    HASH 2f5e26ecf86c3b7a262753e7da69ac59928e78e9534361f3d00c1ad5879e4023
)

o3de_declare_package(RapidXML
    PACKAGE RapidXML-1.13-rev1-multiplatform
    HASH 4b7b5651e47cfd019b6b295cc17bb147b65e53073eaab4a0c0d20a37ab74a246
)

o3de_declare_package(pybind11
    PACKAGE pybind11-2.10.0-rev1-multiplatform
    HASH 6690acc531d4b8cd453c19b448e2fb8066b2362cbdd2af1ad5df6e0019e6c6c4
)

o3de_declare_package(glad
    PACKAGE glad-2.0.0-beta-rev2-multiplatform
    HASH ff97ee9664e97d0854b52a3734c2289329d9f2b4cd69478df6d0ca1f1c9392ee
)

o3de_declare_package(xxhash
    PACKAGE xxhash-0.7.4-rev1-multiplatform
    HASH e81f3e6c4065975833996dd1fcffe46c3cf0f9e3a4207ec5f4a1b564ba75861e
)

if(PAL_ARCHITECTURE_ARM)
    o3de_declare_package(cityhash
        PACKAGE cityhash-1.1-rev1-linux-aarch64
        HASH c4fafa13add81c6ca03338462af78eabbdea917de68c599f11c4a36b0982cec2
    )

    o3de_declare_package(expat
        PACKAGE expat-2.4.2-rev2-linux-aarch64
        HASH 934a535c1492d11906789d7ddf105b1a530cf8d8fb126063ffde16c5caeb0179
    )

    o3de_declare_package(assimp
        PACKAGE assimp-5.4.3-rev3-linux-aarch64
        HASH e8e434eddd6ef418c38ce69c243b751c800d02e7691fe779144f5d5cb8716732
    )

    o3de_declare_package(AWSNativeSDK
        PACKAGE AWSNativeSDK-1.11.288-rev1-linux-aarch64
        HASH 55791343d3aaa07a4242190cc9d6f1f2448c55125839255bdec25efdbab46efa
    )

    o3de_declare_package(TIFF
        PACKAGE tiff-4.2.0.15-rev3-linux-aarch64
        HASH 429461014b21a530dcad597c2d91072ae39d937a04b7bbbf5c34491c41767f7f
    )

    o3de_declare_package(Freetype
        PACKAGE freetype-2.11.1-rev1-linux-aarch64
        HASH b4e3069acdcdae2f977108679d0986fb57371b9a7d4a3a496ab16909feabcba6
    )

    o3de_declare_package(Lua
        PACKAGE Lua-5.4.4-rev1-linux-aarch64
        HASH 4d30067fc494ac27acd72b0bf18099c19c0a44ac9bd46b23db66ad780e72374a
    )

    o3de_declare_package(mcpp
        PACKAGE mcpp-2.7.2_az.1-rev1-linux-aarch64
        HASH 817d31b94d1217b6e47bd5357b3a07a79ab6aa93452c65ff56831d0590c5169d
    )

    o3de_declare_package(mikkelsen
        PACKAGE mikkelsen-1.0.0.4-linux-aarch64
        HASH 62f3f316c971239a2b86d7c47a68fee9be744de3a4f9b00533b32f33a4764f8b
    )

    o3de_declare_package(GoogleBenchmark
        PACKAGE googlebenchmark-1.7.0-rev1-linux-aarch64
        HASH 06fbfeaba2aeae20197da631019e52105dc1f69e702151a76c6aba2c27c03acb
    )

    o3de_declare_package(Qt
        PACKAGE qt-5.15.2-rev9-linux-aarch64
        HASH da80840ecd3f7a074edecbb3dedb1ff36c568cfe4943e18d9559e9fca9f151bc
    )

    o3de_declare_package(PNG
        PACKAGE png-1.6.37-rev2-linux-aarch64
        HASH fcf646c1b1b4163000efdb56d7c8f086b6ce0a520da5c8d3ffce4e1329ae798a
    )

    o3de_declare_package(libsamplerate
        PACKAGE libsamplerate-0.2.1-rev2-linux-aarch64
        HASH 751484da1527432cd19263909f69164d67b25644f87ec1d4ec974a343defacea
    )

    o3de_declare_package(OpenImageIO
        PACKAGE openimageio-opencolorio-2.3.17-rev2-linux-aarch64
        HASH 2bc6a43f60c8206b2606a65738e0fcf3b3b17e0db16089404d8389d337c85ad6
        TARGETS OpenImageIO OpenColorIO OpenColorIO::Runtime OpenImageIO::Tools::Binaries OpenImageIO::Tools::PythonPlugins
    )

    o3de_declare_package(OpenEXR
        PACKAGE OpenEXR-3.1.3-rev4-linux-aarch64
        HASH c9a81050f0d550ab03d2f5801e2f67f9f02747c26f4b39647e9919278585ad6a
        TARGETS OpenEXR Imath
    )

    o3de_declare_package(OpenSSL
        PACKAGE OpenSSL-1.1.1t-rev1-linux-aarch64
        HASH f32721bec9c82d1bd7fb244d78d5dc4e2a47e7b808bb36027236ad377e241ea5
    )

    o3de_declare_package(DirectXShaderCompilerDxc
        PACKAGE DirectXShaderCompilerDxc-1.8.2505.1-o3de-rev3-linux-aarch64
        HASH 7cb521963ae5f4fcd2c488c0ddf8cddc2aeebe3dd37e48a528d87cb541b2f511
    )

    o3de_declare_package(SPIRVCross
        PACKAGE SPIRVCross-1.3.275.0-rev1-linux-aarch64
        HASH 8cd6e4b26202d657e221c4513916ba82b17c73caf1533c8dd833bce6c5c88c2b
    )

    o3de_declare_package(azslc
        PACKAGE azslc-1.8.22-rev1-linux-aarch64
        HASH 5f7c59e4991a22439bbe4af8deab079ec056f90bdd3642eb417c72a15613ecc9
    )

    o3de_declare_package(ZLIB
        PACKAGE zlib-1.2.11-rev5-linux-aarch64
        HASH ce9d1ed2883d77ffc69c7982c078595c1f89ca55ec19d89fe7e6beb05f774775
    )

    o3de_declare_package(squish-ccr
        PACKAGE squish-ccr-deb557d-rev1-linux-aarch64
        HASH d3e54df2defff9f9254085acbf7c61dfda56f72ad10d34e1dd3b5d1bd2b8129f
    )

    o3de_declare_package(astc-encoder
        PACKAGE astc-encoder-3.2-rev3-linux-aarch64
        HASH 60ef2a8adc15767dc263860e1e3befc2f3acea26987442a7e80783f1b2158c73
    )

    o3de_declare_package(ISPCTexComp
        PACKAGE ISPCTexComp-36b80aa-rev2-linux-aarch64
        HASH c29aafa32f13839a394424cf674b5cdb323fab22bcca43c38b43adfe13fc415c
    )

    o3de_declare_package(lz4
        PACKAGE lz4-1.9.4-rev2-linux-aarch64
        HASH 725ca4a02bcf961dc68fb525d0509c311536b5a0f0f9885244fab282cdc55d1f
    )

    o3de_declare_package(pyside2
        PACKAGE pyside2-5.15.2.1-py3.10-rev7-linux-aarch64
        HASH 3210d697299d9c943ac4dfddb95513a7781d8505da0f241f445bd15101529e69
    )

    o3de_declare_package(SQLite
        PACKAGE SQLite-3.37.2-rev1-linux-aarch64
        HASH 5cc1fd9294af72514eba60509414e58f1a268996940be31d0ab6919383f05118
    )

    o3de_declare_package(AwsIotDeviceSdkCpp
        PACKAGE AwsIotDeviceSdkCpp-1.15.2-rev1-linux-aarch64
        HASH 0bac80fc09094c4fd89a845af57ebe4ef86ff8d46e92a448c6986f9880f9ee62
    )

    o3de_declare_package(vulkan-validationlayers
        PACKAGE vulkan-validationlayers-1.2.198-rev1-linux-aarch64
        HASH e67a15a95e14397ccdffd70d17f61079e5720fea22b0d21e135497312419a23f
    )
elseif(PAL_ARCHITECTURE_X64)
    o3de_declare_package(cityhash
        PACKAGE cityhash-1.1-multiplatform
        HASH 0ace9e6f0b2438c5837510032d2d4109125845c0efd7d807f4561ec905512dd2
    )

    o3de_declare_package(expat
        PACKAGE expat-2.4.2-rev2-linux
        HASH 755369a919e744b9b3f835d1acc684f02e43987832ad4a1c0b6bbf884e6cd45b
    )

    o3de_declare_package(assimp
        PACKAGE assimp-5.4.3-rev3-linux
        HASH 62ddd306d520b9b9ac6f587927ff82a90fb1e9e81d7609d67984551da376cf98
    )

    o3de_declare_package(AWSNativeSDK
        PACKAGE AWSNativeSDK-1.11.288-rev1-linux
        HASH 20421c93a5d32feae636c6dc46323b10547f2c5e7e62b63db00319765bb45331
    )

    o3de_declare_package(TIFF
        PACKAGE tiff-4.2.0.15-rev3-linux
        HASH 2377f48b2ebc2d1628d9f65186c881544c92891312abe478a20d10b85877409a
    )

    o3de_declare_package(Freetype
        PACKAGE freetype-2.11.1-rev1-linux
        HASH 28bbb850590507eff85154604787881ead6780e6eeee9e71ed09cd1d48d85983
    )

    o3de_declare_package(Lua
        PACKAGE Lua-5.4.4-rev1-linux
        HASH d582362c3ef90e1ef175a874abda2265839ffc2e40778fa293f10b443b4697ac
    )

    o3de_declare_package(mcpp
        PACKAGE mcpp-2.7.2_az.2-rev1-linux
        HASH df7a998d0bc3fedf44b5bdebaf69ddad6033355b71a590e8642445ec77bc6c41
    )

    o3de_declare_package(mikkelsen
        PACKAGE mikkelsen-1.0.0.4-linux
        HASH 5973b1e71a64633588eecdb5b5c06ca0081f7be97230f6ef64365cbda315b9c8
    )

    o3de_declare_package(GoogleBenchmark
        PACKAGE googlebenchmark-1.7.0-rev1-linux
        HASH 230e1881e31490820f0bd2059df4741455b52809ac73367e278e1e821ac89c9b
    )

    o3de_declare_package(Qt
        PACKAGE qt-5.15.2-rev9-linux
        HASH db4bcd2003262f4d8c7d7da832758824fc24e53da5895edef743f67a64a5c734
    )

    o3de_declare_package(PNG
        PACKAGE png-1.6.37-rev2-linux
        HASH 5c82945a1648905a5c4c5cee30dfb53a01618da1bf58d489610636c7ade5adf5
    )

    o3de_declare_package(libsamplerate
        PACKAGE libsamplerate-0.2.1-rev2-linux
        HASH 41643c31bc6b7d037f895f89d8d8d6369e906b92eff42b0fe05ee6a100f06261
    )

    o3de_declare_package(OpenImageIO
        PACKAGE openimageio-opencolorio-2.3.17-rev2-linux
        HASH c8a9f1d9d6c9f8c3defdbc3761ba391d175b1cb62a70473183af1eaeaef70c36
        TARGETS OpenImageIO OpenColorIO OpenColorIO::Runtime OpenImageIO::Tools::Binaries OpenImageIO::Tools::PythonPlugins
    )

    o3de_declare_package(OpenEXR
        PACKAGE OpenEXR-3.1.3-rev4-linux
        HASH fcbac68cfb4e3b694580bc3741443e111aced5f08fde21a92e0c768e8803c7af
        TARGETS OpenEXR Imath
    )

    o3de_declare_package(OpenSSL
        PACKAGE OpenSSL-1.1.1t-rev1-linux
        HASH 63aea898b7afe8faccd0c7261e62d2f8b7b870f678a4520d5be81e5815542b39
    )

    o3de_declare_package(DirectXShaderCompilerDxc
        PACKAGE DirectXShaderCompilerDxc-1.8.2505.1-o3de-rev3-linux
        HASH 396929d1fa237c49973244f8406a87d84430f653cb1a95b929a8b884f4967a75
    )

    o3de_declare_package(SPIRVCross
        PACKAGE SPIRVCross-1.3.275.0-rev1-linux
        HASH 9df035eabcb33c95a940afb0dbdd0781465d4e2d8ba4d5ca874f9ee3fb2295fc
    )

    o3de_declare_package(azslc
        PACKAGE azslc-1.8.22-rev1-linux
        HASH 0b7bbda949f0991adccbca3fb91e5373180d43c54ed7740ad6d54c1c72dce661
    )

    o3de_declare_package(ZLIB
        PACKAGE zlib-1.2.11-rev5-linux
        HASH 9be5ea85722fc27a8645a9c8a812669d107c68e6baa2ca0740872eaeb6a8b0fc
    )

    o3de_declare_package(squish-ccr
        PACKAGE squish-ccr-deb557d-rev1-linux
        HASH 85fecafbddc6a41a27c5f59ed4a5dfb123a94cb4666782cf26e63c0a4724c530
    )

    o3de_declare_package(astc-encoder
        PACKAGE astc-encoder-3.2-rev2-linux
        HASH 71549d1ca9e4d48391b92a89ea23656d3393810e6777879f6f8a9def2db1610c
    )

    o3de_declare_package(ISPCTexComp
        PACKAGE ISPCTexComp-36b80aa-rev1-linux
        HASH 065fd12abe4247dde247330313763cf816c3375c221da030bdec35024947f259
    )

    o3de_declare_package(lz4
        PACKAGE lz4-1.9.4-rev2-linux
        HASH 5d7e5d087c224dd26edb19deaa73673eefa2dc73f40d0709739e60f2ad35060b
    )

    o3de_declare_package(pyside2
        PACKAGE pyside2-5.15.2.1-py3.10-rev7-linux
        HASH bae4598cb5579d835e90e8435181bb3c5222449ce9c2665143a618dac6122be7
    )

    o3de_declare_package(SQLite
        PACKAGE SQLite-3.37.2-rev1-linux
        HASH bee80d6c6db3e312c1f4f089c90894436ea9c9b74d67256d8c1fb00d4d81fe46
    )

    o3de_declare_package(AwsIotDeviceSdkCpp
        PACKAGE AwsIotDeviceSdkCpp-1.15.2-rev1-linux
        HASH 83fc1711404d3e5b2faabb1134e97cc92b748d8b87ff4ea99599d8c750b8eff0
    )

    o3de_declare_package(vulkan-validationlayers
        PACKAGE vulkan-validationlayers-1.2.198-rev1-linux
        HASH 9195c7959695bcbcd1bc1dc5c425c14639a759733b3abe2ffa87eb3915b12c71
    )
endif()
