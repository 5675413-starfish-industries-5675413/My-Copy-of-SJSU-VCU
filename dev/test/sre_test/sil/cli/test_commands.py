"""
Test-specific CLI commands that wrap pytest.
Auto-discovers test functions and creates CLI commands dynamically.
"""

import pytest
import click
import inspect
from pathlib import Path
from sre_test.sil.cli.console import console


def discover_test_functions():
    """
    Step 1: Find all test functions and their docstrings.
    Returns a dict mapping test names to their descriptions.
    """
    script_dir = Path(__file__).parent.parent
    test_dir = script_dir / 'core' / 'tests'
    
    test_info = {}  # {test_name: description}
    
    for test_file in test_dir.glob('test_*.py'):
        module_name = f"sre_test.sil.core.tests.{test_file.stem}"
        try:
            module = __import__(module_name, fromlist=[''])
            for name, obj in inspect.getmembers(module, inspect.isfunction):
                if name.startswith('test_'):
                    # Get the docstring from the test function
                    docstring = inspect.getdoc(obj)
                    if docstring:
                        # Use the docstring as description
                        test_info[name] = docstring.strip()
                    else:
                        # Fallback if no docstring
                        test_info[name] = f"Run {name} pytest test."
        except ImportError:
            continue
    
    return test_info


def create_test_command(test_name, description):
    """
    Step 2: Create a Click command for one test function.
    
    Args:
        test_name: The pytest test name (e.g., 'test_eff')
        description: The description/docstring for the test
    """
    @click.command(
        name=test_name,
        help=description  # Use the extracted docstring
    )
    @click.option('-v', '--verbose', is_flag=True, help='Verbose output')
    def test_command(verbose):
        script_dir = Path(__file__).parent.parent
        test_dir = script_dir / 'core' / 'tests'
        
        args = [str(test_dir), '-k', test_name, '-s']  # -s disables output capturing
        if verbose:
            args.append('-v')
        
        exit_code = pytest.main(args)
        raise SystemExit(exit_code)
    
    return test_command


# Step 3: Discover tests and create commands
TEST_INFO = discover_test_functions()  # Returns dict: {name: description}
TEST_COMMANDS = sorted(TEST_INFO.keys())  # Just the names for __all__

for test_name, description in TEST_INFO.items():
    command_func = create_test_command(test_name, description)
    globals()[test_name] = command_func

__all__ = TEST_COMMANDS