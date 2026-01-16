# SIL Integration Refactoring Guide

This document outlines how to integrate the Software-in-the-Loop (SIL) system into the comprehensive CLI at `dev/test/`.

## Current State

### What's Done
- [x] Directory structure created (`dev/test/sre_test/sil/`)
- [x] `inc (retained)/` directory moved (contains C hardware stubs)
- [x] `build_main_sil (retained)/` retained as reference (contains working Python code)
- [x] `pycparser` dependency added to `pyproject.toml`
- [x] `__init__.py` files created for `cli/`, `core/`, `tests/`

### Directory Structure
```
dev/test/sre_test/sil/
├── build_main_sil (retained)/   # REFERENCE - original working code (delete after migration)
│   ├── test_main.py             # Main compilation/run logic
│   ├── test_closed_loop.py      # Closed-loop efficiency testing
│   ├── extract_struct_members.py  # C header parsing
│   ├── parse_values.c/.h        # JSON-to-struct parser (C code)
│   ├── plot_results.py          # Result visualization
│   ├── json_files/              # Config files
│   └── csv_files/               # Test data
├── inc (retained)/              # C hardware stubs (keep as-is)
│   ├── IO_CAN_sil.c
│   ├── IO_ADC_sil.c
│   ├── cJSON.c/.h
│   └── ...
├── cli/                         # TO CREATE - Click commands
│   ├── __init__.py              # ✓ Created
│   └── main.py
├── core/                        # TO CREATE - Business logic
│   ├── __init__.py              # ✓ Created
│   ├── compiler.py
│   ├── struct_parser.py
│   └── simulator.py
├── config/                      # TO CREATE - JSON configs
│   ├── PL_config.json
│   ├── MCM_config.json
│   └── TorqueEncoder_config.json
└── tests/                       # TO CREATE - Pytest tests
    ├── __init__.py              # ✓ Created
    └── test_efficiency.py
```

---

## Step 1: Create Core Modules

### 1.1 `core/compiler.py`
Encapsulates the GCC compilation logic from `build_main_sil (retained)/test_main.py:214-349`.

