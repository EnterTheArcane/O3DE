/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Serialization/Locale_Platform.h>
#include <langinfo.h>

namespace AZ::Locale
{
    namespace
    {
        locale_t GetSerializationLocale()
        {
            // Locale objects are immutable after construction and may be shared by threads.
            // Keeping one process-lifetime C locale also makes locale switching safe across
            // Lua's longjmp-based error handling, which can bypass nested C++ destructors.
            static locale_t serializationLocale = newlocale(LC_ALL_MASK, "C", nullptr);
            return serializationLocale;
        }
    }

    void ScopedSerializationLocale_Platform::Activate()
    {
        if (m_isActive)
        {
            Deactivate();
        }

        if (locale_t serializationLocale = GetSerializationLocale())
        {
            m_previousLocale = uselocale(serializationLocale);
            m_isActive = m_previousLocale != nullptr;
        }
    }

    void ScopedSerializationLocale_Platform::Deactivate()
    {
        if (m_isActive)
        {
            uselocale(m_previousLocale);
            m_isActive = false;
        }
    }
} // namespace AZ::Locale
