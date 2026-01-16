"""
SIL Simulator - Runs the compiled executable and handles JSON communication.
"""

import subprocess
import json
import threading
import queue
from pathlib import Path
from typing import Optional, Dict, Any, Iterator


class SILSimulator:
    """Runs the SIL executable and handles stdin/stdout JSON communication."""

    def __init__(self, executable: Path):
        self.executable = executable
        self.process: Optional[subprocess.Popen] = None
        self.stdout_queue: queue.Queue = queue.Queue()
        self._reader_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    def start(self) -> None:
        """Start the SIL executable."""
        if not self.executable.exists():
            raise FileNotFoundError(f"Executable not found: {self.executable}")

        self.process = subprocess.Popen(
            [str(self.executable.resolve())],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            bufsize=1,
        )

        # Start stdout reader thread
        self._reader_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader_thread.start()

    def _read_stdout(self) -> None:
        """Background thread to read JSON from stdout."""
        while not self._stop_event.is_set() and self.process and self.process.poll() is None:
            try:
                line = self.process.stdout.readline()
                if not line:
                    break
                line = line.strip()
                if line:
                    try:
                        self.stdout_queue.put(json.loads(line))
                    except json.JSONDecodeError:
                        pass
            except Exception as e:
                if not self._stop_event.is_set():
                    print(f"Error reading stdout: {e}")
                break

    def send(self, data: Dict[str, Any]) -> None:
        """Send JSON data to the simulator via stdin."""
        if self.process and self.process.poll() is None:
            json_str = json.dumps(data, separators=(',', ':'))
            self.process.stdin.write(json_str + '\n')
            self.process.stdin.flush()

    def receive(self, timeout: float = 2.0) -> Optional[Dict[str, Any]]:
        """Receive JSON response from simulator."""
        try:
            return self.stdout_queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def stop(self) -> None:
        """Stop the simulator."""
        self._stop_event.set()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                if self.process:
                    self.process.kill()
                    self.process.wait()

    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.stop()