```python
"""
SIL Compiler - Compiles VCU source code into a native executable.
"""

import subprocess
from pathlib import Path
from typing import List, Optional

class SILCompiler:
    """Compiles VCU C code with SIL stubs into a native executable."""

    def __init__(self,
                 dev_dir: Optional[Path] = None,
                 inc_dir: Optional[Path] = None,
                 sil_inc_dir: Optional[Path] = None):
        """
        Initialize compiler with source directories.

        Args:
            dev_dir: Path to dev/ directory containing VCU source files
            inc_dir: Path to inc/ directory containing VCU headers
            sil_inc_dir: Path to SIL stub directory (dev/test/sre_test/sil/inc (retained)/)
        """
        # Auto-detect paths relative to this file
        self.script_dir = Path(__file__).parent.parent  # dev/test/sre_test/sil/
        self.sil_inc_dir = sil_inc_dir or self.script_dir / 'inc (retained)'

        # Navigate up to find dev/ and inc/
        sil_dir = self.script_dir  # dev/test/sre_test/sil/
        sre_test_dir = sil_dir.parent  # dev/test/sre_test/
        test_dir = sre_test_dir.parent  # dev/test/
        self.dev_dir = dev_dir or test_dir.parent  # dev/
        self.root_dir = self.dev_dir.parent  # VCU root
        self.inc_dir = inc_dir or self.root_dir / 'inc'

        # Build output
        self.build_dir = self.script_dir / 'build'
        self.executable = self.build_dir / self._get_exe_name()

    def _get_exe_name(self) -> str:
        """Get platform-appropriate executable name."""
        import platform
        if platform.system() == 'Windows':
            return 'main.exe'
        return 'main'

    def get_source_files(self) -> List[Path]:
        """Get list of all source files needed for compilation."""
        # VCU source files
        vcu_sources = [
            'main.c', 'initializations.c', 'sensors.c', 'canManager.c',
            'motorController.c', 'instrumentCluster.c', 'readyToDriveSound.c',
            'torqueEncoder.c', 'brakePressureSensor.c', 'wheelSpeeds.c',
            'safety.c', 'sensorCalculations.c', 'serial.c', 'cooling.c',
            'bms.c', 'LaunchControl.c', 'regen.c', 'drs.c', 'powerLimit.c',
            'PID.c', 'efficiency.c', 'hashTable.c', 'avlTree.c',
            'watchdog.c', 'mathFunctions.c',
        ]

        # SIL stub files
        sil_sources = [
            'cstart_sil.c', 'IO_Driver_sil.c', 'IO_RTC_sil.c', 'IO_UART_sil.c',
            'IO_ADC_sil.c', 'IO_CAN_sil.c', 'IO_DIO_sil.c', 'IO_PWM_sil.c',
            'IO_EEPROM_sil.c', 'IO_POWER_sil.c', 'IO_PWD_sil.c',
            'IO_WDTimer_sil.c', 'DIAG_Functions_sil.c', 'cJSON.c',
        ]

        sources = []
        sources.extend(self.dev_dir / f for f in vcu_sources)
        sources.extend(self.sil_inc_dir / f for f in sil_sources)

        # Add parse_values.c from build_main_sil (retained)/
        parse_values = self.script_dir / 'build_main_sil (retained)' / 'parse_values.c'
        if parse_values.exists():
            sources.append(parse_values)

        return [f for f in sources if f.exists()]

    def get_compiler_flags(self) -> List[str]:
        """Get GCC compiler flags."""
        return [
            '-O2', '-Wall',
            '-Wno-unused-variable',
            '-Wno-main',
            '-Wno-pointer-sign',
            '-Wno-incompatible-pointer-types',
            '-Wno-format-overflow',
            '-Wno-array-bounds',
            '-Wno-unused-function',
            '-Wno-implicit-function-declaration',
            '-Wno-builtin-declaration-mismatch',
            '-DRTS_TTC_FLASH_DATE_YEAR=2025',
            '-DRTS_TTC_FLASH_DATE_MONTH=1',
            '-DRTS_TTC_FLASH_DATE_DAY=1',
            '-DRTS_TTC_FLASH_DATE_HOUR=0',
            '-DRTS_TTC_FLASH_DATE_MINUTE=0',
            '-DSIL_BUILD',
        ]

    def get_include_paths(self) -> List[str]:
        """Get include paths (SIL headers take precedence)."""
        paths = [
            f'-I{self.sil_inc_dir.resolve()}',
            f'-I{self.inc_dir.resolve()}',
            f'-I{self.dev_dir.resolve()}',
        ]
        # Also include build_main_sil (retained) for parse_values.h
        parse_values_dir = self.script_dir / 'build_main_sil (retained)'
        if parse_values_dir.exists():
            paths.append(f'-I{parse_values_dir.resolve()}')
        return paths

    def compile(self, verbose: bool = False) -> bool:
        """
        Compile the SIL executable.

        Returns:
            True if compilation succeeded, False otherwise
        """
        self.build_dir.mkdir(exist_ok=True)

        sources = [str(f.resolve()) for f in self.get_source_files()]
        cmd = (
            ['gcc'] +
            self.get_compiler_flags() +
            self.get_include_paths() +
            sources +
            ['-o', str(self.executable.resolve())] +
            ['-lm']  # Math library
        )

        if verbose:
            print(f"Compiling {len(sources)} source files...")

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)

        if result.returncode != 0:
            if verbose:
                print(f"Compilation failed:\n{result.stderr}")
            return False

        return self.executable.exists()
```

### 1.2 `core/struct_parser.py`
Move logic from `build_main_sil (retained)/extract_struct_members.py`.

