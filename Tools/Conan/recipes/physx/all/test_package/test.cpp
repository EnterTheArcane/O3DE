// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <PxPhysicsAPI.h>

static_assert(PX_PHYSICS_VERSION_MAJOR == 5);
static_assert(PX_PHYSICS_VERSION_MINOR == 13);
static_assert(PX_PHYSICS_VERSION_BUGFIX == 0);

int main()
{
    physx::PxDefaultAllocator allocator;
    physx::PxDefaultErrorCallback errorCallback;
    physx::PxFoundation* foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
    if (!foundation)
    {
        return 1;
    }

    physx::PxPhysics* physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale());
    if (!physics)
    {
        foundation->release();
        return 1;
    }

    physics->release();
    foundation->release();
    return 0;
}
