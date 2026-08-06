#!/usr/bin/env python3
"""Diagnostic-only BC/tile probe; native decoder is gated on visual proof."""

import struct
import sys
from pathlib import Path

from PIL import Image


def tiled_combine(outer_inner_bytes, bank, pipe, y_lsb):
    return ((y_lsb << 4) | (pipe << 6) | (bank << 11) |
            (outer_inner_bytes & 0xF) |
            (((outer_inner_bytes >> 4) & 1) << 5) |
            (((outer_inner_bytes >> 5) & 7) << 8) |
            ((outer_inner_bytes >> 8) << 12))


def tiled_2d(x, y, pitch, log2_bpp):
    outer = (((y >> 5) * (pitch >> 5) + (x >> 5)) << 6)
    inner = (((y >> 1) & 7) << 3) | (x & 7)
    outer_inner = (outer | inner) << log2_bpp
    bank = (y >> 4) & 1
    pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1)
    return tiled_combine(outer_inner, bank, pipe, y & 1)


def rgb565(value):
    return ((value >> 11) * 255 // 31,
            ((value >> 5) & 63) * 255 // 63,
            (value & 31) * 255 // 31)


def bc1_colors(block):
    c0, c1 = struct.unpack_from("<HH", block, 0)
    a, b = rgb565(c0), rgb565(c1)
    return [a, b,
            tuple((2 * a[i] + b[i]) // 3 for i in range(3)),
            tuple((a[i] + 2 * b[i]) // 3 for i in range(3))]


def decode_bc3(block):
    a0, a1 = block[0], block[1]
    alpha_bits = int.from_bytes(block[2:8], "little")
    alphas = [a0, a1]
    if a0 > a1:
        alphas += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        alphas += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)]
        alphas += [0, 255]
    colors = bc1_colors(block[8:16])
    color_bits = int.from_bytes(block[12:16], "little")
    result = []
    for i in range(16):
        rgb = colors[(color_bits >> (2 * i)) & 3]
        result.append((*rgb, alphas[(alpha_bits >> (3 * i)) & 7]))
    return result


def main():
    source, output, swap_mode = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    data = source.read_bytes()
    words = struct.unpack_from(">12I", data, 0x10)
    width, height = words[5] >> 16, words[5] & 0xFFFF
    start = 0x10 + words[8]
    blocks_x, blocks_y = width // 4, height // 4
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    decoded_blocks = 0
    skipped_blocks = 0
    for by in range(blocks_y):
        for bx in range(blocks_x):
            offset = start + tiled_2d(bx, by, blocks_x, 4)
            if offset < 0 or offset + 16 > len(data):
                skipped_blocks += 1
                continue
            block = bytearray(data[offset:offset + 16])
            if swap_mode == "swap16":
                for i in range(0, 16, 2):
                    block[i], block[i + 1] = block[i + 1], block[i]
            decoded = decode_bc3(block)
            decoded_blocks += 1
            for py in range(4):
                for px in range(4):
                    pixels[bx * 4 + px, by * 4 + py] = decoded[py * 4 + px]
    image.save(output)
    print(f"decoded_blocks={decoded_blocks} skipped_blocks={skipped_blocks}")


if __name__ == "__main__":
    main()
