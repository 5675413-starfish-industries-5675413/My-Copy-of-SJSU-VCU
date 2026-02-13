"""
Standardized path utilities for SIL testing.
Provides common paths as global constants.
"""

from pathlib import Path

# Start from this file's location
# This file is at: dev/test/sre_test/sil/core/helpers/path.py
_helpers_dir = Path(__file__).parent  # dev/test/sre_test/sil/core/helpers/
_core_dir = _helpers_dir.parent  # dev/test/sre_test/sil/core/
_sil_dir = _core_dir.parent  # dev/test/sre_test/sil/
_sre_test_dir = _sil_dir.parent  # dev/test/sre_test/
_test_dir = _sre_test_dir.parent  # dev/test/
_dev_dir = _test_dir.parent  # dev/
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
CLI_TESTS = _sil_dir / 'cli' / 'tests'
DATA = _sil_dir / 'tests' / 'data'

# Common files
STRUCT_MEMBERS = _sil_dir / 'config' / 'struct_members_output.json'