```python
"""
Struct Parser - Extracts struct definitions from C headers using pycparser.
"""

from pathlib import Path
import json
# Import and refactor the logic from build_main_sil (retained)/extract_struct_members.py
# Key functions to port:
#   - find_struct_definitions()
#   - StructVisitor class
#   - format_output()
#   - main() -> extract_all()

def extract_struct_definitions(dev_dir: Path, inc_dir: Path, output_file: Path) -> dict:
    """
    Extract all struct definitions from C headers.

    Args:
        dev_dir: Path to dev/ directory
        inc_dir: Path to inc/ directory
        output_file: Path to write struct_members_output.json

    Returns:
        Dictionary of struct definitions
    """
    # Port logic from extract_struct_members.py
    pass
```

### 1.3 `core/simulator.py`
Encapsulates running the SIL executable and JSON I/O.

```python
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
        while not self._stop_event.is_set() and self.process.poll() is None:
            line = self.process.stdout.readline()
            if not line:
                break
            line = line.strip()
            if line:
                try:
                    self.stdout_queue.put(json.loads(line))
                except json.JSONDecodeError:
                    pass

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
                self.process.kill()
                self.process.wait()
```

---

## Step 2: Create CLI Commands

### 2.1 `cli/main.py`

```python
"""
SIL CLI Commands
"""

import click
from pathlib import Path
from rich.console import Console

console = Console()

@click.group()
def cli():
    """
    Software-in-the-Loop Testing for VCU

    Compile and run VCU firmware in simulation mode.
    """
    pass


@cli.command()
@click.option('--verbose', '-v', is_flag=True, help='Show compilation output')
def build(verbose):
    """Compile the SIL executable."""
    from sre_test.sil.core.compiler import SILCompiler

    console.print("[cyan]Compiling SIL executable...[/cyan]")

    compiler = SILCompiler()
    if compiler.compile(verbose=verbose):
        console.print(f"[green]Success![/green] Executable: {compiler.executable}")
    else:
        console.print("[red]Compilation failed[/red]")
        raise SystemExit(1)


@cli.command()
@click.option('--output', '-o', type=click.Path(), help='Output JSON file')
def extract(output):
    """Extract struct definitions from C headers."""
    from sre_test.sil.core.struct_parser import extract_struct_definitions

    console.print("[cyan]Extracting struct definitions...[/cyan]")
    # TODO: Implement
    console.print("[green]Done![/green]")


@cli.command()
@click.option('--config', '-c', type=click.Path(exists=True), help='Config JSON file')
@click.option('--timeout', '-t', type=float, default=2.0, help='Run timeout in seconds')
def run(config, timeout):
    """Run a single SIL simulation."""
    from sre_test.sil.core.compiler import SILCompiler
    from sre_test.sil.core.simulator import SILSimulator

    compiler = SILCompiler()
    if not compiler.executable.exists():
        console.print("[yellow]Executable not found. Building...[/yellow]")
        if not compiler.compile():
            console.print("[red]Build failed[/red]")
            raise SystemExit(1)

    console.print("[cyan]Starting SIL simulation...[/cyan]")
    sim = SILSimulator(compiler.executable)
    sim.start()

    # TODO: Load config, send to simulator, display results

    sim.stop()


@cli.command()
@click.argument('csv_file', type=click.Path(exists=True))
@click.option('--output', '-o', type=click.Path(), help='Output CSV file')
@click.option('--config', '-c', type=click.Path(exists=True), help='PowerLimit config')
def efficiency(csv_file, output, config):
    """
    Run closed-loop efficiency test with CSV data.

    Reads motor data from CSV, sends to SIL simulator row-by-row,
    and collects efficiency results.
    """
    console.print(f"[cyan]Running efficiency test with {csv_file}...[/cyan]")
    # TODO: Port logic from test_closed_loop.py
    console.print("[green]Done![/green]")


@cli.command()
@click.option('-k', 'expression', help='Only run tests matching expression')
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
def test(expression, verbose):
    """Run SIL pytest tests."""
    import subprocess

    cmd = ['pytest', 'sre_test/sil/tests/']
    if expression:
        cmd.extend(['-k', expression])
    if verbose:
        cmd.append('-v')

    subprocess.run(cmd)
```

