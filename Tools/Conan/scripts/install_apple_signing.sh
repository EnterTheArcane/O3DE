#!/usr/bin/env bash
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set -euo pipefail

if [[ -z ${APPLE_CODE_SIGN_IDENTITY:-} && -z ${APPLE_SIGNING_CERTIFICATE_BASE64:-} ]]; then
    exit 0
fi
: "${APPLE_CODE_SIGN_IDENTITY:?Set the Apple signing identity}"
: "${APPLE_SIGNING_CERTIFICATE_BASE64:?Set the Apple signing certificate}"

directory=$(mktemp -d "$RUNNER_TEMP/o3de-signing.XXXXXX")
certificate="$directory/certificate.p12"
keychain="$directory/signing.keychain-db"
password=$(openssl rand -hex 32)
trap 'rm -f -- "$certificate"' EXIT
printf '%s' "$APPLE_SIGNING_CERTIFICATE_BASE64" | base64 -D -o "$certificate"
security create-keychain -p "$password" "$keychain"
security set-keychain-settings -lut 18000 "$keychain"
security unlock-keychain -p "$password" "$keychain"
security import "$certificate" -k "$keychain" -P "${APPLE_SIGNING_CERTIFICATE_PASSWORD:-}" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple: -s -k "$password" "$keychain"
printf 'APPLE_CODE_SIGN_KEYCHAIN=%s\nAPPLE_CODE_SIGN_IDENTITY=%s\n' \
    "$keychain" "$APPLE_CODE_SIGN_IDENTITY" >> "$GITHUB_ENV"
