#!/usr/bin/env python3
"""
Main CLI entry point for SRE HIL testing platform.

This module provides the command-line interface for Hardware-in-the-Loop
testing of the VCU firmware.
"""

import click
import time
import platform
from pathlib import Path
from typing import Dict, Optional, Any
from dataclasses import dataclass
from rich.console import Console
from rich.table import Table
from rich.live import Live

from sre_test.hil.core.can_interface import CANInterface
from sre_test.hil.core.dbc_loader import DBCLoader

__version__ = "0.1.0"  # TODO: Use shared version from pyproject.toml

console = Console()

@dataclass
class SignalState:
    """ Tracks state of watched signal """
    name: str                               # CAN signal name
    message_name: str                       # CAN parent message name
    message_id: int                         # CAN ID
    unit: str = "-"                         # e.g., RPM, Nm...
    value: Optional[float] = None           # Decoded value
    last_update: Optional[float] = None     # Timestamp of last update

    @property
    def age_ms(self) -> Optional[int]:
        """ Returns age (in ms) since last update """
        if self.last_update is None:
            return None
        return int((time.time() - self.last_update) * 1000)

@click.group()
@click.version_option(version=__version__, prog_name='sre-hil')
@click.option('--dbc', type=click.Path(exists=True), help='Override DBC file path')
@click.pass_context
def cli(ctx, dbc):
    """
    HIL Testing Platform for Spartan Racing Electric VCU

    Hardware-in-the-Loop testing framework for CAN communication,
    signal injection, and automated testing.

    Examples:
        hil listen --duration 10
        hil listen --decode
        hil --dbc dbcs/10.03.25_LC_Main.dbc listen
    """
    ctx.ensure_object(dict)
    ctx.obj['dbc_override'] = dbc


@cli.command()
@click.option('--duration', type=float, help='Listen duration in seconds')
@click.option('--decode', is_flag=True, help='Decode messages using DBC')
@click.option('--filter', 'filter_id', help='Filter by CAN ID (hex, e.g., 0x50B)')
@click.option('--message', help='Filter by message name')
@click.pass_context
def listen(ctx, duration, decode, filter_id, message):
    """
    Listen to CAN bus traffic.

    Monitor raw or decoded CAN messages on the bus. This is the basic
    BUSMASTER message monitoring equivalent.

    Examples:
        hil listen --duration 30
        hil listen --decode --filter 0x50B
        hil listen --message VCU_LC_Status_A --decode
    """
    console.print(f"[bold green]SRE HIL v{__version__}[/bold green] - CAN Bus Listener")
    console.print()

    # Initialize CAN interface
    try:
        can_interface = CANInterface()
        console.print(f"[green]✓[/green] Connected to {can_interface.channel_info}")
    except Exception as e:
        console.print(f"[yellow]⚠[/yellow] CAN hardware not available: {e}")
        console.print("[dim]Running in simulation mode (no actual CAN traffic)[/dim]")
        console.print()
        console.print("[cyan]To connect PCAN hardware:[/cyan]")
        console.print("  1. Connect PCAN USB interface")
        console.print("  2. Configure config/can.ini")
        console.print("  3. Run this command again")
        return

    # Load DBC if decode requested
    dbc_loader = None
    if decode:
        dbc_path = ctx.obj.get('dbc_override')
        try:
            dbc_loader = DBCLoader(dbc_path)
            console.print(f"[green]✓[/green] Loaded DBC: {dbc_loader.dbc_name}")
        except Exception as e:
            console.print(f"[red]✗[/red] Failed to load DBC: {e}")
            return

    console.print()
    console.print(f"[cyan]Listening...[/cyan] (Press Ctrl+C to stop)")
    if duration:
        console.print(f"[dim]Duration: {duration} seconds[/dim]")
    console.print()

    # Listen loop
    start_time = time.time()
    message_count = 0
    filtered_message_count = 0

    try:
        while True:
            # Check duration
            if duration and (time.time() - start_time) >= duration:
                break

            # Receive message (with timeout)
            msg = can_interface.receive(timeout=0.1)
            if msg is None:
                continue

            message_count += 1

            # Apply filters
            if filter_id:
                filter_val = int(filter_id, 16) if filter_id.startswith('0x') else int(filter_id)
                if msg.arbitration_id != filter_val:
                    continue
                filtered_message_count += 1

            # Display message
            if decode and dbc_loader:
                try:
                    decoded = dbc_loader.decode_message(msg)
                    if message:
                        if decoded['name'] != message:
                            continue
                    console.print(f"[blue]{decoded['name']}[/blue] (0x{msg.arbitration_id:03X})")
                    for signal_name, signal_value in decoded['signals'].items():
                        console.print(f"  {signal_name}: {signal_value}")
                except Exception as e:
                    # Message not in DBC, show raw
                    console.print(f"[dim]0x{msg.arbitration_id:03X}: {msg.data.hex()}[/dim]")
            else:
                # Raw display
                console.print(f"0x{msg.arbitration_id:03X} [{msg.dlc}] {msg.data.hex()}")

    except KeyboardInterrupt:
        console.print()
        console.print("[yellow]Stopped by user[/yellow]")

    finally:
        can_interface.shutdown()
        console.print()
        console.print(f"[green]✓[/green] Received {message_count} messages")
        if filter_id:
            console.print(f"[green]✓[/green] {filtered_message_count} messages matched filter 0x{filter_val:03X}")
        if duration:
            elapsed = time.time() - start_time
            console.print(f"[dim]Elapsed time: {elapsed:.1f}s[/dim]")

