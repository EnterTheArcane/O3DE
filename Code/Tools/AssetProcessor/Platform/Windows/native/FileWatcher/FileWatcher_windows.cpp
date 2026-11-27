/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/tuple.h>
#include <AzCore/std/utils.h>
#include <native/FileWatcher/FileWatcher.h>
#include <native/FileWatcher/FileWatcher_platform.h>
#include <QDir>
#include <QSet>
#include <QString>

bool FileWatcher::PlatformStart()
{
    bool allSucceeded = true;
    for (const auto& [directory, recursive] : m_folderWatchRoots)
    {
        if (QDir(directory).exists())
        {
            allSucceeded &= m_platformImpl->AddWatchFolder(directory, recursive);
        }
    }
    return allSucceeded;
}

bool FileWatcher::PlatformImplementation::AddWatchFolder(QString root, bool recursive)
{
    HandleUniquePtr directoryHandle{::CreateFileW(
        root.toStdWString().data(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    )};

    if (directoryHandle.get() == INVALID_HANDLE_VALUE)
    {
        AZ_Warning("FileWatcher", false, "Failed to start watching %s", root.toUtf8().constData());
        return false;
    }

    // Associate this file handle with our existing io completion port handle
    if (!::CreateIoCompletionPort(directoryHandle.get(), m_ioHandle.get(), /*CompletionKey =*/ static_cast<ULONG_PTR>(PlatformImplementation::EventType::FileRead), 1))
    {
        return false;
    }

    auto id = AZStd::make_unique<OVERLAPPED>();
    auto* idp = id.get();
    const auto& [folderWatch, inserted] = m_folderRootWatches.emplace(AZStd::piecewise_construct, AZStd::forward_as_tuple(idp),
        AZStd::forward_as_tuple(AZStd::move(id), AZStd::move(directoryHandle), root, recursive));

    if (!inserted)
    {
        return false;
    }

    return folderWatch->second.ReadChanges();
}

bool FileWatcher::PlatformImplementation::FolderRootWatch::ReadChanges()
{
    // Register to get directory change notifications for our directory handle
    return ::ReadDirectoryChangesW(
        m_directoryHandle.get(),
        &m_fileNotifyInformationList,
        sizeof(m_fileNotifyInformationList),
        m_recursive,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_FILE_NAME,
        nullptr,
        m_overlapped.get(),
        nullptr
    );
}

void FileWatcher::PlatformImplementation::FolderRootWatch::DispatchChanges(FileWatcher& watcher) const
{
    const QDir root(m_directoryRoot);
    const auto* notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(&m_fileNotifyInformationList);
    for (;;)
    {
        const QString file = QDir::toNativeSeparators(
            root.filePath(QString::fromWCharArray(notification->FileName, notification->FileNameLength / sizeof(wchar_t))));

        switch (notification->Action)
        {
        case FILE_ACTION_ADDED:
        case FILE_ACTION_RENAMED_NEW_NAME:
            watcher.rawFileAdded(file);
            break;
        case FILE_ACTION_REMOVED:
        case FILE_ACTION_RENAMED_OLD_NAME:
            watcher.rawFileRemoved(file);
            break;
        case FILE_ACTION_MODIFIED:
            // Changes to size, attributes, data, or timestamps can produce multiple notifications.
            watcher.rawFileModified(file);
            break;
        }

        if (notification->NextEntryOffset == 0)
        {
            break;
        }
        notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const char*>(notification) + notification->NextEntryOffset);
    }
}

void FileWatcher::PlatformStop()
{
    // Send a special signal to the child thread, that is blocked in a GetQueuedCompletionStatus call, with a completion
    // key set to Shutdown. The child thread will stop its processing when it receives this value for the completion key
    PostQueuedCompletionStatus(m_platformImpl->m_ioHandle.get(), 0, /*CompletionKey =*/ static_cast<ULONG_PTR>(PlatformImplementation::EventType::Shutdown), nullptr);
    if (m_thread.joinable())
    {
        m_thread.join(); // wait for the thread to finish
    }
}

void FileWatcher::WatchFolderLoop()
{
    m_startedSignal = true; // signal that we are no longer going to drop any events.

    while (!m_shutdownThreadSignal)
    {
        LPOVERLAPPED directoryId = nullptr;
        ULONG_PTR completionKey = 0;
        DWORD byteCount = 0;
        const BOOL completionSucceeded = ::GetQueuedCompletionStatus(m_platformImpl->m_ioHandle.get(), &byteCount, &completionKey, &directoryId, INFINITE);
        DWORD completionError = ERROR_SUCCESS;
        if (!completionSucceeded)
        {
            completionError = ::GetLastError();
        }
        if (m_shutdownThreadSignal || (completionSucceeded && completionKey == static_cast<ULONG_PTR>(PlatformImplementation::EventType::Shutdown)))
        {
            break;
        }
        if (!completionSucceeded && !directoryId)
        {
            AZ_Warning("FileWatcher", false, "Waiting for directory changes failed (Windows error %lu).", completionError);
            break;
        }

        const auto foundFolderRoot = m_platformImpl->m_folderRootWatches.find(directoryId);
        if (foundFolderRoot == end(m_platformImpl->m_folderRootWatches))
        {
            continue;
        }

        PlatformImplementation::FolderRootWatch& folderRoot = foundFolderRoot->second;
        if (!completionSucceeded && completionError != ERROR_NOTIFY_ENUM_DIR)
        {
            AZ_Warning("FileWatcher", false, "Watching %s failed (Windows error %lu).", folderRoot.m_directoryRoot.toUtf8().constData(), completionError);
            continue;
        }

        const bool notificationsLost = completionError == ERROR_NOTIFY_ENUM_DIR || byteCount == 0;
        if (!notificationsLost)
        {
            folderRoot.DispatchChanges(*this);
        }

        // A completion reporting notification loss also consumes the pending read.
        if (!folderRoot.ReadChanges())
        {
            const DWORD error = ::GetLastError();
            AZ_Warning("FileWatcher", false, "Resuming the directory watch for %s failed (Windows error %lu).", folderRoot.m_directoryRoot.toUtf8().constData(), error);
        }
        AZ_Warning("FileWatcher", !notificationsLost, "Directory change notifications were lost for %s.", folderRoot.m_directoryRoot.toUtf8().constData());
    }
}
