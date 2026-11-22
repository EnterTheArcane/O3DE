// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <handler/handler_main.h>
#include <util/net/http_multipart_builder.h>
#include <util/net/http_transport.h>

namespace O3de
{
    bool CheckConfirmation(const crashpad::CrashReportDatabase::Report&)
    {
        return false;
    }

    bool AddAttachments(crashpad::HTTPMultipartBuilder&)
    {
        return false;
    }

    bool UpdateHttpTransport(std::unique_ptr<crashpad::HTTPTransport>&, const std::string&)
    {
        return false;
    }
}

int main()
{
    crashpad::CrashpadClient client;
    char program[] = "crashpad_handler";
    char help[] = "--help";
    char* arguments[] = {program, help, nullptr};
    return crashpad::HandlerMain(2, arguments, nullptr);
}
