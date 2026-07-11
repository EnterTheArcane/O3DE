/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "StreamableInterface.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace AZ
{
    //! Wraps and forwards data to a std::ostream,
    //! Overrides operator<< for char, const char * and string data and counts
    //! the number of '\n' (new line) characters that go through it.
    class NewLineCounterStream : public MakeOStreamStreamable
    {
        using Self = NewLineCounterStream;

    public:
        using MakeOStreamStreamable::MakeOStreamStreamable;

        ~NewLineCounterStream() override = default;

        Self& operator<<(const char c) override
        {
            if (c == '\n')
            {
                m_lineCount++;
            }
            m_wrappedStream << c;
            return *this;
        }

        Self& operator<<(const char* nts) override
        {
            const char* tmp = nts;
            while (const char c = *tmp++)
            {
                if (c == '\n')
                {
                    m_lineCount++;
                }
            }
            m_wrappedStream << nts;
            return *this;
        }

        Self& operator<<(const std::string& str) override
        {
            for (const char c : str)
            {
                if (c == '\n')
                {
                    m_lineCount++;
                }
            }
            m_wrappedStream << str;
            return *this;
        }

        Self& operator<<(std::string_view str) override
        {
            for (const char c : str)
            {
                if (c == '\n')
                {
                    m_lineCount++;
                }
            }
            m_wrappedStream << str;
            return *this;
        }

        Self& operator<<(const double n) override
        {
            m_wrappedStream << n;
            return *this;
        }

        Self& operator<<(const int n) override
        {
            m_wrappedStream << n;
            return *this;
        }

        Self& operator<<(const int64_t n) override
        {
            m_wrappedStream << n;
            return *this;
        }

        Self& operator<<(const uint32_t n) override
        {
            m_wrappedStream << n;
            return *this;
        }

        Self& operator<<(const size_t n) override
        {
            m_wrappedStream << n;
            return *this;
        }

        Self& operator<<(const bool b) override
        {
            m_wrappedStream << b;
            return *this;
        }

        int GetLineCount() const
        {
            return m_lineCount;
        }

    private:
        int m_lineCount = 0;
    };
}
