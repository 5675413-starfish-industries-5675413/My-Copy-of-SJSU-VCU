"""
State/reset helpers for SIL tests.
"""

import json
from pathlib import Path
from typing import Any

from .path import STRUCT_MEMBERS


def reset_struct_members_to_null(struct_members_file: Path = STRUCT_MEMBERS) -> None:
    """Reset all struct parameter values to null in struct_members_output.json."""
    if not struct_members_file.exists():
        return

    with open(struct_members_file, "r", encoding="utf-8") as f:
        structs: Any = json.load(f)

    if not isinstance(structs, list):
        return

    for struct in structs:
        if not isinstance(struct, dict):
            continue

        params = struct.get("parameters")
        if not isinstance(params, dict):
            continue

        for idx, (key, entry) in enumerate(params.items()):
            if isinstance(entry, dict):
                entry["value"] = None
                if "id" not in entry:
                    entry["id"] = idx
            else:
                params[key] = {"id": idx, "value": None}

    with open(struct_members_file, "w", encoding="utf-8") as f:
        json.dump(structs, f, indent=2, ensure_ascii=False)
        f.write("\n")
