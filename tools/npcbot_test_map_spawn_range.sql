USE `acore_world`;

-- Manual NPCBot test-map spawn dump.
--
-- Run this against the WORLD database while worldserver is offline, then restart.
-- It is intentionally stored under tools/ so it is not auto-applied by the DB updater.
--
-- This mirrors the durable side of `.npcbot spawn`:
--   1. inserts free bot rows into characters_npcbot
--   2. inserts persistent creature spawns into world.creature
--
-- Defaults below place the selected bots in a compact holding grid on map 13.
-- Change the map/coordinates to your own test holding area before running.

SET @NPCBOT_CHARACTERS_DB := 'acore_characters';

SET @NPCBOT_ENTRY_MIN := 70001;
SET @NPCBOT_ENTRY_MAX := 73586;

SET @NPCBOT_MAP := 13;
SET @NPCBOT_ORIGIN_X := -46.6300;
SET @NPCBOT_ORIGIN_Y := 52.8400;
SET @NPCBOT_ORIGIN_Z := -144.7100;
SET @NPCBOT_ORIENTATION := 1.5708;

SET @NPCBOT_GRID_COLUMNS := 10;
SET @NPCBOT_GRID_SPACING := 2.0000;

SET @NPCBOT_SPAWN_MASK := 1;
SET @NPCBOT_PHASE_MASK := 1;
SET @NPCBOT_RESPAWN_SECONDS := 120;
SET @NPCBOT_WANDER_DISTANCE := 0.0000;
SET @NPCBOT_COMMENT := 'NPCBot test-map spawn range';

SET @NPCBOT_GRID_COLUMNS := GREATEST(1, @NPCBOT_GRID_COLUMNS);
SET @NPCBOT_CHARACTERS_DB_ESCAPED := REPLACE(@NPCBOT_CHARACTERS_DB, '`', '``');

SELECT
    @NPCBOT_ENTRY_MIN AS `entry_min`,
    @NPCBOT_ENTRY_MAX AS `entry_max`,
    @NPCBOT_MAP AS `map`,
    @NPCBOT_ORIGIN_X AS `origin_x`,
    @NPCBOT_ORIGIN_Y AS `origin_y`,
    @NPCBOT_ORIGIN_Z AS `origin_z`;

SELECT COUNT(*) AS `npcbot_templates_in_range`
FROM `creature_template` `ct`
INNER JOIN `creature_template_npcbot_extras` `extras`
    ON `extras`.`entry` = `ct`.`entry`
WHERE `ct`.`entry` BETWEEN @NPCBOT_ENTRY_MIN AND @NPCBOT_ENTRY_MAX
    AND `ct`.`entry` <> 70000
    AND `ct`.`entry` <> 70552;

SELECT COUNT(DISTINCT `ct`.`entry`) AS `npcbot_templates_already_in_creature`
FROM `creature_template` `ct`
INNER JOIN `creature_template_npcbot_extras` `extras`
    ON `extras`.`entry` = `ct`.`entry`
INNER JOIN `creature` `existing_creature`
    ON `existing_creature`.`id1` = `ct`.`entry`
    OR `existing_creature`.`id2` = `ct`.`entry`
    OR `existing_creature`.`id3` = `ct`.`entry`
WHERE `ct`.`entry` BETWEEN @NPCBOT_ENTRY_MIN AND @NPCBOT_ENTRY_MAX
    AND `ct`.`entry` <> 70000
    AND `ct`.`entry` <> 70552;

DROP TEMPORARY TABLE IF EXISTS tmp_npcbot_test_existing_characters;

SET @NPCBOT_SQL := CONCAT(
    'CREATE TEMPORARY TABLE tmp_npcbot_test_existing_characters ENGINE=MEMORY AS ',
    'SELECT `entry` FROM `', @NPCBOT_CHARACTERS_DB_ESCAPED, '`.`characters_npcbot` ',
    'WHERE `entry` BETWEEN ', @NPCBOT_ENTRY_MIN, ' AND ', @NPCBOT_ENTRY_MAX
);
PREPARE npcbot_stmt FROM @NPCBOT_SQL;
EXECUTE npcbot_stmt;
DEALLOCATE PREPARE npcbot_stmt;

DROP TEMPORARY TABLE IF EXISTS tmp_npcbot_test_spawn_entries;

