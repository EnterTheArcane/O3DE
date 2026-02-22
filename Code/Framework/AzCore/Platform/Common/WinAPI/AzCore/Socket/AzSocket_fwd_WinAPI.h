
/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

// When building as a C++20 module, these types are already fully defined
// via <WinSock2.h> in the global module fragment — forward declarations
// would conflict with the existing definitions.
#if !defined(AZ_BUILD_CXX_MODULE)
struct sockaddr;
struct sockaddr_in;
#endif
