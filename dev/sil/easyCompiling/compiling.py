#!/usr/bin/env python3
"""
Automated PowerLimit Build Script
Reads configurations from power_limit_params.json and builds multiple
firmware hex files with different power limit settings.
"""

import json
import os
import shutil
import subprocess
import sys
import re
from pathlib import Path
from datetime import datetime

# Paths
# Script location: VCU/dev/sil/easyCompiling/compiling.py
SCRIPT_DIR = Path(__file__).parent.resolve()          # VCU/dev/sil/easyCompiling/
DEV_DIR = SCRIPT_DIR.parent.parent                     # VCU/dev/
PROJECT_ROOT = DEV_DIR.parent                          # VCU/
POWERLIMIT_C = DEV_DIR / "powerLimit.c"                # VCU/dev/powerLimit.c
BUILD_DIR = DEV_DIR / "build"                          # VCU/dev/build/
OUTPUT_DIR = SCRIPT_DIR / "builds"                     # VCU/dev/sil/easyCompiling/builds/
CONFIG_FILE = SCRIPT_DIR / "power_limit_params.json"   # VCU/dev/sil/easyCompiling/power_limit_params.json
BACKUP_FILE = DEV_DIR / "powerLimit.c.backup"          # VCU/dev/powerLimit.c.backup

def load_configurations():
    """Load build configurations from JSON file."""
    print(f"Loading configurations from: {CONFIG_FILE}")
    with open(CONFIG_FILE, 'r') as f:
        data = json.load(f)
    return data['build_configurations']

def backup_original_file():
    """Backup the original powerLimit.c file."""
    print(f"Backing up original file to: {BACKUP_FILE}")
    shutil.copy2(POWERLIMIT_C, BACKUP_FILE)

def restore_original_file():
    """Restore the original powerLimit.c file."""
    print(f"Restoring original file from backup")
    if BACKUP_FILE.exists():
        shutil.copy2(BACKUP_FILE, POWERLIMIT_C)
        BACKUP_FILE.unlink()

