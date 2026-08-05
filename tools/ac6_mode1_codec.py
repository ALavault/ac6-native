#!/usr/bin/env python3
"""Bounded PAL AC6 mode-1 DATA.TBL payload descrambler and inflater."""

from __future__ import annotations

import struct
import zlib
from functools import lru_cache


_PAD_COUNT = 256
_PI_WORDS_NEEDED = 2 * (_PAD_COUNT - 1) + 3


@lru_cache(maxsize=1)
def _pi_fractional_words(nwords: int) -> tuple[int, ...]:
    """Return fractional pi as big-endian base-2^32 words."""
    bits = 32 * nwords + 64

    def arctan_inv(inverse: int) -> int:
        total = 0
        term = (1 << bits) // inverse
        inverse_squared = inverse * inverse
        k = 0
        sign = 1
        while term // (2 * k + 1):
            total += sign * (term // (2 * k + 1))
            term //= inverse_squared
            k += 1
            sign = -sign
        return total

    pi_scaled = 16 * arctan_inv(5) - 4 * arctan_inv(239)
    fractional = pi_scaled - (3 << bits)
    return tuple(
        (fractional >> (bits - 32 * (index + 1))) & 0xFFFFFFFF
        for index in range(nwords)
    )


@lru_cache(maxsize=_PAD_COUNT)
def pad_for_index(index: int) -> bytes:
    """Return the eight-byte retail descramble pad for a table index."""
    if index < 0:
        raise ValueError("negative DATA.TBL index")
    words = _pi_fractional_words(_PI_WORDS_NEEDED)
    first_word = 2 * (index & 0xFF) + 1
    return struct.pack(">II", words[first_word], words[first_word + 1])


def descramble(data: bytes, index: int) -> bytes:
    pad = pad_for_index(index)
    return bytes(value ^ pad[offset & 7] for offset, value in enumerate(data))


def decompress_entry(data: bytes, index: int, expected_size: int | None = None) -> bytes:
    """Descramble one on-disc mode-1 blob and inflate its raw DEFLATE stream."""
    raw = descramble(data, index)
    decoded = zlib.decompress(raw, wbits=-15)
    if expected_size is not None and len(decoded) != expected_size:
        raise ValueError(
            f"decompressed size mismatch: got {len(decoded)}, expected {expected_size}"
        )
    return decoded
