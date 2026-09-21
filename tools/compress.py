# Copyright (C) 2026 Ricardo Guzman - CA2RXU
#
# This file is part of LoRa APRS iGate.
#
# LoRa APRS iGate is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# LoRa APRS iGate is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with LoRa APRS iGate. If not, see <https://www.gnu.org/licenses/>.

import gzip
import os
import datetime
Import("env")

files = [
  'data_embed/index.html',
  'data_embed/script.js',
  'data_embed/style.css',
  'data_embed/bootstrap.js',
  'data_embed/bootstrap.css',
  'data_embed/leaflet.js',
  'data_embed/leaflet.css',
  'data_embed/favicon.png',
  'data_embed/aprs-symbols-24-0.png',
  'data_embed/aprs-symbols-24-1.png',
  'data_embed/aprs-symbols-24-2.png',
]

string_to_find_str = "String"
versionDate = "unknown"
versionNumber = "unknown"

with open('src/LoRa_APRS_iGate.cpp', encoding='utf-8') as cpp_file:
  for line in cpp_file:
    if string_to_find_str in line and ("versionDate" in line or "versionNumber" in line):
      start = line.find('"') + 1
      end = line.find('"', start)
      if start > 0 and end > start:
        if "versionDate" in line:
          versionDate = line[start:end]
        elif "versionNumber" in line:
          versionNumber = line[start:end]

for src in files:
  out = src + ".gz"


  with open(src, 'rb') as f:
    content = f.read()

  if src == 'data_embed/index.html':
    env_vars = "Board / Environment: " + env["BOARD"]
    current_date = datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S') + " UTC"
    build_info = (
      f'Firmware: {versionNumber}<br>'
      f'Version date: {versionDate}<br>'
      f'Build date: {current_date}<br>'
      f'{env_vars}'
    ).encode()

    content = content.replace(b'%BUILD_INFO%', build_info)

  with open(out, 'wb') as f:
    f.write(gzip.compress(content, compresslevel=9))
