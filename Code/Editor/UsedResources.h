/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

//! Class passed to resource gathering functions

#include "Include/EditorCoreAPI.h"
#include "QtUtil.h"

class EDITOR_CORE_API CUsedResources
{
public:
    using TResourceFiles = std::set<QString, Editor::CaseInsensitiveQStringLess>;

    CUsedResources();
    void Add(const char* pResourceFileName);

    TResourceFiles files;
};
