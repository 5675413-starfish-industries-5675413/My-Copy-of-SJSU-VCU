# Backward compatibility shim — DynamicConfig now lives in sre_test.core.config
from sre_test.core.config import DynamicConfig, configs_to_json  # noqa: F401

__all__ = ["DynamicConfig", "configs_to_json"]
