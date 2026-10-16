#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
"""OTA steps for gtaxlwifi: ota/gtaxl-ota.sh checks the partition table and
keeps the addon backup on USERDATA; vbmeta, which the non-A/B package does
not write, is written at the end.
"""

import common

OTA_SH = "/tmp/install/bin/gtaxl-ota.sh"


def FullOTA_InstallBegin(info):
  # The generic script unpacks install/ only after the backup
  info.script.AppendExtra('package_extract_dir("install", "/tmp/install");')
  info.script.AppendExtra(
      'set_metadata_recursive("/tmp/install", "uid", 0, "gid", 0, '
      '"dmode", 0755, "fmode", 0755);')
  info.script.AppendExtra(
      'run_program("/system/bin/sh", "%s", "check") == 0 || '
      'abort("E3004: gtaxlwifi: the partition table is not migrated, '
      'install the repartition package first");' % OTA_SH)
  info.script.AppendExtra(
      'run_program("/system/bin/sh", "%s", "prepare") == 0 || '
      'abort("E3004: gtaxlwifi: no room for the addon backup, see '
      '/tmp/gtaxl-ota.log");' % OTA_SH)


def FullOTA_InstallEnd(info):
  data = info.input_zip.read("IMAGES/vbmeta.img")
  common.ZipWriteStr(info.output_zip, "vbmeta.img", data)
  info.script.AppendExtra(
      'package_extract_file("vbmeta.img", "/dev/block/by-name/vbmeta");')
  info.script.AppendExtra('run_program("/system/bin/sh", "%s", "finish");' % OTA_SH)
