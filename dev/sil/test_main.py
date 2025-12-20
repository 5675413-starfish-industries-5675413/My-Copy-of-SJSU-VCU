"""
Pytest test to run main.c
=========================
Compiles main.c with SIL mock headers and runs it.
"""

import pytest
import subprocess
import os
from pathlib import Path

# Get paths
script_dir = Path(__file__).parent
sil_dir = script_dir
dev_dir = script_dir.parent
root_dir = dev_dir.parent
inc_dir = root_dir / 'inc'
sil_inc_dir = sil_dir / 'inc'
build_dir = sil_dir / 'build_main_sil'
main_exe = build_dir / 'main.exe'


def test_run_main():
    """Compile main.c with SIL headers and run it."""
    # Check if gcc is available
    try:
        subprocess.run(['gcc', '--version'], capture_output=True, timeout=5, check=True)
    except (FileNotFoundError, subprocess.TimeoutExpired, subprocess.CalledProcessError):
        pytest.skip("gcc not available")
    
    # Create build directory
    build_dir.mkdir(exist_ok=True)
    
    # List of all source files needed for main.c
    source_files = [
        dev_dir / 'main.c',
        dev_dir / 'initializations.c',
        dev_dir / 'sensors.c',
        dev_dir / 'canManager.c',
        dev_dir / 'motorController.c',
        dev_dir / 'instrumentCluster.c',
        dev_dir / 'readyToDriveSound.c',
        dev_dir / 'torqueEncoder.c',
        dev_dir / 'brakePressureSensor.c',
        dev_dir / 'wheelSpeeds.c',
        dev_dir / 'safety.c',
        dev_dir / 'sensorCalculations.c',
        dev_dir / 'serial.c',
        dev_dir / 'cooling.c',
        dev_dir / 'bms.c',
        dev_dir / 'LaunchControl.c',
        dev_dir / 'regen.c',
        dev_dir / 'drs.c',
        dev_dir / 'powerLimit.c',
        dev_dir / 'PID.c',
        dev_dir / 'efficiency.c',
        dev_dir / 'hashTable.c',
        dev_dir / 'avlTree.c',
        dev_dir / 'watchdog.c',
        dev_dir / 'mathFunctions.c',
        # SIL stub files
        sil_inc_dir / 'cstart_sil.c',  # Stub for _cstart function
        sil_inc_dir / 'sensors_sil_stubs.c',
        sil_inc_dir / 'IO_RTC_sil.c',
        sil_inc_dir / 'IO_UART_sil.c',
        sil_inc_dir / 'IO_ADC_sil.c',
        sil_inc_dir / 'IO_CAN_sil.c',
        sil_inc_dir / 'IO_DIO_sil.c',
        sil_inc_dir / 'IO_PWM_sil.c',
        sil_inc_dir / 'IO_EEPROM_sil.c',
        sil_inc_dir / 'IO_POWER_sil.c',
        sil_inc_dir / 'IO_PWD_sil.c',
        sil_inc_dir / 'IO_WDTimer_sil.c',
        sil_inc_dir / 'DIAG_Functions_sil.c',
    ]
    
    # Filter out files that don't exist
    existing_sources = [str(f) for f in source_files if f.exists()]
    
    if not existing_sources:
        pytest.skip("No source files found")
    
    # Compiler flags
    # Suppress warnings that are expected for embedded code compiled for PC
    cflags = [
        '-O2', 
        '-Wall', 
        '-Wno-unused-variable',
        '-Wno-main',  # Allow void main() for embedded code
        '-Wno-pointer-sign',  # Suppress char* vs ubyte1* warnings
        '-Wno-incompatible-pointer-types',  # Suppress pointer type warnings
        '-Wno-format-overflow',  # Suppress format overflow warnings
        '-Wno-array-bounds',  # Suppress array bounds warnings
        '-Wno-unused-function',  # Suppress unused function warnings
        '-Wno-implicit-function-declaration',  # Suppress implicit function declaration warnings
        '-Wno-builtin-declaration-mismatch',  # Suppress builtin declaration mismatch
        '-lm'
    ]
    # Include SIL headers first so they override real headers
    includes = [
        f'-I{sil_inc_dir}',  # SIL headers take precedence (checked first)
        f'-I{inc_dir}',
        f'-I{dev_dir}',
    ]
    
    # Compile main.c with all dependencies
    try:
        result = subprocess.run(
            ['gcc'] + cflags + includes + existing_sources + ['-o', str(main_exe)],
            cwd=str(sil_dir),
            capture_output=True,
            text=True,
            timeout=120
        )
        
        if result.returncode != 0:
            pytest.fail(f"Compilation failed:\nSTDOUT: {result.stdout}\nSTDERR: {result.stderr}")
            
    except subprocess.TimeoutExpired:
        pytest.fail("Compilation timed out")
    except Exception as e:
        pytest.fail(f"Compilation error: {e}")
    
    # Check if executable was created
    if not main_exe.exists():
        pytest.fail("Compilation completed but executable not found")
    
    # Run main.c with timeout (it has an infinite loop)
    try:
        subprocess.run(
            [str(main_exe)],
            timeout=2.0,
            cwd=str(build_dir)
        )
    except subprocess.TimeoutExpired:
        # Expected - main.c runs forever
        pass
    except Exception as e:
        pytest.fail(f"Failed to run main.c: {e}")


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
