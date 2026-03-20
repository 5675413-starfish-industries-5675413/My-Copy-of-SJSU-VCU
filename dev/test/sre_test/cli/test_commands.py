"""
Unified test CLI commands — `sre test <test_name> --backend sil|hil`.

Auto-discovers test functions from sre_test/tests/test_*.py and registers each
as a Click subcommand of `test_group`. Backend is passed through to pytest via
the --backend option defined in sre_test/tests/conftest.py.
"""

import inspect

import click
import pytest

from sre_test.core.path import SHARED_TESTS


def _discover_test_functions():
    """Find all test_* functions and their docstrings in sre_test/tests/."""
    test_info = {}
    for test_file in SHARED_TESTS.glob("test_*.py"):
        module_name = f"sre_test.tests.{test_file.stem}"
        try:
            module = __import__(module_name, fromlist=[""])
            for name, obj in inspect.getmembers(module, inspect.isfunction):
                if name.startswith("test_"):
                    docstring = inspect.getdoc(obj)
                    test_info[name] = docstring.strip() if docstring else f"Run {name}."
        except ImportError:
            continue
    return test_info


def _create_test_command(test_name, description):
    @click.command(name=test_name, help=description)
    @click.option(
        "--backend", "-b",
        default="sil",
        type=click.Choice(["sil", "hil"]),
        show_default=True,
        help="Test backend",
    )
    @click.option("-v", "--verbose", is_flag=True, help="Verbose output")
    def _cmd(backend, verbose):
        args = [str(SHARED_TESTS), "-k", test_name, "-s", f"--backend={backend}"]
        if verbose:
            args.append("-v")
        raise SystemExit(pytest.main(args))

    return _cmd


@click.group()
def test_group():
    """Run backend-agnostic tests against SIL or HIL."""
    pass


# Discover and register all tests at import time
for _name, _desc in _discover_test_functions().items():
    test_group.add_command(_create_test_command(_name, _desc))
