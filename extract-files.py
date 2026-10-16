#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

blob_fixups: blob_fixups_user_type = {
    # gpsd does not start with SSLv3_client_method or android.hidl.base@1.0.
    # libicuuc (absent from vendor) and libgui are linked but unused.
    'vendor/bin/hw/gpsd': blob_fixup()
        .binary_regex_replace(b'SSLv3_client_method', b'SSLv23_method\x00\x00\x00\x00\x00\x00')
        .remove_needed('android.hidl.base@1.0.so')
        .remove_needed('libicuuc.so')
        .remove_needed('libgui.so')
        .binary_regex_replace(b'/data/system/gps/', b'/data/vendor/gps/'),
    # The SONAME is gps.samsungT0EVOLight64android.so; Soong wants the file name
    'vendor/lib64/hw/gps.default.so': blob_fixup()
        .fix_soname()
        .binary_regex_replace(b'/data/system/gps/', b'/data/vendor/gps/'),
}  # fmt: skip

module = ExtractUtilsModule(
    'gtaxlwifi',
    'samsung',
    blob_fixups=blob_fixups,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
