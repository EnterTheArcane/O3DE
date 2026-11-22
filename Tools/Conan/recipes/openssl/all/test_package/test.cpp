// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <openssl/ssl.h>

static_assert(OPENSSL_VERSION_MAJOR == 4);

int main()
{
    SSL_CTX* context = SSL_CTX_new(TLS_method());
    if (!context)
    {
        return 1;
    }
    SSL_CTX_free(context);
    if (OPENSSL_version_major() != OPENSSL_VERSION_MAJOR)
    {
        return 1;
    }
    return 0;
}
