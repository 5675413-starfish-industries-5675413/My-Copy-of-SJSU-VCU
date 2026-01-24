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
    Step 1: Find all test functions.
    Scans test files and returns list of test function names.
    """
    script_dir = Path(__file__).parent.parent
    test_dir = script_dir / 'core' / 'tests'
    
    test_functions = []
    
    for test_file in test_dir.glob('test_*.py'):
        module_name = f"sre_test.sil.core.tests.{test_file.stem}"
        try:
            module = __import__(module_name, fromlist=[''])
            for name, obj in inspect.getmembers(module, inspect.isfunction):
                if name.startswith('test_'):
                    test_functions.append(name)
        except ImportError:
            continue
    
    return sorted(set(test_functions))


def create_test_command(test_name):
    """
    Step 2: Create a Click command for one test function.
    Takes a test name and returns a Click command function.
    """
    @click.command(name=test_name)
    @click.option('-v', '--verbose', is_flag=True, help='Verbose output')
    def test_command(verbose):
        """Run {test_name} pytest test."""
        script_dir = Path(__file__).parent.parent
        test_dir = script_dir / 'core' / 'tests'
        
        args = [str(test_dir), '-k', test_name]
        if verbose:
            args.append('-v')
        
        exit_code = pytest.main(args)
        raise SystemExit(exit_code)
    
    return test_command


# Step 3: Combine them - discover tests, then create commands for each
TEST_COMMANDS = discover_test_functions()  # Find tests

for test_name in TEST_COMMANDS:  # Create command for each test
    command_func = create_test_command(test_name)
    globals()[test_name] = command_func

__all__ = TEST_COMMANDS