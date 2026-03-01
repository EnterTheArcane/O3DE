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

o3de_declare_package(cityhash
    PACKAGE cityhash-1.1-multiplatform
    HASH 0ace9e6f0b2438c5837510032d2d4109125845c0efd7d807f4561ec905512dd2
)

o3de_declare_package(zstd
    PACKAGE zstd-1.35-multiplatform
    HASH 45d466c435f1095898578eedde85acf1fd27190e7ea99aeaa9acfd2f09e12665
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
    o3de_declare_package(expat
        PACKAGE expat-2.7.3-rev1-mac-arm64
        HASH 76a6793f180f6df456394d02d9a23df585af6a10689308e539b50e26c5edf437
    )

    o3de_declare_package(assimp
        PACKAGE assimp-5.4.3-rev3-mac-arm64
        HASH bfda2c319bb4cc26aea8445a9ad33347e0e5ead2a959a5eafb5eed47431f56ef
    )

    o3de_declare_package(DirectXShaderCompilerDxc
        PACKAGE DirectXShaderCompilerDxc-1.8.2505.1-o3de-rev4-mac-arm64
        HASH 75a9c9c9bad393f6571737def7b8d8a09c00e02e415680e8c0d1652459740676
    )

    o3de_declare_package(SPIRVCross
        PACKAGE SPIRVCross-1.3.275.0-rev1-mac-arm64
        HASH 5ad9629f677c42847daf8b097728323685d7018d3ac8af0508d1bd0727a81304
    )

    o3de_declare_package(TIFF
        PACKAGE tiff-4.2.0.15-rev3-mac-arm64
        HASH bffbf8bf099ae5d3d49967536a8fcd7fcf747fd6fa92ba945a0e64eead9636d9
        TARGETS tiff
    )

    o3de_declare_package(Freetype
        PACKAGE freetype-2.11.1-rev1-mac-arm64
        HASH eae257c78c2da47ca02ca17e949c665c28a59215d756c137c87220c85a7f8488
        TARGETS freetype
    )

    o3de_declare_package(AWSNativeSDK
        PACKAGE AWSNativeSDK-1.11.361-rev1-mac-arm64
        HASH 88fb6ac72314b5993e2c24d90bd409016657658711996f416875ea3a0118a521
    )

    o3de_declare_package(Lua
        PACKAGE Lua-5.4.4-rev1-mac-arm64
        HASH b44daae6bfdf092c7935e4aebafded6772853250c6f0a209866a1ac599857d58
    )

    o3de_declare_package(mcpp
        PACKAGE mcpp-2.7.2_az.2-rev1-mac-arm64
        HASH 7826e3cdb70940c3efa788ab28ba02133ad494a123ae5c71ff38732ba1dabfef
    )

    o3de_declare_package(mikkelsen
        PACKAGE mikkelsen-1.0.0.4-mac-arm64
        HASH 83af99ca8bee123684ad254263add556f0cf49486c0b3e32e6d303535714e505
    )

    o3de_declare_package(GoogleBenchmark
        PACKAGE googlebenchmark-1.7.0-rev1-mac-arm64
        HASH a1c8793eb1760905290065929b45600a4b4457345fcc129fce253d1a8980bbce
    )

    o3de_declare_package(OpenImageIO
        PACKAGE openimageio-opencolorio-2.3.17-rev3-mac-arm64
        HASH bc322f9e28d519ab5959a638b38ee3b773fefb868802823fad2396ab4f7bcbc8
        TARGETS OpenImageIO OpenColorIO OpenColorIO::Runtime OpenImageIO::Tools::Binaries OpenImageIO::Tools::PythonPlugins
    )

    o3de_declare_package(OpenSSL
        PACKAGE OpenSSL-1.1.1w-rev1-mac-arm64
        HASH 3367bdf98e73cf2413eb495853972aa4ccd29c2ef58392fa7b7fa99001b1e2e0
    )

    o3de_declare_package(OpenEXR
        PACKAGE OpenEXR-3.4.4-rev1-mac-arm64
        HASH 4a093f5ca03836631dc66166b8f493925d0445467219efcbca3a5a0ee2ccbf4b
    )

    o3de_declare_package(Qt
        PACKAGE qt-5.15.2-rev8-mac-arm64
        HASH d0f97579ea2822c73f0b316a26c68ceb5332763e691d7e78d6b02fe3104b1d31
    )

    o3de_declare_package(PNG
        PACKAGE png-1.6.37-rev2-mac-arm64
        HASH 515252226a6958c459f53d8598d80ec4f90df33d2f1637104fd1a636f4962f07
    )

    o3de_declare_package(libsamplerate
        PACKAGE libsamplerate-0.2.1-rev2-mac-arm64
        HASH 1a4954bd2e24b04da6c121e36fde1884e1e3f9492f580cf347637d0bea4b65e0
    )

    o3de_declare_package(ZLIB
        PACKAGE zlib-1.3.1-rev2-mac-arm64
        HASH 52e62890329d3e003226fca88df30701cdd862a5f137eb5f75dff504377c13b3
        TARGETS zlib
    )

    o3de_declare_package(squish-ccr
        PACKAGE squish-ccr-deb557d-rev1-mac-arm64
        HASH 51346fba3ba2380cfe82d6af9e2e9284ccdfd6093349e9de88078c52c28c6327
        TARGETS squish
    )

    o3de_declare_package(astc-encoder
        PACKAGE astc-encoder-3.2-rev5-mac-arm64
        HASH be8c272683e1cd50e2ecdd16abf3188bc7543654acbc43c01533921486db828e
        TARGETS astc
    )

    o3de_declare_package(ISPCTexComp
        PACKAGE ISPCTexComp-36b80aa-rev1-mac-arm64
        HASH 0992e6662f193379cdc9ba8ab9b7a24404564df9bcc5f39d9527b7258ae4172c
    )

    o3de_declare_package(lz4
        PACKAGE lz4-1.9.4-rev2-mac-arm64
        HASH d85fe35ce176967199fe6e11fce684e6c05f0c5533892a3785a458872a1d5229
    )

    o3de_declare_package(azslc
        PACKAGE azslc-1.8.22-rev1-mac-arm64
        HASH ff7c0bb755ae1fc7f2f5e2b02bb4ddfdf85deea5b22ba2f8baae4ff7b0fc8374
    )

    o3de_declare_package(SQLite
        PACKAGE SQLite-3.37.2-rev2-mac-arm64
        HASH 6fa05df3f97fed97bdef293ac85b250ffe443a43e776ad54312b7b356d41fccb
    )

    o3de_declare_package(AwsIotDeviceSdkCpp
        PACKAGE AwsIotDeviceSdkCpp-1.15.2-rev2-mac-arm64
        HASH 4854edb7b88fa6437b4e69e87d0ee111a25313ac2a2db5bb2f8b674ba0974f95
    )
elseif(PAL_ARCHITECTURE_X86_64)
    o3de_declare_package(expat
        PACKAGE expat-2.4.2-rev2-mac
        HASH 70f195977a17b08a4dc8687400fd7f2589e3b414d4961b562129166965b6f658
    )

    o3de_declare_package(assimp
        PACKAGE assimp-5.4.3-rev3-mac
        HASH bfda2c319bb4cc26aea8445a9ad33347e0e5ead2a959a5eafb5eed47431f56ef
    )

    o3de_declare_package(DirectXShaderCompilerDxc
        PACKAGE DirectXShaderCompilerDxc-1.8.2505.1-o3de-rev4-mac
        HASH d2f8671430fffa59d6fd7383fe9422b197ff52d51b8235469588281b821b0c31
    )

    o3de_declare_package(SPIRVCross
        PACKAGE SPIRVCross-1.3.275.0-rev1-mac
        HASH 5ad9629f677c42847daf8b097728323685d7018d3ac8af0508d1bd0727a81304
    )

    o3de_declare_package(TIFF
        PACKAGE tiff-4.2.0.15-rev3-mac
        HASH c2615ccdadcc0e1d6c5ed61e5965c4d3a82193d206591b79b805c3b3ff35a4bf
    )

    o3de_declare_package(Freetype
        PACKAGE freetype-2.11.1-rev1-mac
        HASH b66107d3499f2e9c072bd88db26e0e5c1b8013128699393c6a8495afca3d2548
    )

    o3de_declare_package(AWSNativeSDK
        PACKAGE AWSNativeSDK-1.11.288-rev1-mac
        HASH 4cf12956d005d4025bea48ddb3856874f6234bc3118f0471fcaae5f28a92e42a
    )

    o3de_declare_package(Lua
        PACKAGE Lua-5.4.4-rev1-mac
        HASH b44daae6bfdf092c7935e4aebafded6772853250c6f0a209866a1ac599857d58
    )

    o3de_declare_package(mcpp
        PACKAGE mcpp-2.7.2_az.2-rev1-mac
        HASH be9558905c9c49179ef3d7d84f0a5472415acdf7fe2d76eb060d9431723ddf2e
    )

    o3de_declare_package(mikkelsen
        PACKAGE mikkelsen-1.0.0.4-mac
        HASH 83af99ca8bee123684ad254263add556f0cf49486c0b3e32e6d303535714e505
    )

    o3de_declare_package(GoogleBenchmark
        PACKAGE googlebenchmark-1.7.0-rev1-mac
        HASH a1c8793eb1760905290065929b45600a4b4457345fcc129fce253d1a8980bbce
    )

    o3de_declare_package(OpenImageIO
        PACKAGE openimageio-opencolorio-2.3.17-rev3-mac
        HASH bc322f9e28d519ab5959a638b38ee3b773fefb868802823fad2396ab4f7bcbc8
        TARGETS OpenImageIO OpenColorIO OpenColorIO::Runtime OpenImageIO::Tools::Binaries OpenImageIO::Tools::PythonPlugins
    )

    o3de_declare_package(OpenSSL
        PACKAGE OpenSSL-1.1.1o-rev1-mac
        HASH 73a4bd7856b53edf5ab9d2ff1d31ebb02301be818680a59206ce8ec5940f3468
    )

    o3de_declare_package(OpenEXR
        PACKAGE OpenEXR-3.1.3-rev4-mac
        HASH 927b8ca6cc5815fa8ee4efe6ea2845487cba2540f7958d537692e7c9481a68fc
        TARGETS OpenEXR Imath
    )

    o3de_declare_package(Qt
        PACKAGE qt-5.15.2-rev8-mac
        HASH d0f97579ea2822c73f0b316a26c68ceb5332763e691d7e78d6b02fe3104b1d31
    )

    o3de_declare_package(PNG
        PACKAGE png-1.6.37-rev2-mac
        HASH 515252226a6958c459f53d8598d80ec4f90df33d2f1637104fd1a636f4962f07
    )

    o3de_declare_package(libsamplerate
        PACKAGE libsamplerate-0.2.1-rev2-mac
        HASH b912af40c0ac197af9c43d85004395ba92a6a859a24b7eacd920fed5854a97fe
    )

    o3de_declare_package(ZLIB
        PACKAGE zlib-1.2.11-rev5-mac
        HASH b6fea9c79b8bf106d4703b67fecaa133f832ad28696c2ceef45fb5f20013c096
    )

    o3de_declare_package(squish-ccr
        PACKAGE squish-ccr-deb557d-rev1-mac
        HASH 155bfbfa17c19a9cd2ef025de14c5db598f4290045d5b0d83ab58cb345089a77
    )

    o3de_declare_package(astc-encoder
        PACKAGE astc-encoder-3.2-rev5-mac
        HASH bdb1146cc6bbacc07901564fe884529d7cacc9bb44895597327341d3b9833ab0
    )

    o3de_declare_package(ISPCTexComp
        PACKAGE ISPCTexComp-36b80aa-rev1-mac
        HASH 8a4e93277b8face6ea2fd57c6d017bdb55643ed3d6387110bc5f6b3b884dd169
    )

    o3de_declare_package(lz4
        PACKAGE lz4-1.9.4-rev1-mac
        HASH d52e34e5e2f93acb914fd5f8e5247b67f2e7f15fae0586b5813c2721c8345a0d
    )

    o3de_declare_package(azslc
        PACKAGE azslc-1.8.22-rev1-mac
        HASH d34b416b181b4ad5f8bee7c4976fe74dd503318060d2062385351601fa9f967d
    )

    o3de_declare_package(SQLite
        PACKAGE SQLite-3.37.2-rev2-mac
        HASH b7d9abdb68045003e030e1a9a805db1aefa5e8fde6dccfbb4fab3a06249a41fc
    )

    o3de_declare_package(AwsIotDeviceSdkCpp
        PACKAGE AwsIotDeviceSdkCpp-1.15.2-rev2-mac
        HASH 4854edb7b88fa6437b4e69e87d0ee111a25313ac2a2db5bb2f8b674ba0974f95
    )
endif()
