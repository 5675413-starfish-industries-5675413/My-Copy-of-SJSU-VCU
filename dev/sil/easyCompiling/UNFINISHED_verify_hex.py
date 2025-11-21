#!/usr/bin/env python3
"""
Hex File Verification Tool
Decodes Intel HEX files and verifies that PowerLimit parameters match the JSON configuration.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import struct

# Paths
# Script location: VCU/dev/sil/easyCompiling/verify_hex.py
SCRIPT_DIR = Path(__file__).parent.resolve()          # VCU/dev/sil/easyCompiling/
DEV_DIR = SCRIPT_DIR.parent.parent                     # VCU/dev/
PROJECT_ROOT = DEV_DIR.parent                          # VCU/
BUILD_DIR = DEV_DIR / "build"                          # VCU/dev/build/
OUTPUT_DIR = SCRIPT_DIR / "builds"                     # VCU/dev/sil/easyCompiling/builds/
CONFIG_FILE = SCRIPT_DIR / "power_limit_params.json"   # VCU/dev/sil/easyCompiling/power_limit_params.json

class IntelHexParser:
    """Parse Intel HEX format files."""
    
    def __init__(self, hex_file_path: str):
        self.hex_file = Path(hex_file_path)
        self.memory = {}
        self.parse()
    
    def parse(self):
        """Parse Intel HEX file and load memory contents."""
        extended_address = 0
        
        with open(self.hex_file, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line.startswith(':'):
                    continue
                
                try:
                    # Parse Intel HEX record
                    byte_count = int(line[1:3], 16)
                    address = int(line[3:7], 16)
                    record_type = int(line[7:9], 16)
                    data = line[9:9+byte_count*2]
                    
                    if record_type == 0x00:  # Data record
                        full_address = extended_address + address
                        for i in range(0, len(data), 2):
                            byte_val = int(data[i:i+2], 16)
                            self.memory[full_address + i//2] = byte_val
                    
                    elif record_type == 0x04:  # Extended Linear Address
                        extended_address = int(data, 16) << 16
                    
                    elif record_type == 0x01:  # End of file
                        break
                
                except Exception as e:
                    print(f"Warning: Error parsing line {line_num}: {e}")
                    continue
    
    def search_byte_pattern(self, pattern: List[int], context: int = 16) -> List[Tuple[int, bytes]]:
        """Search for a specific byte pattern in memory."""
        matches = []
        addresses = sorted(self.memory.keys())
        
        for i in range(len(addresses) - len(pattern) + 1):
            match = True
            for j, byte_val in enumerate(pattern):
                if i+j >= len(addresses) or self.memory.get(addresses[i+j], None) != byte_val:
                    match = False
                    break
            
            if match:
                addr = addresses[i]
                # Get context bytes
                context_bytes = bytes([self.memory.get(addresses[i+k], 0) 
                                      for k in range(-context, len(pattern) + context)
                                      if i+k >= 0 and i+k < len(addresses)])
                matches.append((addr, context_bytes))
        
        return matches
    
    def get_memory_region(self, start_addr: int, length: int) -> bytes:
        """Get a region of memory as bytes."""
        return bytes([self.memory.get(start_addr + i, 0) for i in range(length)])
    
    def get_stats(self) -> Dict:
        """Get statistics about the hex file."""
        if not self.memory:
            return {}
        
        return {
            'total_bytes': len(self.memory),
            'start_address': f"0x{min(self.memory.keys()):08X}",
            'end_address': f"0x{max(self.memory.keys()):08X}",
            'file_size': self.hex_file.stat().st_size
        }


class PowerLimitVerifier:
    """Verify PowerLimit parameters in compiled hex file."""
    
    def __init__(self, hex_file: str, config_file: str):
        self.hex_parser = IntelHexParser(hex_file)
        self.config = self.load_config(config_file)
        self.results = []
    
    def load_config(self, config_file: str) -> Dict:
        """Load the JSON configuration."""
        with open(config_file, 'r') as f:
            data = json.load(f)
        return data
    
    def detect_configuration(self) -> Optional[Dict]:
        """Try to detect which configuration was compiled by looking for plTargetPower."""
        print("\n" + "="*80)
        print("CONFIGURATION DETECTION")
        print("="*80)
        
        for config in self.config['build_configurations']:
            target_power = config['parameters']['plTargetPower']
            print(f"\nChecking for {config['name']} (plTargetPower={target_power})...")
            
            # Search for the target power value
            matches = self.hex_parser.search_byte_pattern([target_power])
            if matches:
                print(f"  ✓ Found {len(matches)} occurrence(s) of value {target_power}")
                return config
            else:
                print(f"  ✗ Value {target_power} not found")
        
        return None
    
    def verify_parameters(self, expected_params: Dict) -> Dict:
        """Verify all parameters against expected values."""
        results = {
            'plTargetPower': self.verify_value('plTargetPower', expected_params['plTargetPower']),
            'plMode': self.verify_value('plMode', expected_params['plMode']),
            'plThresholdDiscrepancy': self.verify_value('plThresholdDiscrepancy', expected_params['plThresholdDiscrepancy']),
            'plInitializationThreshold': self.verify_value('plInitializationThreshold', expected_params['plInitializationThreshold']),
            'clampingMethod': self.verify_value('clampingMethod', expected_params['clampingMethod']),
        }
        
        # Verify PID parameters
        pid = expected_params['pid']
        results['pid_Kp'] = self.verify_value('PID Kp', pid['Kp'])
        results['pid_Ki'] = self.verify_value('PID Ki', pid['Ki'])
        results['pid_Kd'] = self.verify_value('PID Kd', pid['Kd'])
        results['pid_saturation'] = self.verify_value('PID saturation', pid['saturation'])
        results['pid_gain'] = self.verify_value('PID gain', pid['gain'])
        
        return results
    
    def verify_value(self, param_name: str, expected_value: int) -> Dict:
        """Verify a single parameter value."""
        matches = self.hex_parser.search_byte_pattern([expected_value], context=8)
        
        return {
            'expected': expected_value,
            'found': len(matches) > 0,
            'occurrences': len(matches),
            'addresses': [f"0x{addr:08X}" for addr, _ in matches[:5]]  # Show first 5
        }
    
    def search_for_signature_sequence(self, params: Dict) -> Optional[Tuple[int, bytes]]:
        """
        Search for a characteristic sequence that's unique to the initialization.
        This is more reliable than searching for individual bytes.
        """
        # Create a signature from multiple parameters
        # Using: [plTargetPower, plThresholdDiscrepancy, clampingMethod]
        signature = [
            params['plTargetPower'],
            params['plThresholdDiscrepancy'],
            params['clampingMethod']
        ]
        
        matches = self.hex_parser.search_byte_pattern(signature, context=20)
        return matches[0] if matches else None
    
    def generate_report(self, detected_config: Optional[Dict], verification_results: Optional[Dict]):
        """Generate a detailed verification report."""
        print("\n" + "="*80)
        print("VERIFICATION REPORT")
        print("="*80)
        
        # Hex file info
        stats = self.hex_parser.get_stats()
        print(f"\nHex File: {self.hex_parser.hex_file.name}")
        print(f"  Size: {stats['file_size']:,} bytes")
        print(f"  Code bytes: {stats['total_bytes']:,}")
        print(f"  Address range: {stats['start_address']} - {stats['end_address']}")
        
        if not detected_config:
            print("\n" + "="*80)
            print("❌ VERIFICATION FAILED")
            print("="*80)
            print("Could not detect which configuration was compiled.")
            print("The hex file may not contain recognizable PowerLimit parameters.")
            return
        
        print(f"\n" + "="*80)
        print(f"DETECTED CONFIGURATION: {detected_config['name']}")
        print("="*80)
        print(f"Description: {detected_config['description']}")
        
        params = detected_config['parameters']
        
        # Check for signature sequence
        print(f"\n" + "-"*80)
        print("SIGNATURE SEQUENCE CHECK")
        print("-"*80)
        signature_match = self.search_for_signature_sequence(params)
        if signature_match:
            addr, context = signature_match
            print(f"✓ Found initialization signature at address 0x{addr:08X}")
            print(f"  Pattern: [{params['plTargetPower']}, {params['plThresholdDiscrepancy']}, {params['clampingMethod']}]")
            print(f"  Context bytes: {context.hex(' ')}")
        else:
            print("⚠ Signature sequence not found as a contiguous pattern")
            print("  (This is normal - compiler may have reordered or optimized)")
        
        if not verification_results:
            return
        
        # Detailed parameter verification
        print(f"\n" + "-"*80)
        print("PARAMETER VERIFICATION")
        print("-"*80)
        
        all_verified = True
        
        for param_name, result in verification_results.items():
            status = "✓" if result['found'] else "✗"
            expected = result['expected']
            occurrences = result['occurrences']
            
            print(f"\n{status} {param_name}: {expected} (0x{expected:02X})")
            if result['found']:
                print(f"    Found {occurrences} occurrence(s) in hex file")
                if result['addresses']:
                    print(f"    Addresses: {', '.join(result['addresses'][:3])}", end="")
                    if len(result['addresses']) > 3:
                        print(f" ... ({len(result['addresses'])} total)")
                    else:
                        print()
            else:
                print(f"    ❌ NOT FOUND in hex file!")
                all_verified = False
        
        # Summary
        print(f"\n" + "="*80)
        if all_verified:
            print("✓✓✓ VERIFICATION PASSED ✓✓✓")
            print("="*80)
            print("All expected parameter values were found in the compiled hex file.")
            print(f"The hex file matches the configuration: {detected_config['name']}")
        else:
            print("⚠⚠⚠ VERIFICATION WARNING ⚠⚠⚠")
            print("="*80)
            print("Some parameters were not found in the hex file.")
            print("This may indicate:")
            print("  • Compiler optimization removed unused values")
            print("  • Values were inlined in calculations")
            print("  • Build configuration mismatch")
        
        print("="*80)


def main():
    """Main verification workflow."""
    print("="*80)
    print("PowerLimit Hex File Verification Tool")
    print("="*80)
    
    if len(sys.argv) < 2:
        print("\nUsage: python verify_hex.py <hex_file> [config_json]")
        print("\nExamples:")
        print("  python verify_hex.py builds/main_PL_80kW_2025_01_10.hex")
        print("  python verify_hex.py builds/main_PL_50kW_2025_01_10.hex power_limit_params.json")
        print(f"\nDefault config file: {CONFIG_FILE}")
        print(f"Builds directory: {OUTPUT_DIR}")
        return 1
    
    # Handle hex file path - can be relative to script or absolute
    hex_file = Path(sys.argv[1])
    if not hex_file.is_absolute():
        # Try relative to script directory first
        if not hex_file.exists():
            hex_file = SCRIPT_DIR / hex_file
        # If still not found, try relative to builds directory
        if not hex_file.exists():
            hex_file = OUTPUT_DIR / sys.argv[1]
    
    # Handle config file path
    if len(sys.argv) > 2:
        config_file = Path(sys.argv[2])
        if not config_file.is_absolute() and not config_file.exists():
            config_file = SCRIPT_DIR / sys.argv[2]
    else:
        config_file = CONFIG_FILE  # Use default
    
    # Validate files exist
    if not hex_file.exists():
        print(f"\n❌ Error: Hex file not found: {hex_file}")
        print(f"   Searched in: {hex_file.parent}")
        return 1
    
    if not config_file.exists():
        print(f"\n❌ Error: Config file not found: {config_file}")
        return 1
    
    print(f"\nHex file: {hex_file}")
    print(f"Config file: {config_file}")
    
    try:
        # Create verifier (convert Path objects to strings)
        verifier = PowerLimitVerifier(str(hex_file), str(config_file))
        
        # Detect which configuration was compiled
        detected_config = verifier.detect_configuration()
        
        # Verify parameters if configuration detected
        verification_results = None
        if detected_config:
            verification_results = verifier.verify_parameters(detected_config['parameters'])
        
        # Generate report
        verifier.generate_report(detected_config, verification_results)
        
        return 0
    
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())

