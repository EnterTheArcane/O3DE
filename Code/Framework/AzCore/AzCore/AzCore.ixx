/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// =============================================================================
// AzCore C++20 Module Interface
//
// This follows the same pattern as the Microsoft STL's std.ixx:
//   - System and third-party headers go in the global module fragment
//   - AZ_BUILD_CXX_MODULE is defined so that AZ_EXPORT expands to 'export'
//   - AzCore public headers are included in the module purview
//   - Only declarations annotated with AZ_EXPORT are visible to importers
//
// Usage for consumers (once fully annotated):
//   import AzCore;          // instead of individual #include lines
//
// To add more exports, annotate declarations in AzCore headers with AZ_EXPORT:
//   AZ_EXPORT class MyClass { ... };
//   AZ_EXPORT void MyFunction();
//   AZ_EXPORT using MyAlias = ...;
// =============================================================================

module;

// ---- Global module fragment ------------------------------------------------
// Standard library headers used transitively by AzCore.
// These MUST be here (not in the module purview) to avoid ODR violations when
// other translation units include the same standard headers.

// C compatibility
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>

// Type support / traits
#include <limits>
#include <new>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

// Concepts / comparisons
#include <compare>
#include <concepts>

// Utilities
#include <any>
#include <bitset>
#include <functional>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <version>

// Memory
#include <memory>
#include <memory_resource>
#include <scoped_allocator>

// Iterators / ranges
#include <iterator>
#include <ranges>

// Strings
#include <charconv>
#include <string>
#include <string_view>

// Containers
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <span>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Algorithms / numerics
#include <algorithm>
#include <numeric>

// I/O
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <streambuf>

// Threading
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>

// Misc
#include <exception>
#include <random>
#include <ratio>
#include <regex>
#include <stdexcept>
#include <system_error>

// Platform SDK headers — must be in the global module fragment so that
// re-inclusion from AzCore platform headers doesn't create conflicting
// declarations between the global module and module AzCore.
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <WinSock2.h>
#   include <WS2tcpip.h>
#   include <Windows.h>
#   include <intrin.h>
    // Undo Windows.h macro pollution that would rename C++ identifiers
    // (e.g. GetObject -> GetObjectA, GetMessage -> GetMessageA, etc.)
#   undef GetObject
#   undef GetMessage
#   undef SendMessage
#   undef PostMessage
#   undef CreateFile
#   undef DeleteFile
#   undef CopyFile
#   undef MoveFile
#   undef LoadLibrary
#   undef GetClassName
#   undef GetCommandLine
#   undef CreateEvent
#   undef CreateDirectory
#   undef RemoveDirectory
#   undef GetCurrentDirectory
#   undef SetCurrentDirectory
#   undef GetEnvironmentVariable
#   undef SetEnvironmentVariable
#   undef GetTempPath
#   undef GetTempFileName
#   undef CreateProcess
#   undef DispatchMessage
#endif

// Define AZ_BUILD_CXX_MODULE so that AZ_EXPORT expands to 'export' for every
// AzCore header included in the module purview below.
// When building via the CMake helper target, AZ_BUILD_CXX_MODULE is already
// defined as a compile definition; the #define here covers the static-lib path
// where the .ixx is compiled directly inside the main target.
#ifndef AZ_BUILD_CXX_MODULE
#define AZ_BUILD_CXX_MODULE
#endif

// ---- Module purview --------------------------------------------------------
export module AzCore;

// Pre-declare exported AZStd types so that non-exported forward declarations
// in later headers don't conflict with the exported definitions.
// (In C++20 modules the FIRST declaration in the purview sets the export status;
// all redeclarations of the same entity inherit that status.)
namespace AZStd
{
    export template <class Element, class Traits, class Allocator>
    class basic_string;

    export template <class Element, class Traits>
    class basic_string_view;

    export template <class T, class Allocator>
    class vector;

    export template <class Key, class MappedType, class Hasher, class EqualKey, class Allocator>
    class unordered_map;

    export template <class T>
    class shared_ptr;
}

// -- Core / base -------------------------------------------------------------
#include <AzCore/PlatformDef.h>
#include <AzCore/variadic.h>
#include <AzCore/base.h>
#include <AzCore/Platform.h>

// -- Preprocessor utilities --------------------------------------------------
#include <AzCore/Preprocessor/Enum.h>
#include <AzCore/Preprocessor/EnumReflectUtils.h>
#include <AzCore/Preprocessor/Sequences.h>

// -- Casting -----------------------------------------------------------------
#include <AzCore/Casting/lossy_cast.h>
#include <AzCore/Casting/numeric_cast.h>

// -- Outcome -----------------------------------------------------------------
#include <AzCore/Outcome/Outcome.h>

// -- Debug -------------------------------------------------------------------
#include <AzCore/Debug/Budget.h>
#include <AzCore/Debug/BudgetTracker.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Debug/ProfilerBus.h>
#include <AzCore/Debug/StackTracer.h>
#include <AzCore/Debug/Timer.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Debug/TraceMessageBus.h>

