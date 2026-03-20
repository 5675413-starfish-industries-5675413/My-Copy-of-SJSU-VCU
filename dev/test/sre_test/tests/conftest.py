"""
Shared pytest configuration for backend-agnostic (unified) tests.

Pass --backend sil (default) or --backend hil to select the test backend.
The 'env' fixture yields a ready-to-use TestEnvironment for the chosen backend.
"""

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--backend",
        action="store",
        default="sil",
        choices=["sil", "hil"],
        help="Test backend: 'sil' (compiled binary) or 'hil' (real hardware via CAN)",
    )


@pytest.fixture(scope="session")
def env(request):
    """Session-scoped TestEnvironment for the selected backend."""
    backend = request.config.getoption("--backend")

    if backend == "sil":
        from sre_test.sil.core.sil_environment import SILEnvironment
        environment = SILEnvironment()
    else:
        from sre_test.hil.core.hil_environment import HILEnvironment
        environment = HILEnvironment()

    environment.setup()
    yield environment
    environment.teardown()
