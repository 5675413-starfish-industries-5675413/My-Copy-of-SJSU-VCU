"""Abstraction Layer for SRE HIL testing platform."""

from .can_interface import CANInterface
from .dbc_loader import DBCLoader
from .param_injector import HILParamInjector, encode_name, decode_name
from .mcm import MCM
from .power_limit import PowerLimitDecoder, PowerLimitStatus, PowerLimitMonitor, PowerLimitController
from .efficiency import EfficiencyController, EfficiencyMonitor