def modify_powerlimit_params(params):
    """
    Modify the POWERLIMIT_new() function in powerLimit.c with new parameters.
    Uses //STARTOFPARAMETERS and //ENDOFPARAMETERS markers for reliable parsing.
    """
    print(f"  Modifying powerLimit.c with new parameters...")
    
    with open(POWERLIMIT_C, 'r') as f:
        lines = f.readlines()
    
    # Find the markers
    start_idx = None
    end_idx = None
    
    for i, line in enumerate(lines):
        if '//STARTOFPARAMETERS' in line:
            start_idx = i
        elif '//ENDOFPARAMETERS' in line:
            end_idx = i
            break
    
    if start_idx is None or end_idx is None:
        raise Exception(
            "Could not find //STARTOFPARAMETERS and //ENDOFPARAMETERS markers in powerLimit.c\n"
            "Please add these comments around the POWERLIMIT_new() function."
        )
    
    # Extract the parameter section
    param_section = ''.join(lines[start_idx:end_idx+1])
    
    # Now modify the parameters within this section
    pid = params['pid']
    
    # Replace PID variables (Kp, Ki, Kd, saturationValue, scalefactor)
    param_section = re.sub(
        r'(sbyte1\s+Kp\s*=\s*)\d+;',
        f'\\g<1>{pid["Kp"]};',
        param_section,
        count=1
    )
    param_section = re.sub(
        r'(sbyte1\s+Ki\s*=\s*)\d+;',
        f'\\g<1>{pid["Ki"]};',
        param_section,
        count=1
    )
    param_section = re.sub(
        r'(sbyte1\s+Kd\s*=\s*)\d+;',
        f'\\g<1>{pid["Kd"]};',
        param_section,
        count=1
    )
    param_section = re.sub(
        r'(sbyte2\s+saturationValue\s*=\s*)\d+;',
        f'\\g<1>{pid["saturation"]};',
        param_section,
        count=1
    )
    param_section = re.sub(
        r'(sbyte2\s+scalefactor\s*=\s*)\d+;',
        f'\\g<1>{pid["gain"]};',
        param_section,
        count=1
    )
    
    # Replace plMode
    param_section = re.sub(
        r'(me->plMode\s*=\s*)\d+;',
        f'\\g<1>{params["plMode"]};',
        param_section,
        count=1
    )
    
    # Replace plTargetPower - this is the key one!
    param_section = re.sub(
        r'(me->plTargetPower\s*=\s*)\d+;',
        f'\\g<1>{params["plTargetPower"]};',
        param_section,
        count=1
    )
    
    # Replace plThresholdDiscrepancy
    param_section = re.sub(
        r'(me->plThresholdDiscrepancy\s*=\s*)\d+;',
        f'\\g<1>{params["plThresholdDiscrepancy"]};',
        param_section,
        count=1
    )
    
    # Replace plInitializationThreshold
    param_section = re.sub(
        r'(me->plInitializationThreshold\s*=\s*)\d+;',
        f'\\g<1>{params["plInitializationThreshold"]};',
        param_section,
        count=1
    )
    
    # Replace clampingMethod
    param_section = re.sub(
        r'(me->clampingMethod\s*=\s*)\d+;',
        f'\\g<1>{params["clampingMethod"]};',
        param_section,
        count=1
    )
    
    # Replace plAlwaysOn
    always_on_value = "TRUE" if params["plAlwaysOn"] else "FALSE"
    param_section = re.sub(
        r'(me->plAlwaysOn\s*=\s*)(TRUE|FALSE);',
        f'\\g<1>{always_on_value};',
        param_section,
        count=1
    )
    
    # Reconstruct the file
    new_lines = lines[:start_idx] + [param_section] + lines[end_idx+1:]
    
    with open(POWERLIMIT_C, 'w') as f:
        f.writelines(new_lines)
    
    print(f"    plTargetPower: {params['plTargetPower']}")
    print(f"    plMode: {params['plMode']}")
    print(f"    clampingMethod: {params['clampingMethod']}")
    print(f"    PID: Kp={pid['Kp']}, Ki={pid['Ki']}, Kd={pid['Kd']}, sat={pid['saturation']}, gain={pid['gain']}")

def run_build():
    """Run make clean and make to build the firmware."""
    print(f"  Running build process...")
    
    # Change to dev directory
    original_dir = os.getcwd()
    os.chdir(DEV_DIR)
    
    try:
        # Clean previous build
        print(f"    Running 'make clean'...")
        result = subprocess.run(['make', 'clean'], 
                              capture_output=True, 
                              text=True,
                              shell=True)
        if result.returncode != 0:
            print(f"    Warning: make clean returned {result.returncode}")
        
        # Build
        print(f"    Running 'make'...")
        result = subprocess.run(['make'], 
                              capture_output=True, 
                              text=True,
                              shell=True,
                              timeout=120)
        
        if result.returncode != 0:
            print(f"    ERROR: Build failed with return code {result.returncode}")
            print(f"    STDOUT: {result.stdout}")
            print(f"    STDERR: {result.stderr}")
            return False
        
        print(f"    Build completed successfully!")
        return True
        
    except subprocess.TimeoutExpired:
        print(f"    ERROR: Build timed out after 120 seconds")
        return False
    except Exception as e:
        print(f"    ERROR: Build failed: {e}")
        return False
    finally:
        os.chdir(original_dir)

