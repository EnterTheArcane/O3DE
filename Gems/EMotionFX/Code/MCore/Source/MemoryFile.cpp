/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// include required headers
#include "MemoryFile.h"
#include "DiskFile.h"
#include "LogManager.h"
#include <AzCore/std/limits.h>


namespace MCore
{
    // return the unique type identification number
    uint32 MemoryFile::GetType() const
    {
        return TYPE_ID;
    }


    // try to open the memory location
    bool MemoryFile::Open(uint8* memoryStart, size_t length)
    {
        m_memoryStart    = memoryStart;
        m_currentPos     = memoryStart;
        m_length         = length;
        m_usedLength     = length;
        m_preAllocSize   = 1024; // pre-allocate 1 extra KB

        // if we need to create a new memory block
        if (memoryStart == nullptr)
        {
            m_allocate = true;
            if (length > 0)
            {
                m_memoryStart = (uint8*)MCore::Allocate(length, MCORE_MEMCATEGORY_MEMORYFILE);
                m_currentPos  = m_memoryStart;
            }
        }
        else
        {
            m_allocate = false;
        }

        return true;
    }


    // close the file
    void MemoryFile::Close()
    {
        // get rid of the allocated memory
        if (m_allocate)
        {
            MCore::Free(m_memoryStart);
        }

        m_memoryStart    = nullptr;
        m_currentPos     = nullptr;
        m_length         = 0;
        m_usedLength     = 0;
    }


    // flush the file
    void MemoryFile::Flush()
    {
    }


    bool MemoryFile::GetIsOpen() const
    {
        return (m_memoryStart != nullptr);
    }


    // return true when we have reached the end of the file
    bool MemoryFile::GetIsEOF() const
    {
        return (GetPos() >= GetFileSize());
    }


    // returns the next byte in the file
    uint8 MemoryFile::GetNextByte()
    {
        uint8 value = *m_currentPos;
        m_currentPos++;
        return value;
    }


    // returns the position (offset) in the file in bytes
    size_t MemoryFile::GetPos() const
    {
        if (m_memoryStart == nullptr)
        {
            return 0;
        }
        return (size_t)(m_currentPos - m_memoryStart);
    }


    // write a given byte to the file
    bool MemoryFile::WriteByte(const uint8 value)
    {
        return Write(&value, 1) == 1;
    }


    // seek a given number of bytes ahead from it's current position
    bool MemoryFile::Forward(size_t numBytes)
    {
        if (numBytes > m_length - MemoryFile::GetPos())
        {
            return false;
        }

        if (numBytes > 0)
        {
            m_currentPos += numBytes;
        }
        return true;
    }


    // seek to an absolute position in the file (offset in bytes)
    bool MemoryFile::Seek(size_t offset)
    {
        if (offset > m_length)
        {
            return false;
        }

        m_currentPos = m_memoryStart + offset;
        return true;
    }


    // write data to the file
    size_t MemoryFile::Write(const void* data, size_t length)
    {
        const size_t offset = MemoryFile::GetPos();
        // if it won't fit in our allocated buffer, we have to enlarge it
        if (length > m_length - offset)
        {
            if (!m_allocate)
            {
                return 0;
            }

            const size_t maxExtra = AZStd::numeric_limits<size_t>::max() - m_length;
            if (offset > maxExtra || length > maxExtra - offset || m_preAllocSize > maxExtra - offset - length)
            {
                return 0;
            }
            const size_t numBytesExtra = offset + length + m_preAllocSize;
            uint8* memoryStart = static_cast<uint8*>(MCore::Realloc(m_memoryStart, m_length + numBytesExtra, MCORE_MEMCATEGORY_MEMORYFILE));
            if (memoryStart == nullptr)
            {
                return 0;
            }
            m_memoryStart = memoryStart;
            m_length += numBytesExtra;
            m_currentPos = m_memoryStart + offset;
        }

        // memcopy over the data
        MCore::MemCopy(m_currentPos, data, length);
        if (length > 0)
        {
            m_currentPos += length;
        }

        // only overwrite the used length in case we reached the boundary (don't do it in case we modify some data in the middle etc.)
        const size_t newPos = offset + length;
        if (m_usedLength < newPos)
        {
            m_usedLength = newPos;
        }

        return length;
    }


    // read data from the file
    size_t MemoryFile::Read(void* data, size_t length)
    {
        const size_t available = m_length - MemoryFile::GetPos();
        size_t numRead = length;
        if (length > available)
        {
            numRead = available;
            MCore::LogWarning("MCore::MemoryFile::Read() - We can only read %zu bytes of the %zu bytes requested, as we are reading past the end of the memory file!", numRead, length);
        }

        MCore::MemCopy(data, m_currentPos, numRead);
        if (numRead > 0)
        {
            m_currentPos += numRead;
        }
        return numRead;
    }


    // returns the filesize in bytes
    size_t MemoryFile::GetFileSize() const
    {
        return m_usedLength;
    }


    // get the memory start address
    uint8* MemoryFile::GetMemoryStart() const
    {
        return m_memoryStart;
    }


    // get the pre-alloc size
    size_t MemoryFile::GetPreAllocSize() const
    {
        return m_preAllocSize;
    }


    // set the pre-alloc size
    void MemoryFile::SetPreAllocSize(size_t newSizeInBytes)
    {
        m_preAllocSize = newSizeInBytes;
    }


    // load memory file from disk
    bool MemoryFile::LoadFromDiskFile(const char* fileName)
    {
        MCORE_ASSERT(fileName);

        // check if the file name is valid
        if (fileName[0] == 0)
        {
            LogError("Cannot load disk file. File name is empty.");
            return false;
        }

        // create the new disk file
        DiskFile diskFile;
        if (diskFile.Open(fileName, DiskFile::READ) == false)
        {
            LogError("Cannot open file '%s' in read mode. Please check if the file actually exists and try again.", fileName);
            return false;
        }

        // get the length of the disk file
        const size_t fileSize = diskFile.GetFileSize();

        // read from our incoming disk file and copy over the data to a temporary buffer
        uint8* data = (uint8*)Allocate(fileSize, MCORE_MEMCATEGORY_MEMORYFILE, 16);
        diskFile.Read(data, fileSize);
        diskFile.Close();

        // write the disk file data to the memory file
        Write(data, fileSize);

        // get rid of the temporary buffer
        Free(data);

        return true;
    }


    // save memory file to disk
    bool MemoryFile::SaveToDiskFile(const char* fileName)
    {
        MCORE_ASSERT(fileName);

        // check if the file name is valid
        if (fileName[0] == 0)
        {
            LogError("Cannot save to disk file. File name is empty.");
            return false;
        }

        // check if the memory file is opened
        if (GetIsOpen() == false)
        {
            LogError("Memory file not opened.");
            return false;
        }

        // seek to the beginning of the memory file
        Seek(0);

        // create the new disk file
        DiskFile newDiskFile;
        if (newDiskFile.Open(fileName, DiskFile::WRITE) == false)
        {
            LogError("Cannot open file '%s' in write mode. Please check if the file is write protected. It might be in use by another application or it is read-only. Please try again after resolving any possible issues.", fileName);
            return false;
        }

        newDiskFile.Write(GetMemoryStart(), GetFileSize());
        newDiskFile.Close();

        return true;
    }
} // namespace MCore