@cli.command('signal-watch')
@click.option('--signal', 
              '-s', 
              'signals', 
              multiple=True, 
              required=True, 
              help='Specify signal(s) to watch')
@click.option('--refresh-rate', 
              '-r',
              type=float,
              default=10.0,
              help='Specify display refresh rate in Hz (default: 10.0)')
@click.option('--duration',
              '-d',
              type=float,
              help='Specify watch duration in seconds')
@click.pass_context
def signal_watch(ctx, signals, refresh_rate, duration):
    # TODO
    return

@cli.command()
@click.argument('name')
@click.argument('value', type=int)
@click.pass_context
def inject(ctx, name, value):
    """
    Inject a parameter value into the VCU.

    Examples:
        hil inject pltog 1
        hil inject plmode 2
        hil inject pltarg 80
    """
    from sre_test.hil.core import HILParamInjector

    try:
        can_interface = CANInterface()
        console.print(f"[green]✓[/green] Connected to {can_interface.channel_info}")
    except Exception as e:
        console.print(f"[red]✗[/red] CAN hardware not available: {e}")
        return

    try:
        injector = HILParamInjector(can_interface)
        result = injector.inject(name, value)

        if result:
            console.print(f"[green]✓[/green] {name} <- {value}")
        else:
            console.print(f"[red]✗[/red] Failed to send {name} <- {value}")
    finally:
        can_interface.shutdown()

@cli.command()
def version():
    """Show version information."""
    console.print(f"[bold]sre-hil[/bold] version {__version__}")
    console.print()
    console.print("Spartan Racing Electric - Formula SAE")
    console.print("Hardware-in-the-Loop Testing Platform for VCU")


@cli.command()
@click.option('--interface', default='pcan', help='CAN interface type (default: pcan)')
@click.option('--channel', default='PCAN_USBBUS1', help='CAN channel (default: PCAN_USBBUS1)')
@click.option('--bitrate', default=500000, type=int, help='CAN bitrate in bps (default: 500000)')
@click.option('--force', is_flag=True, help='Overwrite existing config file')
def init(interface, channel, bitrate, force):
    """
    Initialize CAN configuration for your system.

    Creates the python-can config file in your home directory with
    the specified settings. This file is used by the HIL platform
    to connect to your CAN hardware.

    Examples:
        hil init
        hil init --interface socketcan --channel vcan0
        hil init --force
    """
    # Determine config file path based on OS
    home = Path.home()
    if platform.system() == 'Windows':
        config_path = home / 'can.conf'
    else:
        config_path = home / '.canrc'

    # Check if file already exists
    if config_path.exists() and not force:
        console.print(f"[yellow]⚠[/yellow] Config file already exists: {config_path}")
        console.print()
        console.print("Current contents:")
        console.print(f"[dim]{config_path.read_text()}[/dim]")
        console.print()
        console.print("Use [bold]--force[/bold] to overwrite.")
        return

    # Generate config content
    config_content = f"""\
# SRE HIL Platform - CAN Configuration
# Generated by: hil init
#
# This file configures the python-can library for the HIL test bench.
# Edit these values to match your CAN hardware setup.
#
# Common interfaces:
#   pcan      - PEAK PCAN-USB adapter (macOS, Windows, Linux)
#   socketcan - Linux SocketCAN (vcan0 for virtual, can0 for real)
#   virtual   - No hardware, for testing only

[default]
interface = {interface}
channel = {channel}
bitrate = {bitrate}
"""

    # Write the config file
    try:
        config_path.write_text(config_content)
        console.print(f"[green]✓[/green] Created CAN config: {config_path}")
        console.print()
        console.print("[cyan]Configuration:[/cyan]")
        console.print(f"  interface = {interface}")
        console.print(f"  channel   = {channel}")
        console.print(f"  bitrate   = {bitrate}")
        console.print()
        console.print("[dim]Edit this file to change your CAN settings.[/dim]")
    except Exception as e:
        console.print(f"[red]✗[/red] Failed to create config: {e}")


if __name__ == '__main__':
    cli()
