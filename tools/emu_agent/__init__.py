"""Small, dependency-free helpers for the AC6 emulator-agent boundary.

The package intentionally contains no emulator, shell, or memory transport.  It
only describes a bounded, macro-level protocol, deterministic simulator, and
fail-closed Xenia Edge identity/transport seam.
"""

__all__ = ["artifacts", "backends", "exploration", "identity", "mcp_server", "protocol", "runner"]
