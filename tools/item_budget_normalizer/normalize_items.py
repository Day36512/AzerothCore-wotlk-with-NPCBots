#!/usr/bin/env python3
"""
Safe item budget normalization report generator for AzerothCore 3.3.5a.

This tool reads an item_template CSV export, computes current and target budgets,
and writes reports. It never connects to or mutates the database. SQL files are
generated only when --write-sql is passed.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import math
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCRIPT_PATH = Path(__file__).resolve()
DEFAULT_CONFIG = SCRIPT_PATH.with_name("item_budget_config.yml")
DEFAULT_OUTPUT_DIR = SCRIPT_PATH.with_name("reports")


STAT_LABELS = {
    1: "Health",
    3: "Agility",
    4: "Strength",
    5: "Intellect",
    6: "Spirit",
    7: "Stamina",
    12: "Defense Rating",
    13: "Dodge Rating",
    14: "Parry Rating",
    15: "Block Rating",
    31: "Hit Rating",
    32: "Crit Rating",
    35: "Resilience Rating",
    36: "Haste Rating",
    37: "Expertise Rating",
    38: "Attack Power",
    39: "Ranged Attack Power",
    40: "Feral Attack Power",
    43: "Mana per 5 sec",
    44: "Armor Penetration Rating",
    45: "Spell Power",
    46: "Health Regen",
    47: "Spell Penetration",
    48: "Block Value",
}


INVENTORY_LABELS = {
    0: "Non-equip",
    1: "Head",
    2: "Neck",
    3: "Shoulder",
    4: "Shirt",
    5: "Chest",
    6: "Waist",
    7: "Legs",
    8: "Feet",
    9: "Wrist",
    10: "Hands",
    11: "Finger",
    12: "Trinket",
    13: "One-hand Weapon",
    14: "Shield",
    15: "Ranged",
    16: "Cloak",
    17: "Two-hand Weapon",
    18: "Bag",
    19: "Tabard",
    20: "Robe",
    21: "Main-hand Weapon",
    22: "Off-hand Weapon",
    23: "Held in Off-hand",
    24: "Ammo",
    25: "Thrown",
    26: "Ranged",
    28: "Relic",
}


ARMOR_SUBCLASS_LABELS = {
    0: "Misc",
    1: "Cloth",
    2: "Leather",
    3: "Mail",
    4: "Plate",
    6: "Shield",
}


WEAPON_SUBCLASS_LABELS = {
    0: "Axe",
    1: "Two-hand Axe",
    2: "Bow",
    3: "Gun",
    4: "Mace",
    5: "Two-hand Mace",
    6: "Polearm",
    7: "Sword",
    8: "Two-hand Sword",
    10: "Staff",
    13: "Fist",
    15: "Dagger",
    16: "Thrown",
    18: "Crossbow",
    19: "Wand",
}


SLOT_KEY_BY_INVENTORY = {
    1: "head",
    2: "neck",
    3: "shoulder",
    5: "chest",
    6: "waist",
    7: "legs",
    8: "feet",
    9: "wrist",
    10: "hands",
    11: "finger",
    13: "weapon_one_hand",
    14: "shield",
    15: "ranged",
    16: "cloak",
    17: "weapon_two_hand",
    20: "chest",
    21: "weapon_main_hand",
    22: "weapon_off_hand",
    23: "held_in_off_hand",
    25: "ranged",
    26: "ranged",
    28: "relic",
}


EXCLUDED_INVENTORY_TYPES = {
    4: "shirt",
    12: "trinket",
    18: "bag/container",
    19: "tabard",
    24: "ammo/projectile",
}


DEFAULT_CONFIG_DATA = {
    "safety": {
        "max_sql_updates": 250,
        "dry_run_reviewed": False,
        "visible_budget_change_cap_percent": 20.0,
        "armor_change_cap_percent": 15.0,
        "allow_large_changes": False,
        "allow_armor_updates": False,
        "allow_large_armor_changes": False,
        "allow_special_effect_items": False,
        "require_sql_origin_filter": True,
        "require_sql_origin_expansion": True,
    },
    "budget": {
        "target_mode": "peer",
        "base_budget_at_ilvl_100": 42.0,
        "budget_exponent": 1.15,
        "minimum_visible_budget": 1.0,
        "rounding_tolerance_percent": 3.0,
        "max_rounding_adjustments": 500,
    },
    "peer": {
        "min_samples": 8,
        "sql_min_samples": 8,
        "strict_sql_min_samples": 10,
        "outlier_tolerance_percent": 15.0,
        "leveling_window_1": 5,
        "leveling_window_2": 10,
        "endgame_window_1": 6,
        "endgame_window_2": 13,
    },
    "calibration": {
        "sane_median_abs_percent": 10.0,
        "sane_min_samples": 10,
    },
    "origin": {
        "manifest_path": "F:/Downloads/private_server_item_origin_builder/private_server_item_origin_builder/item_origin_manifests/item_origin_manifest.csv",
        "custom_label": "custom_or_unknown",
    },
    "stat_weights": {
        1: 0.067,
        3: 1.0,
        4: 1.0,
        5: 1.0,
        6: 1.0,
        7: 0.67,
        12: 1.0,
        13: 1.0,
        14: 1.0,
        15: 1.0,
        31: 1.0,
        32: 1.0,
        35: 1.0,
        36: 1.0,
        37: 1.0,
        38: 0.5,
        39: 0.5,
        40: 0.5,
        43: 2.5,
        44: 1.0,
        45: 0.86,
        46: 2.5,
        47: 1.0,
        48: 0.65,
    },
    "socket_budget": {
        "level_70": {"red": 8.0, "yellow": 8.0, "blue": 8.0, "meta": 12.0},
        "level_80": {"red": 16.0, "yellow": 16.0, "blue": 16.0, "meta": 24.0},
    },
    "slot_multipliers": {
        "head": 1.0,
        "neck": 0.56,
        "shoulder": 0.74,
        "chest": 1.0,
        "waist": 0.74,
        "legs": 1.0,
        "feet": 0.74,
        "wrist": 0.56,
        "hands": 0.74,
        "finger": 0.56,
        "weapon_one_hand": 1.0,
        "weapon_main_hand": 1.0,
        "weapon_off_hand": 0.56,
        "weapon_two_hand": 1.8,
        "ranged": 1.0,
        "shield": 0.74,
        "cloak": 0.56,
        "held_in_off_hand": 0.56,
        "relic": 0.4,
    },
    "quality_multipliers": {2: 0.85, 3: 1.0, 4: 1.15, 5: 1.3},
    "armor": {"nearby_ilvl_window": 6, "max_ilvl_window": 13, "min_samples": 8},
    "weapon": {
        "nearby_ilvl_window": 6,
        "max_ilvl_window": 13,
        "min_samples": 8,
        "caster_spell_power_share_threshold": 0.45,
    },
    "exclusions": {
        "exclude_name_pattern_1": "[PH]",
        "exclude_name_pattern_2": "Monster -",
        "exclude_name_pattern_3": "QA",
        "exclude_name_pattern_4": "Test ",
    },
}


@dataclass
class StatLine:
    slot: int
    stat_type: int
    old_value: int
    weight: float
    new_value: int | None = None

    @property
    def label(self) -> str:
        return STAT_LABELS.get(self.stat_type, f"Unknown {self.stat_type}")

    @property
    def old_budget(self) -> float:
        return self.old_value * self.weight

    @property
    def new_budget(self) -> float:
        return (self.new_value if self.new_value is not None else self.old_value) * self.weight


@dataclass
class CurvePoint:
    entry: int
    ilvl: int
    value: float
    caster: bool = False


@dataclass
class TargetBudget:
    budget: float
    source: str
    sample_count: int
    peer_key: str = ""
    peer_window: str = ""
    p10: float = 0.0
    p25: float = 0.0
    p75: float = 0.0
    p90: float = 0.0
    outlier: bool = False
    sql_blocked: bool = False


@dataclass
class ArmorResult:
    new_armor: int
    median_armor: float = 0.0
    sample_count: int = 0
    peer_window: str = ""
    classification: str = "not_checked"
    change_percent: float = 0.0


@dataclass
class WeaponDamageResult:
    new_min: float
    new_max: float
    median_dps: float = 0.0
    sample_count: int = 0
    peer_window: str = ""
    change_percent: float = 0.0


@dataclass
class CalibrationIndex:
    groups: dict[tuple[Any, ...], list["PeerBudgetPoint"]]


@dataclass
class PeerBudgetPoint:
    entry: int
    ilvl: int
    total_budget: float
    visible_budget: float
    special_weapon_budget: float


@dataclass
class OriginInfo:
    expansion: int
    label: str
    source: str
    present_in_vanilla: bool = False
    present_in_tbc: bool = False
    present_in_wrath: bool = False


def expansion_band(ilvl: int) -> str:
    if ilvl < 60:
        return "classic_leveling"
    if ilvl < 90:
        return "classic_endgame"
    if ilvl < 165:
        return "burning_crusade"
    if ilvl < 200:
        return "wrath_pre_raid"
    if ilvl < 220:
        return "wrath_t7"
    if ilvl < 240:
        return "wrath_t8"
    if ilvl < 260:
        return "wrath_t9"
    if ilvl < 285:
        return "wrath_t10"
    return "post_wrath_or_custom"


def ilvl_band(ilvl: int) -> str:
    if ilvl < 60:
        low = (ilvl // 10) * 10
        return f"{low:03d}-{low + 9:03d}"
    if ilvl < 90:
        return "060-089"
    if ilvl < 165:
        low = 90 + ((ilvl - 90) // 15) * 15
        return f"{low:03d}-{min(low + 14, 164):03d}"
    if ilvl < 200:
        return "165-199"
    if ilvl < 220:
        return "200-219"
    if ilvl < 240:
        return "220-239"
    if ilvl < 260:
        return "240-259"
    if ilvl < 285:
        return "260-284"
    return "285+"


def strip_comment(line: str) -> str:
    in_quote: str | None = None
    escaped = False
    out = []
    for ch in line:
        if escaped:
            out.append(ch)
            escaped = False
            continue
        if ch == "\\":
            escaped = True
            out.append(ch)
            continue
        if ch in ("'", '"'):
            if in_quote == ch:
                in_quote = None
            elif in_quote is None:
                in_quote = ch
            out.append(ch)
            continue
        if ch == "#" and in_quote is None:
            break
        out.append(ch)
    return "".join(out).rstrip()


def parse_scalar(raw: str) -> Any:
    value = raw.strip()
    if value == "":
        return ""
    if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
        return value[1:-1]
    lowered = value.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    if lowered == "null":
        return None
    try:
        if any(ch in value for ch in (".", "e", "E")):
            return float(value)
        return int(value)
    except ValueError:
        return value


def parse_key(raw: str) -> Any:
    key = raw.strip()
    try:
        return int(key)
    except ValueError:
        return key


def load_simple_yaml(path: Path) -> dict[str, Any]:
    root: dict[str, Any] = {}
    stack: list[tuple[int, dict[str, Any]]] = [(-1, root)]
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        clean = strip_comment(raw_line)
        if not clean.strip():
            continue
        indent = len(clean) - len(clean.lstrip(" "))
        text = clean.strip()
        if ":" not in text:
            raise ValueError(f"Unsupported YAML line in {path}: {raw_line}")
        key_text, value_text = text.split(":", 1)
        key = parse_key(key_text)
        while stack and indent <= stack[-1][0]:
            stack.pop()
        parent = stack[-1][1]
        if value_text.strip() == "":
            child: dict[str, Any] = {}
            parent[key] = child
            stack.append((indent, child))
        else:
            parent[key] = parse_scalar(value_text)
    return root


def deep_merge(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in base.items():
        if isinstance(value, dict):
            result[key] = deep_merge(value, {})
        else:
            result[key] = value
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value
    return result


def load_config(path: Path) -> dict[str, Any]:
    data = load_simple_yaml(path) if path.exists() else {}
    return deep_merge(DEFAULT_CONFIG_DATA, data)


def as_int(value: Any, default: int = 0) -> int:
    try:
        if value is None or value == "":
            return default
        return int(float(str(value).strip()))
    except (TypeError, ValueError):
        return default


def as_float(value: Any, default: float = 0.0) -> float:
    try:
        if value is None or value == "":
            return default
        return float(str(value).strip())
    except (TypeError, ValueError):
        return default


def row_int(row: dict[str, str], key: str, default: int = 0) -> int:
    return as_int(row.get(key), default)


def row_float(row: dict[str, str], key: str, default: float = 0.0) -> float:
    return as_float(row.get(key), default)


def quality_label(quality: int) -> str:
    return {2: "Uncommon", 3: "Rare", 4: "Epic", 5: "Legendary"}.get(quality, str(quality))


def inventory_label(inv_type: int) -> str:
    return INVENTORY_LABELS.get(inv_type, f"InventoryType {inv_type}")


def subclass_label(class_id: int, subclass: int) -> str:
    if class_id == 2:
        return WEAPON_SUBCLASS_LABELS.get(subclass, f"Weapon subclass {subclass}")
    if class_id == 4:
        return ARMOR_SUBCLASS_LABELS.get(subclass, f"Armor subclass {subclass}")
    return str(subclass)


def slot_key(row: dict[str, str]) -> str | None:
    inv_type = row_int(row, "InventoryType")
    class_id = row_int(row, "class")
    if class_id == 2 and inv_type in (15, 25, 26):
        return "ranged"
    return SLOT_KEY_BY_INVENTORY.get(inv_type)


def is_weapon(row: dict[str, str]) -> bool:
    return row_int(row, "class") == 2


def is_armor(row: dict[str, str]) -> bool:
    return row_int(row, "class") == 4


def item_name(row: dict[str, str]) -> str:
    return row.get("name") or f"entry {row.get('entry', '?')}"


def half_up(value: float) -> int:
    return int(math.floor(value + 0.5))


def clean_stat_round(value: float) -> int:
    if value <= 0:
        return 0
    nearest = max(1, half_up(value))
    if value >= 100:
        clean = max(1, int(round(nearest / 5.0) * 5))
        if abs(clean - value) <= max(1.5, value * 0.01):
            return clean
    return nearest


def clean_damage_round(value: float) -> float:
    return float(max(1, half_up(value)))


def parse_int_filter(values: list[str] | None) -> set[int]:
    result: set[int] = set()
    for value in values or []:
        for part in str(value).replace(",", " ").split():
            if part:
                result.add(int(part))
    return result


def parse_text_filter(values: list[str] | None) -> set[str]:
    result: set[str] = set()
    for value in values or []:
        for part in str(value).replace(",", " ").split():
            if part:
                result.add(part.strip().lower())
    return result


def read_entries_file(path: Path | None) -> set[int]:
    if not path:
        return set()
    entries: set[int] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.split("#", 1)[0].strip()
        if not text:
            continue
        for part in text.replace(",", " ").split():
                entries.add(int(part))
    return entries


def sql_origin_allowed(
    entry: int,
    origin: OriginInfo,
    explicit_entries: set[int],
    sql_origin_labels: set[str],
    sql_origin_expansions: set[int],
    custom_only: bool,
    require_filter: bool,
    require_origin_expansion: bool,
    custom_label: str,
) -> tuple[bool, str]:
    custom_selected = custom_only or custom_label.lower() in sql_origin_labels
    if require_origin_expansion and origin.expansion < 0 and not custom_selected:
        return False, f"origin_expansion missing; select {custom_label} explicitly for SQL"
    if entry in explicit_entries:
        return True, "entry explicitly whitelisted"
    if custom_only:
        if origin.label == custom_label:
            return True, "custom_or_unknown origin allowed"
        return False, f"origin_label {origin.label} is not {custom_label}"
    if sql_origin_labels or sql_origin_expansions:
        label_allowed = not sql_origin_labels or origin.label.lower() in sql_origin_labels
        expansion_allowed = not sql_origin_expansions or origin.expansion in sql_origin_expansions
        if label_allowed and expansion_allowed:
            return True, "origin filter matched"
        return False, f"origin {origin.label}/{origin.expansion} did not match SQL origin filter"
    if require_filter:
        return False, "SQL origin filter is required; pass --custom-only, --origin-label, --origin-expansion, or explicit --entry"
    return True, "no origin filter required"


def load_origin_manifest(path: Path | None, custom_label: str) -> dict[int, OriginInfo]:
    if not path or not path.exists():
        return {}
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = csv.DictReader(handle)
        origins: dict[int, OriginInfo] = {}
        for row in rows:
            entry = as_int(row.get("entry"))
            if entry <= 0:
                continue
            label = str(row.get("origin_label") or custom_label).strip() or custom_label
            origins[entry] = OriginInfo(
                expansion=as_int(row.get("origin_expansion"), -1),
                label=label,
                source="origin_manifest",
                present_in_vanilla=as_int(row.get("present_in_vanilla")) > 0,
                present_in_tbc=as_int(row.get("present_in_tbc")) > 0,
                present_in_wrath=as_int(row.get("present_in_wrath")) > 0,
            )
    return origins


def origin_for_row(row: dict[str, str], origins: dict[int, OriginInfo], custom_label: str) -> OriginInfo:
    entry = row_int(row, "entry")
    return origins.get(entry, OriginInfo(expansion=-1, label=custom_label, source="missing_from_origin_manifest"))


def configured_entry_set(config: dict[str, Any], section: str, prefix: str) -> set[int]:
    values = config.get(section, {})
    entries: set[int] = set()
    for key, value in values.items():
        if str(key).startswith(prefix):
            for part in str(value).replace(",", " ").split():
                if part:
                    entries.add(int(part))
    return entries


def row_matches_filters(row: dict[str, str], args: argparse.Namespace, entries: set[int], qualities: set[int]) -> bool:
    entry = row_int(row, "entry")
    ilvl = row_int(row, "ItemLevel")
    req_level = row_int(row, "RequiredLevel")
    if args.min_ilvl is not None and ilvl < args.min_ilvl:
        return False
    if args.max_ilvl is not None and ilvl > args.max_ilvl:
        return False
    if args.required_level is not None and req_level != args.required_level:
        return False
    if qualities and row_int(row, "Quality") not in qualities:
        return False
    if entries and entry not in entries:
        return False
    return True


def configured_name_patterns(config: dict[str, Any]) -> list[str]:
    exclusions = config.get("exclusions", {})
    return [str(v) for _, v in sorted(exclusions.items(), key=lambda item: str(item[0])) if str(v).strip()]


def parse_stats(row: dict[str, str], stat_weights: dict[int, float]) -> tuple[list[StatLine], list[str], list[str]]:
    stats: list[StatLine] = []
    warnings: list[str] = []
    fatal: list[str] = []
    seen_types: set[int] = set()
    saw_empty_slot = False
    for slot in range(1, 11):
        stat_type = row_int(row, f"stat_type{slot}")
        stat_value = row_int(row, f"stat_value{slot}")
        if stat_type == 0 and stat_value == 0:
            saw_empty_slot = True
            continue
        if stat_value == 0:
            warnings.append(f"stat slot {slot} has type {stat_type} with zero value")
            saw_empty_slot = True
            continue
        if saw_empty_slot:
            fatal.append(f"non-contiguous stat slot {slot}")
        if stat_type == 0:
            fatal.append(f"stat slot {slot} has value {stat_value} but stat_type 0")
            continue
        if stat_value < 0:
            fatal.append(f"stat slot {slot} has negative value {stat_value}")
            continue
        if stat_type in seen_types:
            fatal.append(f"duplicate stat type {stat_type} in slot {slot}")
            continue
        seen_types.add(stat_type)
        if stat_type not in stat_weights:
            fatal.append(f"unknown stat type {stat_type} in slot {slot}")
            continue
        stats.append(StatLine(slot=slot, stat_type=stat_type, old_value=stat_value, weight=float(stat_weights[stat_type])))
    return stats, warnings, fatal


def weighted_budget(stats: list[StatLine], use_new: bool = False) -> float:
    total = 0.0
    for stat in stats:
        value = stat.new_value if use_new and stat.new_value is not None else stat.old_value
        total += value * stat.weight
    return total


def stat_list(stats: list[StatLine], use_new: bool = False) -> str:
    if not stats:
        return ""
    parts = []
    for stat in stats:
        value = stat.new_value if use_new and stat.new_value is not None else stat.old_value
        parts.append(f"+{value} {stat.label}")
    return ", ".join(parts)


def socket_color_name(color: int) -> str | None:
    if color & 2:
        return "red"
    if color & 4:
        return "yellow"
    if color & 8:
        return "blue"
    if color & 1:
        return "meta"
    return None


def socket_counts(row: dict[str, str]) -> tuple[dict[str, int], list[str]]:
    counts = {"red": 0, "yellow": 0, "blue": 0, "meta": 0}
    warnings: list[str] = []
    for index in range(1, 4):
        raw = row_int(row, f"socketColor_{index}")
        if raw == 0:
            continue
        name = socket_color_name(raw)
        if name is None:
            warnings.append(f"unknown socket color bitmask {raw} in socket {index}")
            continue
        counts[name] += 1
    return counts, warnings


def socket_budget(row: dict[str, str], config: dict[str, Any]) -> tuple[float, dict[str, int], list[str]]:
    counts, warnings = socket_counts(row)
    req_level = row_int(row, "RequiredLevel")
    bracket = "level_70" if req_level <= 70 else "level_80"
    values = config["socket_budget"].get(bracket, {})
    total = sum(count * float(values.get(color, 0.0)) for color, count in counts.items())
    return total, counts, warnings


def has_spell_payload(row: dict[str, str]) -> bool:
    return any(row_int(row, f"spellid_{index}") > 0 for index in range(1, 6))


def special_effect_flags(row: dict[str, str]) -> list[str]:
    flags: list[str] = []
    if has_spell_payload(row):
        flags.append("spell_payload")
    if row_int(row, "itemset") > 0:
        flags.append("itemset")
    return flags


def current_dps(row: dict[str, str]) -> float:
    delay = row_float(row, "delay")
    min_damage = row_float(row, "dmg_min1")
    max_damage = row_float(row, "dmg_max1")
    if delay <= 0 or min_damage <= 0 or max_damage <= 0:
        return 0.0
    return ((min_damage + max_damage) / 2.0) / (delay / 1000.0)


def has_weapon_damage(row: dict[str, str]) -> bool:
    return current_dps(row) > 0


def classify_role(row: dict[str, str], stats: list[StatLine], config: dict[str, Any]) -> str:
    if row_int(row, "InventoryType") == 14:
        return "tank"
    total = weighted_budget(stats)
    if total <= 0:
        return "weapon" if is_weapon(row) else "generic"
    shares: dict[int, float] = {stat.stat_type: stat.old_budget / total for stat in stats}
    tank_share = sum(shares.get(stat, 0.0) for stat in (12, 13, 14, 15, 48))
    caster_share = sum(shares.get(stat, 0.0) for stat in (5, 6, 43, 45, 47))
    physical_share = sum(shares.get(stat, 0.0) for stat in (3, 4, 37, 38, 39, 40, 44))
    if tank_share >= 0.25:
        return "tank"
    if shares.get(45, 0.0) >= float(config["weapon"]["caster_spell_power_share_threshold"]):
        if shares.get(43, 0.0) >= 0.08 or (shares.get(6, 0.0) >= 0.12 and shares.get(31, 0.0) < 0.05):
            return "healer"
        return "caster"
    if caster_share >= 0.35:
        if shares.get(43, 0.0) >= 0.08 or (shares.get(6, 0.0) >= 0.12 and shares.get(31, 0.0) < 0.05):
            return "healer"
        return "caster"
    if physical_share >= 0.35:
        return "physical"
    return "generic"


def weapon_category(row: dict[str, str], stats: list[StatLine], config: dict[str, Any]) -> str:
    if not is_weapon(row):
        if row_int(row, "InventoryType") == 14:
            return "shield"
        return "none"
    subclass = row_int(row, "subclass")
    key = slot_key(row) or "weapon"
    if is_caster_weapon(row, stats, config):
        return f"{key}:caster"
    if any(stat.stat_type == 40 for stat in stats):
        return f"{key}:feral"
    if subclass in (2, 3, 18):
        return "ranged:bow_gun_crossbow"
    if subclass == 16:
        return "ranged:thrown"
    if subclass == 19:
        return "ranged:wand"
    return f"{key}:physical"


def pvp_family(row: dict[str, str]) -> str:
    name = item_name(row).lower()
    if "grand marshal" in name or "high warlord" in name:
        return "classic_rank"
    if "wrathful gladiator" in name:
        return "wrathful_gladiator"
    if "relentless gladiator" in name:
        return "relentless_gladiator"
    if "furious gladiator" in name:
        return "furious_gladiator"
    if "deadly gladiator" in name:
        return "deadly_gladiator"
    if "hateful gladiator" in name:
        return "hateful_gladiator"
    if "savage gladiator" in name:
        return "savage_gladiator"
    if "brutal gladiator" in name:
        return "brutal_gladiator"
    if "vengeful gladiator" in name:
        return "vengeful_gladiator"
    if "merciless gladiator" in name:
        return "merciless_gladiator"
    if "gladiator" in name:
        return "gladiator"
    return "none"


def armor_peer_key(row: dict[str, str]) -> str:
    if is_armor(row) and row_int(row, "InventoryType") != 14:
        return str(row_int(row, "subclass"))
    if row_int(row, "InventoryType") == 14:
        return "shield"
    return "none"


def peer_group_key(row: dict[str, str], stats: list[StatLine], role: str, config: dict[str, Any], origin: OriginInfo) -> tuple[Any, ...] | None:
    key = slot_key(row)
    if key is None:
        return None
    return (
        origin.expansion,
        row_int(row, "Quality"),
        key,
        armor_peer_key(row),
        weapon_category(row, stats, config),
        pvp_family(row),
        role,
    )


def peer_group_label(peer_key: tuple[Any, ...] | None) -> str:
    if peer_key is None:
        return ""
    origin_expansion, quality, key, armor_key, weapon_key, pvp_key, role = peer_key
    return f"origin={origin_expansion};quality={quality};slot={key};armor={armor_key};weapon={weapon_key};pvp={pvp_key};role={role}"


def peer_windows_for_ilvl(ilvl: int, config: dict[str, Any]) -> list[int]:
    settings = config.get("peer", {})
    if ilvl < 90:
        return [
            int(settings.get("leveling_window_1", 5)),
            int(settings.get("leveling_window_2", 10)),
        ]
    return [
        0,
        int(settings.get("endgame_window_1", 6)),
        int(settings.get("endgame_window_2", 13)),
    ]


def strict_sql_peer_categories(row: dict[str, str], role: str) -> list[str]:
    categories: list[str] = []
    key = slot_key(row)
    if key in ("neck", "finger"):
        categories.append("jewelry")
    if is_weapon(row):
        categories.append("weapon")
    if key == "shield":
        categories.append("shield")
    if role == "tank":
        categories.append("tank")
    return categories


def required_sql_peer_samples(row: dict[str, str], role: str, config: dict[str, Any]) -> int:
    peer_settings = config.get("peer", {})
    base = int(peer_settings.get("sql_min_samples", peer_settings.get("min_samples", 8)))
    strict = int(peer_settings.get("strict_sql_min_samples", 10))
    return strict if strict_sql_peer_categories(row, role) else base


def sql_peer_sample_warning(row: dict[str, str], role: str, sample_count: int, required: int) -> str:
    categories = strict_sql_peer_categories(row, role)
    if categories:
        label = "/".join(categories)
        return f"SQL blocked: {label} requires at least {required} peer samples for SQL; found {sample_count}"
    return f"SQL blocked: requires at least {required} peer samples for SQL; found {sample_count}"


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * pct
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return float(ordered[lower])
    fraction = position - lower
    return float(ordered[lower] + ((ordered[upper] - ordered[lower]) * fraction))


def is_caster_weapon(row: dict[str, str], stats: list[StatLine], config: dict[str, Any]) -> bool:
    if not is_weapon(row):
        return False
    total = weighted_budget(stats)
    if total <= 0:
        return False
    spell_power_budget = sum(stat.old_budget for stat in stats if stat.stat_type == 45)
    if spell_power_budget <= 0:
        return False
    if (spell_power_budget / total) >= float(config["weapon"]["caster_spell_power_share_threshold"]):
        return True

    caster_support_budget = sum(stat.old_budget for stat in stats if stat.stat_type in {5, 6, 43, 47})
    physical_budget = sum(stat.old_budget for stat in stats if stat.stat_type in {3, 4, 37, 38, 39, 40, 44})
    if caster_support_budget > 0 and physical_budget <= spell_power_budget:
        return True
    if row_int(row, "subclass") in (10, 19) and physical_budget <= 0:
        return True
    return False


def special_weapon_stat_types(row: dict[str, str], stats: list[StatLine], config: dict[str, Any]) -> set[int]:
    if not is_weapon(row):
        return set()
    special = {38, 39, 40}
    if is_caster_weapon(row, stats, config):
        special.add(45)
    return special


def split_normal_and_special_stats(row: dict[str, str], stats: list[StatLine], config: dict[str, Any]) -> tuple[list[StatLine], list[StatLine]]:
    special_types = special_weapon_stat_types(row, stats, config)
    normal: list[StatLine] = []
    special: list[StatLine] = []
    for stat in stats:
        if stat.stat_type in special_types:
            special.append(stat)
        else:
            normal.append(stat)
    return normal, special


def formula_target_budget(row: dict[str, str], config: dict[str, Any]) -> tuple[float, str | None]:
    ilvl = row_int(row, "ItemLevel")
    quality = row_int(row, "Quality")
    key = slot_key(row)
    if key is None:
        return 0.0, None
    slot_multiplier = float(config["slot_multipliers"].get(key, 0.0))
    quality_multiplier = float(config["quality_multipliers"].get(quality, 0.0))
    if slot_multiplier <= 0 or quality_multiplier <= 0 or ilvl <= 0:
        return 0.0, key
    base = float(config["budget"]["base_budget_at_ilvl_100"])
    exponent = float(config["budget"]["budget_exponent"])
    return base * ((ilvl / 100.0) ** exponent) * slot_multiplier * quality_multiplier, key


def target_budget(
    row: dict[str, str],
    stats: list[StatLine],
    config: dict[str, Any],
    calibration_index: CalibrationIndex | None,
    role: str,
    old_total_budget: float,
    origin: OriginInfo,
) -> TargetBudget:
    peer_key = peer_group_key(row, stats, role, config, origin)
    peer_key_label = peer_group_label(peer_key)
    if peer_key is None:
        return TargetBudget(old_total_budget, "unclassified", 0, peer_key=peer_key_label, sql_blocked=True)
    if str(config["budget"].get("target_mode", "peer")).lower() != "peer" or calibration_index is None:
        formula_budget, _ = formula_target_budget(row, config)
        return TargetBudget(formula_budget, "formula", 0, peer_key=peer_key_label)

    points = calibration_index.groups.get(peer_key, [])
    entry = row_int(row, "entry")
    ilvl = row_int(row, "ItemLevel")
    min_samples = int(config.get("peer", {}).get("min_samples", 8))
    selected: list[PeerBudgetPoint] = []
    selected_window = ""
    low_sample = False
    for window in peer_windows_for_ilvl(ilvl, config):
        if window == 0:
            candidates = [point for point in points if point.entry != entry and point.ilvl == ilvl]
            label = "exact_ilvl"
            if candidates and len(candidates) < min_samples:
                selected = candidates
                selected_window = label
                low_sample = True
                break
        else:
            candidates = [point for point in points if point.entry != entry and abs(point.ilvl - ilvl) <= window]
            label = f"+/-{window}_ilvl"
        if len(candidates) >= min_samples:
            selected = candidates
            selected_window = label
            break

    if not selected:
        return TargetBudget(
            old_total_budget,
            "no_peer_skip",
            0,
            peer_key=peer_key_label,
            peer_window="insufficient_samples",
            outlier=False,
            sql_blocked=True,
        )

    values = [point.total_budget for point in selected]
    target = median(values)
    p10 = percentile(values, 0.10)
    p25 = percentile(values, 0.25)
    p75 = percentile(values, 0.75)
    p90 = percentile(values, 0.90)
    if target <= 0 and old_total_budget > 0:
        return TargetBudget(
            old_total_budget,
            f"peer_{selected_window}_zero_budget_skip",
            len(selected),
            peer_key=peer_key_label,
            peer_window=selected_window,
            p10=p10,
            p25=p25,
            p75=p75,
            p90=p90,
            outlier=False,
            sql_blocked=True,
        )
    tolerance = float(config.get("peer", {}).get("outlier_tolerance_percent", 15.0))
    pct = abs(percent_delta(old_total_budget, target))
    outlier = pct > tolerance
    return TargetBudget(
        target,
        f"peer_{selected_window}{'_low_sample' if low_sample else ''}",
        len(selected),
        peer_key=peer_key_label,
        peer_window=selected_window,
        p10=p10,
        p25=p25,
        p75=p75,
        p90=p90,
        outlier=outlier,
        sql_blocked=low_sample,
    )


def normalize_stats(stats: list[StatLine], target_visible_budget: float, config: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    current = weighted_budget(stats)
    if not stats or current <= 0:
        return warnings
    for stat in stats:
        share = stat.old_budget / current
        raw_value = (target_visible_budget * share) / max(stat.weight, 0.000001)
        stat.new_value = max(1, clean_stat_round(raw_value))
    tolerance = float(config["budget"]["rounding_tolerance_percent"]) / 100.0
    max_adjustments = int(config["budget"]["max_rounding_adjustments"])
    if target_visible_budget <= 0:
        return warnings
    iterations = 0
    while iterations < max_adjustments:
        new_budget = weighted_budget(stats, use_new=True)
        error = new_budget - target_visible_budget
        if abs(error) / target_visible_budget <= tolerance:
            break
        largest = max(stats, key=lambda s: s.new_budget)
        if error < 0:
            largest.new_value = (largest.new_value or largest.old_value) + 1
        else:
            candidates = [s for s in stats if (s.new_value or s.old_value) > 1]
            if not candidates:
                break
            largest = max(candidates, key=lambda s: s.new_budget)
            largest.new_value = max(1, (largest.new_value or largest.old_value) - 1)
        iterations += 1
    final_budget = weighted_budget(stats, use_new=True)
    if target_visible_budget > 0 and abs(final_budget - target_visible_budget) / target_visible_budget > tolerance:
        warnings.append("rounding remained outside configured tolerance")
    return warnings


def visible_budget_cap_bounds(old_budget: float, config: dict[str, Any], whitelisted: bool) -> tuple[float, float, float] | None:
    if whitelisted or old_budget <= 0:
        return None
    cap = float(config["safety"].get("visible_budget_change_cap_percent", 20.0)) / 100.0
    if cap <= 0:
        return None
    return old_budget * (1.0 - cap), old_budget * (1.0 + cap), cap


def clamp_visible_budget_target(old_budget: float, desired_budget: float, config: dict[str, Any], whitelisted: bool) -> tuple[float, str | None]:
    bounds = visible_budget_cap_bounds(old_budget, config, whitelisted)
    if bounds is None:
        return desired_budget, None
    low, high, cap = bounds
    clamped = min(max(desired_budget, low), high)
    if abs(clamped - desired_budget) > 0.000001:
        return clamped, f"visible stat target clamped to +/-{cap * 100:.0f}% safety cap"
    return desired_budget, None


def enforce_final_visible_budget_cap(stats: list[StatLine], old_budget: float, config: dict[str, Any], whitelisted: bool) -> list[str]:
    bounds = visible_budget_cap_bounds(old_budget, config, whitelisted)
    if bounds is None or not stats:
        return []
    low, high, cap = bounds
    max_adjustments = int(config["budget"]["max_rounding_adjustments"])
    adjusted = False
    iterations = 0

    while iterations < max_adjustments:
        current = weighted_budget(stats, use_new=True)
        if low <= current <= high:
            break
        if current < low:
            candidates = []
            for stat in stats:
                next_budget = current + stat.weight
                candidates.append((abs(next_budget - low), next_budget, next_budget > high, stat))
            _, next_budget, crossed, chosen = min(candidates, key=lambda item: (item[2], item[0]))
            if crossed and (next_budget - high) >= (low - current):
                break
            chosen.new_value = (chosen.new_value if chosen.new_value is not None else chosen.old_value) + 1
            adjusted = True
            if crossed:
                break
        else:
            candidates = []
            for stat in stats:
                value = stat.new_value if stat.new_value is not None else stat.old_value
                if value <= 1:
                    continue
                next_budget = current - stat.weight
                candidates.append((abs(next_budget - high), next_budget, next_budget < low, stat))
            if not candidates:
                break
            _, next_budget, crossed, chosen = min(candidates, key=lambda item: (item[2], item[0]))
            if crossed and (low - next_budget) >= (current - high):
                break
            chosen.new_value = max(1, (chosen.new_value if chosen.new_value is not None else chosen.old_value) - 1)
            adjusted = True
            if crossed:
                break
        iterations += 1

    current = weighted_budget(stats, use_new=True)
    if low <= current <= high:
        if adjusted:
            return [f"final visible stat budget adjusted to stay within +/-{cap * 100:.0f}% safety cap"]
        return []
    for stat in stats:
        stat.new_value = stat.old_value
    return [f"visible stat changes reverted: stat rounding granularity could not stay within +/-{cap * 100:.0f}% safety cap"]


def median(values: list[float]) -> float:
    return float(statistics.median(values)) if values else 0.0


def build_armor_index(rows: list[dict[str, str]]) -> dict[tuple[int, int, int], list[CurvePoint]]:
    index: dict[tuple[int, int, int], list[CurvePoint]] = {}
    for row in rows:
        if not is_armor(row):
            continue
        armor = row_float(row, "armor")
        ilvl = row_int(row, "ItemLevel")
        if armor <= 0 or ilvl <= 0:
            continue
        key = (row_int(row, "subclass"), row_int(row, "InventoryType"), row_int(row, "Quality"))
        index.setdefault(key, []).append(CurvePoint(entry=row_int(row, "entry"), ilvl=ilvl, value=armor))
    return index


def build_weapon_index(
    rows: list[dict[str, str]],
    stat_weights: dict[int, float],
    config: dict[str, Any],
) -> dict[tuple[int, str, int, bool], list[CurvePoint]]:
    index: dict[tuple[int, str, int, bool], list[CurvePoint]] = {}
    for row in rows:
        if not is_weapon(row) or not has_weapon_damage(row):
            continue
        key_slot = slot_key(row)
        if key_slot is None:
            continue
        stats, _, fatal = parse_stats(row, stat_weights)
        caster = False if fatal else is_caster_weapon(row, stats, config)
        key = (row_int(row, "subclass"), key_slot, row_int(row, "Quality"), caster)
        index.setdefault(key, []).append(CurvePoint(entry=row_int(row, "entry"), ilvl=row_int(row, "ItemLevel"), value=current_dps(row), caster=caster))
    return index


def calibration_skip_reasons(row: dict[str, str], stats: list[StatLine], fatal_stat_warnings: list[str], config: dict[str, Any]) -> list[str]:
    reasons: list[str] = []
    class_id = row_int(row, "class")
    inv_type = row_int(row, "InventoryType")
    quality = row_int(row, "Quality")
    if fatal_stat_warnings:
        reasons.extend(fatal_stat_warnings)
    if class_id not in (2, 4):
        reasons.append("not armor or weapon class")
    if inv_type in EXCLUDED_INVENTORY_TYPES:
        reasons.append(f"{EXCLUDED_INVENTORY_TYPES[inv_type]} excluded")
    if row_int(row, "ScalingStatDistribution") != 0 or row_int(row, "ScalingStatValue") != 0:
        reasons.append("scaling item excluded")
    if quality not in config["quality_multipliers"]:
        reasons.append(f"quality {quality} not configured")
    if slot_key(row) is None:
        reasons.append("cannot classify inventory slot")
    for pattern in configured_name_patterns(config):
        if pattern and pattern.lower() in item_name(row).lower():
            reasons.append(f"name matched exclusion pattern {pattern}")
            break
    if not stats and not has_weapon_damage(row):
        reasons.append("no normal stats and no weapon damage")
    return reasons


def build_budget_calibration_index(
    rows: list[dict[str, str]],
    stat_weights: dict[int, float],
    config: dict[str, Any],
    origins: dict[int, OriginInfo],
) -> CalibrationIndex:
    groups: dict[tuple[Any, ...], list[PeerBudgetPoint]] = {}

    for row in rows:
        stats, _, fatal = parse_stats(row, stat_weights)
        if calibration_skip_reasons(row, stats, fatal, config):
            continue
        role = classify_role(row, stats, config)
        origin = origin_for_row(row, origins, str(config["origin"].get("custom_label", "custom_or_unknown")))
        peer_key = peer_group_key(row, stats, role, config, origin)
        if peer_key is None:
            continue
        normal_stats, special_stats = split_normal_and_special_stats(row, stats, config)
        sock_budget, _, _ = socket_budget(row, config)
        visible = weighted_budget(normal_stats)
        special = weighted_budget(special_stats)
        total = visible + sock_budget
        if total <= 0 and not (is_weapon(row) and (special_stats or has_weapon_damage(row))):
            continue

        groups.setdefault(peer_key, []).append(PeerBudgetPoint(
            entry=row_int(row, "entry"),
            ilvl=row_int(row, "ItemLevel"),
            total_budget=total,
            visible_budget=visible,
            special_weapon_budget=special,
        ))

    return CalibrationIndex(groups=groups)


def nearby_values(points: list[CurvePoint], entry: int, ilvl: int, windows: list[int], min_samples: int) -> tuple[list[float], int | None]:
    for window in windows:
        if window == 0:
            values = [point.value for point in points if point.entry != entry and point.ilvl == ilvl]
        else:
            values = [point.value for point in points if point.entry != entry and abs(point.ilvl - ilvl) <= window]
        if len(values) >= min_samples:
            return values, window
    return [], None


def derive_armor(
    row: dict[str, str],
    armor_index: dict[tuple[int, int, int], list[CurvePoint]],
    config: dict[str, Any],
) -> tuple[ArmorResult, list[str]]:
    warnings: list[str] = []
    old_armor = row_int(row, "armor")
    if not is_armor(row) or old_armor <= 0:
        return ArmorResult(new_armor=old_armor, classification="not_armor"), warnings
    key = (row_int(row, "subclass"), row_int(row, "InventoryType"), row_int(row, "Quality"))
    points = armor_index.get(key, [])
    min_samples = int(config.get("armor", {}).get("min_samples", config.get("peer", {}).get("min_samples", 8)))
    values, window = nearby_values(
        points,
        row_int(row, "entry"),
        row_int(row, "ItemLevel"),
        peer_windows_for_ilvl(row_int(row, "ItemLevel"), config),
        min_samples,
    )
    if not values:
        warnings.append("armor unchanged: not enough comparable armor samples")
        return ArmorResult(new_armor=old_armor, classification="insufficient_samples"), warnings
    median_armor = median(values)
    change_percent = percent_delta(old_armor, median_armor)
    window_label = "exact ilvl" if window == 0 else f"within {window} ilvl"
    bonus_threshold = float(config.get("armor", {}).get("bonus_armor_threshold_percent", 25.0))
    if change_percent > bonus_threshold:
        warnings.append(f"armor unchanged: likely bonus_armor, {change_percent:.1f}% above peer median from {len(values)} sample(s) {window_label}")
        return ArmorResult(
            new_armor=old_armor,
            median_armor=median_armor,
            sample_count=len(values),
            peer_window=window_label,
            classification="bonus_armor",
            change_percent=0.0,
        ), warnings
    new_armor = max(0, half_up(median_armor))
    rounded_change_percent = percent_delta(new_armor, old_armor) if old_armor > 0 else 0.0
    tolerance = float(config.get("peer", {}).get("outlier_tolerance_percent", 15.0))
    if abs(percent_delta(old_armor, new_armor)) <= tolerance:
        warnings.append(f"armor unchanged: within +/-{tolerance:.0f}% peer tolerance from {len(values)} sample(s)")
        return ArmorResult(
            new_armor=old_armor,
            median_armor=median_armor,
            sample_count=len(values),
            peer_window=window_label,
            classification="within_tolerance",
            change_percent=0.0,
        ), warnings
    warnings.append(f"armor median from {len(values)} sample(s) {window_label}")
    return ArmorResult(
        new_armor=new_armor,
        median_armor=median_armor,
        sample_count=len(values),
        peer_window=window_label,
        classification="peer_median",
        change_percent=rounded_change_percent,
    ), warnings


def derive_weapon_damage(
    row: dict[str, str],
    stats: list[StatLine],
    weapon_index: dict[tuple[int, str, int, bool], list[CurvePoint]],
    config: dict[str, Any],
) -> tuple[WeaponDamageResult, list[str]]:
    warnings: list[str] = []
    old_min = row_float(row, "dmg_min1")
    old_max = row_float(row, "dmg_max1")
    delay = row_float(row, "delay")
    if not is_weapon(row) or old_min <= 0 or old_max <= 0 or delay <= 0:
        return WeaponDamageResult(new_min=old_min, new_max=old_max), warnings
    key_slot = slot_key(row)
    if key_slot is None:
        warnings.append("weapon damage unchanged: unknown weapon slot")
        return WeaponDamageResult(new_min=old_min, new_max=old_max), warnings
    caster = is_caster_weapon(row, stats, config)
    key = (row_int(row, "subclass"), key_slot, row_int(row, "Quality"), caster)
    points = weapon_index.get(key, [])
    min_samples = int(config.get("weapon", {}).get("min_samples", config.get("peer", {}).get("min_samples", 8)))
    values, window = nearby_values(
        points,
        row_int(row, "entry"),
        row_int(row, "ItemLevel"),
        peer_windows_for_ilvl(row_int(row, "ItemLevel"), config),
        min_samples,
    )
    if not values:
        curve = "caster" if caster else "physical"
        warnings.append(f"weapon damage unchanged: not enough comparable {curve} weapon samples")
        return WeaponDamageResult(new_min=old_min, new_max=old_max), warnings
    target_dps = median(values)
    old_dps = current_dps(row)
    window_label = "exact ilvl" if window == 0 else f"within {window} ilvl"
    tolerance = float(config.get("peer", {}).get("outlier_tolerance_percent", 15.0))
    if abs(percent_delta(old_dps, target_dps)) <= tolerance:
        warnings.append(f"weapon DPS unchanged: within +/-{tolerance:.0f}% peer tolerance from {len(values)} sample(s)")
        return WeaponDamageResult(
            new_min=old_min,
            new_max=old_max,
            median_dps=target_dps,
            sample_count=len(values),
            peer_window=window_label,
            change_percent=0.0,
        ), warnings
    old_avg = (old_min + old_max) / 2.0
    if old_avg <= 0:
        warnings.append("weapon damage unchanged: invalid old damage average")
        return WeaponDamageResult(new_min=old_min, new_max=old_max), warnings
    target_avg = target_dps * (delay / 1000.0)
    new_min = clean_damage_round(target_avg * (old_min / old_avg))
    new_max = clean_damage_round(target_avg * (old_max / old_avg))
    if new_max < new_min:
        new_min, new_max = new_max, new_min
    warnings.append(f"weapon DPS median from {len(values)} sample(s) {window_label}")
    new_dps = ((new_min + new_max) / 2.0) / (delay / 1000.0)
    return WeaponDamageResult(
        new_min=new_min,
        new_max=new_max,
        median_dps=target_dps,
        sample_count=len(values),
        peer_window=window_label,
        change_percent=percent_delta(new_dps, old_dps) if old_dps > 0 else 0.0,
    ), warnings


def percent_delta(value: float, target: float) -> float:
    if target <= 0:
        return 0.0
    return ((value - target) / target) * 100.0


def sql_number(value: float | int) -> str:
    if isinstance(value, float) and not value.is_integer():
        return f"{value:.3f}".rstrip("0").rstrip(".")
    return str(int(value))


def build_sql_assignments(result: dict[str, Any], rollback: bool = False) -> list[str]:
    assignments: list[str] = []
    prefix = "old" if rollback else "new"
    for change in result["stat_changes"]:
        value = change["old"] if rollback else change["new"]
        assignments.append(f"stat_value{change['slot']} = {sql_number(value)}")
    if result["old_armor"] != result["new_armor"]:
        assignments.append(f"armor = {sql_number(result[prefix + '_armor'])}")
    if result["old_damage_min"] != result["new_damage_min"] or result["old_damage_max"] != result["new_damage_max"]:
        assignments.append(f"dmg_min1 = {sql_number(result[prefix + '_damage_min'])}")
        assignments.append(f"dmg_max1 = {sql_number(result[prefix + '_damage_max'])}")
    if result.get("stats_count_column") and result["old_stats_count"] != result["new_stats_count"]:
        assignments.append(f"StatsCount = {sql_number(result[prefix + '_stats_count'])}")
    return assignments


def write_sql_files(
    results: list[dict[str, Any]],
    output_dir: Path,
    timestamp: str,
    config_path: Path,
    dry_run_reviewed: bool,
) -> tuple[Path, Path]:
    changed = [r for r in results if r["would_change"] and r["action"] == "updated"]
    backup_table = f"item_template_backup_budget_normalizer_{timestamp}"
    update_path = output_dir / f"{timestamp}_normalization_updates.sql"
    rollback_path = output_dir / f"{timestamp}_normalization_rollback.sql"
    entries = ", ".join(str(r["entry"]) for r in changed)

    header = [
        "-- ITEM BUDGET NORMALIZER BEGIN",
        "-- Generated by tools/item_budget_normalizer/normalize_items.py",
        f"-- Date: {dt.datetime.now().isoformat(timespec='seconds')}",
        f"-- Config: {config_path}",
        f"-- Dry-run reviewed: {'yes' if dry_run_reviewed else 'no'}",
        "",
    ]
    footer = ["-- ITEM BUDGET NORMALIZER END", ""]
    update_lines = header[:]
    rollback_lines = header[:]
    if changed:
        update_lines.extend([
            f"CREATE TABLE IF NOT EXISTS {backup_table} AS",
            "SELECT *",
            "FROM item_template",
            f"WHERE entry IN ({entries});",
            "",
        ])
    for result in changed:
        name = str(result["name"]).replace("\n", " ")
        update_assignments = build_sql_assignments(result, rollback=False)
        rollback_assignments = build_sql_assignments(result, rollback=True)
        if update_assignments:
            update_lines.append(f"-- {result['entry']} {name}")
            update_lines.append("UPDATE item_template")
            update_lines.append("SET")
            update_lines.append("  " + ",\n  ".join(update_assignments))
            update_lines.append(f"WHERE entry = {result['entry']};")
            update_lines.append("")
        if rollback_assignments:
            rollback_lines.append(f"-- {result['entry']} {name}")
            rollback_lines.append("UPDATE item_template")
            rollback_lines.append("SET")
            rollback_lines.append("  " + ",\n  ".join(rollback_assignments))
            rollback_lines.append(f"WHERE entry = {result['entry']};")
            rollback_lines.append("")

    update_lines.extend(footer)
    rollback_lines.extend(footer)
    update_path.write_text("\n".join(update_lines), encoding="utf-8")
    rollback_path.write_text("\n".join(rollback_lines), encoding="utf-8")
    return update_path, rollback_path


def evaluate_item(
    row: dict[str, str],
    config: dict[str, Any],
    calibration_index: CalibrationIndex | None,
    armor_index: dict[tuple[int, int, int], list[CurvePoint]],
    weapon_index: dict[tuple[int, str, int, bool], list[CurvePoint]],
    include_weapons: bool,
    include_armor: bool,
    exclude_trinkets: bool,
    write_sql: bool,
    budget_change_whitelist: set[int],
    origin: OriginInfo,
    sql_origin_allowed: bool,
    sql_origin_reason: str,
) -> dict[str, Any]:
    stat_weights = {int(k): float(v) for k, v in config["stat_weights"].items()}
    entry = row_int(row, "entry")
    class_id = row_int(row, "class")
    subclass = row_int(row, "subclass")
    inv_type = row_int(row, "InventoryType")
    quality = row_int(row, "Quality")
    warnings: list[str] = []
    skip_reasons: list[str] = []

    stats, stat_warnings, fatal_stat_warnings = parse_stats(row, stat_weights)
    warnings.extend(stat_warnings)
    if fatal_stat_warnings:
        skip_reasons.extend(fatal_stat_warnings)

    if class_id not in (2, 4):
        skip_reasons.append("not armor or weapon class")
    if class_id == 2 and not include_weapons:
        skip_reasons.append("weapons excluded by filter")
    if class_id == 4 and not include_armor:
        skip_reasons.append("armor excluded by filter")
    if exclude_trinkets and inv_type == 12:
        skip_reasons.append("trinket excluded")
    elif inv_type in EXCLUDED_INVENTORY_TYPES:
        skip_reasons.append(f"{EXCLUDED_INVENTORY_TYPES[inv_type]} excluded")
    if row_int(row, "ScalingStatDistribution") != 0 or row_int(row, "ScalingStatValue") != 0:
        skip_reasons.append("scaling item excluded")
    if quality not in config["quality_multipliers"]:
        skip_reasons.append(f"quality {quality} not configured")
    key = slot_key(row)
    if key is None:
        skip_reasons.append("cannot classify inventory slot")
    for pattern in configured_name_patterns(config):
        if pattern and pattern.lower() in item_name(row).lower():
            skip_reasons.append(f"name matched exclusion pattern {pattern}")
            break
    if not stats and not has_weapon_damage(row):
        skip_reasons.append("no normal stats and no weapon damage")

    normal_stats, special_stats = split_normal_and_special_stats(row, stats, config)
    role = classify_role(row, stats, config)
    old_budget = weighted_budget(normal_stats)
    special_weapon_budget = weighted_budget(special_stats)
    if special_stats:
        warnings.append("special weapon stat budget excluded from normal visible stat budget")
    sock_budget, sockets, socket_warnings = socket_budget(row, config)
    warnings.extend(socket_warnings)
    if sum(sockets.values()) > 0:
        warnings.append("socket budget counted; socket colors and socket bonus unchanged")
    effect_flags = special_effect_flags(row)
    if has_spell_payload(row):
        warnings.append("spell payload detected and left unchanged")
    if row_int(row, "itemset") > 0:
        warnings.append("itemset detected; set bonuses are not modeled")

    old_total_budget = old_budget + sock_budget
    target_info = target_budget(row, stats, config, calibration_index, role, old_total_budget, origin)
    total_target = target_info.budget
    target_visible_unclamped = old_budget
    target_visible = old_budget
    sql_blocked = bool(target_info.sql_blocked)
    allow_large_changes = bool(config["safety"].get("allow_large_changes", False)) or entry in budget_change_whitelist
    allow_special_effects = bool(config["safety"].get("allow_special_effect_items", False))
    if write_sql and not sql_origin_allowed:
        sql_blocked = True
        warnings.append(f"SQL blocked: {sql_origin_reason}")
    if effect_flags and not allow_special_effects:
        sql_blocked = True
        warnings.append("SQL blocked: special-effect/itemset rows require --allow-special-effects")
    if target_info.source == "no_peer_skip":
        warnings.append("normal stats unchanged: not enough comparable peer samples")
    elif target_info.source == "unclassified":
        warnings.append("normal stats unchanged: item could not be peer-classified")
    elif "zero_budget_skip" in target_info.source:
        warnings.append("normal stats unchanged: comparable peers have zero normal visible budget after special weapon budget split")
    elif "low_sample" in target_info.source:
        warnings.append("SQL blocked: exact peer sample count is below configured minimum")
    elif not target_info.outlier:
        tolerance = float(config.get("peer", {}).get("outlier_tolerance_percent", 15.0))
        warnings.append(f"normal stats unchanged: within +/-{tolerance:.0f}% peer tolerance")
    else:
        target_visible_unclamped = max(float(config["budget"]["minimum_visible_budget"]), total_target - sock_budget)
        if total_target > 0 and target_visible_unclamped > total_target - sock_budget:
            warnings.append("target visible budget raised by configured minimum")
        required_change_pct = abs(percent_delta(target_visible_unclamped, old_budget)) if old_budget > 0 else 0.0
        cap_pct = float(config["safety"].get("visible_budget_change_cap_percent", 20.0))
        if required_change_pct > cap_pct and not allow_large_changes:
            sql_blocked = True
            warnings.append(f"SQL blocked: peer target requires {required_change_pct:.1f}% visible budget change")
        target_visible, clamp_warning = clamp_visible_budget_target(old_budget, target_visible_unclamped, config, allow_large_changes)
        if clamp_warning:
            warnings.append(clamp_warning)

    old_armor = row_int(row, "armor")
    new_armor = old_armor
    armor_result = ArmorResult(new_armor=old_armor, classification="not_checked")
    old_min = row_float(row, "dmg_min1")
    old_max = row_float(row, "dmg_max1")
    new_min = old_min
    new_max = old_max
    weapon_result = WeaponDamageResult(new_min=old_min, new_max=old_max)

    stats_count_column = "StatsCount" in row
    old_stats_count = row_int(row, "StatsCount") if stats_count_column else len(stats)
    new_stats_count = len(stats)

    if not skip_reasons:
        if normal_stats and target_info.outlier:
            warnings.extend(normalize_stats(normal_stats, target_visible, config))
            warnings.extend(enforce_final_visible_budget_cap(normal_stats, old_budget, config, allow_large_changes))
        if is_armor(row):
            armor_result, armor_warnings = derive_armor(row, armor_index, config)
            new_armor = armor_result.new_armor
            warnings.extend(armor_warnings)
        if is_weapon(row):
            weapon_result, weapon_warnings = derive_weapon_damage(row, stats, weapon_index, config)
            new_min = weapon_result.new_min
            new_max = weapon_result.new_max
            warnings.extend(weapon_warnings)

    new_budget = weighted_budget(normal_stats, use_new=True) if normal_stats else old_budget
    new_total_budget = new_budget + sock_budget
    stat_changes = [
        {"slot": stat.slot, "type": stat.stat_type, "old": stat.old_value, "new": stat.new_value}
        for stat in stats
        if stat.new_value is not None and stat.new_value != stat.old_value
    ]
    armor_changed = old_armor != new_armor
    weapon_damage_changed = old_min != new_min or old_max != new_max
    would_change = bool(stat_changes or armor_changed or weapon_damage_changed)
    if not skip_reasons and would_change:
        sql_required_samples = required_sql_peer_samples(row, role, config)
        if target_info.sample_count > 0 and target_info.sample_count < sql_required_samples:
            sql_blocked = True
            warnings.append(sql_peer_sample_warning(row, role, target_info.sample_count, sql_required_samples))
        if armor_changed:
            allow_armor_updates = bool(config["safety"].get("allow_armor_updates", False))
            allow_large_armor_changes = bool(config["safety"].get("allow_large_armor_changes", False))
            armor_cap_pct = float(config["safety"].get("armor_change_cap_percent", 15.0))
            if not allow_armor_updates:
                sql_blocked = True
                warnings.append("SQL blocked: armor updates require allow_armor_updates=true")
            if armor_result.sample_count > 0 and armor_result.sample_count < sql_required_samples:
                sql_blocked = True
                warnings.append(sql_peer_sample_warning(row, role, armor_result.sample_count, sql_required_samples))
            if abs(armor_result.change_percent) > armor_cap_pct and not allow_large_armor_changes:
                sql_blocked = True
                warnings.append(f"SQL blocked: armor change requires {abs(armor_result.change_percent):.1f}% move, above +/-{armor_cap_pct:.0f}% armor cap")
        if weapon_damage_changed and weapon_result.sample_count > 0 and weapon_result.sample_count < sql_required_samples:
            sql_blocked = True
            warnings.append(sql_peer_sample_warning(row, role, weapon_result.sample_count, sql_required_samples))
    if skip_reasons:
        action = "skipped"
    elif would_change and sql_blocked:
        action = "report-only"
    elif would_change and write_sql:
        action = "updated"
    elif would_change:
        action = "report-only"
    else:
        action = "unchanged"

    sql_block_reasons: list[str] = []
    if sql_blocked:
        if target_info.sql_blocked:
            if target_info.source == "unclassified":
                sql_block_reasons.append("item could not be peer-classified")
            elif target_info.source == "no_peer_skip":
                sql_block_reasons.append("not enough comparable peer samples")
            elif "zero_budget_skip" in target_info.source:
                sql_block_reasons.append("comparable peers have zero normal visible budget after special weapon budget split")
            elif "low_sample" in target_info.source:
                sql_block_reasons.append("exact peer sample count is below configured minimum")
        for warning in warnings:
            if warning.startswith("SQL blocked: "):
                sql_block_reasons.append(warning.removeprefix("SQL blocked: ").strip())

    return {
        "entry": entry,
        "name": item_name(row),
        "class": class_id,
        "subclass": subclass,
        "subclass_label": subclass_label(class_id, subclass),
        "inventory_type": inv_type,
        "inventory_label": inventory_label(inv_type),
        "slot_key": key or "",
        "quality": quality,
        "quality_label": quality_label(quality),
        "origin_expansion": origin.expansion,
        "origin_label": origin.label,
        "origin_source": origin.source,
        "present_in_vanilla": origin.present_in_vanilla,
        "present_in_tbc": origin.present_in_tbc,
        "present_in_wrath": origin.present_in_wrath,
        "pvp_family": pvp_family(row),
        "required_level": row_int(row, "RequiredLevel"),
        "item_level": row_int(row, "ItemLevel"),
        "expansion_band": expansion_band(row_int(row, "ItemLevel")),
        "ilvl_band": ilvl_band(row_int(row, "ItemLevel")),
        "old_armor": old_armor,
        "new_armor": new_armor,
        "armor_peer_median": round(armor_result.median_armor, 3),
        "armor_sample_count": armor_result.sample_count,
        "armor_peer_window": armor_result.peer_window,
        "armor_classification": armor_result.classification,
        "armor_change_percent": round(armor_result.change_percent, 3),
        "old_damage_min": old_min,
        "old_damage_max": old_max,
        "new_damage_min": new_min,
        "new_damage_max": new_max,
        "weapon_damage_peer_median_dps": round(weapon_result.median_dps, 3),
        "weapon_damage_sample_count": weapon_result.sample_count,
        "weapon_damage_peer_window": weapon_result.peer_window,
        "weapon_damage_change_percent": round(weapon_result.change_percent, 3),
        "old_dps": current_dps(row),
        "new_dps": ((new_min + new_max) / 2.0) / (row_float(row, "delay") / 1000.0) if row_float(row, "delay") > 0 and new_min > 0 and new_max > 0 else 0.0,
        "old_stat_list": stat_list(stats, use_new=False),
        "new_stat_list": stat_list(stats, use_new=True),
        "old_weighted_budget": old_budget,
        "new_weighted_budget": new_budget,
        "special_weapon_budget": special_weapon_budget,
        "special_weapon_stat_list": stat_list(special_stats, use_new=False),
        "socket_budget": sock_budget,
        "target_budget": total_target,
        "target_visible_budget": target_visible,
        "target_visible_budget_unclamped": target_visible_unclamped,
        "target_source": target_info.source,
        "target_sample_count": target_info.sample_count,
        "peer_group": target_info.peer_key,
        "peer_window": target_info.peer_window,
        "peer_p10": target_info.p10,
        "peer_p25": target_info.p25,
        "peer_p75": target_info.p75,
        "peer_p90": target_info.p90,
        "peer_outlier": target_info.outlier,
        "sql_blocked": sql_blocked,
        "sql_block_reasons": "; ".join(dict.fromkeys(sql_block_reasons)),
        "percent_over_under_before": percent_delta(old_total_budget, total_target),
        "percent_over_under_after": percent_delta(new_total_budget, total_target),
        "action": action,
        "warnings": "; ".join(dict.fromkeys(warnings)),
        "skip_reasons": "; ".join(dict.fromkeys(skip_reasons)),
        "would_change": would_change,
        "stat_changes": stat_changes,
        "stats_count_column": stats_count_column,
        "old_stats_count": old_stats_count,
        "new_stats_count": new_stats_count,
        "role": role,
        "has_sockets": sum(sockets.values()) > 0,
        "has_spell_payload": has_spell_payload(row),
        "has_itemset": row_int(row, "itemset") > 0,
        "special_effect_flags": ",".join(effect_flags),
        "unknown_stats": "; ".join(reason for reason in skip_reasons if "unknown stat type" in reason),
    }


REPORT_FIELDS = [
    "entry",
    "name",
    "class",
    "subclass",
    "subclass_label",
    "inventory_type",
    "inventory_label",
    "slot_key",
    "role",
    "quality",
    "quality_label",
    "origin_expansion",
    "origin_label",
    "origin_source",
    "present_in_vanilla",
    "present_in_tbc",
    "present_in_wrath",
    "pvp_family",
    "required_level",
    "item_level",
    "expansion_band",
    "ilvl_band",
    "old_armor",
    "new_armor",
    "armor_peer_median",
    "armor_sample_count",
    "armor_peer_window",
    "armor_classification",
    "armor_change_percent",
    "old_damage_min",
    "old_damage_max",
    "new_damage_min",
    "new_damage_max",
    "weapon_damage_peer_median_dps",
    "weapon_damage_sample_count",
    "weapon_damage_peer_window",
    "weapon_damage_change_percent",
    "old_dps",
    "new_dps",
    "old_stat_list",
    "new_stat_list",
    "old_weighted_budget",
    "new_weighted_budget",
    "special_weapon_budget",
    "special_weapon_stat_list",
    "socket_budget",
    "target_budget",
    "target_visible_budget",
    "target_visible_budget_unclamped",
    "target_source",
    "target_sample_count",
    "peer_group",
    "peer_window",
    "peer_p10",
    "peer_p25",
    "peer_p75",
    "peer_p90",
    "peer_outlier",
    "sql_blocked",
    "sql_block_reasons",
    "percent_over_under_before",
    "percent_over_under_after",
    "action",
    "skip_reasons",
    "warnings",
    "special_effect_flags",
]


CALIBRATION_FIELDS = [
    "expansion_band",
    "ilvl_band",
    "item_level",
    "origin_expansion",
    "origin_label",
    "pvp_family",
    "quality",
    "quality_label",
    "slot_key",
    "role",
    "peer_group",
    "count",
    "median_visible_budget",
    "median_special_weapon_budget",
    "median_total_budget_with_sockets",
    "median_target_budget",
    "median_percent_over_under",
    "max_abs_median_percent_over_under",
    "peer_outlier_count",
    "sql_blocked_count",
]


def write_csv_report(results: list[dict[str, Any]], path: Path) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=REPORT_FIELDS)
        writer.writeheader()
        for result in results:
            writer.writerow({field: result.get(field, "") for field in REPORT_FIELDS})


def calibration_rows(results: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[Any, ...], list[dict[str, Any]]] = {}
    for result in results:
        if result["action"] == "skipped":
            continue
        key = (
            result["expansion_band"],
            result["ilvl_band"],
            result["item_level"],
            result["origin_expansion"],
            result["origin_label"],
            result["pvp_family"],
            result["quality"],
            result["quality_label"],
            result["slot_key"],
            result["role"],
            result["peer_group"],
        )
        grouped.setdefault(key, []).append(result)
    rows: list[dict[str, Any]] = []
    for key, items in grouped.items():
        visible = [float(item["old_weighted_budget"]) for item in items]
        special = [float(item["special_weapon_budget"]) for item in items]
        total = [float(item["old_weighted_budget"]) + float(item["socket_budget"]) for item in items]
        target = [float(item["target_budget"]) for item in items]
        pct = [float(item["percent_over_under_before"]) for item in items]
        med_pct = median(pct)
        rows.append({
            "expansion_band": key[0],
            "ilvl_band": key[1],
            "item_level": key[2],
            "origin_expansion": key[3],
            "origin_label": key[4],
            "pvp_family": key[5],
            "quality": key[6],
            "quality_label": key[7],
            "slot_key": key[8],
            "role": key[9],
            "peer_group": key[10],
            "count": len(items),
            "median_visible_budget": round(median(visible), 3),
            "median_special_weapon_budget": round(median(special), 3),
            "median_total_budget_with_sockets": round(median(total), 3),
            "median_target_budget": round(median(target), 3),
            "median_percent_over_under": round(med_pct, 3),
            "max_abs_median_percent_over_under": round(abs(med_pct), 3),
            "peer_outlier_count": sum(1 for item in items if str(item.get("peer_outlier", "")).lower() == "true" or item.get("peer_outlier") is True),
            "sql_blocked_count": sum(1 for item in items if str(item.get("sql_blocked", "")).lower() == "true" or item.get("sql_blocked") is True),
        })
    return sorted(rows, key=lambda row: (row["expansion_band"], row["ilvl_band"], row["item_level"], row["origin_expansion"], row["quality"], row["slot_key"], row["role"], row["pvp_family"], row["peer_group"]))


def write_calibration_csv(rows: list[dict[str, Any]], path: Path) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=CALIBRATION_FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def top_items(results: list[dict[str, Any]], key: str, reverse: bool, limit: int = 20) -> list[dict[str, Any]]:
    eligible = [result for result in results if result["action"] != "skipped" and result["target_budget"] > 0]
    return sorted(eligible, key=lambda item: float(item[key]), reverse=reverse)[:limit]


def table_rows(items: list[dict[str, Any]], fields: list[str]) -> list[str]:
    if not items:
        return ["None."]
    lines = ["| " + " | ".join(fields) + " |", "| " + " | ".join(["---"] * len(fields)) + " |"]
    for item in items:
        cells = []
        for field in fields:
            value = item.get(field, "")
            if isinstance(value, float):
                value = round(value, 3)
            cells.append(str(value).replace("|", "/"))
        lines.append("| " + " | ".join(cells) + " |")
    return lines


def grouped_counts(results: list[dict[str, Any]], field: str) -> list[tuple[str, int]]:
    counts: dict[str, int] = {}
    for result in results:
        value = str(result.get(field, "") or "")
        if not value:
            continue
        for part in value.split("; "):
            if part:
                counts[part] = counts.get(part, 0) + 1
    return sorted(counts.items(), key=lambda item: (-item[1], item[0]))


def calibration_is_sane(calibration: list[dict[str, Any]], config: dict[str, Any]) -> tuple[bool, float, list[dict[str, Any]]]:
    threshold = float(config["calibration"].get("sane_median_abs_percent", 5.0))
    min_samples = int(config["calibration"].get("sane_min_samples", config.get("peer", {}).get("min_samples", 8)))
    eligible = [row for row in calibration if int(row.get("count", 0)) >= min_samples]
    offenders = [
        row for row in eligible
        if float(row.get("max_abs_median_percent_over_under", 0.0)) > threshold
    ]
    worst = max((float(row.get("max_abs_median_percent_over_under", 0.0)) for row in eligible), default=0.0)
    return not offenders, worst, sorted(offenders, key=lambda row: float(row["max_abs_median_percent_over_under"]), reverse=True)


def allowed_sql_origin_summary(args: argparse.Namespace, custom_label: str) -> str:
    labels = parse_text_filter(args.origin_label)
    allowed: list[str] = []
    if args.custom_only:
        allowed.append(custom_label)
    allowed.extend(sorted(label for label in labels if label not in allowed))
    for expansion in sorted(parse_int_filter(args.origin_expansion)):
        allowed.append(f"origin_expansion:{expansion}")
    if parse_int_filter(args.entry) or args.entries_file:
        allowed.append("explicit_entries")
    return ", ".join(allowed) if allowed else "none"


def write_markdown_summary(
    results: list[dict[str, Any]],
    calibration: list[dict[str, Any]],
    config: dict[str, Any],
    path: Path,
    report_path: Path,
    calibration_path: Path,
    update_path: Path | None,
    rollback_path: Path | None,
    args: argparse.Namespace,
    config_path: Path,
) -> None:
    total = len(results)
    skipped = sum(1 for r in results if r["action"] == "skipped")
    report_only = sum(1 for r in results if r["action"] == "report-only")
    updated = sum(1 for r in results if r["action"] == "updated")
    unchanged = sum(1 for r in results if r["action"] == "unchanged")
    would_change = sum(1 for r in results if r["would_change"])
    weapons_changed = [r for r in results if r["would_change"] and r["class"] == 2]
    armor_changed = [r for r in results if r["would_change"] and r["class"] == 4 and r["old_armor"] != r["new_armor"]]
    socketed = [r for r in results if r["has_sockets"]]
    spell_payloads = [r for r in results if r["has_spell_payload"]]
    itemsets = [r for r in results if r["has_itemset"]]
    unknown_stats = [r for r in results if r["unknown_stats"]]
    bonus_armor = [r for r in results if r.get("armor_classification") == "bonus_armor"]
    peer_outliers = [r for r in results if r.get("peer_outlier") and r["action"] != "skipped"]
    sql_blocked = [r for r in results if r.get("sql_blocked") and r["action"] != "skipped"]
    sane, worst_calibration, calibration_offenders = calibration_is_sane(calibration, config)
    peer_settings = config.get("peer", {})
    calibration_min = int(config["calibration"].get("sane_min_samples", peer_settings.get("min_samples", 8)))
    low_sample_calibration = [row for row in calibration if int(row.get("count", 0)) < calibration_min]
    origin_manifest = config.get("origin", {}).get("manifest_path", "")
    origin_counts = sorted(
        ((label, sum(1 for r in results if r.get("origin_label") == label)) for label in {str(r.get("origin_label")) for r in results}),
        key=lambda item: item[0],
    )

    lines = [
        "# Item Budget Normalizer Summary",
        "",
        f"- Generated: {dt.datetime.now().isoformat(timespec='seconds')}",
        f"- Input CSV: `{args.input_csv}`",
        f"- Config: `{config_path}`",
        f"- SQL generation: {'enabled' if args.write_sql else 'disabled (report-only dry run)'}",
        f"- Origin manifest: `{origin_manifest or 'none'}`",
        f"- SQL origin filter: custom_only `{args.custom_only}`, labels `{args.origin_label or 'none'}`, expansions `{args.origin_expansion or 'none'}`",
        f"- allowed_sql_origins = `{allowed_sql_origin_summary(args, str(config['origin'].get('custom_label', 'custom_or_unknown')))}`",
        f"- Origin expansion required for SQL: `{config['safety'].get('require_sql_origin_expansion', True)}`; missing origin rows require explicit `{config['origin'].get('custom_label', 'custom_or_unknown')}` selection",
        f"- Target mode: `{config['budget'].get('target_mode', 'formula')}`",
        f"- Peer minimum samples: `{peer_settings.get('min_samples', 8)}`; SQL minimum `{peer_settings.get('sql_min_samples', peer_settings.get('min_samples', 8))}`; strict SQL minimum `{peer_settings.get('strict_sql_min_samples', 10)}` for jewelry/weapons/shields/tank; outlier tolerance: `+/-{peer_settings.get('outlier_tolerance_percent', 15.0)}%`",
        f"- Calibration sanity minimum samples: `{calibration_min}`",
        f"- Visible budget per-pass cap: `+/-{config['safety'].get('visible_budget_change_cap_percent', 20.0)}%`; allow large changes: `{config['safety'].get('allow_large_changes', False)}`",
        f"- Armor SQL updates allowed: `{config['safety'].get('allow_armor_updates', False)}`; armor cap `+/-{config['safety'].get('armor_change_cap_percent', 15.0)}%`; allow large armor changes: `{config['safety'].get('allow_large_armor_changes', False)}`",
        f"- Allow special-effect/itemset SQL: `{config['safety'].get('allow_special_effect_items', False)}`",
        f"- Calibration sanity: {'sane' if sane else 'not sane'}; worst grouped median `{round(worst_calibration, 3)}%`",
        f"- Filters: min ilvl `{args.min_ilvl}`, max ilvl `{args.max_ilvl}`, required level `{args.required_level}`, qualities `{args.quality or 'any'}`",
        "",
        "## Output Files",
        "",
        f"- Before/after CSV: `{report_path}`",
        f"- Calibration CSV: `{calibration_path}`",
    ]
    if update_path and rollback_path:
        lines.extend([f"- Update SQL: `{update_path}`", f"- Rollback SQL: `{rollback_path}`"])
    else:
        lines.append("- Update SQL: not generated")
        lines.append("- Rollback SQL: not generated")

    lines.extend([
        "",
        "## Counts",
        "",
        f"- Report rows: {total}",
        f"- Skipped: {skipped}",
        f"- Report-only changes: {report_only}",
        f"- Updated SQL rows: {updated}",
        f"- Unchanged: {unchanged}",
        f"- Would change if SQL were enabled: {would_change}",
        f"- Special weapon-stat rows: {sum(1 for r in results if r['special_weapon_budget'] > 0)}",
        f"- Likely bonus-armor rows: {len(bonus_armor)}",
        f"- Peer outliers: {len(peer_outliers)}",
        f"- SQL-blocked rows: {len(sql_blocked)}",
        f"- Low-sample calibration groups ignored by sanity: {len(low_sample_calibration)}",
        "",
        "## Origin Coverage",
        "",
    ])
    if origin_counts:
        lines.extend(["| origin_label | count |", "| --- | --- |"])
        for label, count in origin_counts:
            lines.append(f"| {label} | {count} |")
    else:
        lines.append("None.")

    lines.extend([
        "",
        "## Skipped Items And Why",
        "",
    ])
    skip_counts = grouped_counts(results, "skip_reasons")
    if skip_counts:
        lines.extend(["| Reason | Count |", "| --- | --- |"])
        for reason, count in skip_counts[:40]:
            lines.append(f"| {reason.replace('|', '/')} | {count} |")
    else:
        lines.append("None.")

    lines.extend([
        "",
        "## Most Overbudget Before Normalization",
        "",
        *table_rows(top_items(results, "percent_over_under_before", True), ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "slot_key", "role", "target_source", "target_sample_count", "percent_over_under_before", "action"]),
        "",
        "## Most Underbudget Before Normalization",
        "",
        *table_rows(top_items(results, "percent_over_under_before", False), ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "slot_key", "role", "target_source", "target_sample_count", "percent_over_under_before", "action"]),
        "",
        "## Calibration Sanity Offenders",
        "",
        *table_rows(calibration_offenders[:40], ["expansion_band", "ilvl_band", "item_level", "origin_label", "pvp_family", "quality_label", "slot_key", "role", "peer_group", "count", "median_percent_over_under", "median_target_budget"]),
        "",
        "## Low-Sample Calibration Groups",
        "",
        *table_rows(sorted(low_sample_calibration, key=lambda row: (-float(row["max_abs_median_percent_over_under"]), row["origin_label"]))[:40], ["expansion_band", "ilvl_band", "item_level", "origin_label", "pvp_family", "quality_label", "slot_key", "role", "count", "median_percent_over_under", "median_target_budget"]),
        "",
        "## SQL Blocked Rows",
        "",
        *table_rows(sql_blocked[:40], ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "slot_key", "role", "target_source", "target_sample_count", "percent_over_under_before", "sql_block_reasons", "warnings"]),
        "",
        "## Weapons Changed",
        "",
        *table_rows(weapons_changed[:30], ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "subclass_label", "old_dps", "new_dps", "action", "warnings"]),
        "",
        "## Armor Changed",
        "",
        *table_rows(armor_changed[:30], ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "subclass_label", "old_armor", "new_armor", "armor_peer_median", "armor_sample_count", "armor_classification", "armor_change_percent", "action", "sql_block_reasons", "warnings"]),
        "",
        "## Likely Bonus Armor",
        "",
        *table_rows(bonus_armor[:30], ["entry", "name", "origin_label", "pvp_family", "item_level", "quality_label", "subclass_label", "old_armor", "armor_peer_median", "armor_sample_count", "armor_classification", "warnings"]),
        "",
        "## Items With Sockets",
        "",
        *table_rows(socketed[:30], ["entry", "name", "origin_label", "item_level", "quality_label", "slot_key", "socket_budget", "action", "warnings"]),
        "",
        "## Spell Payloads Detected",
        "",
        *table_rows(spell_payloads[:30], ["entry", "name", "origin_label", "item_level", "quality_label", "slot_key", "action", "warnings"]),
        "",
        "## Itemsets Detected",
        "",
        *table_rows(itemsets[:30], ["entry", "name", "origin_label", "item_level", "quality_label", "slot_key", "action", "warnings"]),
        "",
        "## Unknown Stat Types",
        "",
        *table_rows(unknown_stats[:50], ["entry", "name", "item_level", "quality_label", "unknown_stats", "skip_reasons"]),
        "",
        "## Calibration Snapshot",
        "",
        "Grouped by expansion, item-level band, exact item level, origin, PvP family, quality, slot, inferred role, and peer group. Full data is in the calibration CSV.",
        "",
    ])
    calibration_sample = sorted(calibration, key=lambda row: (-int(row["count"]), row["expansion_band"], row["ilvl_band"]))[:40]
    lines.extend(table_rows(calibration_sample, [
        "count",
        "expansion_band",
        "ilvl_band",
        "item_level",
        "origin_label",
        "pvp_family",
        "quality_label",
        "slot_key",
        "role",
        "peer_group",
        "median_visible_budget",
        "median_special_weapon_budget",
        "median_total_budget_with_sockets",
        "median_target_budget",
        "median_percent_over_under",
        "peer_outlier_count",
        "sql_blocked_count",
    ]))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate safe item budget normalization reports and optional SQL.")
    parser.add_argument("--input-csv", required=True, type=Path, help="CSV export of item_template.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG, help="YAML config path.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Directory for generated reports.")
    parser.add_argument("--min-ilvl", type=int)
    parser.add_argument("--max-ilvl", type=int)
    parser.add_argument("--required-level", type=int)
    parser.add_argument("--quality", action="append", help="Quality id. Can be repeated or comma-separated.")
    parser.add_argument("--entry", action="append", help="Entry id. Can be repeated or comma-separated.")
    parser.add_argument("--entries-file", type=Path, help="Text file containing entry ids.")
    parser.add_argument("--budget-change-whitelist", action="append", help="Entry id exempt from the per-pass visible stat budget change cap.")
    parser.add_argument("--budget-change-whitelist-file", type=Path, help="Text file containing entries exempt from the visible stat budget change cap.")
    parser.add_argument("--origin-manifest", type=Path, help="CSV manifest with entry, origin_expansion, and origin_label columns.")
    parser.add_argument("--origin-label", action="append", help="Origin label allowed for SQL updates. Can be repeated or comma-separated.")
    parser.add_argument("--origin-expansion", action="append", help="Origin expansion id allowed for SQL updates. Can be repeated or comma-separated.")
    parser.add_argument("--custom-only", action="store_true", help="Only allow SQL updates for custom_or_unknown origin rows, plus explicit --entry whitelist rows.")
    parser.add_argument("--include-weapons", action="store_true", help="Include weapons. If neither include flag is set, weapons and armor are both included.")
    parser.add_argument("--include-armor", action="store_true", help="Include armor. If neither include flag is set, weapons and armor are both included.")
    parser.add_argument("--exclude-trinkets", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--report-only", action="store_true", help="Explicit dry-run/report-only mode. This is already the default unless --write-sql is passed.")
    parser.add_argument("--write-sql", action="store_true", help="Generate update and rollback SQL files. Never executes SQL.")
    parser.add_argument("--allow-large-run", action="store_true", help="Allow SQL generation above safety.max_sql_updates.")
    parser.add_argument("--allow-large-changes", action="store_true", help="Allow per-item visible stat budget changes beyond safety.visible_budget_change_cap_percent.")
    parser.add_argument("--allow-armor-updates", action="store_true", help="Allow armor SQL updates. Default is report-only armor normalization.")
    parser.add_argument("--allow-large-armor-changes", action="store_true", help="Allow armor SQL updates beyond safety.armor_change_cap_percent.")
    parser.add_argument("--allow-special-effects", action="store_true", help="Allow SQL updates for rows with spell payloads or itemsets.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    config = load_config(args.config)
    if args.allow_large_changes:
        config["safety"]["allow_large_changes"] = True
    if args.allow_armor_updates:
        config["safety"]["allow_armor_updates"] = True
    if args.allow_large_armor_changes:
        config["safety"]["allow_large_armor_changes"] = True
    if args.allow_special_effects:
        config["safety"]["allow_special_effect_items"] = True
    if args.origin_manifest:
        config["origin"]["manifest_path"] = str(args.origin_manifest)
    stat_weights = {int(k): float(v) for k, v in config["stat_weights"].items()}
    qualities = parse_int_filter(args.quality)
    entries = parse_int_filter(args.entry) | read_entries_file(args.entries_file)
    sql_origin_labels = parse_text_filter(args.origin_label)
    sql_origin_expansions = parse_int_filter(args.origin_expansion)
    custom_label = str(config["origin"].get("custom_label", "custom_or_unknown"))
    origin_manifest_path = Path(str(config["origin"].get("manifest_path", ""))) if str(config["origin"].get("manifest_path", "")).strip() else None
    origin_index = load_origin_manifest(origin_manifest_path, custom_label)
    budget_change_whitelist = (
        parse_int_filter(args.budget_change_whitelist)
        | read_entries_file(args.budget_change_whitelist_file)
        | configured_entry_set(config, "safety", "budget_change_whitelist")
    )
    include_weapons = args.include_weapons
    include_armor = args.include_armor
    if not include_weapons and not include_armor:
        include_weapons = True
        include_armor = True

    if not args.input_csv.exists():
        print(f"Input CSV not found: {args.input_csv}", file=sys.stderr)
        return 2

    with args.input_csv.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    selected = [row for row in rows if row_matches_filters(row, args, entries, qualities)]
    armor_index = build_armor_index(rows)
    weapon_index = build_weapon_index(rows, stat_weights, config)
    calibration_index = build_budget_calibration_index(rows, stat_weights, config, origin_index)

    results = []
    for row in selected:
        origin = origin_for_row(row, origin_index, custom_label)
        origin_allowed, origin_reason = sql_origin_allowed(
            row_int(row, "entry"),
            origin,
            entries,
            sql_origin_labels,
            sql_origin_expansions,
            args.custom_only,
            bool(config["safety"].get("require_sql_origin_filter", True)),
            bool(config["safety"].get("require_sql_origin_expansion", True)),
            custom_label,
        )
        results.append(evaluate_item(
            row,
            config,
            calibration_index,
            armor_index,
            weapon_index,
            include_weapons,
            include_armor,
            args.exclude_trinkets,
            args.write_sql,
            budget_change_whitelist,
            origin,
            origin_allowed,
            origin_reason,
        ))

    changed = [result for result in results if result["would_change"] and result["action"] != "skipped"]
    sql_changed = [result for result in results if result["action"] == "updated"]
    max_updates = int(config["safety"]["max_sql_updates"])
    sql_allowed = args.write_sql
    large_run_refused = False
    origin_filter_refused = False
    if args.write_sql and bool(config["safety"].get("require_sql_origin_filter", True)) and not (args.custom_only or sql_origin_labels or sql_origin_expansions or entries):
        sql_allowed = False
        origin_filter_refused = True
        for result in results:
            if result["action"] == "updated":
                result["action"] = "report-only"
                warning = "SQL refused: pass --custom-only, --origin-label, --origin-expansion, or explicit --entry"
                result["warnings"] = "; ".join([part for part in [result["warnings"], warning] if part])
    if sql_allowed and len(sql_changed) > max_updates and not args.allow_large_run:
        sql_allowed = False
        large_run_refused = True
        for result in results:
            if result["action"] == "updated":
                result["action"] = "report-only"
                warning = "SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run"
                result["warnings"] = "; ".join([part for part in [result["warnings"], warning] if part])

    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.output_dir / f"{timestamp}_normalization_report.csv"
    summary_path = args.output_dir / f"{timestamp}_normalization_summary.md"
    calibration_path = args.output_dir / f"{timestamp}_calibration.csv"
    calibration = calibration_rows(results)
    sane_calibration, worst_calibration, _ = calibration_is_sane(calibration, config)
    calibration_refused = False
    if sql_allowed and not sane_calibration:
        sql_allowed = False
        calibration_refused = True
        for result in results:
            if result["action"] == "updated":
                result["action"] = "report-only"
                warning = f"SQL refused: calibration median sanity failed; worst grouped median {worst_calibration:.3f}%"
                result["warnings"] = "; ".join([part for part in [result["warnings"], warning] if part])

    write_csv_report(results, report_path)
    write_calibration_csv(calibration, calibration_path)

    update_path: Path | None = None
    rollback_path: Path | None = None
    if sql_allowed and any(result["action"] == "updated" for result in results):
        dry_run_reviewed = bool(config["safety"].get("dry_run_reviewed", False))
        update_path, rollback_path = write_sql_files(results, args.output_dir, timestamp, args.config, dry_run_reviewed)
        custom_sql_name = args.custom_only or sql_origin_labels == {custom_label}
        if custom_sql_name:
            renamed_update = update_path.with_name(update_path.stem + "_zz_custom" + update_path.suffix)
            renamed_rollback = rollback_path.with_name(rollback_path.stem + "_zz_custom" + rollback_path.suffix)
            update_path.replace(renamed_update)
            rollback_path.replace(renamed_rollback)
            update_path, rollback_path = renamed_update, renamed_rollback

    write_markdown_summary(results, calibration, config, summary_path, report_path, calibration_path, update_path, rollback_path, args, args.config)

    print(f"Rows selected: {len(selected)}")
    print(f"Rows skipped: {sum(1 for r in results if r['action'] == 'skipped')}")
    print(f"Rows that would change: {len(changed)}")
    print(f"Report CSV: {report_path}")
    print(f"Calibration CSV: {calibration_path}")
    print(f"Markdown summary: {summary_path}")
    if update_path and rollback_path:
        print(f"Update SQL: {update_path}")
        print(f"Rollback SQL: {rollback_path}")
    else:
        print("Update SQL: not generated")
        print("Rollback SQL: not generated")
    if large_run_refused:
        print(f"SQL generation refused: {len(sql_changed)} SQL rows exceeds safety.max_sql_updates={max_updates}.", file=sys.stderr)
        return 3
    if calibration_refused:
        print(f"SQL generation refused: calibration median sanity failed; worst grouped median {worst_calibration:.3f}%.", file=sys.stderr)
        return 4
    if origin_filter_refused:
        print("SQL generation refused: origin filter required for SQL output.", file=sys.stderr)
        return 5
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
