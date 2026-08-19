#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-/tmp/ac6-audit-screencap-test.png}
black=${2:-/tmp/ac6-audit-screencap-black-1280x720.png}
bin=$(mktemp /tmp/ac6-audit-screencap-test.XXXXXX)
trap 'rm -f "$bin"' EXIT
c++ -std=c++20 -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -I"$root/include" "$root/tests/audit_screencap_tests.cpp" -o "$bin"
"$bin" "$out" "$black"
python3 - "$out" "$black" <<'PY'
from __future__ import annotations
import hashlib
import struct
import sys
import zlib
from pathlib import Path

for index, p in enumerate(map(Path, sys.argv[1:])):
    data = p.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset = 8
    idat = bytearray()
    width = height = None
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        chunk_type = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        crc = struct.unpack_from('>I', data, offset + 8 + size)[0]
        assert zlib.crc32(chunk_type + payload) & 0xFFFFFFFF == crc
        if chunk_type == b'IHDR':
            width, height = struct.unpack_from('>II', payload, 0)
        elif chunk_type == b'IDAT':
            idat += payload
        offset += 12 + size
    raw = zlib.decompress(bytes(idat))
    assert width is not None and height is not None
    assert len(raw) == height * (1 + width * 4)
    if index == 0:
        assert len(raw) == 18
    else:
        row_size = 1 + width * 4
        rgba = b''.join(raw[y * row_size + 1:(y + 1) * row_size]
                        for y in range(height))
        assert not any(rgba)
        assert hashlib.sha256(rgba).hexdigest() == (
            '0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f')
    print(f'audit_png={p} width={width} height={height} bytes={len(data)} '
          f'decompressed={len(raw)} status=PASS')
PY
