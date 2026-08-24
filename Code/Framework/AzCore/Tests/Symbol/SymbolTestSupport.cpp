/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Tests/Symbol/SymbolTestSupport.h>

#include <AzCore/Symbol/SymbolLiteral.h>

namespace UnitTest
{
    AZ::Symbol GetCrossTranslationUnitSymbol()
    {
        using namespace AZ::Literals;
        return "CrossTranslationUnitSymbol"_sym;
    }
} // namespace UnitTest
