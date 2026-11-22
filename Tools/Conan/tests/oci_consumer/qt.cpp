// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <QtCore/QCoreApplication>
#include <QtNetwork/QSslSocket>

#include <string_view>

int main(int argc, char** argv)
{
    const QCoreApplication application(argc, argv);
    if (std::string_view(qVersion()) != QT_VERSION_STR)
    {
        return 1;
    }
    if (!QSslSocket::supportsSsl())
    {
        return 2;
    }
    return 0;
}
