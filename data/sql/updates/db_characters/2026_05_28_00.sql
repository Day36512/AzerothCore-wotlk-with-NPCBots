-- DB update 2026_04_12_00 -> 2026_05_28_00
-- ExtraGlyphSlots addon: one server-backed major and one server-backed minor glyph per talent spec.

CREATE TABLE IF NOT EXISTS `character_extra_glyphs` (
    `guid` INT UNSIGNED NOT NULL,
    `talentGroup` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `slot` TINYINT UNSIGNED NOT NULL COMMENT '0 = extra major, 1 = extra minor',
    `glyph` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'GlyphProperties.dbc ID',
    PRIMARY KEY (`guid`, `talentGroup`, `slot`)
)
ENGINE = InnoDB
DEFAULT CHARSET = utf8mb4
COLLATE = utf8mb4_unicode_ci;