### 2.2 Register SIL commands in main CLI

Edit `dev/test/sre_test/cli/main.py`:

```python
"""
SRE Testing Platform CLI Entry Point
"""

import click

from sre_test.version import __version__
from sre_test.hil.cli.main import cli as hil_group
from sre_test.sil.cli.main import cli as sil_group  # ADD THIS

@click.group()
@click.version_option(version=__version__, prog_name='sre')
def sre():
    pass

# HIL commands subgroup
sre.add_command(hil_group, name='hil')

# SIL commands subgroup  # ADD THIS
sre.add_command(sil_group, name='sil')

if __name__ == '__main__':
    sre()
```

---

## Step 3: Move Config and Data Files

1. Copy JSON configs from `build_main_sil (retained)/json_files/` to `config/`:
   ```bash
   cp "build_main_sil (retained)/json_files/PL_config.json" config/
   cp "build_main_sil (retained)/json_files/MCM_config.json" config/
   cp "build_main_sil (retained)/json_files/TorqueEncoder_config.json" config/
   ```

2. Decide where CSV test data should live (suggest `tests/data/`)

3. The `parse_values.c` and `parse_values.h` files are currently referenced from
   `build_main_sil (retained)/`. Options after migration:
   - Option A: Keep referencing from `build_main_sil (retained)/` (current approach)
   - Option B: Copy to `core/c_src/` subdirectory
   - Option C: Move to `inc (retained)/`

---

## Step 4: Testing

After implementation, verify:

```bash
# Install the package in dev mode
cd dev/test
pip install -e .

# Test CLI commands
sre sil --help
sre sil build --verbose
sre sil extract
sre sil run --config config/PL_config.json
sre sil efficiency "build_main_sil (retained)/csv_files/eff_data.csv" -o results.csv
sre sil test -v
```

---

## Important Notes

### System Requirements
- GCC or Clang compiler must be installed
- On Windows: MinGW or MSYS2
- On macOS: Xcode Command Line Tools (`xcode-select --install`)
- On Linux: `build-essential` package

### Handling "(retained)" Directory Names
The `(retained)` suffix in directory names requires proper quoting in shell commands
and careful path handling in Python. The `pathlib.Path` API handles spaces and
special characters correctly, so use `Path` objects rather than string concatenation.

### Cleanup
After migration is complete and tested:
```bash
rm -rf "dev/test/sre_test/sil/build_main_sil (retained)/"
```

Note: Keep `inc (retained)/` as it contains the C stubs needed for compilation.

---

## Reference: Original File Mapping

| Original Location | New Location |
|-------------------|--------------|
| `dev/sil/inc/*` | `dev/test/sre_test/sil/inc (retained)/*` |
| `dev/sil/build_main_sil/test_main.py` | `dev/test/sre_test/sil/core/compiler.py` + `cli/main.py` |
| `dev/sil/build_main_sil/test_closed_loop.py` | `dev/test/sre_test/sil/core/simulator.py` + `cli/main.py` |
| `dev/sil/build_main_sil/extract_struct_members.py` | `dev/test/sre_test/sil/core/struct_parser.py` |
| `dev/sil/build_main_sil/parse_values.c/.h` | `dev/test/sre_test/sil/build_main_sil (retained)/parse_values.c/.h` |
| `dev/sil/build_main_sil/json_files/*.json` | `dev/test/sre_test/sil/config/*.json` |
| `dev/sil/build_main_sil/csv_files/*` | `dev/test/sre_test/sil/tests/data/*` |
