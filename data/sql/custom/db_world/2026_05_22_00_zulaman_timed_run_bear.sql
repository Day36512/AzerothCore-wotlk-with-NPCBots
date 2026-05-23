-- Zul'Aman timed run: award the Amani War Bear from the fourth rescued hostage chest.
-- The script increments DATA_CHEST_LOOTED when each hostage is rescued; condition 17 == 4
-- means all four hostages were rescued before the timed run expired.

DELETE FROM `gameobject_loot_template`
WHERE `Entry` IN (22699, 22790, 22797, 22968)
  AND `Item` = 35103
  AND `Reference` = 35103;

INSERT INTO `gameobject_loot_template`
    (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
    (22699, 35103, 35103, 100, 0, 1, 0, 1, 1, 'Tanzar''s Trunk - Fourth Hostage Loot'),
    (22790, 35103, 35103, 100, 0, 1, 0, 1, 1, 'Kraz''s Package - Fourth Hostage Loot'),
    (22797, 35103, 35103, 100, 0, 1, 0, 1, 1, 'Ashli''s Bag - Fourth Hostage Loot'),
    (22968, 35103, 35103, 100, 0, 1, 0, 1, 1, 'Harkor''s Satchel - Fourth Hostage Loot');

DELETE FROM `reference_loot_template`
WHERE `Entry` = 35103
  AND `Item` = 33809;

INSERT INTO `reference_loot_template`
    (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
    (35103, 33809, 0, 100, 0, 1, 0, 1, 1, 'Zul''Aman - Fourth Hostage Loot - Amani War Bear');

DELETE FROM `conditions`
WHERE `SourceTypeOrReferenceId` = 10
  AND `SourceGroup` = 35103
  AND `SourceEntry` = 33809;

INSERT INTO `conditions`
    (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`,
     `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`,
     `ErrorTextId`, `ScriptName`, `Comment`)
VALUES
    (10, 35103, 33809, 0, 0, 13, 0, 17, 4, 0, 0, 0, 0, '', 'Yield fourth hostage loot if all hostages have been rescued');
