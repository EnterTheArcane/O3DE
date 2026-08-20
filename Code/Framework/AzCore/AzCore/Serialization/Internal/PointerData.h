/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Serialize::Internal
{
    //! Destroys a reflected pointer using the pointee's actual dynamic type and adjusted address.
    void DestroyPointerData(
        SerializeContext* context,
        const SerializeContext::ClassElement* classElement,
        const void* pointerStorage);
}
