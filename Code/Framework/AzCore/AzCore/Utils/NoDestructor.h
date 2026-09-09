/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/typetraits/is_destructible.h>
#include <AzCore/std/utility/move.h>
#include <AzCore/std/utils.h>

#include <new>

namespace AZ
{
    namespace Internal
    {
        template<class T, bool = AZStd::is_trivially_destructible_v<T>>
        class NoDestructorStorage;

        template<class T>
        class NoDestructorStorage<T, true> final
        {
        public:
            template<class... Args>
            explicit constexpr NoDestructorStorage(Args&&... args)
                : m_value(AZStd::forward<Args>(args)...)
            {
            }

            [[nodiscard]]
            constexpr T& Get()
            {
                return m_value;
            }

            [[nodiscard]]
            constexpr const T& Get() const
            {
                return m_value;
            }

        private:
            T m_value;
        };

        template<class T>
        class NoDestructorStorage<T, false> final
        {
        public:
            template<class... Args>
            explicit NoDestructorStorage(Args&&... args)
            {
                AZStd::construct_at(reinterpret_cast<T*>(m_storage), AZStd::forward<Args>(args)...);
            }

            [[nodiscard]]
            T& Get()
            {
                return *std::launder(reinterpret_cast<T*>(m_storage));
            }

            [[nodiscard]]
            const T& Get() const
            {
                return *std::launder(reinterpret_cast<const T*>(m_storage));
            }

        private:
            alignas(T) AZStd::byte m_storage[sizeof(T)];
        };
    } // namespace Internal

    //! Owns an in-place value whose destructor is deliberately never invoked.
    //! Trivially destructible values are stored directly.
    //! Other values use aligned byte storage so this wrapper remains trivially destructible.
    //! This suppresses destruction only.
    //! It does not pin the containing module or extend the lifetime of allocators, services, vtables, or constructor arguments used by T.
    template<class T>
    class NoDestructor final
    {
    public:
        AZ_DISABLE_COPY_MOVE(NoDestructor);

        template<class... Args>
        explicit constexpr NoDestructor(Args&&... args)
            : m_storage(AZStd::forward<Args>(args)...)
        {
        }

        [[nodiscard]]
        constexpr T& Get()
        {
            return m_storage.Get();
        }

        [[nodiscard]]
        constexpr const T& Get() const
        {
            return m_storage.Get();
        }

        [[nodiscard]]
        constexpr T& operator*()
        {
            return Get();
        }

        [[nodiscard]]
        constexpr const T& operator*() const
        {
            return Get();
        }

        [[nodiscard]]
        constexpr T* operator->()
        {
            return AZStd::addressof(Get());
        }

        [[nodiscard]]
        constexpr const T* operator->() const
        {
            return AZStd::addressof(Get());
        }

    private:
        Internal::NoDestructorStorage<T> m_storage;
    };
} // namespace AZ
