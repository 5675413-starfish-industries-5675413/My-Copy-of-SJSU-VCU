"""
Standardized path utilities for SIL testing.
Provides common paths as global constants.
"""

from pathlib import Path

# Start from this file's location
# This file is at: sre_test/sil/core/paths.py
_core_dir = Path(__file__).parent  # sre_test/sil/core/
_sil_dir = _core_dir.parent  # sre_test/sil/
_sre_test_dir = _sil_dir.parent  # sre_test/
_test_dir = _sre_test_dir.parent  # test/
_dev_dir = _test_dir.parent / 'dev'  # dev/
_root_dir = _dev_dir.parent  # VCU root/

# Global path constants
ROOT = _root_dir
DEV = _dev_dir
INC = _root_dir / 'inc'
TEST = _test_dir
SRE_TEST = _sre_test_dir
SIL = _sil_dir
CORE = _core_dir

# SIL-specific directories
CONFIG = _sil_dir / 'config'
TESTS = _sil_dir / 'core' / 'tests'
DATA = _sil_dir / 'tests' / 'data'

# Common files
STRUCT_MEMBERS = _sil_dir / 'config' / 'struct_members_output.json'