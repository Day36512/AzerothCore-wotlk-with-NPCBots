-- Keep fake-player ambience text/emotes quiet except for dance behavior.
UPDATE `mod_fake_player_ambience_text`
SET `enabled` = IF(`behavior` = 8, 1, 0);
