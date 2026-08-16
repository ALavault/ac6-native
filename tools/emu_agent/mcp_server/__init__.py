"""MCP stdio façade for bounded emulator-agent operations."""

from .server import (
    ALLOWED_TOOLS,
    DENIED_OPERATIONS,
    EmuMcpServer,
    MCPServer,
    dispatch,
    serve_stdio,
)
from .manifest import build_ac6_work_manifest, build_work_manifest

__all__ = [
    "ALLOWED_TOOLS",
    "DENIED_OPERATIONS",
    "EmuMcpServer",
    "MCPServer",
    "dispatch",
    "serve_stdio",
    "build_work_manifest",
    "build_ac6_work_manifest",
]
