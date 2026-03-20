"""
SRE Testing Platform CLI Entry Point
"""

import click

from sre_test.version import __version__
from sre_test.hil.cli.main import cli as hil_group
from sre_test.sil.cli.main import cli as sil_group
from sre_test.cli.test_commands import test_group

@click.group()
@click.version_option(version=__version__, prog_name='sre')

def sre():
    pass

# HIL commands subgroup
sre.add_command(hil_group, name='hil')

# SIL commands subgroup
sre.add_command(sil_group, name='sil')

# Unified test commands subgroup
sre.add_command(test_group, name='test')

if __name__ == '__main__':
    sre()