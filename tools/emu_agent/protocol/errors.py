"""Errors raised by the emu-agent JSON protocol.

The protocol deliberately has one failure mode for malformed or unsupported
input: :class:`ValidationError`.  Callers can therefore fail closed without
having to guess whether a partially decoded document is safe to execute.
"""

from __future__ import annotations

from typing import Any


class ProtocolError(ValueError):
    """Base class for protocol and runner contract errors."""


class ValidationError(ProtocolError):
    """A request or result does not satisfy its versioned contract."""

    def __init__(self, message: str, *, path: str = "$") -> None:
        self.path = path
        self.reason = message
        super().__init__(f"{path}: {message}")


class UnsupportedVersionError(ValidationError):
    """The document declares a protocol version this implementation rejects."""


def fail(message: str, *, path: str = "$") -> Any:
    """Raise a typed validation error (handy in expression-oriented checks)."""

    raise ValidationError(message, path=path)

