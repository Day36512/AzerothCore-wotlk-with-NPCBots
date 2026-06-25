# Item Budget Normalizer

Safe report-first item budget normalization for AzerothCore 3.3.5a `item_template` exports.

The tool reads a CSV export, classifies armor and weapons, joins an origin manifest, compares each item against nearby comparable peer groups, computes current weighted stat budget, socket hidden budget, peer median budget, armor medians, and weapon DPS medians, then writes reports. It never connects to MySQL and never executes SQL.

## First Pass

Recommended first-pass command for level 70 heroic/raid gear:

```powershell
& "C:\Users\Randall\AppData\Local\Python\bin\python.exe" .\tools\item_budget_normalizer\normalize_items.py `
  --input-csv "F:\Downloads\First_pass_normalization.csv" `
  --min-ilvl 110 `
  --max-ilvl 164 `
  --required-level 70 `
  --output-dir .\tools\item_budget_normalizer\reports
```

This generates:

- `*_normalization_report.csv`: before/after item-level report
- `*_calibration.csv`: peer-group median budget sanity report by expansion, ilvl band, exact ilvl, origin, PvP family, quality, slot, and role
- `*_normalization_summary.md`: human-readable summary

No SQL is generated unless `--write-sql` is passed.

## Peer Normalization Model

The normalizer does not rebuild items from a global item-level budget curve. It compares each row to nearby peers with the same origin expansion, quality, slot, armor subclass when relevant, weapon category when relevant, PvP family, and inferred role.

Origin comes from `origin.manifest_path`, which should point at `item_origin_manifest.csv`. Rows missing from the manifest are labeled `custom_or_unknown`.

Peer lookup uses exact ilvl first for TBC/WotLK-style endgame items, then expands to +/-6 and +/-13 ilvl. Leveling gear uses +/-5 and then +/-10. A peer group must meet `peer.min_samples` before it can drive stat changes. SQL eligibility uses `peer.sql_min_samples`, and jewelry, weapons, shields, and tank gear require `peer.strict_sql_min_samples` because those buckets are noisier.

Items inside `peer.outlier_tolerance_percent` of their peer median are left unchanged. Outliers are scaled toward the peer median while preserving their original stat ratios. Sockets count toward total item value, but socket colors and socket bonuses are never changed.

Caster weapon spell power, weapon attack power, ranged attack power, and feral attack power are reported as special weapon budgets instead of ordinary visible stat budget. Weapon DPS is normalized separately against comparable weapon DPS peers.

Grand Marshal, High Warlord, and Gladiator-season gear is tagged with `pvp_family` and kept out of ordinary PvE/world peer groups.

## SQL Generation

SQL output is intentionally guarded:

```powershell
& "C:\Users\Randall\AppData\Local\Python\bin\python.exe" .\tools\item_budget_normalizer\normalize_items.py `
  --input-csv "F:\Downloads\First_pass_normalization.csv" `
  --entry 34610 `
  --write-sql `
  --output-dir .\tools\item_budget_normalizer\reports
```

The SQL files are only migrations. The script does not execute them.

For broad SQL runs, the script refuses to generate updates above `safety.max_sql_updates` unless `--allow-large-run` is passed. Review the dry-run reports first.

Rows with too few peers, failed eligible calibration sanity, a required visible-stat budget move beyond `safety.visible_budget_change_cap_percent`, armor changes beyond `safety.armor_change_cap_percent`, spell payloads, or itemsets stay report-only unless the matching explicit allow flag is passed.

SQL generation requires an origin filter by default. Use `--custom-only` for custom/unknown rows, `--origin-label` / `--origin-expansion` for an intentional stock-origin run, or explicit `--entry` values for hand-picked whitelist rows. Rows without a resolved `origin_expansion` remain report-only unless `custom_or_unknown` is explicitly selected. `--custom-only` SQL files are suffixed `_zz_custom` and may only include `custom_or_unknown` rows plus explicit entry whitelist rows.

The Markdown summary header prints `allowed_sql_origins` so broad runs are reviewable at a glance.

## What It Changes

The normalizer only proposes changes to:

- `stat_value1` through `stat_value10`, preserving existing stat types and slots
- `armor`, when enough comparable armor samples exist and armor SQL updates are explicitly allowed
- `dmg_min1` and `dmg_max1`, when enough comparable weapon samples exist

It does not change names, display IDs, quality, class/subclass, inventory type, allowable class/race, required level, item level, sockets, socket bonuses, spell payloads, itemsets, flags, material, durability, delay, sheath, or required skill.

## Safety Rules

Items are skipped when the tool cannot confidently classify or preserve them, including:

- unknown stat types
- duplicate or non-contiguous stat slots
- scaling/heirloom rows
- trinkets by default
- shirts, tabards, bags, ammo/projectiles
- configured placeholder/test name patterns
- rows with no normal stats and no weapon damage

Armor normalization is report-only by default. Pass `--allow-armor-updates` or set `safety.allow_armor_updates: true` before allowing armor SQL. Armor changes larger than `safety.armor_change_cap_percent` are still SQL-blocked unless `--allow-large-armor-changes` / `safety.allow_large_armor_changes` is enabled. If old armor is more than `armor.bonus_armor_threshold_percent` above the peer median, the row is classified as `bonus_armor` and armor is not normalized automatically.

Weapons with procs and armor with use/equip effects are reported and left unchanged. Their visible stats may be evaluated in report-only mode, but SQL updates for spell payload or itemset rows require `--allow-special-effects`.

## Config

Tune `item_budget_config.yml` for:

- stat weights
- socket budget assumptions
- origin manifest path
- peer sample thresholds and outlier tolerance
- eligible calibration sanity threshold
- visible budget, armor change caps, and large-change allow lists
- custom-only / origin-filtered SQL behavior
- armor and weapon median fallback thresholds
- SQL safety cap
- placeholder/test name exclusions

The config is deliberately plain YAML and uses only simple mappings so the script can run without installing PyYAML.
