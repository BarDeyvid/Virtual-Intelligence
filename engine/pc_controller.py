# pc_controller.py
"""
PC controller – a tiny wrapper around common OS commands.
All functions are pure and side‑effect free except the actual execution
which is performed in an executor to keep the event loop responsive.
"""

import os
import platform
import subprocess
from typing import Any, Dict
import asyncio

class PCController:
    def __init__(self):
        self.os_name = platform.system()

    # ------------------------------------------------------------------
    def _open_browser(self, url: str = "https://www.google.com"):
        return ["xdg-open", url] if self.os_name != "Windows" else ["start", "", url]

    def _play_music(self, file_path: str):
        # assumes mpg123 or similar is installed
        return ["mpg123", "-q", file_path]

    def _clear_screen(self):
        if self.os_name == "Windows":
            return ["cmd", "/c", "cls"]
        else:
            return ["clear"]   

    def _shutdown(self):
        if self.os_name == "Windows":
            return ["shutdown", "/s", "/t", "0"]
        else:
            # Linux/macOS
            return ["sudo", "shutdown", "-h", "now"]

    # ------------------------------------------------------------------
    async def execute(self, action: str, **kwargs) -> Dict[str, Any]:
        """
        Public async API. Supported actions:
            - open_browser
            - play_music
            - clear_screen
            - shutdown
        kwargs are passed to the underlying command builder.
        Returns a dict with `success`, `output` and `error`.
        """
        try:
            cmd_builder = getattr(self, f"_{action}")
        except AttributeError:
            return {"success": False, "error": f"Unknown action: {action}"}

        cmd_args = cmd_builder(**kwargs)
        if not isinstance(cmd_args, list):
            # In case the builder returns a string
            cmd_args = [cmd_args]

        proc = await asyncio.create_subprocess_exec(
            *cmd_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        out, err = await proc.communicate()
        return {
            "success": proc.returncode == 0,
            "output": out.decode().strip(),
            "error": err.decode().strip() if err else None,
        }
