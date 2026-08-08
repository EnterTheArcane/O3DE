/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#include "EditorDefs.h"

#include "LevelIndependentFileMan.h"

#include <AzCore/std/algorithm.h>

CLevelIndependentFileMan::CLevelIndependentFileMan()
{
}

CLevelIndependentFileMan::~CLevelIndependentFileMan()
{
    assert(m_Modules.size() == 0);
}

bool CLevelIndependentFileMan::PromptChangedFiles()
{
    for (std::vector<ILevelIndependentFileModule*>::iterator it = m_Modules.begin(); it != m_Modules.end(); ++it)
    {
        if (!(*it)->PromptChanges())
        {
            return false;
        }
    }
    return true;
}

void CLevelIndependentFileMan::RegisterModule(ILevelIndependentFileModule* pModule)
{
    if (AZStd::find(m_Modules.begin(), m_Modules.end(), pModule) == m_Modules.end())
    {
        m_Modules.push_back(pModule);
    }
}
void CLevelIndependentFileMan::UnregisterModule(ILevelIndependentFileModule* pModule)
{
    if (auto moduleIterator = AZStd::find(m_Modules.begin(), m_Modules.end(), pModule); moduleIterator != m_Modules.end())
    {
        m_Modules.erase(moduleIterator);
    }
}
