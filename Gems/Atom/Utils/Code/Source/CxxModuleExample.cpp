/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// =============================================================================
// C++20 Module Consumer — Real-world Atom example
//
// This file is a module-aware counterpart to AssetCollectionAsyncLoader.cpp.
// It replaces a dozen #include directives with a single `import AzCore;` and
// uses the same AZStd types (vector, string, unordered_map, shared_ptr, …)
// to demonstrate that the module provides full access to AzCore's public API.
//
// Both patterns coexist in the same build:
//   - Traditional .cpp files continue to use #include (no changes needed)
//   - New .cpp files CAN use `import AzCore;` when O3DE_CXX_MODULES is ON
//
// Build:
//   cmake -B Build/Modules -G Ninja \
//         -DLY_UNITY_BUILD=OFF -DO3DE_CXX_MODULES=ON -DLY_MONOLITHIC_GAME=ON
//   cmake --build Build/Modules --target Atom_Utils.Static
// =============================================================================

import AzCore;

// ---------------------------------------------------------------------------
// A lightweight registry that maps string keys to typed values, implemented
// entirely with types imported from the AzCore module.
// ---------------------------------------------------------------------------

namespace Atom::Utils
{
    //! Simple typed property bag — stores named values of arbitrary type.
    //! Uses AZStd::any for type-erased storage, AZStd::string as keys,
    //! and AZStd::unordered_map as the backing container.
    class PropertyBag final
    {
    public:
        //! Set a named property.
        template <typename T>
        void Set(AZStd::string_view key, T&& value)
        {
            m_properties[AZStd::string(key)] = AZStd::any(AZStd::forward<T>(value));
        }

        //! Try to retrieve a property by name.  Returns nullptr if the key is
        //! missing or the type doesn't match.
        template <typename T>
        const T* Get(AZStd::string_view key) const
        {
            auto it = m_properties.find(AZStd::string(key));
            if (it == m_properties.end())
            {
                return nullptr;
            }
            return AZStd::any_cast<T>(&it->second);
        }

        //! Return the number of stored properties.
        AZ::u64 Count() const { return static_cast<AZ::u64>(m_properties.size()); }

        //! Collect all keys into a sorted vector.
        AZStd::vector<AZStd::string> Keys() const
        {
            AZStd::vector<AZStd::string> result;
            result.reserve(m_properties.size());
            for (auto it = m_properties.begin(); it != m_properties.end(); ++it)
            {
                result.push_back(it->first);
            }
            return result;
        }

    private:
        AZStd::unordered_map<AZStd::string, AZStd::any> m_properties;
    };

    // -----------------------------------------------------------------------
    // A shared-ownership asset path list — demonstrates AZStd::shared_ptr
    // and AZStd::unique_ptr from the module.
    // -----------------------------------------------------------------------

    struct AssetPathList
    {
        AZStd::vector<AZStd::string> m_paths;
    };

    //! Create a shared asset path list from individual paths.
    inline AZStd::shared_ptr<AssetPathList> MakeSharedPathList(
        AZStd::vector<AZStd::string> paths)
    {
        auto list = AZStd::shared_ptr<AssetPathList>(new AssetPathList{});
        list->m_paths = AZStd::move(paths);
        return list;
    }

    //! Create a unique asset path list.
    inline AZStd::unique_ptr<AssetPathList> MakeUniquePathList(
        AZStd::vector<AZStd::string> paths)
    {
        auto list = AZStd::unique_ptr<AssetPathList>(new AssetPathList{});
        list->m_paths = AZStd::move(paths);
        return list;
    }

} // namespace Atom::Utils
