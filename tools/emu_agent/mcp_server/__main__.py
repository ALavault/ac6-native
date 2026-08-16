"""Command-line entry point for ``python -m tools.emu_agent.mcp_server``."""

from .server import serve_stdio


if __name__ == "__main__":
    serve_stdio()