CREATE TEMPORARY TABLE tmp_npcbot_test_spawn_entries
(
    `spawn_index` int unsigned NOT NULL AUTO_INCREMENT,
    `entry` int unsigned NOT NULL,
    `bot_class` tinyint unsigned NOT NULL,
    `bot_spec` tinyint unsigned NOT NULL,
    `bot_roles` int unsigned NOT NULL,
    `bot_faction` int unsigned NOT NULL,
    `curhealth` int unsigned NOT NULL,
    `curmana` int unsigned NOT NULL,
    `movement_type` tinyint unsigned NOT NULL,
    `npcflag` int unsigned NOT NULL,
    `unit_flags` int unsigned NOT NULL,
    `dynamicflags` int unsigned NOT NULL,
    PRIMARY KEY (`spawn_index`),
    UNIQUE KEY `idx_entry` (`entry`)
) ENGINE=MEMORY;

INSERT INTO tmp_npcbot_test_spawn_entries
(
    `entry`,
    `bot_class`,
    `bot_spec`,
    `bot_roles`,
    `bot_faction`,
    `curhealth`,
    `curmana`,
    `movement_type`,
    `npcflag`,
    `unit_flags`,
    `dynamicflags`
)
SELECT
    `picked`.`entry`,
    `picked`.`bot_class`,
    `picked`.`bot_spec`,
    `picked`.`bot_roles`,
    `picked`.`bot_faction`,
    `picked`.`curhealth`,
    `picked`.`curmana`,
    `picked`.`movement_type`,
    `picked`.`npcflag`,
    `picked`.`unit_flags`,
    `picked`.`dynamicflags`
FROM
(
    SELECT
        `ct`.`entry`,
        `extras`.`class` AS `bot_class`,
        CASE `extras`.`class`
            WHEN 1  THEN 1   -- Warrior: Arms
            WHEN 2  THEN 6   -- Paladin: Retribution
            WHEN 3  THEN 7   -- Hunter: Beast Mastery
            WHEN 4  THEN 10  -- Rogue: Assassination
            WHEN 5  THEN 15  -- Priest: Shadow
            WHEN 6  THEN 16  -- Death Knight: Blood
            WHEN 7  THEN 19  -- Shaman: Elemental
            WHEN 8  THEN 23  -- Mage: Fire
            WHEN 9  THEN 25  -- Warlock: Affliction
            WHEN 11 THEN 28  -- Druid: Balance
            ELSE 31          -- Custom classes: default
        END AS `bot_spec`,
        CASE
            WHEN `extras`.`class` IN (1, 2, 4, 6, 12, 15, 16, 20) THEN 4
            ELSE 20
        END AS `bot_roles`,
        `ct`.`faction` AS `bot_faction`,
        GREATEST(
            1,
            CEILING(
                COALESCE(
                    CASE COALESCE(`ct`.`exp`, 2)
                        WHEN 0 THEN `stats`.`basehp0`
                        WHEN 1 THEN `stats`.`basehp1`
                        ELSE `stats`.`basehp2`
                    END,
                    1
                ) * `ct`.`HealthModifier`
            )
        ) AS `curhealth`,
        COALESCE(CEILING(`stats`.`basemana` * `ct`.`ManaModifier`), 0) AS `curmana`,
        `ct`.`MovementType` AS `movement_type`,
        `ct`.`npcflag`,
        `ct`.`unit_flags`,
        `ct`.`dynamicflags`
    FROM `creature_template` `ct`
    INNER JOIN `creature_template_npcbot_extras` `extras`
        ON `extras`.`entry` = `ct`.`entry`
    LEFT JOIN `creature_classlevelstats` `stats`
        ON `stats`.`level` = `ct`.`minlevel`
        AND `stats`.`class` = `ct`.`unit_class`
    LEFT JOIN `creature` `existing_creature`
        ON `existing_creature`.`id1` = `ct`.`entry`
        OR `existing_creature`.`id2` = `ct`.`entry`
        OR `existing_creature`.`id3` = `ct`.`entry`
    LEFT JOIN `tmp_npcbot_test_existing_characters` `existing_bot`
        ON `existing_bot`.`entry` = `ct`.`entry`
    WHERE `ct`.`entry` BETWEEN @NPCBOT_ENTRY_MIN AND @NPCBOT_ENTRY_MAX
        AND `ct`.`entry` <> 70000
        AND `ct`.`entry` <> 70552
        AND `existing_creature`.`guid` IS NULL
        AND `existing_bot`.`entry` IS NULL
    ORDER BY `ct`.`entry`
) `picked`;

