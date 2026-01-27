"""
SIL CLI Commands
"""

import json
import subprocess
from pathlib import Path

import click

from sre_test.sil.cli.console import console
from sre_test.sil.cli.efficiency_commands import efficiency
from sre_test.sil.core.compiler import SILCompiler
from sre_test.sil.core.simulator import SILSimulator
from sre_test.sil.core.struct_parser import extract_struct_definitions
import sre_test.sil.cli.test_commands as test_cmds


@click.group()
def cli():
    """
    Software-in-the-Loop Testing for VCU

    Compile and run VCU firmware in simulation mode.
    """
    pass

# Register commands from other modules
cli.add_command(efficiency)

# Automatically register all discovered test commandsimport sre_test.sil.cli.test_commands as test_cmds
for test_name in test_cmds.TEST_COMMANDS:
    test_command = getattr(test_cmds, test_name)
    cli.add_command(test_command)

@cli.command()
@click.option('--verbose', '-v', is_flag=True, help='Show compilation output')
@click.option('--output-mode', type=str, default='0x01', help='SIL output mode (0x01=efficiency, 0x07=all)')
def build(verbose, output_mode):
    """Compile the SIL executable."""
    # Parse output mode
    try:
        sil_output_mode = int(output_mode, 0)  # Allow hex (0x01) or decimal (1)
    except ValueError:
        console.print(f"[red]Invalid output mode: {output_mode}[/red]")
        raise SystemExit(1)

    console.print("[cyan]Compiling SIL executable...[/cyan]")

    compiler = SILCompiler(sil_output_mode=sil_output_mode)
    if compiler.compile(verbose=verbose):
        console.print(f"[green]Success![/green] Executable: {compiler.executable}")
    else:
        console.print("[red]Compilation failed[/red]")
        raise SystemExit(1)


@cli.command()
@click.option('--output', '-o', type=click.Path(), help='Output JSON file')
def extract(output):
    """Extract struct definitions from C headers."""
    # Determine paths
    script_dir = Path(__file__).parent.parent  # dev/test/sre_test/sil/
    sre_test_dir = script_dir.parent  # dev/test/sre_test/
    test_dir = sre_test_dir.parent  # dev/test/
    dev_dir = test_dir.parent / 'dev'
    root_dir = dev_dir.parent
    inc_dir = root_dir / 'inc'
    
    # Determine output file
    if output:
        output_file = Path(output)
    else:
        config_dir = script_dir / 'config'
        config_dir.mkdir(exist_ok=True)
        output_file = config_dir / 'struct_members_output.json'

    console.print("[cyan]Extracting struct definitions...[/cyan]")
    
    try:
        extract_struct_definitions(dev_dir, inc_dir, output_file)
        console.print(f"[green]Done![/green] Output written to {output_file}")
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        raise SystemExit(1)


@cli.command()
@click.option('--config', '-c', type=click.Path(exists=True), help='Config JSON file')
@click.option('--timeout', '-t', type=float, default=2.0, help='Run timeout in seconds')
def run(config, timeout):
    """Run a single SIL simulation."""
    compiler = SILCompiler()
    if not compiler.executable.exists():
        console.print("[yellow]Executable not found. Building...[/yellow]")
        if not compiler.compile():
            console.print("[red]Build failed[/red]")
            raise SystemExit(1)

    console.print("[cyan]Starting SIL simulation...[/cyan]")
    
    with SILSimulator(compiler.executable) as sim:
        # Load config if provided
        if config:
            with open(config, 'r') as f:
                config_data = json.load(f)
                sim.send(config_data)
                console.print(f"[green]Sent config: {config}[/green]")
        
        # Wait for and display output
        response = sim.receive(timeout=timeout)
        if response:
            console.print("[green]Response:[/green]")
            console.print(json.dumps(response, indent=2))
        else:
            console.print("[yellow]No response received within timeout[/yellow]")
