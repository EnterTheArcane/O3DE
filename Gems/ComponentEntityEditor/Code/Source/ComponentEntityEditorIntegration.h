/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/smart_ptr/unique_ptr.h>

class ComponentEntityEditorIntegration
{
public:
    ComponentEntityEditorIntegration();
    ~ComponentEntityEditorIntegration();

private:
    bool m_registered = false;
    AZStd::unique_ptr<class SandboxIntegrationManager> m_appListener;
};
