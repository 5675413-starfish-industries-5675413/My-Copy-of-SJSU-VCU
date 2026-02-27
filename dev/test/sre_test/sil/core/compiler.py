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
            sil_inc_dir: Path to SIL stub directory (dev/test/sre_test/sil/inc/)
        """
        # Auto-detect paths relative to this file
        self.script_dir = Path(__file__).parent.parent  # dev/test/sre_test/sil/
        self.sil_inc_dir = sil_inc_dir or self.script_dir / 'inc'

        # Navigate up to find dev/ and inc/
        sil_dir = self.script_dir  # dev/test/sre_test/sil/
        sre_test_dir = sil_dir.parent  # dev/test/sre_test/
        test_dir = sre_test_dir.parent  # dev/test/
        # test_dir.parent is dev/, parent of that is VCU root
        self.root_dir = test_dir.parent.parent  # VCU root
        self.dev_dir = dev_dir or self.root_dir / 'dev'
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
            'watchdog.c', 'mathFunctions.c', 'sil.c',
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

        found = [f for f in sources if f.exists()]
        missing = [f for f in sources if not f.exists()]
        
        if missing:
            print(f"Warning: {len(missing)} source files not found:")
            for f in missing:
                print(f"  - {f}")

        return found

    def get_compiler_flags(self) -> List[str]:
        """Get GCC compiler flags."""
        flags = [
            '-std=c11',  # Use C11 standard to avoid C23's built-in bool keyword
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
        return flags

    def get_include_paths(self) -> List[str]:
        """Get include paths (SIL headers take precedence)."""
        paths = [
            f'-I{self.sil_inc_dir.resolve()}',
            f'-I{self.inc_dir.resolve()}',
            f'-I{self.dev_dir.resolve()}',
        ]
        return paths

    def _get_compiler(self) -> str:
        """Get platform-appropriate compiler command."""
        import shutil
        
        # Try to find GCC
        compiler_names = ['gcc', 'gcc.exe']
        for compiler in compiler_names:
            if shutil.which(compiler):
                return compiler
        return 'gcc'  # Default, will fail with clear error if not found
    
    def compile(self, verbose: bool = False) -> bool:
        """
        Compile the SIL executable.

        Returns:
            True if compilation succeeded, False otherwise
        """
        import platform
        
        self.build_dir.mkdir(parents=True, exist_ok=True)

        sources = [str(f.resolve()) for f in self.get_source_files()]
        compiler = self._get_compiler()
        
        # Build command
        cmd = (
            [compiler] +
            self.get_compiler_flags() +
            self.get_include_paths() +
            sources +
            ['-o', str(self.executable.resolve())]
        )
        
        # Platform-specific library linking
        if platform.system() == 'Windows':
            cmd.append('-lws2_32')  # Winsock for select()
        cmd.append('-lm')  # Math library (works on both platforms)

        if verbose:
            print(f"Compiling {len(sources)} source files...")
            print(f"Dev dir: {self.dev_dir}")
            print(f"Inc dir: {self.inc_dir}")
            print(f"SIL inc dir: {self.sil_inc_dir}")
            print(f"Build dir: {self.build_dir}")
            print(f"Executable: {self.executable}")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        except FileNotFoundError:
            print(f"Error: Compiler '{compiler}' not found")
            return False
        except subprocess.TimeoutExpired:
            print("Compilation timed out")
            return False

        if result.returncode != 0:
            # Always print errors, even if verbose=False
            print(f"\n{'='*60}")
            print("COMPILATION FAILED")
            print(f"{'='*60}")
            if result.stdout:
                print("STDOUT:")
                print(result.stdout)
            if result.stderr:
                print("STDERR:")
                print(result.stderr)
            print(f"{'='*60}\n")
            return False

        if verbose:
            print(f"Compilation successful! Executable: {self.executable}")

        return self.executable.exists()

