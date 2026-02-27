# config.py
"""
Dynamic configuration classes that auto-discover parameters from C struct definitions.
Shared by both SIL and HIL backends.
"""
from typing import Dict, Any, Optional, Set, List
from pathlib import Path
import json
from sre_test.core.path import STRUCT_MEMBERS


class DynamicConfig:
    """Base class for configs that auto-loads from struct definitions."""

    # Cache of valid struct names to avoid reloading JSON multiple times
    _valid_struct_names: Optional[Set[str]] = None

    @classmethod
    def _get_valid_struct_names(cls) -> Set[str]:
        """Get the set of valid struct names from the JSON file."""
        if cls._valid_struct_names is not None:
            return cls._valid_struct_names

        valid_names = set()

        if not STRUCT_MEMBERS.exists():
            cls._valid_struct_names = valid_names
            return valid_names

        try:
            with open(STRUCT_MEMBERS, 'r') as f:
                structs = json.load(f)

            # Extract struct names from the JSON array
            if isinstance(structs, list):
                for struct in structs:
                    if isinstance(struct, dict) and 'name' in struct:
                        valid_names.add(struct['name'])

            cls._valid_struct_names = valid_names
        except Exception:
            # If file doesn't exist or parsing fails, allow any name (backward compatibility)
            pass

        return valid_names

    @classmethod
    def _validate_struct_name(cls, struct_name: str) -> None:
        """Validate that the struct name exists in the JSON file."""
        # If file doesn't exist, warn but allow (backward compatibility)
        if not STRUCT_MEMBERS.exists():
            import warnings
            warnings.warn(
                f"Struct members file not found at {STRUCT_MEMBERS}. "
                f"Cannot validate struct name '{struct_name}'. "
                f"Run 'sre sil extract' to generate the struct definitions file.",
                UserWarning
            )
            return

        valid_names = cls._get_valid_struct_names()

        # If file exists but is empty, raise an error
        if not valid_names:
            raise ValueError(
                f"Struct members file exists but is empty. "
                f"Cannot validate struct name '{struct_name}'. "
                f"Run 'sre sil extract' to populate the struct definitions file."
            )

        # Validate against known struct names
        if struct_name not in valid_names:
            # Create a helpful error message with available struct names
            available = ', '.join(sorted(valid_names)) if valid_names else 'none'
            raise ValueError(
                f"Invalid struct name '{struct_name}'. "
                f"Valid struct names are: {available}"
            )

    def __init__(self, struct_name: str, default_values: Optional[Dict[str, Any]] = None):
        # Validate struct name before proceeding
        self._validate_struct_name(struct_name)

        self.struct_name = struct_name
        self._values = default_values or {}
        self._valid_parameters: Set[str] = set()  # Store valid parameter names for this struct
        self._load_struct_definition()

    def _load_struct_definition(self):
        """Load struct definition from struct_members_output.json."""
        if not STRUCT_MEMBERS.exists():
            return

        try:
            with open(STRUCT_MEMBERS, 'r') as f:
                structs = json.load(f)

            # Find our struct
            for struct in structs:
                if struct.get('name') == self.struct_name:
                    # Store valid parameter names
                    parameters = struct.get('parameters', {})
                    self._valid_parameters = set(parameters.keys())

                    # Initialize with default values for all members
                    for member_name in self._valid_parameters:
                        if member_name not in self._values:
                            # Set default to None (can be overridden)
                            self._values[member_name] = None
                    break
        except Exception:
            # If file doesn't exist or parsing fails, continue with provided values
            pass

    def _validate_parameter_name(self, param_name: str) -> None:
        """Validate that the parameter name exists in the struct definition."""
        # If no valid parameters loaded (file doesn't exist or struct not found), allow any
        if not self._valid_parameters:
            return

        if param_name not in self._valid_parameters:
            # Create a helpful error message with available parameter names
            available = ', '.join(sorted(self._valid_parameters)) if self._valid_parameters else 'none'
            raise ValueError(
                f"Invalid parameter name '{param_name}' for struct '{self.struct_name}'. "
                f"Valid parameter names are: {available}"
            )

    def set(self, key: str, value: Any) -> 'DynamicConfig':
        """Set a parameter value. Returns self for chaining."""
        self._validate_parameter_name(key)
        self._values[key] = value
        return self

    def get(self, key: str, default: Any = None) -> Any:
        """Get a parameter value."""
        return self._values.get(key, default)

    def to_json_dict(self) -> Dict[str, Any]:
        """Convert to JSON dict for C side."""
        # Filter out None values (optional - remove if you want to send all)
        parameters = {k: v for k, v in self._values.items() if v is not None}

        return {
            "name": self.struct_name,
            "parameters": parameters
        }

    def __getattr__(self, name: str):
        """Allow attribute-style access: config.plTargetPower"""
        if name.startswith('_') or name in ['struct_name', 'to_json_dict', 'set', 'get', '_load_struct_definition', '_validate_struct_name', '_get_valid_struct_names']:
            return super().__getattribute__(name)
        return self._values.get(name)

    def __setattr__(self, name: str, value: Any):
        """Allow attribute-style setting: config.plTargetPower = 50"""
        # Always allow setting internal attributes (starting with _) and struct_name
        if name.startswith('_') or name in ['struct_name']:
            super().__setattr__(name, value)
        else:
            # Initialize _values and _valid_parameters if they don't exist yet
            if not hasattr(self, '_values'):
                super().__setattr__('_values', {})
            if not hasattr(self, '_valid_parameters'):
                super().__setattr__('_valid_parameters', set())

            # Validate parameter name if _valid_parameters is already populated
            if hasattr(self, '_valid_parameters') and self._valid_parameters:
                self._validate_parameter_name(name)

            self._values[name] = value