// -- Memory ------------------------------------------------------------------
#include <AzCore/Memory/IAllocator.h>
#include <AzCore/Memory/Memory_fwd.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/AllocatorBase.h>
#include <AzCore/Memory/AllocatorInstance.h>
#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Memory/AllocatorWrapper.h>
#include <AzCore/Memory/Config.h>
#include <AzCore/Memory/HphaAllocator.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/Memory/SimpleSchemaAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>

// -- RTTI / Reflection -------------------------------------------------------
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/RTTIMacros.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/RTTI/TemplateInfo.h>
#include <AzCore/RTTI/TypeSafeIntegral.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

// -- Math --------------------------------------------------------------------
#include <AzCore/Math/MathIntrinsics.h>
#include <AzCore/Math/SimdMath.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/VectorN.h>
#include <AzCore/Math/VectorConversions.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/Math/PackedVector3.h>
#include <AzCore/Math/PackedVector4.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/MatrixMxN.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Obb.h>
#include <AzCore/Math/Sphere.h>
#include <AzCore/Math/Capsule.h>
#include <AzCore/Math/Hemisphere.h>
#include <AzCore/Math/Plane.h>
#include <AzCore/Math/Frustum.h>
#include <AzCore/Math/Ray.h>
#include <AzCore/Math/LineSegment.h>
#include <AzCore/Math/Spline.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Guid.h>
#include <AzCore/Math/Random.h>
#include <AzCore/Math/Sha1.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/ShapeIntersection.h>
#include <AzCore/Math/IntersectPoint.h>
#include <AzCore/Math/IntersectSegment.h>
#include <AzCore/Math/InterpolationSample.h>
#include <AzCore/Math/PolygonPrism.h>
#include <AzCore/Math/VertexContainer.h>
#include <AzCore/Math/VertexContainerInterface.h>

// -- EBus --------------------------------------------------------------------
#include <AzCore/EBus/EBus.h>
#include <AzCore/EBus/Event.h>
#include <AzCore/EBus/OrderedEvent.h>
#include <AzCore/EBus/Policies.h>
#include <AzCore/EBus/Results.h>
#include <AzCore/EBus/Environment.h>
#include <AzCore/EBus/IEventScheduler.h>
#include <AzCore/EBus/ScheduledEvent.h>

// -- Component / Entity ------------------------------------------------------
#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/ComponentExport.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/EntityUtils.h>
#include <AzCore/Component/NamedEntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/NonUniformScaleBus.h>

// -- IO / Streams ------------------------------------------------------------
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/FileReader.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/OpenMode.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/TextStreamWriters.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/Path/Path_fwd.h>
#include <AzCore/IO/IStreamer.h>

// -- Module ------------------------------------------------------------------
#include <AzCore/Module/Module.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/Module/ModuleManagerBus.h>

// -- Serialization -----------------------------------------------------------
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/JsonUtils.h>

// -- Settings ----------------------------------------------------------------
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/Settings/SettingsRegistryImpl.h>
#include <AzCore/Settings/CommandLine.h>

// -- Console -----------------------------------------------------------------
#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/Console.h>

// -- Name --------------------------------------------------------------------
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>

// -- Jobs / Tasks ------------------------------------------------------------
#include <AzCore/Jobs/Job.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Task/TaskGraph.h>
#include <AzCore/Task/TaskExecutor.h>

// -- Interface / Instance ----------------------------------------------------
#include <AzCore/Interface/Interface.h>
#include <AzCore/Instance/InstancePool.h>

// -- Asset -------------------------------------------------------------------
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Asset/AssetTypeInfoBus.h>

// -- DOM ---------------------------------------------------------------------
#include <AzCore/DOM/DomPath.h>
#include <AzCore/DOM/DomValue.h>
#include <AzCore/DOM/DomUtils.h>
#include <AzCore/DOM/DomPatch.h>

// -- String ------------------------------------------------------------------
#include <AzCore/StringFunc/StringFunc.h>

// -- Socket ------------------------------------------------------------------
#include <AzCore/Socket/AzSocket.h>

// -- Time --------------------------------------------------------------------
#include <AzCore/Time/ITime.h>

// -- Utils -------------------------------------------------------------------
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/Utils/Utils.h>

// -- Threading ---------------------------------------------------------------
#include <AzCore/Threading/ThreadSafeDeque.h>
#include <AzCore/Threading/ThreadSafeObject.h>

// -- Statistics --------------------------------------------------------------
#include <AzCore/Statistics/RunningStatistic.h>
#include <AzCore/Statistics/StatisticalProfiler.h>

// -- Dependency --------------------------------------------------------------
#include <AzCore/Dependency/Dependency.h>

// -- Platform ----------------------------------------------------------------
#include <AzCore/PlatformId/PlatformId.h>
#include <AzCore/PlatformId/PlatformDefaults.h>

// -- Module entry point ------------------------------------------------------
#include <AzCore/AzCoreModule.h>
