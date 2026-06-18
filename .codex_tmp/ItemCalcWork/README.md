# Wrath 3.3.5 Item Level Calculator - Reference Data Build v4.6.6

Static/offline WotLK 3.3.5 itemization workbench for AzerothCore / TrinityCore `item_template` data. Open `index.html` directly, or run the optional local MySQL helper if you want the tool to query your local world database.

## What This Local Build Adds

- Batch generator tab inspired by Brytenwally/Azerothcore-RandomItemGenerator, using this calculator's existing exponent/stat-mod/reference-budget model instead of raw stat sums.
- Forward builder shell can now seed batches with item-level ranges, quality pools, budget variance, role blueprints, stat-density rules, and even/weighted/varied stat allocation.
- Generated batches include reference-guided names, display IDs, material, sheath, required level, sell price, SQL `INSERT` output, JSON output, and WDBX-style CSV export.
- Reference SQL now exports `displayid`, `Material`, `sheath`, and `SellPrice` so loaded DB data can drive visual and client-side metadata.

## What v4.6.6 Adds

- Compare item now separates formula verdict, reference verdict, and design verdict before showing normalization candidates.
- Design verdicts flag weak profile-neighbor similarity, set/tier items, sockets, socket bonuses, spell payloads, formula/reference disagreement, and excluded generic-normalization cases.
- Compare CSV export now includes recommendation fields such as reference quality, design flags, auto-apply allowance, recommended action, target source, and candidate normalized stats.

## What v4.6.5 Adds

- Compare item now shows separate reference-based and formula-based normalization suggestions.
- Compare CSV export now includes separate reference and formula normalization sections.

## What v4.6.4 Adds

- Compare item now shows a normalization suggestion that scales the current passive stat line toward the reference/model budget.
- Compare item can export the current audit, normalization suggestion, and nearest DB neighbors as CSV.

## What v4.6.3 Adds

- Reference passive-budget scaling and reverse/compare neighbors now use stat archetype matching.
- Spell-power leather/mail/plate rows are kept away from agi/strength physical rows when the target stat line is clear.
- Hybrid armor such as mail and paladin plate is matched by actual stat purpose instead of only armor subclass and slot.

## What v4.6.2 Adds

- Presets were rebuilt from actual local `item_template` PvE armor examples by 2/3/4/5 stat-count bucket.
- Melee, caster, and tank presets all carry stamina in every stat-count mode.
- DPS stamina is present but weighted as supporting budget, while tank stamina remains a leading budget stat.

## What v4.6.1 Adds

- Presets now support 2-stat, 3-stat, 4-stat, and 5-stat versions for melee, caster, and tank profiles.
- PvE DPS presets avoid stamina-dominant output; tank remains the stamina-leading preset.
- Socket bonus value is now an internal model constant instead of a user-entered budget field.

## What v4.6 Adds

- Forward-builder presets now use fixed Wrath-style budget shares instead of copying one database row too literally.
- Socket modeling now shows the item socket penalty, estimated gem value, estimated socket bonus, and the net budget after gemming.
- Reference data can exclude PvP-stat rows, set pieces, and spell-payload/proc items when those rows would be bad neighbors for normal item tuning.
- Compare item mode lets you enter an `item_template.entry` from loaded reference data and see observed budget versus model expectations.
- Spell payload audit flags use, equip, proc, and learn-spell payloads so hidden power is easier to spot before trusting a row.

## What v4.5 Adds

- Robust DB reference estimates for weapon DPS, damage spread, armor, and passive-budget scale. Nearby rows still vote, but obvious outliers are trimmed before they can skew the result.
- Reverse estimates now show the closest actual loaded `item_template` rows, not just formula candidates.
- Reference rows now carry DB fact flags such as set item, spell payload, class mask, race mask, and required skill.
- The SQL export query includes more factual columns: `description`, `itemset`, `AllowableClass`, `AllowableRace`, `Flags`, `FlagsExtra`, `RequiredSkill`, `RequiredSkillRank`, `bonding`, and spell trigger/cooldown metadata.
- Optional localhost-only MySQL helper for direct local database loading.

## Reference Mode

When reference data is loaded, the forward builder uses your actual database in four ways:

1. Weapon damage: nearby real weapons by quality, subclass, slot group, speed, and item level.
2. Armor: nearby real armor by quality, armor type, slot, and item level.
3. Passive budget: formula budget scaled by observed nearby database item budgets.
4. Stat profile: nearby real items vote on normalized stat-budget shares.

The formula model remains as a fallback when the local corpus is thin.

## CSV / JSON Workflow

1. Open `index.html`.
2. Go to **Reference data**.
3. Copy the SQL query.
4. Run it against your 3.3.5 world database.
5. Export as CSV with headers.
6. Load the CSV on the left side of the Reference data tab.
7. Keep **Reference mode** set to **Prefer loaded DB references**.

The app does not upload anything. CSV/JSON parsing happens in the browser.

## Optional Local MySQL Helper

Browsers cannot safely connect to MySQL directly. The helper is a tiny localhost-only Node server that reads credentials from a local config file, runs the same reference SQL, and returns rows to the browser.

Setup:

```powershell
npm.cmd install
Copy-Item tools\mysql-reference.config.example.json tools\mysql-reference.config.json
notepad tools\mysql-reference.config.json
npm.cmd run mysql-helper
```

Then open:

```text
http://127.0.0.1:33544
```

Or keep using the static page and click **Load from local MySQL** in the Reference data tab.

Default helper config path:

```text
tools/mysql-reference.config.json
```

You can override it with:

```powershell
$env:ITEMLEVELCALC_MYSQL_CONFIG = "A:\path\to\mysql-reference.config.json"
npm.cmd run mysql-helper
```

The helper binds to `127.0.0.1` by default. Credentials stay in the local config file and are not embedded in browser JavaScript.

## Reverse Estimator

The reverse estimator now reports two kinds of evidence:

- Formula candidate band: which item levels best match the entered stat/armor/DPS payload.
- Closest loaded DB items: actual `item_template` rows closest to the entered item, with residuals for budget, stat profile, armor, and DPS.

This is useful when tuning custom gear because you can see whether the model is matching real database neighbors or only the fallback curve.

## Compare Item Mode

Load reference data, open **Compare item**, enter an item entry, and click **Compare item**. The tool reports the row's visible passive budget, socket/gem model, spell payload flags, nearest reference neighbors, and expected model budget.

Reference quality controls on the Reference data tab affect the Compare item neighbor list. Keep those controls strict when tuning clean starter gear, then loosen them when you intentionally want to inspect set items, PvP gear, or proc-heavy items.

## Limitations

- Hidden use/proc/equip spell power is not decoded into exact budget cost.
- Socket bonuses and gem values are modeled as estimated budget, not decoded from DBC gem design tables.
- Random suffix/property and scaling-stat rows are excluded by default.
- Set items and spell-payload items can still be used as weak references, but they are flagged and down-weighted because their true power is not fully represented by passive stat columns.
- Archetype detection is heuristic. Use the audit tables and nearest DB rows when tuning important custom items.
