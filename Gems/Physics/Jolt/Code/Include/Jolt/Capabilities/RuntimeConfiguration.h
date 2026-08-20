/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SystemConfiguration.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/string/string_view.h>

namespace Jolt
{
    class Runtime;

    enum class Precision : AZ::u8
    {
        None = 0,
        Double,
        Single,
    };

    enum class SimdLevel : AZ::u8
    {
        None = 0,
        Avx,
        Avx2,
        Avx512,
        Neon,
        Rvv,
        Scalar,
        Sse2,
        Sse41,
        Sse42,
        WasmSimd,
    };

    enum class DeterminismCertification : AZ::u8
    {
        None = 0,
        //! Repeatable only with the same executable and deterministic application inputs.
        SameBinary,
        //! Repeatable across supported platforms when source, defines, inputs, and mutation order match.
        CrossPlatform,
    };

    struct Version final
    {
        AZ_TYPE_INFO(Version, VersionTypeId);

        AZ::u32 m_major = 0;
        AZ::u32 m_minor = 0;
        AZ::u32 m_patch = 0;

        friend constexpr bool operator==(const Version&, const Version&) = default;
    };

    struct RuntimeInfo final
    {
        AZ_TYPE_INFO(RuntimeInfo, RuntimeInfoTypeId);

        Version m_version;
        //! Compare this before exchanging snapshots or deterministic state.
        AZ::u64 m_buildFingerprint = 0;

        AZStd::string_view m_configuration;

        DeterminismCertification m_hairDeterminism = DeterminismCertification::None;
        DeterminismCertification m_physicsDeterminism = DeterminismCertification::None;
        Precision m_precision = Precision::None;
        SimdLevel m_simdLevel = SimdLevel::None;

        bool m_broadPhaseStatistics = false;
        bool m_detailedProfiling = false;
        bool m_narrowPhaseStatistics = false;
        bool m_simulationStatistics = false;
    };

    class JOLT_API RuntimeConfiguration
    {
    public:
        [[nodiscard]]
        static RuntimeConfiguration* Get();

        [[nodiscard]]
        const SystemConfiguration& GetConfiguration() const;

        [[nodiscard]]
        RuntimeInfo GetRuntimeInfo() const;

    private:
        friend class Runtime;

        RuntimeConfiguration() = default;
        ~RuntimeConfiguration() = default;

        static AZStd::atomic<RuntimeConfiguration*> s_instance;
    };
} // namespace Jolt