def configs_to_json(*configs: DynamicConfig, output_file: Optional[Path] = None, include_none: bool = False) -> List[Dict[str, Any]]:
    # Validate all configs
    for config in configs:
        if not isinstance(config, DynamicConfig):
            raise TypeError(f"Expected DynamicConfig, got {type(config).__name__}")

    # Determine output file
    if output_file is None:
        output_file = STRUCT_MEMBERS

    # Read existing struct_members_output.json if it exists
    existing_structs = []
    if output_file.exists():
        try:
            with open(output_file, 'r', encoding='utf-8') as f:
                existing_structs = json.load(f)
                if not isinstance(existing_structs, list):
                    existing_structs = []
        except Exception:
            existing_structs = []

    # Create a map of struct names to their definitions (preserve original order)
    struct_map = {}
    struct_order = []
    for struct in existing_structs:
        if isinstance(struct, dict) and 'name' in struct:
            struct_name = struct['name']
            struct_map[struct_name] = struct.copy()  # Make a copy to avoid modifying original
            struct_order.append(struct_name)

    # Process each config and update the corresponding struct
    for config in configs:
        struct_name = config.struct_name

        # Get config values (filter None if requested)
        config_params = config._values.copy()
        if not include_none:
            config_params = {k: v for k, v in config_params.items() if v is not None}

        # If struct exists in the map, update its parameters
        if struct_name in struct_map:
            # Update existing parameters with config values, but keep all original parameters
            original_params = struct_map[struct_name].get('parameters', {})
            # Merge: keep all original parameters, but update with config values
            updated_params = original_params.copy()
            updated_params.update(config_params)
            struct_map[struct_name]['parameters'] = updated_params
        else:
            # Struct doesn't exist, create new entry and add to order
            struct_map[struct_name] = {
                "name": struct_name,
                "parameters": config_params
            }
            struct_order.append(struct_name)

    # Build result list preserving original order, then new structs
    result = []
    for struct_name in struct_order:
        if struct_name in struct_map:
            result.append(struct_map[struct_name])

    # Write to file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    return result
