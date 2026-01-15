"""
SRE Testing Platform CLI Entry Point
"""

import click

from sre_test.version import __version__
from sre_test.hil.cli.main import cli as hil_group

@click.group()
@click.version_option(version=__version__, prog_name='sre')

def sre():
    pass

# HIL commands subgroup
sre.add_command(hil_group, name='hil')

if __name__ == '__main__':
    sre()