SELECT COUNT(*) AS `npcbot_rows_to_insert`
FROM `tmp_npcbot_test_spawn_entries`;

START TRANSACTION;

SET @NPCBOT_SQL := CONCAT(
    'INSERT INTO `', @NPCBOT_CHARACTERS_DB_ESCAPED, '`.`characters_npcbot` ',
    '(`entry`, `roles`, `spec`, `faction`) ',
    'SELECT `entry`, `bot_roles`, `bot_spec`, `bot_faction` ',
    'FROM `tmp_npcbot_test_spawn_entries` ',
    'ORDER BY `entry`'
);
PREPARE npcbot_stmt FROM @NPCBOT_SQL;
EXECUTE npcbot_stmt;
DEALLOCATE PREPARE npcbot_stmt;

INSERT INTO `creature`
(
    `id1`, `id2`, `id3`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
    `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
    `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`,
    `dynamicflags`, `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`
)
SELECT
    `entry`,
    0,
    0,
    @NPCBOT_MAP,
    0,
    0,
    @NPCBOT_SPAWN_MASK,
    @NPCBOT_PHASE_MASK,
    0,
    @NPCBOT_ORIGIN_X + (MOD(`spawn_index` - 1, @NPCBOT_GRID_COLUMNS) * @NPCBOT_GRID_SPACING),
    @NPCBOT_ORIGIN_Y + (FLOOR((`spawn_index` - 1) / @NPCBOT_GRID_COLUMNS) * @NPCBOT_GRID_SPACING),
    @NPCBOT_ORIGIN_Z,
    @NPCBOT_ORIENTATION,
    @NPCBOT_RESPAWN_SECONDS,
    @NPCBOT_WANDER_DISTANCE,
    0,
    `curhealth`,
    `curmana`,
    `movement_type`,
    `npcflag`,
    `unit_flags`,
    `dynamicflags`,
    '',
    NULL,
    0,
    @NPCBOT_COMMENT
FROM `tmp_npcbot_test_spawn_entries`
ORDER BY `entry`;

COMMIT;

SELECT
    `entry`,
    `bot_class`,
    `bot_spec`,
    `bot_roles`,
    @NPCBOT_MAP AS `map`,
    @NPCBOT_ORIGIN_X + (MOD(`spawn_index` - 1, @NPCBOT_GRID_COLUMNS) * @NPCBOT_GRID_SPACING) AS `position_x`,
    @NPCBOT_ORIGIN_Y + (FLOOR((`spawn_index` - 1) / @NPCBOT_GRID_COLUMNS) * @NPCBOT_GRID_SPACING) AS `position_y`,
    @NPCBOT_ORIGIN_Z AS `position_z`
FROM `tmp_npcbot_test_spawn_entries`
ORDER BY `entry`;

-- Rollback helper for unused test spawns only.
-- Run this against the WORLD database, after setting the same variables above.
-- It avoids deleting character bot rows that have been hired.
--
-- DROP TEMPORARY TABLE IF EXISTS tmp_npcbot_test_rollback_entries;
-- CREATE TEMPORARY TABLE tmp_npcbot_test_rollback_entries ENGINE=MEMORY AS
-- SELECT `id1` AS `entry`
-- FROM `creature`
-- WHERE `Comment` = @NPCBOT_COMMENT
--     AND `id1` BETWEEN @NPCBOT_ENTRY_MIN AND @NPCBOT_ENTRY_MAX;
--
-- SET @NPCBOT_SQL := CONCAT(
--     'DELETE `bot_rows` FROM `', @NPCBOT_CHARACTERS_DB_ESCAPED, '`.`characters_npcbot` `bot_rows` ',
--     'INNER JOIN `tmp_npcbot_test_rollback_entries` `rollback_entries` ',
--     'ON `rollback_entries`.`entry` = `bot_rows`.`entry` ',
--     'WHERE `bot_rows`.`owner` = 0'
-- );
-- PREPARE npcbot_stmt FROM @NPCBOT_SQL;
-- EXECUTE npcbot_stmt;
-- DEALLOCATE PREPARE npcbot_stmt;
--
-- DELETE FROM `creature`
-- WHERE `Comment` = @NPCBOT_COMMENT
--     AND `id1` BETWEEN @NPCBOT_ENTRY_MIN AND @NPCBOT_ENTRY_MAX;