def copy_hex_file(config_name):
    """Copy the built hex file to the output directory with a unique name."""
    source_hex = BUILD_DIR / "main.hex"
    # source_s19 = BUILD_DIR / "main.s19"
    # source_elf = BUILD_DIR / "main.elf"
    
    if not source_hex.exists():
        print(f"    ERROR: main.hex not found at {source_hex}")
        return False
    
    # Create output directory if it doesn't exist
    OUTPUT_DIR.mkdir(exist_ok=True)
    
    # Create timestamped filename
    timestamp = datetime.now().strftime("%Y_%m_%d")
    dest_hex = OUTPUT_DIR / f"main_{config_name}_{timestamp}.hex"
    # dest_s19 = OUTPUT_DIR / f"main_{config_name}_{timestamp}.s19"
    # dest_elf = OUTPUT_DIR / f"main_{config_name}_{timestamp}.elf"
    
    # Also create a "latest" version without timestamp
    # dest_hex_latest = OUTPUT_DIR / f"main_{config_name}_latest.hex"
    # dest_s19_latest = OUTPUT_DIR / f"main_{config_name}_latest.s19"
    # dest_elf_latest = OUTPUT_DIR / f"main_{config_name}_latest.elf"
    
    # Copy files
    print(f"  Copying build artifacts to output directory...")
    shutil.copy2(source_hex, dest_hex)
    # shutil.copy2(source_hex, dest_hex_latest)
    print(f"    Saved: {dest_hex.name}")
    
    # if source_s19.exists():
    #     shutil.copy2(source_s19, dest_s19)
    #     shutil.copy2(source_s19, dest_s19_latest)
    #     print(f"    Saved: {dest_s19.name}")
    
    # if source_elf.exists():
    #     shutil.copy2(source_elf, dest_elf)
    #     shutil.copy2(source_elf, dest_elf_latest)
    #     print(f"    Saved: {dest_elf.name}")
    
    return True

def main():
    """Main build automation workflow."""
    print("=" * 80)
    print("PowerLimit Automated Build System")
    print("=" * 80)
    print()
    
    try:
        # Load configurations
        configs = load_configurations()
        print(f"Loaded {len(configs)} build configuration(s)\n")
        
        # Backup original file
        backup_original_file()
        print()
        
        # Track results
        successful_builds = []
        failed_builds = []
        
        # Build each configuration
        for idx, config in enumerate(configs, 1):
            print("-" * 80)
            print(f"BUILD {idx}/{len(configs)}: {config['name']}")
            print(f"Description: {config['description']}")
            print("-" * 80)
            
            try:
                # Modify parameters
                modify_powerlimit_params(config['parameters'])
                
                # Build
                if run_build():
                    # Copy output files
                    if copy_hex_file(config['name']):
                        successful_builds.append(config['name'])
                        print(f"✓ {config['name']} completed successfully!")
                    else:
                        failed_builds.append(config['name'])
                        print(f"✗ {config['name']} failed: Could not copy hex file")
                else:
                    failed_builds.append(config['name'])
                    print(f"✗ {config['name']} failed: Build error")
                
            except Exception as e:
                failed_builds.append(config['name'])
                print(f"✗ {config['name']} failed with exception: {e}")
            
            print()
        
        # Restore original file
        restore_original_file()
        
        # Print summary
        print("=" * 80)
        print("BUILD SUMMARY")
        print("=" * 80)
        print(f"Total builds: {len(configs)}")
        print(f"Successful: {len(successful_builds)}")
        print(f"Failed: {len(failed_builds)}")
        print()
        
        if successful_builds:
            print("✓ Successful builds:")
            for name in successful_builds:
                print(f"  - {name}")
            print()
        
        if failed_builds:
            print("✗ Failed builds:")
            for name in failed_builds:
                print(f"  - {name}")
            print()
        
        print(f"Output directory: {OUTPUT_DIR}")
        print("=" * 80)
        
        return 0 if not failed_builds else 1
        
    except KeyboardInterrupt:
        print("\n\nBuild process interrupted by user")
        restore_original_file()
        return 130
    except Exception as e:
        print(f"\n\nFATAL ERROR: {e}")
        import traceback
        traceback.print_exc()
        restore_original_file()
        return 1

if __name__ == "__main__":
    sys.exit(main())

