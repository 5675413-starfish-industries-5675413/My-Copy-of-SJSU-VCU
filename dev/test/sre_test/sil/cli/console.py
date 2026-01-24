"""
Shared console instance for CLI commands.
"""

try:
    from rich.console import Console
    console = Console()
except ImportError:
    # Fallback if rich is not available
    class Console:
        def print(self, *args, **kwargs):
            print(*args, **kwargs)
    console = Console()

