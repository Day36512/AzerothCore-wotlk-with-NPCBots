/*
  Wrath 3.3.5 Item Level Calculator - Reference Data Build v4.6.6
  Static/offline JavaScript build.

  This build intentionally separates passive budget, sockets, armor, and weapon curves.
  It uses 3.3.5 community reverse-engineering coefficients as priors, with an architecture
  able to ingest item_template CSV/JSON exports and use them as local empirical reference points.
*/

const EXPONENT = Math.log(2) / Math.log(1.5);
const MAX_ILVL = 350;

const QUALITY = {
  0: { name: "Poor", color: "#9d9d9d" },
  1: { name: "Common", color: "#ffffff" },
  2: { name: "Uncommon", color: "#1eff00" },
  3: { name: "Rare", color: "#0070dd" },
  4: { name: "Epic", color: "#a335ee" },
  5: { name: "Legendary", color: "#ff8000" },
};

const SLOTS = {
  1: "Head",
  2: "Neck",
  3: "Shoulder",
  5: "Chest",
  6: "Waist",
  7: "Legs",
  8: "Feet",
  9: "Wrists",
  10: "Hands",
  11: "Finger",
  12: "Trinket",
  13: "One-Hand Weapon",
  14: "Shield",
  15: "Bow",
  16: "Back",
  17: "Two-Hand Weapon",
  20: "Robe",
  21: "Main-Hand Weapon",
  22: "Off-Hand Weapon",
  23: "Held Off-hand",
  25: "Thrown",
  26: "Ranged / Wand",
  28: "Relic",
};

const ARMOR_TYPES = {
  none: "None / jewelry",
  cloth: "Cloth",
  leather: "Leather",
  mail: "Mail",
  plate: "Plate",
  shield: "Shield",
};

const WEAPON_SUBCLASSES = {
  none: "None / not a weapon",
  axe1h: "1H Axe",
  axe2h: "2H Axe",
  mace1h: "1H Mace",
  mace2h: "2H Mace",
  sword1h: "1H Sword",
  sword2h: "2H Sword",
  dagger: "Dagger",
  fist: "Fist Weapon",
  polearm: "Polearm",
  staff: "Staff",
  bow: "Bow",
  gun: "Gun",
  crossbow: "Crossbow",
  thrown: "Thrown",
  wand: "Wand",
  caster_mh: "Caster Main-hand",
  caster_staff: "Caster Staff",
  tank_1h: "Tank 1H",
};


const WEAPON_SLOT_IDS = new Set([13, 15, 17, 21, 22, 25, 26]);
const JEWELRY_SLOT_IDS = new Set([2, 11]);
const NON_ARMOR_PASSIVE_SLOT_IDS = new Set([2, 11, 12, 23, 28]);

function isWeaponSlot(invType) {
  return WEAPON_SLOT_IDS.has(Number(invType));
}

function isJewelrySlot(invType) {
  return JEWELRY_SLOT_IDS.has(Number(invType));
}

function isPassiveNonArmorSlot(invType) {
  return NON_ARMOR_PASSIVE_SLOT_IDS.has(Number(invType));
}

function defaultWeaponSubclassForSlot(invType) {
  const slot = Number(invType);
  if (slot === 15) return "bow";
  if (slot === 17) return "sword2h";
  if (slot === 25) return "thrown";
  if (slot === 26) return "wand";
  if ([13, 21, 22].includes(slot)) return "sword1h";
  return "none";
}

function usableWeaponSubclass(subclass) {
  return subclass && subclass !== "none";
}

function targetUsesWeaponSubclass(target) {
  return target && target.family === "weapon" && usableWeaponSubclass(target.weaponSubclass);
}

function targetUsesWeaponSpeed(target) {
  return target && target.family === "weapon" && Number(target.speed || 0) > 0;
}

function profileSlotWeight(itemSlot, targetSlot, family) {
  const a = Number(itemSlot);
  const b = Number(targetSlot);
  if (a === b) return 1;
  if (family === "weapon" && sameWeaponSlotGroup(a, b)) return 0.65;
  if (isJewelrySlot(a) && isJewelrySlot(b)) return 0.55;
  return 0.25;
}

const STATS = {
  // AzerothCore / TrinityCore 3.3.5 item_template stat_type values.
  // Source naming follows ItemPrototype.h / the AzerothCore item_template docs.
  // Do not collapse split melee/ranged/spell stats while parsing reference data:
  // many TBC-era rows inside a 3.3.5 database still use 16-30, 41, and 42.
  0:  { key: "mana", label: "Mana", sql: 0, group: "resource" },
  1:  { key: "health", label: "Health", sql: 1, group: "resource" },

  3:  { key: "agility", label: "Agility", sql: 3, group: "primary" },
  4:  { key: "strength", label: "Strength", sql: 4, group: "primary" },
  5:  { key: "intellect", label: "Intellect", sql: 5, group: "primary" },
  6:  { key: "spirit", label: "Spirit", sql: 6, group: "primary" },
  7:  { key: "stamina", label: "Stamina", sql: 7, group: "stamina" },

  12: { key: "defense", label: "Defense Rating", sql: 12, group: "rating" },
  13: { key: "dodge", label: "Dodge Rating", sql: 13, group: "rating" },
  14: { key: "parry", label: "Parry Rating", sql: 14, group: "rating" },
  15: { key: "block_rating", label: "Block Rating", sql: 15, group: "rating" },

  16: { key: "hit_melee", label: "Melee Hit Rating", sql: 16, group: "rating" },
  17: { key: "hit_ranged", label: "Ranged Hit Rating", sql: 17, group: "rating" },
  18: { key: "hit_spell", label: "Spell Hit Rating", sql: 18, group: "rating" },

  19: { key: "crit_melee", label: "Melee Crit Rating", sql: 19, group: "rating" },
  20: { key: "crit_ranged", label: "Ranged Crit Rating", sql: 20, group: "rating" },
  21: { key: "crit_spell", label: "Spell Crit Rating", sql: 21, group: "rating" },

  22: { key: "hit_taken_melee", label: "Melee Hit Taken Rating", sql: 22, group: "rating" },
  23: { key: "hit_taken_ranged", label: "Ranged Hit Taken Rating", sql: 23, group: "rating" },
  24: { key: "hit_taken_spell", label: "Spell Hit Taken Rating", sql: 24, group: "rating" },

  25: { key: "crit_taken_melee", label: "Melee Crit Taken Rating", sql: 25, group: "rating" },
  26: { key: "crit_taken_ranged", label: "Ranged Crit Taken Rating", sql: 26, group: "rating" },
  27: { key: "crit_taken_spell", label: "Spell Crit Taken Rating", sql: 27, group: "rating" },

  28: { key: "haste_melee", label: "Melee Haste Rating", sql: 28, group: "rating" },
  29: { key: "haste_ranged", label: "Ranged Haste Rating", sql: 29, group: "rating" },
  30: { key: "haste_spell", label: "Spell Haste Rating", sql: 30, group: "rating" },

  31: { key: "hit", label: "Hit Rating", sql: 31, group: "rating" },
  32: { key: "crit", label: "Crit Rating", sql: 32, group: "rating" },
  33: { key: "hit_taken", label: "Hit Taken Rating", sql: 33, group: "rating" },
  34: { key: "crit_taken", label: "Crit Taken Rating", sql: 34, group: "rating" },
  35: { key: "resilience", label: "Resilience Rating", sql: 35, group: "rating" },
  36: { key: "haste", label: "Haste Rating", sql: 36, group: "rating" },
  37: { key: "expertise", label: "Expertise Rating", sql: 37, group: "rating" },

  38: { key: "attack_power", label: "Attack Power", sql: 38, group: "ap" },
  39: { key: "ranged_attack_power", label: "Ranged Attack Power", sql: 39, group: "ap" },
  40: { key: "feral_attack_power", label: "Feral Attack Power", sql: 40, group: "ap" },

  41: { key: "spell_healing_done", label: "Spell Healing Done", sql: 41, group: "spellpower" },
  42: { key: "spell_damage_done", label: "Spell Damage Done", sql: 42, group: "spellpower" },
  43: { key: "mp5", label: "Mana per 5", sql: 43, group: "mp5" },
  44: { key: "armor_pen", label: "Armor Pen Rating", sql: 44, group: "rating" },
  45: { key: "spell_power", label: "Spell Power", sql: 45, group: "spellpower" },
  46: { key: "health_regen", label: "Health Regen", sql: 46, group: "regen" },
  47: { key: "spell_pen", label: "Spell Penetration", sql: 47, group: "spellpen" },
  48: { key: "block_value", label: "Block Value", sql: 48, group: "blockvalue" },

  // Synthetic internal pseudo-stat used only if bonus armor is modeled as a stat.
  9991: { key: "bonus_armor", label: "Bonus Armor", sql: null, group: "bonusarmor" },
};

const ARCHETYPE_LABELS = {
  auto: "Auto from current stats",
  any: "Any / slot-only",
  melee: "Melee DPS",
  hunter: "Hunter / ranged physical",
  caster: "Caster DPS",
  healer: "Healer",
  tank: "Tank",
  pvp: "PvP",
};

const ARCHETYPE_STAT_WEIGHTS = {
  melee: {
    3: 0.85, 4: 1.00, 7: 0.15, 16: 0.90, 19: 0.90, 28: 0.75,
    31: 0.90, 32: 0.90, 36: 0.70, 37: 1.00, 38: 1.00, 40: 0.85, 44: 0.90,
  },
  hunter: {
    3: 1.00, 7: 0.15, 17: 1.00, 20: 1.00, 29: 1.00,
    31: 0.80, 32: 0.80, 36: 0.60, 38: 0.45, 39: 1.00, 44: 0.85,
  },
  caster: {
    5: 0.65, 6: 0.25, 7: 0.15, 18: 1.00, 21: 1.00, 30: 1.00,
    31: 0.45, 32: 0.45, 36: 0.80, 42: 1.00, 45: 1.00, 47: 0.70,
  },
  healer: {
    5: 0.70, 6: 0.65, 7: 0.10, 30: 0.55, 36: 0.45,
    41: 1.00, 43: 1.00, 45: 0.85,
  },
  tank: {
    4: 0.35, 7: 0.65, 12: 1.00, 13: 1.00, 14: 1.00, 15: 1.00,
    22: 0.65, 25: 0.65, 33: 0.70, 34: 0.70, 48: 1.00, 9991: 0.80,
    31: 0.20, 37: 0.20,
  },
  pvp: {
    7: 0.40, 22: 0.35, 23: 0.35, 24: 0.35, 25: 0.35, 26: 0.35, 27: 0.35,
    33: 0.50, 34: 0.50, 35: 2.00, 47: 0.60,
  },
};

const PROFILE_STAT_RULES = {
  melee: {
    allow: [3, 4, 7, 16, 19, 28, 31, 32, 36, 37, 38, 40, 44],
    block: [5, 6, 12, 13, 14, 15, 18, 21, 22, 23, 24, 25, 26, 27, 30, 33, 34, 35, 41, 42, 43, 45, 47, 48, 9991],
  },
  hunter: {
    allow: [3, 5, 7, 17, 20, 29, 31, 32, 36, 38, 39, 44],
    block: [4, 6, 12, 13, 14, 15, 18, 21, 22, 23, 24, 25, 26, 27, 30, 33, 34, 35, 37, 40, 41, 42, 43, 45, 47, 48, 9991],
  },
  caster: {
    allow: [5, 6, 7, 18, 21, 30, 31, 32, 36, 42, 45, 47],
    block: [3, 4, 12, 13, 14, 15, 16, 17, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29, 33, 34, 35, 37, 38, 39, 40, 41, 43, 44, 48, 9991],
  },
  healer: {
    allow: [5, 6, 7, 21, 30, 32, 36, 41, 43, 45],
    block: [3, 4, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29, 31, 33, 34, 35, 37, 38, 39, 40, 42, 44, 47, 48, 9991],
  },
  tank: {
    allow: [4, 7, 12, 13, 14, 15, 31, 37, 48, 9991],
    block: [3, 5, 6, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 47],
  },
};

const PVP_EXTRA_STATS = [35];
const PVP_ALWAYS_BLOCKED_STATS = [12, 13, 14, 15, 22, 23, 24, 25, 26, 27, 33, 34, 48, 9991];
const PROFILE_BLOCK_THRESHOLD = 0.04;

const SOCKET_COLORS = {
  red: { label: "Red", sql: 1 },
  yellow: { label: "Yellow", sql: 2 },
  blue: { label: "Blue", sql: 4 },
  meta: { label: "Meta", sql: 8 },
};

const GEM_MODELS = {
  none: { label: "No gem value", raw: 0, metaMultiplier: 0 },
  uncommon: { label: "Uncommon gems", raw: 12, metaMultiplier: 1.35 },
  rare: { label: "Rare gems", raw: 16, metaMultiplier: 1.35 },
  epic: { label: "Epic gems", raw: 20, metaMultiplier: 1.35 },
};

const SOCKET_BONUS_PER_COLORED_SOCKET_GEM_FRACTION = 0.25;
const SOCKET_BONUS_PER_META_SOCKET_GEM_FRACTION = 0.15;

const ITEM_SPELL_TRIGGER_LABELS = {
  0: "Use",
  1: "On equip",
  2: "Chance on hit",
  4: "Soulstone",
  5: "Learn spell",
  6: "No-delay use",
};

const TOLERANCES = {
  tight: { label: "Tight", score: 0.035, ilvl: 1 },
  normal: { label: "Normal", score: 0.075, ilvl: 2 },
  wide: { label: "Wide", score: 0.14, ilvl: 5 },
};

let lastForwardResult = null;
let lastSql = "";
let referenceItems = [];
let referenceSummary = null;
let activeForwardReferenceProfile = null;
let lastCompareCsvRows = [];
let lastCompareCsvFilename = "compare-item.csv";
let lastBatchItems = [];
let lastBatchSql = "";
let lastBatchCsvRows = [];
const MODEL_VERSION = "v4.6.6";
const DEFAULT_LOCAL_MYSQL_HELPER_URL = "http://127.0.0.1:33544";

const REFERENCE_EXPORT_SQL = `SELECT
  entry, name, description, class, subclass, Quality, InventoryType, ItemLevel, RequiredLevel,
  displayid, Material, sheath, SellPrice,
  AllowableClass, AllowableRace, itemset, Flags, FlagsExtra, RequiredSkill, RequiredSkillRank, bonding,
  stat_type1, stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5,
  stat_type6, stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10,
  socketColor_1, socketColor_2, socketColor_3, socketBonus,
  dmg_min1, dmg_max1, dmg_type1, armor, block, delay,
  spellid_1, spelltrigger_1, spellcharges_1, spellcooldown_1, spellcategorycooldown_1,
  spellid_2, spelltrigger_2, spellcharges_2, spellcooldown_2, spellcategorycooldown_2,
  spellid_3, spelltrigger_3, spellcharges_3, spellcooldown_3, spellcategorycooldown_3,
  spellid_4, spelltrigger_4, spellcharges_4, spellcooldown_4, spellcategorycooldown_4,
  spellid_5, spelltrigger_5, spellcharges_5, spellcooldown_5, spellcategorycooldown_5,
  RandomProperty, RandomSuffix, ScalingStatDistribution, ScalingStatValue
FROM item_template
WHERE class IN (2,4)
  AND Quality IN (2,3,4,5)
  AND InventoryType IN (1,2,3,5,6,7,8,9,10,11,12,13,14,15,16,17,20,21,22,23,25,26,28)
  AND ItemLevel > 0
  AND COALESCE(RandomProperty,0) = 0
  AND COALESCE(RandomSuffix,0) = 0
  AND COALESCE(ScalingStatDistribution,0) = 0
ORDER BY Quality, InventoryType, subclass, ItemLevel, entry;`;

function epicQualityCurve(ilvl) {
  const L = Number(ilvl);
  if (L >= 200) return 1.32 * L - 120;
  if (L >= 100) return 0.700 * L - 2;
  return 0.689 * L + 1;
}

function qualityCurve(quality, ilvl) {
  const L = Number(ilvl);
  switch (Number(quality)) {
    case 5:
      return Math.max(1, 1.36 * L - 116, epicQualityCurve(L) * 1.06);
    case 4:
      return epicQualityCurve(L);
    case 3:
      if (L >= 136) return 0.880 * L - 39.25;
      if (L >= 80) return 0.674 * L - 8.0;
      return 0.641 * L - 4.0;
    case 2:
      if (L >= 130) return 0.801 * L - 38.3;
      if (L >= 80) return 0.505 * L - 4.5;
      return 0.495 * L - 2.85;
    case 1:
      return 0.43 * L - 2.0;
    default:
      return 0.36 * L - 1.5;
  }
}

function slotMod(invType, quality, ilvl, family) {
  const q = Number(quality);
  const L = Number(ilvl);
  const slot = Number(invType);

  if (slot === 28) return 1 / 32;
  if ([4, 19].includes(slot)) return 1 / 32;
  if ([5, 20, 7].includes(slot)) return 1;
  if (slot === 15 || slot === 17) return 1;
  if ([25, 26].includes(slot)) return 5 / 16;

  if (q >= 4 && L >= 200) {
    if (slot === 1) return 1;
    if ([3, 6, 8, 10].includes(slot)) return 8 / 16;
    if ([2, 9, 11].includes(slot)) return 4 / 16;
    if ([14, 16, 23].includes(slot)) return 3 / 16;
    if (slot === 12) return 6 / 16;
    if ([13, 21, 22].includes(slot)) return 2 / 16;
  }

  if (q >= 4 && L >= 90 && L < 200) {
    if (slot === 1) return 11 / 16;
    if ([3, 6, 8, 10].includes(slot)) return 6 / 16;
    if ([2, 9, 11, 14, 16, 23].includes(slot)) return 3 / 16;
    if (slot === 12) return 6 / 16;
    // Physical weapon damage is handled by the DPS subsystem. The passive stat
    // budget on TBC/Wrath one-hand weapons is much closer to the Wrath 2/16
    // weapon coefficient than to the older 7/16 fallback. Malchazeen, Spiteblade,
    // Emerald Ripper, and Talon of Azshara all prove that 7/16 wildly overstates
    // their green stat payload.
    if ([13, 21, 22].includes(slot)) return 2 / 16;
  }

  if ([13, 21, 22].includes(slot)) return 2 / 16;
  if ([1, 5, 7, 20].includes(slot)) return 1;
  if ([3, 6, 8, 10, 12].includes(slot)) return 8 / 16;
  if ([2, 9, 11, 14, 16, 23].includes(slot)) return 4 / 16;
  return 4 / 16;
}

function passiveBudget(ilvl, quality, invType, family) {
  const sm = slotMod(invType, quality, ilvl, family);
  const qv = Math.max(1, qualityCurve(quality, ilvl));
  return Math.pow(qv * sm, EXPONENT) / Math.max(sm, 0.00001);
}

function statMod(statId, quality, ilvl, invType) {
  const stat = STATS[Number(statId)];
  if (!stat) return 1;
  const q = Number(quality);
  const L = Number(ilvl);
  const slot = Number(invType);

  switch (stat.group) {
    case "stamina":
      if ((q >= 4 && L >= 90) || (q <= 3 && L >= 80)) return 2 / 3;
      return 1;
    case "ap":
      return 0.5;
    case "spellpower":
      if (q >= 4 && L >= 90) return 45 / 64;
      return 55 / 64;
    case "mp5":
      if ([2, 11, 12, 23].includes(slot)) return q >= 4 && L >= 200 ? 24 / 16 : 32 / 16;
      return q >= 4 && L >= 200 ? 32 / 16 : 32 / 16;
    case "regen":
      return 32 / 16;
    case "spellpen":
      return 12 / 16;
    case "blockvalue":
      if ([2, 11, 12, 14].includes(slot) && q >= 4 && L >= 200) return 4 / 64;
      return 21 / 64;
    case "bonusarmor":
      return 2 / 32;
    default:
      return 1;
  }
}

function socketCostEach(quality, ilvl, invType) {
  const q = Number(quality);
  const L = Number(ilvl);
  const lowBudgetSlot = [2, 11, 14, 23].includes(Number(invType));
  if (q >= 4 && L >= 200) return 24;
  if (q >= 4 && L >= 90) return lowBudgetSlot ? 10 : 10;
  if (q === 3) return lowBudgetSlot ? 10 : 20;
  if (q === 2) return lowBudgetSlot ? 5 : 10;
  return 8;
}

function socketCounts(prefix) {
  return {
    red: numberValue(`${prefix}RedSockets`),
    yellow: numberValue(`${prefix}YellowSockets`),
    blue: numberValue(`${prefix}BlueSockets`),
    meta: numberValue(`${prefix}MetaSockets`),
  };
}

function socketTotal(counts) {
  return Object.values(counts).reduce((a, b) => a + Number(b || 0), 0);
}

function socketBudget(counts, quality, ilvl, invType) {
  const each = socketCostEach(quality, ilvl, invType);
  return socketTotal(counts) * Math.pow(each, EXPONENT);
}

function resolveGemModel(model, ilvl) {
  const requested = String(model || "auto");
  if (requested !== "auto") return GEM_MODELS[requested] || GEM_MODELS.none;
  const L = Number(ilvl || 0);
  if (L >= 200) return GEM_MODELS.epic;
  if (L >= 80) return GEM_MODELS.rare;
  return GEM_MODELS.uncommon;
}

function gemBudgetForSockets(counts, ilvl, model) {
  const resolved = resolveGemModel(model, ilvl);
  const raw = Number(resolved.raw || 0);
  if (raw <= 0) {
    return { model: resolved, coloredBudget: 0, metaBudget: 0, totalBudget: 0 };
  }

  const coloredSockets = Number(counts.red || 0) + Number(counts.yellow || 0) + Number(counts.blue || 0);
  const metaSockets = Number(counts.meta || 0);
  const coloredBudget = coloredSockets * Math.pow(raw, EXPONENT);
  const metaBudget = metaSockets * Math.pow(raw * Number(resolved.metaMultiplier || 1), EXPONENT);
  return {
    model: resolved,
    coloredBudget,
    metaBudget,
    totalBudget: coloredBudget + metaBudget,
  };
}

function estimatedSocketBonusBudget(counts, ilvl, gemModel) {
  const resolved = resolveGemModel(gemModel, ilvl);
  const raw = Number(resolved.raw || 0);
  if (raw <= 0) return 0;

  const coloredSockets = Number(counts.red || 0) + Number(counts.yellow || 0) + Number(counts.blue || 0);
  const metaSockets = Number(counts.meta || 0);
  const weightedSocketBonus = (coloredSockets * SOCKET_BONUS_PER_COLORED_SOCKET_GEM_FRACTION) +
    (metaSockets * SOCKET_BONUS_PER_META_SOCKET_GEM_FRACTION);
  return weightedSocketBonus * Math.pow(raw, EXPONENT);
}

function socketBonusModelLabel() {
  return `${SOCKET_BONUS_PER_COLORED_SOCKET_GEM_FRACTION}x gem budget per colored socket, ${SOCKET_BONUS_PER_META_SOCKET_GEM_FRACTION}x per meta socket`;
}

function socketModelAudit(counts, quality, ilvl, invType, gemModel) {
  const itemSocketCost = socketBudget(counts, quality, ilvl, invType);
  const gem = gemBudgetForSockets(counts, ilvl, gemModel);
  const bonusBudget = estimatedSocketBonusBudget(counts, ilvl, gemModel);
  return {
    socketCount: socketTotal(counts),
    socketCostEach: socketCostEach(quality, ilvl, invType),
    itemSocketCost,
    gemBudget: gem.totalBudget,
    gemModel: gem.model.label,
    socketBonusBudget: bonusBudget,
    socketBonusModel: socketBonusModelLabel(),
    netExternalBudget: gem.totalBudget + bonusBudget - itemSocketCost,
  };
}

function armorSlotMod(invType) {
  switch (Number(invType)) {
    case 5:
    case 20:
      return 16 / 16;
    case 7:
      return 14 / 16;
    case 1:
      return 13 / 16;
    case 3:
      return 12 / 16;
    case 8:
      return 11 / 16;
    case 10:
      return 10 / 16;
    case 6:
      return 9 / 16;
    case 16:
      return 8 / 16;
    case 9:
      return 7 / 16;
    case 14:
      return 16 / 16;
    default:
      return 0;
  }
}

function baseArmorByType(armorType, quality, ilvl) {
  const L = Number(ilvl);
  const qScale = Number(quality) >= 4 ? 1 : Number(quality) === 3 ? 0.82 : Number(quality) === 2 ? 0.65 : 0.5;
  switch (armorType) {
    case "cloth":
      return qScale * (10 + 1.07 * L + 0.00115 * L * L);
    case "leather":
      return qScale * (60 + 2.10 * L + 0.00150 * L * L);
    case "mail":
      return qScale * (110 + 3.10 * L + 0.00230 * L * L);
    case "plate":
      return qScale * (170 + 5.00 * L + 0.00350 * L * L);
    case "shield":
      return qScale * (1950 + 17.0 * L + 0.040 * L * L);
    default:
      return 0;
  }
}

function expectedArmor(invType, armorType, quality, ilvl, bonusArmor = 0) {
  if (armorType === "none") return 0;
  if (armorType === "shield" || Number(invType) === 14) {
    return Math.max(0, Math.round(baseArmorByType("shield", quality, ilvl) + Number(bonusArmor || 0)));
  }
  const mod = armorSlotMod(invType);
  if (!mod) return 0;
  return Math.max(0, Math.round(baseArmorByType(armorType, quality, ilvl) * mod + Number(bonusArmor || 0)));
}

function defaultWeaponSpeed(subclass, invType) {
  const slot = Number(invType);
  const table = {
    axe1h: slot === 22 ? 2.0 : slot === 21 ? 2.4 : 2.3,
    axe2h: 3.4,
    mace1h: slot === 22 ? 1.5 : slot === 21 ? 2.0 : 2.3,
    mace2h: 3.3,
    sword1h: slot === 22 ? 1.5 : slot === 21 ? 1.9 : 2.2,
    sword2h: 3.3,
    dagger: slot === 22 ? 1.6 : 1.7,
    fist: slot === 21 ? 2.6 : 2.0,
    polearm: 3.2,
    staff: 2.7,
    bow: 2.7,
    gun: 2.7,
    crossbow: 2.9,
    thrown: 1.9,
    wand: 1.7,
    caster_mh: 1.8,
    caster_staff: 2.1,
    tank_1h: 1.6,
  };
  return table[subclass] || 2.6;
}

const WEAPON_DPS_ANCHORS = {
  // Observed 3.3.5 / WotLK Classic database checkpoints. These are deliberately
  // anchored to actual tooltip DPS instead of trying to fake weapon damage from
  // the passive-stat budget curve. TBC raid weapons and Wrath raid weapons are
  // not one smooth line; there is a real expansion-band step around ilvl 200.
  heavy2hEpic: [
    // Gorehowl, Lionheart Executioner / TBC blacksmithing, Torch/Cataclysm tier, then Wrath tiers.
    [125, 119.86],
    [136, 126.94],
    [141, 130.39],
    [151, 138.00],
    [200, 186.47],
    [213, 203.68],
    [226, 222.94],
    [264, 294.71],
    [271, 310.97],
    [284, 344.17],
  ],
  oneHandPhysicalEpic: [
    // Perdition's Blade, Retainer/Guile/Fireguard neighborhood, Emerald Ripper/Spiteblade,
    // Malchazeen/The Decapitator, Talon of Azshara/Talon of the Phoenix, Tracker's Blade,
    // Titansteel Bonecrusher, Webbed Death, Calamity's Grasp.
    [77, 58.33],
    [100, 81.15],
    [107, 84.40],
    [115, 87.50],
    [125, 91.94],
    [134, 96.48],
    [141, 100.33],
    [200, 143.65],
    [213, 156.43],
    [226, 171.35],
    [245, 188.50],
    [258, 205.00],
    [264, 219.00],
    [271, 226.00],
    [284, 250.00],
  ],
};

function interpolateAnchors(points, ilvl) {
  const L = Number(ilvl);
  if (!points || points.length === 0) return 1;
  if (L <= points[0][0]) {
    const [x0, y0] = points[0];
    const [x1, y1] = points[1];
    const slope = (y1 - y0) / (x1 - x0);
    return Math.max(1, y0 + slope * (L - x0));
  }
  for (let i = 0; i < points.length - 1; i++) {
    const [x0, y0] = points[i];
    const [x1, y1] = points[i + 1];
    if (L >= x0 && L <= x1) {
      const t = (L - x0) / (x1 - x0);
      return y0 + t * (y1 - y0);
    }
  }
  const [x0, y0] = points[points.length - 2];
  const [x1, y1] = points[points.length - 1];
  const slope = (y1 - y0) / (x1 - x0);
  return Math.max(1, y1 + slope * (L - x1));
}

function weaponQualityScale(quality) {
  const q = Number(quality);
  // Weapon DPS quality scaling is not the same as passive-stat budget scaling.
  // Wrath legendary 2H examples share the same ilvl DPS envelope as epic peers,
  // while heroic/rare 2H dungeon weapons sit roughly 9-10% below epic peers.
  if (q >= 4) return 1.0;
  if (q === 3) return 0.91;
  if (q === 2) return 0.78;
  if (q === 1) return 0.62;
  return 0.45;
}

function oneHandPhysicalDpsEpic(ilvl) {
  return interpolateAnchors(WEAPON_DPS_ANCHORS.oneHandPhysicalEpic, ilvl);
}

function weaponDps(quality, ilvl, invType, subclass) {
  const slot = Number(invType);
  const qScale = weaponQualityScale(quality);
  let dps;

  if (["axe2h", "mace2h", "sword2h", "polearm"].includes(subclass) || slot === 17) {
    dps = interpolateAnchors(WEAPON_DPS_ANCHORS.heavy2hEpic, ilvl);
  } else {
    dps = oneHandPhysicalDpsEpic(ilvl);

    if (subclass === "staff") dps *= 0.92;
    if (subclass === "caster_staff") dps *= 0.56;
    if (subclass === "caster_mh") dps *= 0.58;
    if (["bow", "gun", "crossbow"].includes(subclass) || slot === 15) dps *= 0.98;
    if (subclass === "thrown" || slot === 25) dps *= 0.62;
    if (subclass === "wand") dps *= 0.58;
    if (subclass === "tank_1h") dps *= 0.72;
  }

  return Math.max(1, dps * qScale);
}

function damageSpreadCoefficient(subclass, invType, quality, ilvl = 0) {
  const slot = Number(invType);
  const q = Number(quality);
  const L = Number(ilvl || 0);
  // Coefficient = (max - min) / averageDamage.
  // Important correction: TBC raid daggers such as Emerald Ripper and Malchazeen
  // use a narrow ~0.40 damage band. The previous build used the Wrath dagger
  // ~0.60 band for everything, which is why Malchazeen exploded into nonsense.
  if ((["axe2h", "mace2h", "sword2h", "polearm", "staff", "caster_staff"].includes(subclass) || slot === 17) && q >= 5) return 0.50;
  if (["axe2h", "mace2h", "sword2h", "polearm", "staff", "caster_staff"].includes(subclass) || slot === 17) return 0.40;

  if (subclass === "dagger") return L > 0 && L < 150 ? 0.40 : 0.60;
  if (["sword1h", "axe1h", "mace1h"].includes(subclass)) return L > 0 && L < 150 ? 0.60 : 0.60;
  if (subclass === "fist") return L > 0 && L < 150 ? 0.60 : 0.40;
  if (["tank_1h", "caster_mh"].includes(subclass)) return 0.60;
  if (["bow", "gun", "crossbow"].includes(subclass)) return 0.50;
  if (["thrown", "wand"].includes(subclass)) return 0.45;
  return 0.50;
}

function expectedWeaponDamage(quality, ilvl, invType, subclass, speedSeconds) {
  const speed = Number(speedSeconds || 0) > 0 ? Number(speedSeconds) : defaultWeaponSpeed(subclass, invType);
  const dps = weaponDps(quality, ilvl, invType, subclass);
  const coeff = damageSpreadCoefficient(subclass, invType, quality, ilvl);
  const avg = dps * speed;
  return {
    dps,
    speed,
    coeff,
    min: Math.max(1, Math.floor(avg * (1 - coeff / 2))),
    max: Math.max(1, Math.ceil(avg * (1 + coeff / 2))),
  };
}

function allocateStats(statRows, ilvl, quality, invType, family, sockets, budgetUsage = 1, budgetOverride = null, referenceInfo = null) {
  const baseBudget = budgetOverride && Number.isFinite(Number(budgetOverride)) ? Number(budgetOverride) : passiveBudget(ilvl, quality, invType, family);
  const sockBudget = socketBudget(sockets, quality, ilvl, invType);
  const usedBudget = Math.max(0, baseBudget * Math.max(0.01, Number(budgetUsage || 1)));
  const remaining = Math.max(0, usedBudget - sockBudget);
  const cleanRows = statRows
    .filter(r => Number(r.share) > 0 && STATS[Number(r.statId)])
    .map(r => ({ statId: Number(r.statId), share: Number(r.share) }));
  const totalShare = cleanRows.reduce((sum, r) => sum + r.share, 0) || 1;
  const generated = cleanRows.map(r => {
    const normalizedShare = r.share / totalShare;
    const mod = statMod(r.statId, quality, ilvl, invType);
    // observedBudget uses (statValue * statMod)^EXPONENT, so the inverse is
    // statValue = powerShare^(1/EXPONENT) / statMod. The previous build divided
    // by statMod before taking the root, which badly distorted AP, stamina, spell power, etc.
    const rawValue = Math.pow(remaining * normalizedShare, 1 / EXPONENT) / Math.max(mod, 0.00001);
    return {
      statId: r.statId,
      key: STATS[r.statId].key,
      label: STATS[r.statId].label,
      value: Math.max(0, Math.ceil(rawValue)),
      share: normalizedShare,
      mod,
    };
  });

  return {
    baseBudget,
    usedBudget,
    budgetUsage,
    socketBudget: sockBudget,
    remainingBudget: remaining,
    socketCostEach: socketCostEach(quality, ilvl, invType),
    referenceInfo,
    stats: generated,
  };
}

function observedBudget(statRows, quality, ilvl, invType, sockets) {
  const statsBudget = statRows.reduce((sum, r) => {
    const id = Number(r.statId);
    const val = Number(r.value || 0);
    if (!STATS[id] || val <= 0) return sum;
    const mod = statMod(id, quality, ilvl, invType);
    return sum + Math.pow(val * mod, EXPONENT);
  }, 0);
  return statsBudget + socketBudget(sockets, quality, ilvl, invType);
}

function reverseEstimate(input) {
  const candidates = [];
  const knownArmor = Number(input.armor || 0);
  const knownMin = Number(input.minDamage || 0);
  const knownMax = Number(input.maxDamage || 0);
  const knownSpeed = Number(input.speed || 0);
  const hasDamage = knownMin > 0 && knownMax > 0 && knownSpeed > 0;

  for (let L = 1; L <= MAX_ILVL; L++) {
    const obs = observedBudget(input.stats, input.quality, L, input.slot, input.sockets);
    const expBudget = passiveBudget(L, input.quality, input.slot, input.family);
    const statsErr = Math.abs(expBudget - obs) / Math.max(expBudget, 1);
    let armorErr = 0;
    let dpsErr = 0;

    if (knownArmor > 0) {
      const refArmor = referenceItems.length ? referenceArmorValue(input.slot, input.armorType, input.quality, L, 0) : null;
      const expArmor = refArmor ? refArmor.armor : expectedArmor(input.slot, input.armorType, input.quality, L, 0);
      armorErr = expArmor > 0 ? Math.abs(expArmor - knownArmor) / Math.max(knownArmor, 1) : 0.25;
    }

    if (hasDamage) {
      const obsDps = ((knownMin + knownMax) / 2) / knownSpeed;
      const refDmg = referenceItems.length ? referenceWeaponDamage(input.quality, L, input.slot, input.weaponSubclass, knownSpeed) : null;
      const expDmg = refDmg || expectedWeaponDamage(input.quality, L, input.slot, input.weaponSubclass, knownSpeed);
      dpsErr = Math.abs(expDmg.dps - obsDps) / Math.max(obsDps, 1);
    }

    let score = statsErr;
    if (knownArmor > 0) score = 0.72 * score + 0.28 * armorErr;
    if (hasDamage) score = 0.62 * score + 0.38 * dpsErr;

    candidates.push({ ilvl: L, score, statsErr, armorErr, dpsErr, obsBudget: obs, expBudget });
  }

  candidates.sort((a, b) => a.score - b.score);
  return candidates;
}

function statBudgetShareMap(stats, quality, ilvl, invType) {
  const map = new Map();
  let total = 0;
  for (const stat of stats || []) {
    const budget = statBudgetContribution(stat.statId, stat.value, quality, ilvl, invType);
    if (budget <= 0) continue;
    map.set(Number(stat.statId), (map.get(Number(stat.statId)) || 0) + budget);
    total += budget;
  }
  if (total <= 0) return { total: 0, shares: new Map() };
  for (const [statId, value] of map.entries()) {
    map.set(statId, value / total);
  }
  return { total, shares: map };
}

function statProfileDistance(a, b) {
  const keys = new Set([...a.shares.keys(), ...b.shares.keys()]);
  if (!keys.size) return 0;
  let diff = 0;
  for (const key of keys) {
    diff += Math.abs(Number(a.shares.get(key) || 0) - Number(b.shares.get(key) || 0));
  }
  return diff / 2;
}

function referenceReverseMatches(input, limit = 12) {
  if (!referenceItems.length) return [];
  const knownArmor = Number(input.armor || 0);
  const knownMin = Number(input.minDamage || 0);
  const knownMax = Number(input.maxDamage || 0);
  const knownSpeed = Number(input.speed || 0);
  const hasDamage = knownMin > 0 && knownMax > 0 && knownSpeed > 0;
  const obsDps = hasDamage ? ((knownMin + knownMax) / 2) / knownSpeed : 0;
  const target = { slot: input.slot, family: input.family, armorType: input.armorType, excludeEntry: input.excludeEntry };
  const archetypeInfo = referenceArchetypeRule(target, input.stats, "auto");
  target.archetype = archetypeInfo.archetype;
  const filters = referenceFilterOptions();

  const candidates = referenceItems
    .filter(item => itemPassesReferenceFilters(item, target, filters))
    .filter(item => item.quality === input.quality && item.family === input.family)
    .filter(item => referenceItemMatchesArchetype(item, archetypeInfo.archetype, archetypeInfo.rule, 0.65))
    .filter(item => {
      if (input.family === "weapon") {
        return sameWeaponSlotGroup(item.invType, input.slot) && (!usableWeaponSubclass(input.weaponSubclass) || item.weaponSubclass === input.weaponSubclass);
      }
      if (input.armorType && input.armorType !== "none" && item.armorType !== input.armorType) return false;
      if (item.invType === input.slot) return true;
      return isJewelrySlot(item.invType) && isJewelrySlot(input.slot);
    })
    .map(item => {
      const inputBudget = observedBudget(input.stats, input.quality, item.ilvl, input.slot, input.sockets);
      const itemBudget = observedBudget(item.stats, item.quality, item.ilvl, item.invType, item.sockets);
      const budgetErr = itemBudget > 0 ? Math.abs(inputBudget - itemBudget) / Math.max(itemBudget, 1) : 0.35;
      const inputProfile = statBudgetShareMap(input.stats, input.quality, item.ilvl, input.slot);
      const itemProfile = statBudgetShareMap(item.stats, item.quality, item.ilvl, item.invType);
      const profileErr = inputProfile.total > 0 && itemProfile.total > 0 ? statProfileDistance(inputProfile, itemProfile) : 0.2;
      const armorErr = knownArmor > 0 && item.armor > 0 ? Math.abs(item.armor - knownArmor) / Math.max(knownArmor, 1) : 0;
      const dpsErr = hasDamage && item.dps > 0 ? Math.abs(item.dps - obsDps) / Math.max(obsDps, 1) : 0;
      const slotPenalty = item.invType === input.slot ? 0 : sameWeaponSlotGroup(item.invType, input.slot) || (isJewelrySlot(item.invType) && isJewelrySlot(input.slot)) ? 0.04 : 0.12;
      const payloadPenalty = isPurePassiveReference(item) ? 0 : 0.04;
      const archetypePenalty = archetypeInfo.archetype === "any" ? 0 : (1 - archetypeMatchWeight(itemArchetype(item), archetypeInfo.archetype)) * 0.08;
      let score = 0.58 * budgetErr + 0.34 * profileErr + slotPenalty + payloadPenalty + archetypePenalty;
      if (knownArmor > 0) score = 0.72 * score + 0.28 * armorErr;
      if (hasDamage) score = 0.68 * score + 0.32 * dpsErr;
      return { item, score, budgetErr, profileErr, armorErr, dpsErr, archetype: itemArchetype(item) };
    })
    .sort((a, b) => a.score - b.score)
    .slice(0, limit);

  return candidates;
}

function confidenceForScore(score, input) {
  const warnings = [];
  let confidence = "High";
  let cls = "good";

  if (score > 0.075) { confidence = "Medium"; cls = "warn"; }
  if (score > 0.14) { confidence = "Low"; cls = "bad"; }

  if (["trinket", "relic"].includes(input.family)) {
    warnings.push("Trinkets/relics often hide power in spell payloads or class effects; treat passive-budget ilvl as a guess.");
    if (confidence === "High") { confidence = "Medium"; cls = "warn"; }
  }
  if (Number(input.quality) >= 4 && input.bestIlvl >= 260) {
    warnings.push("Late Wrath blanket tiers, especially ICC/Ruby Sanctum ranges, have real handcrafted variance.");
  }
  if (socketTotal(input.sockets) >= 3) {
    warnings.push("Socket-heavy items are especially sensitive to socket-cost assumptions and socket-bonus treatment.");
  }

  return { confidence, cls, warnings };
}

function wantReferenceMode() {
  const el = byId("fReferenceMode");
  return el ? el.value !== "formula" && referenceItems.length > 0 : referenceItems.length > 0;
}

function optionalChecked(id, fallback = false) {
  const el = byId(id);
  return el ? !!el.checked : fallback;
}

function referenceFilterOptions() {
  return {
    excludePvp: optionalChecked("refExcludePvp", true),
    excludeItemSets: optionalChecked("refExcludeItemSets", true),
    excludeSpellPayloads: optionalChecked("refExcludeSpellPayloads", true),
    exactSlot: optionalChecked("refExactSlot", false),
    exactArmorType: optionalChecked("refExactArmorType", false),
  };
}

function itemHasPvpStats(item) {
  return (item.stats || []).some(stat => [35, 22, 23, 24, 25, 26, 27, 33, 34].includes(Number(stat.statId)));
}

function itemHasSpellPayload(item) {
  return !!(item.spellPayloads && item.spellPayloads.length);
}

function itemPassesReferenceFilters(item, target = {}, options = referenceFilterOptions()) {
  if (!item) return false;
  if (target.excludeEntry && Number(item.entry) === Number(target.excludeEntry)) return false;
  if (options.excludePvp && itemHasPvpStats(item)) return false;
  if (options.excludeItemSets && Number(item.itemset || 0) > 0) return false;
  if (options.excludeSpellPayloads && itemHasSpellPayload(item)) return false;
  if (options.exactSlot && target.slot && Number(item.invType) !== Number(target.slot)) return false;
  if (options.exactArmorType && target.family !== "weapon" && target.armorType && target.armorType !== "none" && item.armorType !== target.armorType) return false;
  return true;
}

function fieldValue(row, names, fallback = "") {
  const lookup = row.__lowerMap || Object.fromEntries(Object.keys(row).map(k => [String(k).toLowerCase(), k]));
  row.__lowerMap = lookup;
  for (const name of names) {
    const key = lookup[String(name).toLowerCase()];
    if (key !== undefined) return row[key];
  }
  return fallback;
}

function toNum(value, fallback = 0) {
  if (value === null || value === undefined || value === "") return fallback;
  const cleaned = String(value).replace(/,/g, "").trim();
  const n = Number(cleaned);
  return Number.isFinite(n) ? n : fallback;
}

function parseDelimited(text) {
  const rows = [];
  let row = [];
  let cell = "";
  let inQuotes = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    const next = text[i + 1];
    if (ch === '"') {
      if (inQuotes && next === '"') { cell += '"'; i++; }
      else inQuotes = !inQuotes;
    } else if (ch === "," && !inQuotes) {
      row.push(cell); cell = "";
    } else if ((ch === "\n" || ch === "\r") && !inQuotes) {
      if (ch === "\r" && next === "\n") i++;
      row.push(cell); cell = "";
      if (row.some(v => String(v).trim() !== "")) rows.push(row);
      row = [];
    } else {
      cell += ch;
    }
  }
  row.push(cell);
  if (row.some(v => String(v).trim() !== "")) rows.push(row);
  if (rows.length < 2) return [];
  const headers = rows[0].map(h => String(h).trim().replace(/^`|`$/g, ""));
  return rows.slice(1).map(values => {
    const obj = {};
    headers.forEach((h, i) => { obj[h] = values[i] !== undefined ? values[i] : ""; });
    return obj;
  });
}

function parseReferenceText(text) {
  const trimmed = String(text || "").trim();
  if (!trimmed) return [];
  if (trimmed.startsWith("[") || trimmed.startsWith("{")) {
    const parsed = JSON.parse(trimmed);
    if (Array.isArray(parsed)) return parsed;
    if (Array.isArray(parsed.items)) return parsed.items;
    if (Array.isArray(parsed.rows)) return parsed.rows;
    throw new Error("JSON must be an array, or an object with items[] / rows[].");
  }
  return parseDelimited(trimmed);
}

function subclassKeyFromDb(classId, subclass, invType, stats) {
  const cls = Number(classId);
  const sub = Number(subclass);
  const slot = Number(invType);
  const hasSpellPower = (stats || []).some(s => [41, 42, 45, 47].includes(Number(s.statId)) && Number(s.value) > 0);
  if (cls !== 2) return "";
  if (sub === 0) return "axe1h";
  if (sub === 1) return "axe2h";
  if (sub === 2) return "bow";
  if (sub === 3) return "gun";
  if (sub === 4) return hasSpellPower ? "caster_mh" : "mace1h";
  if (sub === 5) return "mace2h";
  if (sub === 6) return "polearm";
  if (sub === 7) return hasSpellPower ? "caster_mh" : "sword1h";
  if (sub === 8) return "sword2h";
  if (sub === 10) return hasSpellPower ? "caster_staff" : "staff";
  if (sub === 13) return "fist";
  if (sub === 15) return "dagger";
  if (sub === 16) return "thrown";
  if (sub === 18) return "crossbow";
  if (sub === 19) return "wand";
  if (slot === 17) return "axe2h";
  if ([13, 21, 22].includes(slot)) return hasSpellPower ? "caster_mh" : "sword1h";
  return "";
}

function armorTypeFromDb(classId, subclass, invType) {
  if (Number(classId) !== 4) return "none";
  switch (Number(subclass)) {
    case 1: return "cloth";
    case 2: return "leather";
    case 3: return "mail";
    case 4: return "plate";
    case 6: return "shield";
    default: return [2, 11, 12, 16, 23, 28].includes(Number(invType)) ? "none" : "none";
  }
}

function familyFromReference(classId, invType) {
  const cls = Number(classId);
  const slot = Number(invType);
  if (cls === 2 || [13, 15, 17, 21, 22, 25, 26].includes(slot)) return "weapon";
  if (slot === 12) return "trinket";
  if (slot === 28) return "relic";
  return "armor";
}

function normalizeReferenceRow(row) {
  const classId = toNum(fieldValue(row, ["class", "Class"]));
  const subclass = toNum(fieldValue(row, ["subclass", "SubClass"]));
  const quality = toNum(fieldValue(row, ["Quality", "quality"]));
  const invType = toNum(fieldValue(row, ["InventoryType", "inventorytype", "inventory_type"]));
  const ilvl = toNum(fieldValue(row, ["ItemLevel", "itemlevel", "item_level", "ilvl"]));
  if (!SLOTS[invType] || ilvl <= 0 || quality < 2 || quality > 5) return null;
  if (toNum(fieldValue(row, ["RandomProperty", "randomproperty"])) !== 0) return null;
  if (toNum(fieldValue(row, ["RandomSuffix", "randomsuffix"])) !== 0) return null;
  if (toNum(fieldValue(row, ["ScalingStatDistribution", "scalingstatdistribution"])) !== 0) return null;

  const stats = [];
  for (let i = 1; i <= 10; i++) {
    const statId = toNum(fieldValue(row, [`stat_type${i}`, `statType${i}`, `stat_type_${i}`]));
    const value = toNum(fieldValue(row, [`stat_value${i}`, `statValue${i}`, `stat_value_${i}`]));
    if (value > 0) {
      if (Object.prototype.hasOwnProperty.call(STATS, statId)) stats.push({ statId, value });
      else console.warn(`Unknown AzerothCore item stat_type ${statId} with value ${value} on item ${fieldValue(row, ["entry", "id", "Entry"])}`);
    }
  }

  const sockets = { red: 0, yellow: 0, blue: 0, meta: 0 };
  for (let i = 1; i <= 3; i++) {
    const c = toNum(fieldValue(row, [`socketColor_${i}`, `socketColor${i}`, `socketcolor_${i}`]));
    if (c & 1) sockets.red++;
    else if (c & 2) sockets.yellow++;
    else if (c & 4) sockets.blue++;
    else if (c & 8) sockets.meta++;
  }

  const dmgMin = toNum(fieldValue(row, ["dmg_min1", "dmgMin1", "dmg_min_1"]));
  const dmgMax = toNum(fieldValue(row, ["dmg_max1", "dmgMax1", "dmg_max_1"]));
  const delay = toNum(fieldValue(row, ["delay", "Delay"]));
  const speed = delay > 0 ? delay / 1000 : 0;
  const dps = dmgMin > 0 && dmgMax > 0 && speed > 0 ? ((dmgMin + dmgMax) / 2) / speed : 0;
  const avg = dmgMin > 0 && dmgMax > 0 ? (dmgMin + dmgMax) / 2 : 0;
  const spread = avg > 0 ? (dmgMax - dmgMin) / avg : 0;
  const itemset = toNum(fieldValue(row, ["itemset", "ItemSet"]));
  const flags = toNum(fieldValue(row, ["Flags", "flags"]));
  const flagsExtra = toNum(fieldValue(row, ["FlagsExtra", "flagsextra", "flags_extra"]));
  const allowableClass = toNum(fieldValue(row, ["AllowableClass", "allowableclass"], 0));
  const allowableRace = toNum(fieldValue(row, ["AllowableRace", "allowablerace"], 0));
  const requiredSkill = toNum(fieldValue(row, ["RequiredSkill", "requiredskill"]));
  const requiredSkillRank = toNum(fieldValue(row, ["RequiredSkillRank", "requiredskillrank"]));
  const spellIds = [];
  const spellPayloads = [];
  for (let i = 1; i <= 5; i++) {
    const spell = toNum(fieldValue(row, [`spellid_${i}`, `spellid${i}`, `spellId${i}`]));
    if (spell > 0) {
      const trigger = toNum(fieldValue(row, [`spelltrigger_${i}`, `spelltrigger${i}`, `spellTrigger${i}`]));
      const charges = toNum(fieldValue(row, [`spellcharges_${i}`, `spellcharges${i}`, `spellCharges${i}`]));
      const cooldown = toNum(fieldValue(row, [`spellcooldown_${i}`, `spellcooldown${i}`, `spellCooldown${i}`]));
      const categoryCooldown = toNum(fieldValue(row, [`spellcategorycooldown_${i}`, `spellcategorycooldown${i}`, `spellCategoryCooldown${i}`]));
      spellIds.push(spell);
      spellPayloads.push({ slot: i, spell, trigger, charges, cooldown, categoryCooldown });
    }
  }

  const family = familyFromReference(classId, invType);
  return {
    entry: toNum(fieldValue(row, ["entry", "id", "Entry"])),
    name: String(fieldValue(row, ["name", "Name"], "Unnamed item")),
    description: String(fieldValue(row, ["description", "Description"], "")),
    classId,
    subclass,
    quality,
    invType,
    ilvl,
    reqLevel: toNum(fieldValue(row, ["RequiredLevel", "requiredlevel"])),
    displayId: toNum(fieldValue(row, ["displayid", "displayId", "DisplayID", "DisplayInfoID"])),
    material: toNum(fieldValue(row, ["Material", "material"]), null),
    sheath: toNum(fieldValue(row, ["sheath", "Sheath", "SheatheType"]), null),
    sellPrice: toNum(fieldValue(row, ["SellPrice", "sellprice", "sell_price"])),
    family,
    armorType: armorTypeFromDb(classId, subclass, invType),
    weaponSubclass: subclassKeyFromDb(classId, subclass, invType, stats),
    stats,
    sockets,
    socketBonus: toNum(fieldValue(row, ["socketBonus", "socketbonus"])),
    dmgMin,
    dmgMax,
    delay,
    speed,
    dps,
    spread,
    armor: toNum(fieldValue(row, ["armor", "Armor"])),
    block: toNum(fieldValue(row, ["block", "Block"])),
    itemset,
    flags,
    flagsExtra,
    allowableClass,
    allowableRace,
    bonding: toNum(fieldValue(row, ["bonding", "Bonding"])),
    requiredSkill,
    requiredSkillRank,
    spellIds,
    spellPayloads,
  };
}

function loadReferenceRows(rows) {
  const normalized = rows.map(normalizeReferenceRow).filter(Boolean);
  referenceItems = normalized;
  referenceSummary = summarizeReferenceItems(normalized);
  renderReferenceStatus();
  return normalized.length;
}

async function loadReferenceDataFromUi() {
  try {
    let text = byId("referencePaste").value || "";
    const file = byId("referenceFile").files && byId("referenceFile").files[0];
    if (file) text = await file.text();
    const rows = parseReferenceText(text);
    const count = loadReferenceRows(rows);
    if (!count) throw new Error("No usable equipable item_template rows survived filtering.");
  } catch (err) {
    byId("referenceStatus").innerHTML = `<span class="pill bad">Load failed: ${escapeHtml(err.message || err)}</span>`;
  }
}

async function loadReferenceDataFromLocalMySql() {
  const status = byId("localMysqlStatus");
  const url = String(byId("localMysqlHelperUrl").value || DEFAULT_LOCAL_MYSQL_HELPER_URL).replace(/\/+$/, "");
  status.innerHTML = `<span class="pill warn">Querying ${escapeHtml(url)}...</span>`;
  try {
    const response = await fetch(`${url}/api/reference-items`, { method: "GET" });
    if (!response.ok) {
      const text = await response.text();
      throw new Error(text || `HTTP ${response.status}`);
    }
    const payload = await response.json();
    if (!payload || !Array.isArray(payload.rows)) {
      throw new Error("Helper response did not include rows[].");
    }
    const count = loadReferenceRows(payload.rows);
    if (!count) throw new Error("No usable equipable item_template rows survived filtering.");
    status.innerHTML = `<div class="pill-row"><span class="pill good">Loaded ${count} row(s) from local MySQL</span><span class="pill">${escapeHtml(payload.database || "world DB")}</span><span class="pill">${escapeHtml(payload.version || MODEL_VERSION)}</span></div>`;
  } catch (err) {
    status.innerHTML = `<span class="pill bad">Local MySQL load failed: ${escapeHtml(err.message || err)}</span>`;
  }
}

function clearReferenceData() {
  referenceItems = [];
  referenceSummary = null;
  byId("referenceFile").value = "";
  byId("referencePaste").value = "";
  renderReferenceStatus();
}

function summarizeReferenceItems(items) {
  const summary = { total: items.length, families: {}, weapons: {}, qualities: {}, ranges: {} };
  for (const item of items) {
    summary.families[item.family] = (summary.families[item.family] || 0) + 1;
    summary.qualities[item.quality] = (summary.qualities[item.quality] || 0) + 1;
    if (item.family === "weapon") summary.weapons[item.weaponSubclass || "other"] = (summary.weapons[item.weaponSubclass || "other"] || 0) + 1;
    const key = `${item.quality}:${item.invType}:${item.weaponSubclass || item.armorType || item.family}`;
    if (!summary.ranges[key]) summary.ranges[key] = { min: item.ilvl, max: item.ilvl, count: 0 };
    summary.ranges[key].min = Math.min(summary.ranges[key].min, item.ilvl);
    summary.ranges[key].max = Math.max(summary.ranges[key].max, item.ilvl);
    summary.ranges[key].count++;
  }
  return summary;
}

function renderActiveReferenceFilterPills() {
  const filters = referenceFilterOptions();
  const pills = [];
  if (filters.excludePvp) pills.push("No PvP/resilience");
  if (filters.excludeItemSets) pills.push("No item sets");
  if (filters.excludeSpellPayloads) pills.push("No spell payloads");
  if (filters.exactSlot) pills.push("Exact slot");
  if (filters.exactArmorType) pills.push("Exact armor type");
  if (!pills.length) pills.push("Loose reference filters");
  return `<h3>Active filters</h3><div class="pill-row">${pills.map(p => `<span class="pill">${escapeHtml(p)}</span>`).join("")}</div>`;
}

function renderReferenceStatus() {
  if (!referenceItems.length) {
    byId("referenceStatus").innerHTML = "No reference dataset loaded yet.";
    byId("referenceDiagnostics").innerHTML = `<p class="hint">Load data to see family counts and available ranges.</p>${renderActiveReferenceFilterPills()}`;
    return;
  }
  const families = Object.entries(referenceSummary.families).map(([k, v]) => `<span class="pill">${escapeHtml(k)}: ${v}</span>`).join("");
  const qualities = Object.entries(referenceSummary.qualities).map(([k, v]) => `<span class="pill" style="color:${QUALITY[k]?.color || '#fff'}">${escapeHtml(QUALITY[k]?.name || k)}: ${v}</span>`).join("");
  const weapons = Object.entries(referenceSummary.weapons).sort((a,b)=>b[1]-a[1]).slice(0,12).map(([k, v]) => `<span class="pill">${escapeHtml(WEAPON_SUBCLASSES[k] || k)}: ${v}</span>`).join("");
  byId("referenceStatus").innerHTML = `<div class="pill-row"><span class="pill good">Loaded ${referenceSummary.total} usable items</span>${families}</div>`;
  byId("referenceDiagnostics").innerHTML = `
    ${renderActiveReferenceFilterPills()}
    <h3>Qualities</h3><div class="pill-row">${qualities}</div>
    <h3>Weapon families</h3><div class="pill-row">${weapons || '<span class="pill warn">No weapon rows found</span>'}</div>
    <p class="hint">Forward builder now uses these rows when “Reference mode” is set to prefer loaded DB references.</p>`;
}

function sameWeaponSlotGroup(a, b) {
  const wa = [13, 21, 22].includes(Number(a)) ? "1h" : Number(a) === 17 ? "2h" : [15, 26].includes(Number(a)) ? "ranged" : Number(a);
  const wb = [13, 21, 22].includes(Number(b)) ? "1h" : Number(b) === 17 ? "2h" : [15, 26].includes(Number(b)) ? "ranged" : Number(b);
  return wa === wb;
}

function referenceBaseScore(item, target) {
  let score = Math.abs(item.ilvl - target.ilvl);
  if (item.quality !== target.quality) score += 80;
  if (target.family && item.family !== target.family) score += 80;

  if (target.slot && item.invType !== target.slot) {
    if (target.family === "weapon" && sameWeaponSlotGroup(item.invType, target.slot)) score += 5;
    else if (isJewelrySlot(item.invType) && isJewelrySlot(target.slot)) score += 12;
    else score += 25;
  }

  if (targetUsesWeaponSubclass(target) && item.weaponSubclass !== target.weaponSubclass) score += 35;

  if (target.family !== "weapon" && target.armorType && target.armorType !== "none" && item.armorType !== target.armorType) {
    score += 35;
  }

  if (target.archetype && target.archetype !== "any") {
    score += (1 - archetypeMatchWeight(itemArchetype(item), target.archetype)) * 45;
  }

  if (targetUsesWeaponSpeed(target) && item.speed > 0) score += Math.abs(item.speed - target.speed) * 5;
  return score;
}

function nearbyReferenceItems(target, predicate, limit = 24) {
  if (!referenceItems.length) return [];
  const filterOptions = referenceFilterOptions();
  return referenceItems
    .filter(item => predicate(item) && itemPassesReferenceFilters(item, target, filterOptions))
    .map(item => ({ item, score: referenceBaseScore(item, target) }))
    .sort((a, b) => a.score - b.score)
    .slice(0, limit);
}

function referenceWeight(score) {
  return 1 / (1 + Math.pow(score / 8, 2));
}

function weightedAverage(pairs, fallback = 0) {
  let n = 0;
  let d = 0;
  for (const pair of pairs) {
    const value = Number(pair.value);
    const weight = Number(pair.weight);
    if (!Number.isFinite(value) || !Number.isFinite(weight) || weight <= 0) continue;
    n += value * weight;
    d += weight;
  }
  return d > 0 ? n / d : fallback;
}

function weightedQuantile(pairs, quantile, fallback = 0) {
  const clean = (pairs || [])
    .map(pair => ({ ...pair, value: Number(pair.value), weight: Number(pair.weight) }))
    .filter(pair => Number.isFinite(pair.value) && Number.isFinite(pair.weight) && pair.weight > 0)
    .sort((a, b) => a.value - b.value);
  if (!clean.length) return fallback;
  const total = clean.reduce((sum, pair) => sum + pair.weight, 0);
  if (total <= 0) return fallback;
  const target = total * Math.max(0, Math.min(1, Number(quantile)));
  let seen = 0;
  for (const pair of clean) {
    seen += pair.weight;
    if (seen >= target) return pair.value;
  }
  return clean[clean.length - 1].value;
}

function robustWeightedEstimate(pairs, fallback = 0, options = {}) {
  const clean = (pairs || [])
    .map(pair => ({ ...pair, value: Number(pair.value), weight: Number(pair.weight) }))
    .filter(pair => Number.isFinite(pair.value) && Number.isFinite(pair.weight) && pair.weight > 0);
  if (!clean.length) {
    return { value: fallback, median: fallback, mad: 0, count: 0, inputCount: 0, outlierCount: 0, kept: [] };
  }

  const median = weightedQuantile(clean, 0.5, clean[0].value);
  const deviations = clean.map(pair => ({ value: Math.abs(pair.value - median), weight: pair.weight }));
  const mad = weightedQuantile(deviations, 0.5, 0);
  const relativeBand = options.relativeBand !== undefined ? Number(options.relativeBand) : 0.08;
  const absoluteBand = options.absoluteBand !== undefined ? Number(options.absoluteBand) : 0;
  const madScale = options.madScale !== undefined ? Number(options.madScale) : 3.5;
  const minKept = Math.min(clean.length, Math.max(1, Number(options.minKept || Math.min(3, clean.length))));
  const band = Math.max(Math.abs(median) * relativeBand, absoluteBand, mad * madScale);
  let kept = clean.filter(pair => Math.abs(pair.value - median) <= band);

  if (kept.length < minKept) {
    kept = [...clean]
      .sort((a, b) => Math.abs(a.value - median) - Math.abs(b.value - median))
      .slice(0, minKept);
  }

  return {
    value: weightedAverage(kept, fallback),
    median,
    mad,
    count: kept.length,
    inputCount: clean.length,
    outlierCount: Math.max(0, clean.length - kept.length),
    kept,
  };
}

function referenceEstimateConfidence(estimate) {
  const count = Number(estimate?.count || 0);
  const outliers = Number(estimate?.outlierCount || 0);
  if (count >= 6 && outliers <= 1) return "high";
  if (count >= 3) return "medium";
  return "low";
}

function isPurePassiveReference(item) {
  return !!item
    && Number(item.itemset || 0) === 0
    && !(item.spellIds && item.spellIds.length)
    && !(item.spellPayloads && item.spellPayloads.length);
}

function referenceFactFlags(item) {
  const flags = [];
  if (!item) return flags;
  if (Number(item.itemset || 0) > 0) flags.push(`set ${item.itemset}`);
  if (item.spellPayloads && item.spellPayloads.length) flags.push(`${item.spellPayloads.length} spell payload`);
  if (Number(item.requiredSkill || 0) > 0) flags.push(`skill ${item.requiredSkill}/${item.requiredSkillRank || 0}`);
  if (Number(item.allowableClass || 0) && Number(item.allowableClass || 0) !== -1) flags.push(`class mask ${item.allowableClass}`);
  if (Number(item.allowableRace || 0) && Number(item.allowableRace || 0) !== -1) flags.push(`race mask ${item.allowableRace}`);
  if (flags.length === 0) flags.push("pure passive");
  return flags;
}

function spellTriggerLabel(trigger) {
  return ITEM_SPELL_TRIGGER_LABELS[Number(trigger)] || `Trigger ${trigger}`;
}

function itemPayloadWarnings(item) {
  const warnings = [];
  if (!item) return warnings;
  if (Number(item.itemset || 0) > 0) warnings.push(`Item set ${item.itemset}: set bonuses are hidden power outside passive stat columns.`);
  if (Number(item.socketBonus || 0) > 0) warnings.push(`Socket bonus spell ${item.socketBonus}: bonus value is not decoded automatically.`);
  for (const payload of item.spellPayloads || []) {
    warnings.push(`Spell ${payload.spell} (${spellTriggerLabel(payload.trigger)}): charges ${payload.charges || 0}, cooldown ${payload.cooldown || 0}, category cooldown ${payload.categoryCooldown || 0}.`);
  }
  return warnings;
}

function renderPayloadAudit(item) {
  if (!item) return "";
  const warnings = itemPayloadWarnings(item);
  if (!warnings.length) {
    return `<h3>Spell / set payload audit</h3><div class="pill-row"><span class="pill good">No set, socket-bonus, or spell payload flags detected</span></div>`;
  }

  const rows = warnings.map(text => `<tr><td>${escapeHtml(text)}</td></tr>`).join("");
  return `
    <h3>Spell / set payload audit</h3>
    <div class="pill-row"><span class="pill warn">Hidden power detected</span><span class="pill">${warnings.length} flag(s)</span></div>
    <table class="result-table">
      <thead><tr><th>Payload fact</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>
    <p class="hint">These facts are why set/proc/use items should usually be excluded from passive budget calibration unless you are intentionally studying those rows.</p>`;
}

function referenceWeaponDamage(quality, ilvl, invType, subclass, speedSeconds, excludeEntry = null) {
  if (!referenceItems.length) return null;
  const target = { quality: Number(quality), ilvl: Number(ilvl), slot: Number(invType), family: "weapon", weaponSubclass: subclass, speed: Number(speedSeconds || 0), excludeEntry };
  let candidates = nearbyReferenceItems(target, item => item.family === "weapon" && item.dps > 0 && item.quality === target.quality && item.weaponSubclass === subclass && sameWeaponSlotGroup(item.invType, target.slot), 18);
  if (candidates.length < 2) {
    candidates = nearbyReferenceItems(target, item => item.family === "weapon" && item.dps > 0 && item.quality === target.quality && item.weaponSubclass === subclass, 18);
  }
  if (candidates.length < 2) {
    candidates = nearbyReferenceItems(target, item => item.family === "weapon" && item.dps > 0 && item.quality === target.quality && sameWeaponSlotGroup(item.invType, target.slot), 18);
  }
  if (!candidates.length) return null;

  const exact = candidates.filter(c => c.item.ilvl === target.ilvl && c.item.weaponSubclass === subclass && sameWeaponSlotGroup(c.item.invType, target.slot));
  const pool = exact.length ? exact : candidates;
  const dpsEstimate = robustWeightedEstimate(
    pool.map(c => ({ value: c.item.dps, weight: exact.length ? 1 : referenceWeight(c.score), candidate: c })),
    null,
    { relativeBand: 0.075, absoluteBand: 1.5, minKept: Math.min(3, pool.length) }
  );
  const spreadEstimate = robustWeightedEstimate(
    pool.map(c => ({ value: c.item.spread || damageSpreadCoefficient(subclass, invType, quality, ilvl), weight: exact.length ? 1 : referenceWeight(c.score), candidate: c })),
    damageSpreadCoefficient(subclass, invType, quality, ilvl),
    { relativeBand: 0.18, absoluteBand: 0.04, minKept: Math.min(3, pool.length) }
  );
  const speedEstimate = robustWeightedEstimate(
    pool.map(c => ({ value: c.item.speed, weight: exact.length ? 1 : referenceWeight(c.score), candidate: c })),
    defaultWeaponSpeed(subclass, invType),
    { relativeBand: 0.20, absoluteBand: 0.15, minKept: Math.min(3, pool.length) }
  );
  const dps = dpsEstimate.value;
  const spread = spreadEstimate.value;
  const speed = Number(speedSeconds || 0) > 0 ? Number(speedSeconds) : speedEstimate.value;
  const refPool = dpsEstimate.kept.map(pair => pair.candidate).filter(Boolean);
  const avg = dps * speed;
  return {
    dps,
    speed,
    coeff: spread,
    min: Math.max(1, Math.floor(avg * (1 - spread / 2))),
    max: Math.max(1, Math.ceil(avg * (1 + spread / 2))),
    source: "reference",
    referenceCount: dpsEstimate.count,
    candidateCount: pool.length,
    outlierCount: dpsEstimate.outlierCount,
    confidence: referenceEstimateConfidence(dpsEstimate),
    references: (refPool.length ? refPool : pool).slice(0, 6).map(c => referenceMini(c.item, c.score)),
  };
}

function referenceArmorValue(invType, armorType, quality, ilvl, bonusArmor = 0, excludeEntry = null) {
  if (!referenceItems.length || armorType === "none") return null;
  const target = { quality: Number(quality), ilvl: Number(ilvl), slot: Number(invType), family: "armor", armorType, excludeEntry };
  let candidates = nearbyReferenceItems(target, item => item.armor > 0 && item.quality === target.quality && item.invType === target.slot && item.armorType === armorType, 20);
  if (candidates.length < 2) {
    candidates = nearbyReferenceItems(target, item => item.armor > 0 && item.quality === target.quality && item.armorType === armorType, 20);
  }
  if (!candidates.length) return null;
  const exact = candidates.filter(c => c.item.ilvl === target.ilvl && c.item.invType === target.slot && c.item.armorType === armorType);
  const pool = exact.length ? exact : candidates;
  const estimate = robustWeightedEstimate(
    pool.map(c => ({ value: c.item.armor, weight: exact.length ? 1 : referenceWeight(c.score), candidate: c })),
    null,
    { relativeBand: 0.07, absoluteBand: 8, minKept: Math.min(3, pool.length) }
  );
  const refPool = estimate.kept.map(pair => pair.candidate).filter(Boolean);
  return {
    armor: Math.max(0, Math.round(estimate.value + Number(bonusArmor || 0))),
    source: "reference",
    referenceCount: estimate.count,
    candidateCount: pool.length,
    outlierCount: estimate.outlierCount,
    confidence: referenceEstimateConfidence(estimate),
    references: (refPool.length ? refPool : pool).slice(0, 6).map(c => referenceMini(c.item, c.score)),
  };
}

function referencePassiveBudgetValue(ilvl, quality, invType, family, weaponSubclass, armorType, excludeEntry = null, statRows = [], requestedArchetype = "auto") {
  if (!referenceItems.length) return null;
  const target = { quality: Number(quality), ilvl: Number(ilvl), slot: Number(invType), family, weaponSubclass, armorType, excludeEntry };
  const archetypeInfo = referenceArchetypeRule(target, statRows, requestedArchetype);
  target.archetype = archetypeInfo.archetype;
  const archetypeOk = item => referenceItemMatchesArchetype(item, archetypeInfo.archetype, archetypeInfo.rule, 0.65);
  const primary = item => item.quality === target.quality && item.family === family && Math.abs(item.ilvl - target.ilvl) <= 25 &&
    (family !== "weapon" || ((!usableWeaponSubclass(weaponSubclass) || item.weaponSubclass === weaponSubclass) && sameWeaponSlotGroup(item.invType, target.slot))) &&
    (family === "weapon" || !armorType || armorType === "none" || item.armorType === armorType || item.invType === target.slot) &&
    item.stats && item.stats.length > 0 && archetypeOk(item);
  let candidates = nearbyReferenceItems(target, primary, 30);
  if (!candidates.length) {
    candidates = nearbyReferenceItems(target, item => item.quality === target.quality && item.family === family && item.stats && item.stats.length > 0 && archetypeOk(item), 30);
  }
  if (!candidates.length) return null;
  const ratios = [];
  for (const c of candidates) {
    const item = c.item;
    const obs = observedBudget(item.stats, item.quality, item.ilvl, item.invType, item.sockets);
    const exp = passiveBudget(item.ilvl, item.quality, item.invType, item.family);
    if (obs <= 0 || exp <= 0) continue;
    const ratio = obs / exp;
    if (ratio < 0.15 || ratio > 1.35) continue;
    const passiveWeight = isPurePassiveReference(item) ? 1 : 0.55;
    const archetypeWeight = archetypeInfo.archetype === "any" ? 1 : archetypeMatchWeight(itemArchetype(item), archetypeInfo.archetype);
    ratios.push({ value: ratio, weight: referenceWeight(c.score) * passiveWeight * archetypeWeight, candidate: c });
  }
  if (!ratios.length) return null;
  const estimate = robustWeightedEstimate(ratios, 1, { relativeBand: 0.11, absoluteBand: 0.035, minKept: Math.min(4, ratios.length) });
  const scale = Math.max(0.15, Math.min(1.35, estimate.value));
  const formulaBudget = passiveBudget(ilvl, quality, invType, family);
  const refs = estimate.kept.map(pair => pair.candidate).filter(Boolean);
  return {
    budget: formulaBudget * scale,
    scale,
    source: "reference",
    referenceCount: estimate.count,
    candidateCount: ratios.length,
    outlierCount: estimate.outlierCount,
    confidence: referenceEstimateConfidence(estimate),
    archetype: archetypeInfo.archetype,
    references: refs.slice(0, 6).map(c => referenceMini(c.item, c.score)),
  };
}


function statEffectiveValue(statId, value, quality, ilvl, invType) {
  const id = Number(statId);
  const val = Number(value || 0);
  if (!STATS[id] || val <= 0) return 0;
  return val * statMod(id, quality, ilvl, invType);
}

function statBudgetContribution(statId, value, quality, ilvl, invType) {
  const effective = statEffectiveValue(statId, value, quality, ilvl, invType);
  return effective > 0 ? Math.pow(effective, EXPONENT) : 0;
}

function profileRuleFromBase(baseArchetype) {
  const rule = PROFILE_STAT_RULES[baseArchetype];
  if (!rule) return null;
  return {
    archetype: baseArchetype,
    baseArchetype,
    allow: new Set(rule.allow),
    block: new Set(rule.block),
  };
}

function makeProfileStatRule(resolvedArchetype, family, slot, weaponSubclass, armorType, currentRows = []) {
  if (!resolvedArchetype || resolvedArchetype === "any") return null;

  if (resolvedArchetype === "pvp") {
    let base = inferTargetArchetype(currentRows, family, slot, weaponSubclass);
    if (!base || base === "any" || base === "pvp") {
      if (family === "weapon") {
        if (["caster_mh", "caster_staff", "wand"].includes(weaponSubclass)) base = "caster";
        else if (["bow", "gun", "crossbow", "thrown"].includes(weaponSubclass)) base = "hunter";
        else base = "melee";
      } else if (Number(slot) === 14) {
        base = "melee";
      } else if (armorType === "cloth") {
        base = "caster";
      } else {
        base = "melee";
      }
    }

    const baseRule = profileRuleFromBase(base) || profileRuleFromBase("melee");
    for (const statId of PVP_EXTRA_STATS) baseRule.allow.add(statId);
    for (const statId of PVP_ALWAYS_BLOCKED_STATS) {
      baseRule.allow.delete(statId);
      baseRule.block.add(statId);
    }
    baseRule.archetype = `pvp-${base}`;
    baseRule.baseArchetype = base;
    return baseRule;
  }

  return profileRuleFromBase(resolvedArchetype);
}

function profileAllowsStat(statId, rule) {
  if (!rule) return true;
  const id = Number(statId);
  if (rule.block.has(id)) return false;
  return rule.allow.has(id);
}

function itemBlockedStatShare(item, rule) {
  if (!rule) return 0;
  const all = itemStatBudgetProfile(item);
  if (!all || all.total <= 0) return 0;
  let blocked = 0;
  for (const stat of all.contributions) {
    if (!profileAllowsStat(stat.statId, rule)) blocked += stat.budget;
  }
  return blocked / all.total;
}

function itemMatchesProfileRule(item, rule) {
  if (!rule) return true;
  return itemBlockedStatShare(item, rule) <= PROFILE_BLOCK_THRESHOLD;
}

function itemStatBudgetProfile(item, rule = null) {
  const contributions = [];
  let total = 0;
  for (const stat of item.stats || []) {
    if (!profileAllowsStat(stat.statId, rule)) continue;
    const budget = statBudgetContribution(stat.statId, stat.value, item.quality, item.ilvl, item.invType);
    if (budget <= 0) continue;
    contributions.push({ statId: Number(stat.statId), value: Number(stat.value), budget });
    total += budget;
  }
  if (total <= 0) return null;
  return {
    total,
    contributions: contributions.map(c => ({ ...c, share: c.budget / total })),
  };
}

function archetypeScoresForStats(stats, quality, ilvl, invType) {
  const scores = Object.fromEntries(Object.keys(ARCHETYPE_STAT_WEIGHTS).map(k => [k, 0]));
  for (const stat of stats || []) {
    const id = Number(stat.statId);
    const budget = statBudgetContribution(id, stat.value || stat.share || 1, quality, ilvl, invType);
    if (budget <= 0) continue;
    for (const [archetype, weights] of Object.entries(ARCHETYPE_STAT_WEIGHTS)) {
      scores[archetype] += budget * Number(weights[id] || 0);
    }
  }
  return scores;
}

function bestArchetypeFromScores(scores, fallback = "any") {
  let best = fallback;
  let bestScore = 0;
  for (const [archetype, score] of Object.entries(scores || {})) {
    if (score > bestScore) {
      best = archetype;
      bestScore = score;
    }
  }
  return bestScore > 0 ? best : fallback;
}

function itemArchetype(item) {
  return bestArchetypeFromScores(archetypeScoresForStats(item.stats, item.quality, item.ilvl, item.invType), "generic");
}

function inferTargetArchetype(statRows, family, slot, weaponSubclass) {
  const rows = (statRows || []).filter(r => Number(r.share ?? r.value) > 0 && STATS[Number(r.statId)]);
  if (rows.length) {
    const pseudoStats = rows.map(r => ({ statId: Number(r.statId), value: Number(r.share ?? r.value) }));
    return bestArchetypeFromScores(archetypeScoresForStats(pseudoStats, numberValue("fQuality") || 4, numberValue("fItemLevel") || 100, Number(slot)), "any");
  }

  if (family === "weapon") {
    if (["caster_mh", "caster_staff", "wand"].includes(weaponSubclass)) return "caster";
    if (["bow", "gun", "crossbow", "thrown"].includes(weaponSubclass)) return "hunter";
    if (["tank_1h"].includes(weaponSubclass)) return "tank";
    return "melee";
  }

  if (Number(slot) === 14) return "tank";
  if ([2, 11, 12, 16, 23].includes(Number(slot))) return "any";
  return "melee";
}

function archetypeMatchWeight(itemArchetypeName, targetArchetype) {
  if (!targetArchetype || targetArchetype === "any") return 1;
  if (itemArchetypeName === targetArchetype) return 1;
  if (itemArchetypeName === "generic") return 0.45;
  const related = new Set([
    "melee:hunter", "hunter:melee",
    "caster:healer", "healer:caster",
    "tank:pvp", "pvp:tank",
  ]);
  if (related.has(`${itemArchetypeName}:${targetArchetype}`)) return 0.65;
  return 0.25;
}

function targetReferenceArchetype(target, statRows = [], requestedArchetype = "auto") {
  if (requestedArchetype && requestedArchetype !== "auto") return requestedArchetype;
  const hasStatRows = (statRows || []).some(r => Number(r.share ?? r.value) > 0 && STATS[Number(r.statId)]);
  if (!hasStatRows && target.family !== "weapon") return "any";
  return inferTargetArchetype(statRows, target.family, target.slot, target.weaponSubclass);
}

function referenceArchetypeRule(target, statRows = [], requestedArchetype = "auto") {
  const archetype = targetReferenceArchetype(target, statRows, requestedArchetype);
  return {
    archetype,
    rule: makeProfileStatRule(archetype, target.family, target.slot, target.weaponSubclass, target.armorType, statRows),
  };
}

function referenceItemMatchesArchetype(item, archetype, rule, minWeight = 0.65) {
  if (!archetype || archetype === "any") return true;
  if (!itemMatchesProfileRule(item, rule)) return false;
  return archetypeMatchWeight(itemArchetype(item), archetype) >= minWeight;
}

function sameProfileSlotGroup(itemSlot, targetSlot, family) {
  const a = Number(itemSlot);
  const b = Number(targetSlot);
  if (a === b) return true;
  if (family === "weapon") return sameWeaponSlotGroup(a, b);
  if (isJewelrySlot(a) && isJewelrySlot(b)) return true;
  return false;
}

function sameExactProfileSlot(itemSlot, targetSlot) {
  return Number(itemSlot) === Number(targetSlot);
}

function dedupeCandidates(candidates) {
  const seen = new Set();
  const out = [];
  for (const c of candidates) {
    const key = c.item.entry || `${c.item.name}:${c.item.ilvl}:${c.item.invType}`;
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(c);
  }
  return out;
}

function makeReferenceStatDiagnostics(usedRefs, maxRefs = 8, profileRule = null) {
  const diagnostics = [];
  const refs = [...(usedRefs || [])]
    .sort((a, b) => b.weight - a.weight)
    .slice(0, maxRefs);

  for (const ref of refs) {
    const item = ref.item;
    const profile = itemStatBudgetProfile(item, profileRule);
    if (!profile) continue;

    const contributions = [...profile.contributions].sort((a, b) => b.budget - a.budget);
    for (const c of contributions) {
      const mod = statMod(c.statId, item.quality, item.ilvl, item.invType);
      const effective = statEffectiveValue(c.statId, c.value, item.quality, item.ilvl, item.invType);
      diagnostics.push({
        entry: item.entry,
        name: item.name,
        ilvl: item.ilvl,
        slot: SLOTS[item.invType] || item.invType,
        archetype: ref.archetype,
        score: ref.score,
        profileWeight: ref.weight,
        statId: c.statId,
        stat: STATS[c.statId]?.label || `Unknown stat ${c.statId}`,
        rawValue: c.value,
        mod,
        effective,
        budget: c.budget,
        share: c.share,
      });
    }
  }

  return diagnostics;
}

function referenceStatProfileValue(ilvl, quality, invType, family, weaponSubclass, armorType, targetArchetype, currentRows = []) {
  if (!referenceItems.length) return null;
  const slot = Number(invType);
  const target = { quality: Number(quality), ilvl: Number(ilvl), slot, family, weaponSubclass, armorType };
  const resolvedArchetype = targetArchetype === "auto" ? inferTargetArchetype(currentRows, family, slot, weaponSubclass) : (targetArchetype || "any");
  const profileRule = makeProfileStatRule(resolvedArchetype, family, slot, weaponSubclass, armorType, currentRows);

  const hasUsableStats = item => item.stats && item.stats.length > 0 && itemMatchesProfileRule(item, profileRule) && itemStatBudgetProfile(item, profileRule);
  const sameCore = item => item.quality === target.quality && item.family === family && hasUsableStats(item);
  const sameWeapon = item => family !== "weapon" || ((!usableWeaponSubclass(weaponSubclass) || item.weaponSubclass === weaponSubclass) && sameWeaponSlotGroup(item.invType, target.slot));
  const sameArmor = item => family === "weapon" || !armorType || armorType === "none" || item.armorType === armorType || isPassiveNonArmorSlot(target.slot);
  const sameSlot = item => sameProfileSlotGroup(item.invType, target.slot, family);
  const exactSlot = item => sameExactProfileSlot(item.invType, target.slot);
  const near = (item, range) => Math.abs(item.ilvl - target.ilvl) <= range;
  const archOk = item => {
    if (resolvedArchetype === "any") return true;
    if (!itemMatchesProfileRule(item, profileRule)) return false;
    const arch = itemArchetype(item);
    if (resolvedArchetype === "pvp") {
      return archetypeMatchWeight(arch, "pvp") >= 0.65 || archetypeMatchWeight(arch, profileRule?.baseArchetype) >= 0.65;
    }
    return archetypeMatchWeight(arch, resolvedArchetype) >= 0.65;
  };

  let candidates = nearbyReferenceItems(target, item => sameCore(item) && sameWeapon(item) && sameArmor(item) && exactSlot(item) && near(item, 35) && archOk(item), 40);

  if (candidates.length < 3) {
    candidates = dedupeCandidates(candidates.concat(
      nearbyReferenceItems(target, item => sameCore(item) && sameWeapon(item) && sameArmor(item) && sameSlot(item) && near(item, 45) && archOk(item), 40)
    ));
  }

  if (candidates.length < 3) {
    candidates = dedupeCandidates(candidates.concat(
      nearbyReferenceItems(target, item => sameCore(item) && sameWeapon(item) && sameArmor(item) && sameSlot(item) && near(item, 55), 40)
    ));
  }

  if (candidates.length < 3) {
    candidates = dedupeCandidates(candidates.concat(
      nearbyReferenceItems(target, item => sameCore(item) && near(item, 65), 40)
    ));
  }

  if (!candidates.length) return null;

  const accum = new Map();
  let totalWeight = 0;
  const usedRefs = [];

  for (const c of candidates.slice(0, 40)) {
    const profile = itemStatBudgetProfile(c.item, profileRule);
    if (!profile || profile.total <= 0) continue;
    const arch = itemArchetype(c.item);
    const match = archetypeMatchWeight(arch, resolvedArchetype);
    const slotWeight = profileSlotWeight(c.item.invType, target.slot, family);
    const levelWeight = Math.max(0.25, 1 - Math.abs(c.item.ilvl - target.ilvl) / 90);
    const payloadPenalty = isPurePassiveReference(c.item) ? 1 : 0.75;
    const weight = referenceWeight(c.score) * match * slotWeight * levelWeight * payloadPenalty;
    if (weight <= 0) continue;

    for (const stat of profile.contributions) {
      accum.set(stat.statId, (accum.get(stat.statId) || 0) + stat.share * weight);
    }

    totalWeight += weight;
    usedRefs.push({ ...c, archetype: arch, weight });
  }

  if (totalWeight <= 0 || !accum.size) return null;

  let rows = Array.from(accum.entries())
    .map(([statId, weightedShare]) => ({ statId: Number(statId), share: weightedShare / totalWeight }))
    .filter(r => r.share >= 0.015 && STATS[r.statId])
    .sort((a, b) => b.share - a.share)
    .slice(0, 10);

  const keptTotal = rows.reduce((sum, r) => sum + r.share, 0) || 1;
  rows = rows.map(r => ({ statId: r.statId, share: r.share / keptTotal }));

  return {
    archetype: resolvedArchetype,
    archetypeLabel: ARCHETYPE_LABELS[resolvedArchetype] || resolvedArchetype,
    referenceCount: usedRefs.length,
    profileFilter: profileRule ? profileRule.archetype : "none",
    rows,
    diagnostics: makeReferenceStatDiagnostics(usedRefs, 8, profileRule),
    references: usedRefs
      .sort((a, b) => b.weight - a.weight)
      .slice(0, 8)
      .map(c => ({ ...referenceMini(c.item, c.score), archetype: c.archetype, profileWeight: round(c.weight, 4) })),
  };
}

function applyReferenceStatProfile() {
  if (!referenceItems.length) {
    byId("forwardResult").innerHTML = `<span class="pill bad">Load a reference CSV/JSON first, then the local DB profile can vote.</span>`;
    return;
  }

  const ilvl = numberValue("fItemLevel");
  const quality = numberValue("fQuality");
  const slot = numberValue("fSlot");
  const family = byId("fFamily").value;
  const armorType = byId("fArmorType").value;
  const rawWeaponSubclass = byId("fWeaponSubclass").value;
  const weaponSubclass = family === "weapon" ? (usableWeaponSubclass(rawWeaponSubclass) ? rawWeaponSubclass : defaultWeaponSubclassForSlot(slot)) : "none";
  const mode = byId("fReferenceProfileMode") ? byId("fReferenceProfileMode").value : "auto";
  const currentRows = collectForwardStats();
  const profile = referenceStatProfileValue(ilvl, quality, slot, family, weaponSubclass, armorType, mode, currentRows);

  if (!profile || !profile.rows.length) {
    byId("forwardResult").innerHTML = `<span class="pill bad">No usable nearby stat-profile references found for this shell. Try a broader archetype or load a larger item_template export.</span>`;
    return;
  }

  byId("forwardStatsBody").innerHTML = "";
  for (const row of profile.rows) {
    addForwardStat(row.statId, round(row.share * 100, 1));
  }

  activeForwardReferenceProfile = profile;
  calculateForward();
}

function referenceMini(item, score) {
  return {
    entry: item.entry,
    name: item.name,
    ilvl: item.ilvl,
    quality: item.quality,
    slot: SLOTS[item.invType] || item.invType,
    subclass: item.weaponSubclass ? (WEAPON_SUBCLASSES[item.weaponSubclass] || item.weaponSubclass) : (ARMOR_TYPES[item.armorType] || item.armorType),
    dps: item.dps ? round(item.dps, 2) : undefined,
    armor: item.armor || undefined,
    stats: item.stats.map(s => `+${s.value} ${STATS[s.statId]?.label || s.statId}`).join(", "),
    facts: referenceFactFlags(item).join(", "),
    score: round(score, 2),
  };
}

function renderReferencePills(refs) {
  if (!refs || !refs.length) return "";
  return `<div class="reference-list">${refs.map(r => `
    <div class="reference-item">
      <strong>${escapeHtml(r.name)}</strong>
      <small>#${r.entry || "?"} • ilvl ${r.ilvl} • ${escapeHtml(r.slot)} • ${escapeHtml(r.subclass || "")}${r.dps ? ` • ${r.dps} DPS` : ""}${r.armor ? ` • ${r.armor} armor` : ""}${r.archetype ? ` • ${escapeHtml(ARCHETYPE_LABELS[r.archetype] || r.archetype)}` : ""}${r.profileWeight ? ` • profile weight ${r.profileWeight}` : ""}</small>
      ${r.stats ? `<span>${escapeHtml(r.stats)}</span>` : ""}
      ${r.facts ? `<small>${escapeHtml(r.facts)}</small>` : ""}
    </div>`).join("")}</div>`;
}

function renderReferenceStatDiagnostics(profile) {
  if (!profile || !profile.diagnostics || !profile.diagnostics.length) return "";

  const inferredRows = (profile.rows || []).map(r => `
    <tr>
      <td>${escapeHtml(STATS[r.statId]?.label || r.statId)}</td>
      <td>${(r.share * 100).toFixed(2)}%</td>
      <td>${escapeHtml(String(STATS[r.statId]?.group || ""))}</td>
    </tr>`).join("");

  const diagnosticRows = profile.diagnostics.map(d => `
    <tr>
      <td><strong>${escapeHtml(d.name)}</strong><br><small class="muted">#${d.entry || "?"} • ilvl ${d.ilvl} • ${escapeHtml(d.slot)} • ${escapeHtml(ARCHETYPE_LABELS[d.archetype] || d.archetype || "generic")}</small></td>
      <td>${escapeHtml(d.stat)}</td>
      <td>${formatNumber(d.rawValue)}</td>
      <td>${Number(d.mod).toFixed(4)}</td>
      <td>${formatNumber(d.effective)}</td>
      <td>${formatNumber(d.budget)}</td>
      <td>${(d.share * 100).toFixed(2)}%</td>
      <td>${Number(d.profileWeight).toFixed(4)}</td>
    </tr>`).join("");

  return `
    <h3>Reference stat-budget audit</h3>
    <p class="hint">This is the accounting table. Raw item stats are first filtered by the selected profile, then multiplied by the stat modifier and raised by the itemization exponent to become normalized budget contribution. Those budget shares are what nearby DB items use when voting on the generated stat profile.</p>
    <h4>Inferred generated profile</h4>
    <div class="stat-table-wrap">
      <table class="result-table diagnostics-table">
        <thead><tr><th>Generated stat</th><th>Budget share</th><th>Group</th></tr></thead>
        <tbody>${inferredRows}</tbody>
      </table>
    </div>
    <h4>Reference item conversion</h4>
    <div class="stat-table-wrap">
      <table class="result-table diagnostics-table">
        <thead><tr><th>Reference item</th><th>Stat</th><th>Raw</th><th>Mod</th><th>Effective</th><th>Budget contribution</th><th>Item share</th><th>Ref weight</th></tr></thead>
        <tbody>${diagnosticRows}</tbody>
      </table>
    </div>`;
}

function populateSelects() {
  const qualitySelects = ["fQuality", "rQuality"];
  for (const id of qualitySelects) {
    const sel = byId(id);
    sel.innerHTML = Object.entries(QUALITY)
      .filter(([key]) => Number(key) >= 2 && Number(key) <= 5)
      .map(([key, q]) => `<option value="${key}" ${Number(key) === 4 ? "selected" : ""}>${q.name}</option>`)
      .join("");
  }

  for (const id of ["fSlot", "rSlot"]) {
    const sel = byId(id);
    sel.innerHTML = Object.entries(SLOTS)
      .map(([key, label]) => `<option value="${key}">${label}</option>`)
      .join("");
    sel.value = "5";
  }

  for (const id of ["fArmorType", "rArmorType"]) {
    const sel = byId(id);
    sel.innerHTML = Object.entries(ARMOR_TYPES)
      .map(([key, label]) => `<option value="${key}">${label}</option>`)
      .join("");
    sel.value = "plate";
  }

  for (const id of ["fWeaponSubclass", "rWeaponSubclass"]) {
    const sel = byId(id);
    sel.innerHTML = Object.entries(WEAPON_SUBCLASSES)
      .map(([key, label]) => `<option value="${key}">${label}</option>`)
      .join("");
    sel.value = "none";
  }

  syncShellControlState("f");
  syncShellControlState("r");
}

function statOptions(selected) {
  return Object.entries(STATS)
    .filter(([id]) => Number(id) !== 9991)
    .map(([id, stat]) => `<option value="${id}" ${Number(id) === Number(selected) ? "selected" : ""}>${stat.label}</option>`)
    .join("");
}

function addForwardStat(statId = 4, share = 50) {
  const tr = document.createElement("tr");
  tr.innerHTML = `
    <td><select class="fStatId">${statOptions(statId)}</select></td>
    <td><input class="fShare" type="number" min="0" max="100" step="0.1" value="${share}"></td>
    <td><button class="icon-button remove-row" type="button">✕</button></td>
  `;
  byId("forwardStatsBody").appendChild(tr);
}

function addReverseStat(statId = 4, value = 50) {
  const tr = document.createElement("tr");
  tr.innerHTML = `
    <td><select class="rStatId">${statOptions(statId)}</select></td>
    <td><input class="rValue" type="number" min="0" step="1" value="${value}"></td>
    <td><button class="icon-button remove-row" type="button">✕</button></td>
  `;
  byId("reverseStatsBody").appendChild(tr);
}

function collectForwardStats() {
  return Array.from(document.querySelectorAll("#forwardStatsBody tr")).map(tr => ({
    statId: Number(tr.querySelector(".fStatId").value),
    share: Number(tr.querySelector(".fShare").value || 0),
  }));
}

function collectReverseStats() {
  return Array.from(document.querySelectorAll("#reverseStatsBody tr")).map(tr => ({
    statId: Number(tr.querySelector(".rStatId").value),
    value: Number(tr.querySelector(".rValue").value || 0),
  })).filter(r => r.value > 0);
}

const FORWARD_PRESET_ROWS = {
  melee: {
    // DB evidence: strength melee armor is primary + stamina, then ratings.
    // Stamina is budgeted below strength so generated PvE DPS items do not print
    // stamina above the primary stat.
    2: [[4, 70], [7, 30]],
    3: [[4, 50], [7, 24], [32, 26]],
    4: [[4, 43], [7, 21], [32, 18], [31, 18]],
    5: [[4, 40], [7, 18], [32, 16], [31, 16], [36, 10]],
  },
  caster: {
    // DB evidence: small caster stat counts are int/stamina based; high-end
    // 5-stat rows add spell power plus throughput ratings.
    2: [[5, 55], [7, 45]],
    3: [[5, 42], [7, 28], [36, 30]],
    4: [[5, 34], [7, 24], [32, 21], [36, 21]],
    5: [[45, 25], [5, 25], [7, 14], [32, 18], [36, 18]],
  },
  tank: {
    // DB evidence: tank armor is stamina + defense, then strength and avoidance.
    2: [[7, 60], [12, 40]],
    3: [[7, 42], [12, 30], [4, 28]],
    4: [[7, 36], [4, 26], [12, 20], [13, 14]],
    5: [[7, 32], [4, 29], [12, 15], [13, 12], [14, 12]],
  },
};

function selectedForwardPresetStatCount() {
  const el = byId("fPresetStatCount");
  const count = Number(el ? el.value : 5);
  return [2, 3, 4, 5].includes(count) ? count : 5;
}

function loadForwardPreset(kind) {
  activeForwardReferenceProfile = null;
  byId("forwardStatsBody").innerHTML = "";
  const rows = FORWARD_PRESET_ROWS[kind]?.[selectedForwardPresetStatCount()] || FORWARD_PRESET_ROWS.melee[5];
  for (const [statId, share] of rows) {
    addForwardStat(statId, share);
  }
}

const BATCH_BLUEPRINTS = {
  melee: {
    label: "Melee DPS",
    variants: [
      { label: "power", pool: [4, 7, 31, 37, 38, 44], anchors: [4], weights: { 4: 135, 7: 70, 31: 75, 37: 95, 38: 105, 44: 80 } },
      { label: "agility", pool: [3, 4, 7, 32, 38, 44], anchors: [3], weights: { 3: 135, 4: 80, 7: 70, 32: 90, 38: 85, 44: 75 } },
      { label: "haste", pool: [3, 4, 7, 36, 37, 38], anchors: [4], weights: { 4: 125, 3: 95, 7: 65, 36: 105, 37: 80, 38: 95 } },
      { label: "armor penetration", pool: [3, 4, 7, 31, 38, 44], anchors: [4], weights: { 4: 120, 3: 90, 7: 65, 31: 80, 38: 85, 44: 120 } },
    ],
  },
  hunter: {
    label: "Hunter / ranged",
    variants: [
      { label: "marksman", pool: [3, 7, 31, 39, 44], anchors: [3], weights: { 3: 140, 7: 65, 31: 90, 39: 125, 44: 80 } },
      { label: "predator", pool: [3, 7, 32, 38, 39, 44], anchors: [3], weights: { 3: 135, 7: 65, 32: 95, 38: 55, 39: 115, 44: 80 } },
      { label: "quickdraw", pool: [3, 5, 7, 36, 39], anchors: [3], weights: { 3: 140, 5: 55, 7: 65, 36: 105, 39: 120 } },
      { label: "survival", pool: [3, 5, 7, 31, 32, 39], anchors: [3, 5], weights: { 3: 130, 5: 85, 7: 70, 31: 75, 32: 80, 39: 105 } },
    ],
  },
  caster: {
    label: "Caster DPS",
    variants: [
      { label: "spell power", pool: [5, 6, 7, 42, 45, 47], anchors: [45], weights: { 45: 140, 5: 105, 6: 45, 7: 55, 42: 115, 47: 55 } },
      { label: "precision", pool: [5, 7, 18, 21, 42, 45], anchors: [5, 45], weights: { 5: 110, 7: 55, 18: 115, 21: 95, 42: 100, 45: 130 } },
      { label: "haste", pool: [5, 6, 7, 30, 36, 45], anchors: [45], weights: { 45: 135, 5: 105, 6: 45, 7: 55, 30: 105, 36: 85 } },
      { label: "penetration", pool: [5, 7, 18, 42, 45, 47], anchors: [45], weights: { 45: 135, 5: 100, 7: 55, 18: 85, 42: 105, 47: 95 } },
    ],
  },
  healer: {
    label: "Healer",
    variants: [
      { label: "spirit", pool: [5, 6, 7, 41, 43, 45], anchors: [5, 6], weights: { 5: 110, 6: 125, 7: 55, 41: 115, 43: 95, 45: 105 } },
      { label: "throughput", pool: [5, 7, 21, 32, 41, 45], anchors: [5], weights: { 5: 115, 7: 55, 21: 70, 32: 75, 41: 130, 45: 115 } },
      { label: "haste regen", pool: [5, 6, 7, 36, 43, 45], anchors: [5], weights: { 5: 110, 6: 100, 7: 55, 36: 100, 43: 105, 45: 105 } },
    ],
  },
  tank: {
    label: "Tank",
    variants: [
      { label: "defense", pool: [4, 7, 12, 13, 14, 37], anchors: [7, 12], weights: { 7: 135, 12: 130, 4: 80, 13: 85, 14: 75, 37: 55 } },
      { label: "block", pool: [4, 7, 12, 15, 37, 48], anchors: [7, 12, 48], weights: { 7: 130, 12: 115, 48: 120, 15: 110, 4: 75, 37: 50 } },
      { label: "avoidance", pool: [4, 7, 12, 13, 14, 37], anchors: [7, 13], weights: { 7: 130, 13: 125, 14: 105, 12: 95, 4: 75, 37: 55 } },
      { label: "threat", pool: [4, 7, 12, 31, 37, 38], anchors: [7, 4], weights: { 7: 120, 4: 110, 37: 95, 31: 80, 38: 75, 12: 85 } },
    ],
  },
  tankDefense: {
    label: "Tank - defense",
    pool: [4, 7, 12, 13, 14, 37],
    anchors: [7, 12],
    weights: { 7: 135, 12: 135, 4: 80, 13: 85, 14: 75, 37: 55 },
  },
  tankBlock: {
    label: "Tank - block",
    pool: [4, 7, 12, 15, 37, 48],
    anchors: [7, 12, 48],
    weights: { 7: 130, 12: 115, 48: 130, 15: 115, 4: 75, 37: 50 },
  },
  tankAvoidance: {
    label: "Tank - dodge/parry",
    pool: [4, 7, 12, 13, 14, 37],
    anchors: [7, 13],
    weights: { 7: 130, 13: 130, 14: 115, 12: 95, 4: 75, 37: 55 },
  },
  tankThreat: {
    label: "Tank - threat",
    pool: [4, 7, 12, 31, 37, 38],
    anchors: [7, 4],
    weights: { 7: 120, 4: 115, 37: 100, 31: 85, 38: 80, 12: 85 },
  },
  pvp: {
    label: "PvP variant",
    variants: [
      { label: "physical", pool: [3, 4, 7, 31, 32, 35, 38], anchors: [7, 35], weights: { 7: 120, 35: 140, 3: 90, 4: 90, 31: 70, 32: 80, 38: 65 } },
      { label: "caster", pool: [5, 7, 18, 32, 35, 36, 45], anchors: [7, 35], weights: { 7: 115, 35: 140, 5: 95, 18: 75, 32: 75, 36: 65, 45: 110 } },
    ],
  },
};

const BATCH_WEAPON_DB_SUBCLASS = {
  axe1h: 0,
  axe2h: 1,
  bow: 2,
  gun: 3,
  mace1h: 4,
  mace2h: 5,
  polearm: 6,
  sword1h: 7,
  sword2h: 8,
  staff: 10,
  caster_staff: 10,
  fist: 13,
  dagger: 15,
  thrown: 16,
  crossbow: 18,
  wand: 19,
  caster_mh: 4,
  tank_1h: 7,
};

const BATCH_FALLBACK_ADJECTIVES = [
  "Ancient", "Arcane", "Astral", "Blessed", "Bloodforged", "Brutal", "Burnished", "Chilled", "Crimson", "Dire",
  "Dread", "Duskwoven", "Ebon", "Elder", "Furious", "Gilded", "Graven", "Hallowed", "Hardened", "Honored",
  "Icy", "Imperial", "Luminous", "Mystic", "Radiant", "Reinforced", "Resolute", "Runed", "Savage", "Searing",
  "Shadowed", "Shifting", "Silent", "Stalwart", "Stormforged", "Tempered", "Vengeful", "Vicious", "Vigilant", "Wild"
];
const BATCH_FALLBACK_MATERIALS = [
  "Iron", "Steel", "Mithril", "Thorium", "Dark Iron", "Fel Iron", "Adamantite", "Khorium", "Cobalt", "Saronite",
  "Titansteel", "Titanium", "Frostwoven", "Netherweave", "Mooncloth", "Dragonscale", "Wyrmhide", "Nerubian", "Obsidian"
];
const BATCH_MATERIAL_TIERS = [
  { maxIlvl: 64, values: ["Iron", "Steel", "Mithril", "Thorium", "Dark Iron", "Mooncloth", "Dragonscale"] },
  { maxIlvl: 164, values: ["Fel Iron", "Adamantite", "Khorium", "Netherweave", "Nethersteel", "Primal"] },
  { maxIlvl: 350, values: ["Cobalt", "Saronite", "Titansteel", "Titanium", "Frostwoven", "Nerubian", "Obsidian"] },
];
const BATCH_FALLBACK_SUFFIXES = [
  "of the Bear", "of the Boar", "of the Champion", "of the Crusade", "of the Depths", "of the Eclipse",
  "of the Falcon", "of the Glacier", "of the Grove", "of the Guardian", "of the Invoker", "of the Moon",
  "of the North", "of the Seer", "of the Sentinel", "of the Soldier", "of the Sorcerer", "of the Tiger",
  "of the Whale", "of the Wild", "of Deflection", "of Dominion", "of Fortitude", "of Insight", "of Resolve",
  "of Ruin", "of Spellfire", "of Triumph", "of Vengeance", "of Victory", "of Warding"
];
const BATCH_WEAPON_NOUNS = {
  axe1h: ["Axe", "Cleaver", "Hatchet", "Chopper", "Waraxe", "Ravager"],
  axe2h: ["Greataxe", "Battleaxe", "Reaver", "Headsplitter", "Warcleaver", "Executioner"],
  bow: ["Bow", "Longbow", "Warbow", "Greatbow", "Striker", "Huntsman's Bow"],
  gun: ["Rifle", "Blunderbuss", "Handcannon", "Boomstick", "Carbine", "Longrifle"],
  mace1h: ["Mace", "Gavel", "Hammer", "Crusher", "Cudgel", "Morningstar"],
  mace2h: ["Maul", "Warhammer", "Greatmace", "Earthshaker", "Stonebreaker", "Pummeler"],
  polearm: ["Polearm", "Halberd", "Spear", "Lance", "Pike", "Warspear"],
  sword1h: ["Sword", "Blade", "Saber", "Broadsword", "Longsword", "Edge"],
  sword2h: ["Greatsword", "Runeblade", "Warblade", "Claymore", "Zweihander", "Avenger"],
  staff: ["Staff", "Stave", "Spire", "Branch", "Greatstaff", "Pilgrim's Staff"],
  caster_staff: ["Staff", "Conduit", "Spire", "Greatstaff", "Sage's Staff", "Spellstaff"],
  fist: ["Fist", "Claw", "Katar", "Knuckles", "Talons", "Handblade"],
  dagger: ["Dagger", "Dirk", "Fang", "Shiv", "Stiletto", "Sticker"],
  thrown: ["Darts", "Glaive", "Knives", "Throwing Axes", "Shuriken", "Razors"],
  crossbow: ["Crossbow", "Arbalest", "Repeater", "Bolt-Thrower", "Siegebow", "Launcher"],
  wand: ["Wand", "Rod", "Scepter", "Spellwand", "Focus", "Torch"],
  caster_mh: ["Scepter", "Blade", "Mace", "Spellblade", "Invoker", "Focus"],
  tank_1h: ["Sword", "Mace", "Defender", "Bulwark", "Wardblade", "Guard"],
};
const BATCH_ARMOR_NOUNS = {
  1: ["Helm", "Cowl", "Headguard", "Helmet", "Faceguard", "Crown"],
  2: ["Amulet", "Pendant", "Choker", "Talisman", "Medallion", "Necklace"],
  3: ["Shoulders", "Mantle", "Pauldrons", "Spaulders", "Shoulderguards", "Epaulets"],
  5: ["Chestguard", "Robe", "Breastplate", "Cuirass", "Chestpiece", "Harness"],
  6: ["Belt", "Girdle", "Sash", "Waistguard", "Cord", "Greatbelt"],
  7: ["Legguards", "Leggings", "Legplates", "Pants", "Greaves", "Britches"],
  8: ["Boots", "Treads", "Sabatons", "Footguards", "Sandals", "Walkers"],
  9: ["Bracers", "Bindings", "Vambraces", "Wristguards", "Cuffs", "Armplates"],
  10: ["Gloves", "Grips", "Gauntlets", "Handguards", "Mitts", "Handwraps"],
  11: ["Ring", "Band", "Signet", "Loop", "Seal", "Circle"],
  12: ["Charm", "Icon", "Figurine", "Badge", "Stone", "Token"],
  14: ["Shield", "Bulwark", "Aegis", "Barrier", "Deflector", "Wall"],
  16: ["Cloak", "Cape", "Shroud", "Drape", "Wrap", "Greatcloak"],
  20: ["Robe", "Vestments", "Raiment", "Regalia", "Gown", "Tunic"],
  23: ["Tome", "Orb", "Grimoire", "Codex", "Lantern", "Idol"],
  28: ["Relic", "Idol", "Totem", "Libram", "Sigil", "Charm"],
};
const BATCH_ARMOR_TYPE_NOUNS = {
  cloth: {
    1: ["Hood", "Cowl", "Circlet"],
    3: ["Mantle", "Shoulderpads", "Amice"],
    5: ["Robe", "Vestments", "Raiment", "Tunic"],
    6: ["Cord", "Sash", "Wrap"],
    7: ["Leggings", "Pants", "Trousers"],
    8: ["Sandals", "Slippers", "Treads"],
    9: ["Cuffs", "Bindings", "Wristwraps"],
    10: ["Gloves", "Handwraps", "Mitts"],
  },
  leather: {
    1: ["Mask", "Headguard", "Cap"],
    3: ["Shoulderpads", "Spaulders", "Mantle"],
    5: ["Jerkin", "Tunic", "Harness", "Vest"],
    6: ["Belt", "Waistguard", "Cord"],
    7: ["Leggings", "Pants", "Legguards"],
    8: ["Boots", "Treads", "Footpads"],
    9: ["Bracers", "Bindings", "Wristguards"],
    10: ["Gloves", "Grips", "Handguards"],
  },
  mail: {
    1: ["Coif", "Headguard", "Helmet"],
    3: ["Pauldrons", "Spaulders", "Shoulderguards"],
    5: ["Hauberk", "Chainmail", "Chestguard", "Scalemail"],
    6: ["Girdle", "Belt", "Waistguard"],
    7: ["Legguards", "Leggings", "Greaves"],
    8: ["Boots", "Treads", "Sabatons"],
    9: ["Bracers", "Vambraces", "Wristguards"],
    10: ["Gauntlets", "Gloves", "Grips"],
  },
  plate: {
    1: ["Helm", "Helmet", "Faceguard"],
    3: ["Pauldrons", "Shoulderguards", "Spaulders"],
    5: ["Breastplate", "Chestplate", "Cuirass", "Battleplate"],
    6: ["Girdle", "Greatbelt", "Waistguard"],
    7: ["Legplates", "Legguards", "Greaves"],
    8: ["Sabatons", "Warboots", "Treads"],
    9: ["Vambraces", "Armplates", "Bracers"],
    10: ["Gauntlets", "Handguards", "Grips"],
  },
};
const BATCH_NAME_THEMES = {
  tank: {
    adjectives: ["Bastion", "Bulwark", "Enduring", "Fortified", "Guardian", "Ironbound", "Resolute", "Stalwart", "Unbroken", "Vigilant"],
    suffixes: ["of Deflection", "of Fortitude", "of Guarding", "of Resolve", "of the Bulwark", "of the Guardian", "of the Sentinel", "of Warding"],
  },
  tankBlock: {
    adjectives: ["Barricade", "Blockade", "Bulwark", "Fortified", "Shieldwall", "Stoneskin", "Towering", "Unyielding"],
    suffixes: ["of Deflection", "of Shielding", "of the Bulwark", "of the Wall", "of Warding"],
  },
  tankAvoidance: {
    adjectives: ["Elusive", "Nimble", "Parrying", "Quickened", "Riposte", "Sidestepping", "Watchful"],
    suffixes: ["of Avoidance", "of Deflection", "of the Watcher", "of Warding"],
  },
  tankThreat: {
    adjectives: ["Commanding", "Dauntless", "Defiant", "Rallying", "Vengeful", "Warbringer"],
    suffixes: ["of Command", "of Dominance", "of Retaliation", "of Vengeance", "of Victory"],
  },
  melee: {
    adjectives: ["Bloodied", "Brutal", "Cruel", "Furious", "Razor", "Savage", "Vicious", "Warborn"],
    suffixes: ["of Carnage", "of Ruin", "of Slaying", "of the Tiger", "of Vengeance", "of Victory"],
  },
  hunter: {
    adjectives: ["Falcon", "Hawk", "Keen", "Marksman's", "Predator's", "Quickdraw", "Tracker's", "Wildstalker"],
    suffixes: ["of Accuracy", "of Marksmanship", "of the Falcon", "of the Hunt", "of the Tracker", "of the Wild"],
  },
  caster: {
    adjectives: ["Arcane", "Astral", "Eldritch", "Invoker's", "Luminous", "Runed", "Spellbound", "Starfire"],
    suffixes: ["of Insight", "of Invocation", "of Spellfire", "of the Eclipse", "of the Invoker", "of the Sorcerer"],
  },
  healer: {
    adjectives: ["Anointed", "Blessed", "Hallowed", "Merciful", "Radiant", "Renewing", "Serene", "Soothing"],
    suffixes: ["of Grace", "of Mercy", "of Renewal", "of the Grove", "of the Moon", "of the Seer"],
  },
  pvp: {
    adjectives: ["Battleborn", "Bloodied", "Defiant", "Furious", "Gladiator's", "Relentless", "Vengeful"],
    suffixes: ["of Dominance", "of Triumph", "of Vengeance", "of Victory", "of the Champion", "of the Soldier"],
  },
};
const BATCH_QUALITY_NAME_PARTS = {
  2: { adjectives: ["Burnished", "Reliable", "Stout", "Sturdy"], suffixes: ["of the Bear", "of the Boar", "of the Falcon", "of the Whale"] },
  3: { adjectives: ["Ancient", "Hardened", "Honored", "Runed", "Tempered"], suffixes: ["of Resolve", "of Triumph", "of the Champion", "of the Sentinel"] },
  4: { adjectives: ["Astral", "Bloodforged", "Dread", "Ebon", "Radiant", "Stormforged"], suffixes: ["of Dominion", "of Ruin", "of the Eclipse", "of the North"] },
  5: { adjectives: ["Eternal", "Fabled", "Mythic", "Prime", "Sovereign"], suffixes: ["of Ages", "of Legends", "of the Pantheon", "of the Titans"] },
};

function randomBetween(min, max) {
  return Number(min) + Math.random() * (Number(max) - Number(min));
}

function randomInt(min, max) {
  return Math.floor(randomBetween(min, max + 1));
}

function chooseRandom(values) {
  return values && values.length ? values[Math.floor(Math.random() * values.length)] : null;
}

function chooseWeighted(entries) {
  const clean = (entries || []).filter(e => Number(e.weight) > 0);
  const total = clean.reduce((sum, e) => sum + Number(e.weight), 0);
  if (!clean.length || total <= 0) return null;
  let roll = Math.random() * total;
  for (const entry of clean) {
    roll -= Number(entry.weight);
    if (roll <= 0) return entry.value;
  }
  return clean[clean.length - 1].value;
}

function shuffleArray(values) {
  const out = [...values];
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [out[i], out[j]] = [out[j], out[i]];
  }
  return out;
}

function currentForwardShell() {
  const slot = numberValue("fSlot");
  const family = byId("fFamily").value;
  const armorType = byId("fArmorType").value;
  const rawWeaponSubclass = byId("fWeaponSubclass").value;
  const weaponSubclass = family === "weapon" ? (usableWeaponSubclass(rawWeaponSubclass) ? rawWeaponSubclass : defaultWeaponSubclassForSlot(slot)) : "none";
  return {
    slot,
    family,
    armorType,
    weaponSubclass,
    speed: family === "weapon" ? (numberValue("fSpeed") || defaultWeaponSpeed(weaponSubclass, slot)) : 0,
    bonusArmor: numberValue("fBonusArmor"),
    budgetUsage: Math.max(1, Math.min(125, numberValue("fBudgetUsage") || 100)) / 100,
    sockets: socketCounts("f"),
    gemModel: byId("fGemModel").value,
  };
}

function dbClassForShell(shell) {
  return shell.family === "weapon" ? 2 : 4;
}

function dbSubclassForShell(shell) {
  if (shell.family === "weapon") return BATCH_WEAPON_DB_SUBCLASS[shell.weaponSubclass] ?? 7;
  if (Number(shell.slot) === 14 || shell.armorType === "shield") return 6;
  switch (shell.armorType) {
    case "cloth": return 1;
    case "leather": return 2;
    case "mail": return 3;
    case "plate": return 4;
    default: return 0;
  }
}

function damageTypeForShell(shell) {
  return shell.weaponSubclass === "wand" ? 3 : 0;
}

function batchQualityPool() {
  const mode = byId("bQualityMode").value;
  if (mode === "current") return [numberValue("fQuality")];
  return String(mode).split(",").map(Number).filter(q => q >= 2 && q <= 5);
}

function inferBatchBlueprintForShell(shell) {
  if (shell.family === "weapon") {
    if (["caster_mh", "caster_staff", "wand"].includes(shell.weaponSubclass)) return "caster";
    if (["bow", "gun", "crossbow", "thrown"].includes(shell.weaponSubclass)) return "hunter";
    if (shell.weaponSubclass === "tank_1h") return "tank";
    return "melee";
  }
  if (Number(shell.slot) === 14 || shell.armorType === "shield") return "tank";
  if (shell.armorType === "cloth" || Number(shell.slot) === 23) return "caster";
  return "melee";
}

function compatibleBatchBlueprints(shell) {
  if (shell.family === "weapon") {
    if (["caster_mh", "caster_staff", "wand"].includes(shell.weaponSubclass)) return ["caster", "healer"];
    if (["bow", "gun", "crossbow", "thrown"].includes(shell.weaponSubclass)) return ["hunter", "melee"];
    if (shell.weaponSubclass === "tank_1h") return ["tankDefense", "tankBlock", "tankAvoidance", "tankThreat", "melee"];
    return ["melee", "tankThreat", "tankDefense"];
  }
  if (Number(shell.slot) === 14 || shell.armorType === "shield") return ["tankDefense", "tankBlock", "tankAvoidance", "tankThreat", "caster", "healer"];
  if (Number(shell.slot) === 23) return ["caster", "healer"];
  if (["cloth"].includes(shell.armorType)) return ["caster", "healer"];
  if (["leather", "mail"].includes(shell.armorType)) return ["melee", "hunter", "caster", "healer", "tankDefense", "tankBlock", "tankAvoidance", "tankThreat"];
  if (shell.armorType === "plate") return ["melee", "tankDefense", "tankBlock", "tankAvoidance", "tankThreat", "healer"];
  return ["melee", "hunter", "caster", "healer", "tankDefense", "tankBlock", "tankAvoidance", "tankThreat"];
}

function resolveBatchBlueprint(shell) {
  const requested = byId("bBlueprint").value;
  if (requested === "current") return "current";
  if (requested === "auto") return inferBatchBlueprintForShell(shell);
  if (requested === "all") return chooseRandom(compatibleBatchBlueprints(shell)) || inferBatchBlueprintForShell(shell);
  return BATCH_BLUEPRINTS[requested] ? requested : inferBatchBlueprintForShell(shell);
}

function filteredBatchBlueprint(blueprintKey, ilvl, shell) {
  const base = BATCH_BLUEPRINTS[blueprintKey] || BATCH_BLUEPRINTS.melee;
  const variant = base.variants ? chooseRandom(base.variants) : base;
  const mergedPool = base.variants
    ? Array.from(new Set(base.variants.flatMap(v => v.pool || [])))
    : (base.pool || []);
  const blueprint = {
    key: blueprintKey,
    label: variant.label && variant.label !== base.label ? `${base.label} - ${variant.label}` : base.label,
    pool: [...(variant.pool || base.pool || mergedPool)],
    anchors: [...(variant.anchors || base.anchors || [])],
    weights: { ...(base.weights || {}), ...(variant.weights || {}) },
  };
  for (const statId of mergedPool) {
    if (blueprint.pool.length >= 6) break;
    if (!blueprint.pool.includes(statId)) blueprint.pool.push(statId);
  }
  let pool = [...blueprint.pool];
  let anchors = [...blueprint.anchors];
  if (Number(ilvl) < 60) {
    const forbidden = new Set([28, 29, 30, 35, 36, 44]);
    pool = pool.filter(id => !forbidden.has(id));
    anchors = anchors.filter(id => !forbidden.has(id));
  }
  if (shell.family === "weapon" && ([17, 25, 26].includes(Number(shell.slot)) || ["axe2h", "mace2h", "polearm", "sword2h", "staff", "caster_staff"].includes(shell.weaponSubclass))) {
    pool = pool.filter(id => ![15, 48].includes(id));
    anchors = anchors.filter(id => ![15, 48].includes(id));
  }
  return { ...blueprint, pool, anchors };
}

function activeStatCountForReferenceItem(item) {
  return Math.max(0, Math.min(6, (item.stats || []).filter(s => Number(s.value) > 0).length));
}

function referenceStatCountForShell(shell, ilvl, quality, blueprintKey) {
  if (!referenceItems.length) return null;
  const target = {
    quality: Number(quality),
    ilvl: Number(ilvl),
    slot: Number(shell.slot),
    family: shell.family,
    weaponSubclass: shell.weaponSubclass,
    armorType: shell.armorType,
    archetype: blueprintKey === "current" ? "any" : blueprintKey,
  };
  const candidates = nearbyReferenceItems(target, item =>
    item.quality === target.quality &&
    item.family === target.family &&
    activeStatCountForReferenceItem(item) > 0 &&
    (target.family !== "weapon" || sameWeaponSlotGroup(item.invType, target.slot)) &&
    (target.family === "weapon" || target.armorType === "none" || item.armorType === target.armorType || isPassiveNonArmorSlot(target.slot)),
  30);
  const weightedCounts = candidates
    .map(c => ({ value: activeStatCountForReferenceItem(c.item), weight: referenceWeight(c.score) }))
    .filter(c => c.value > 0);
  return chooseWeighted(weightedCounts);
}

function progressiveBatchStatCount(ilvl, quality) {
  const L = Number(ilvl);
  if (L < 30) return chooseWeighted([{ value: 1, weight: 40 }, { value: 2, weight: 60 }]);
  if (L < 45) return chooseWeighted([{ value: 2, weight: 65 }, { value: 3, weight: 35 }]);
  if (L < 60) return chooseWeighted([{ value: 2, weight: 40 }, { value: 3, weight: 60 }]);
  if (L < 100) return chooseWeighted([{ value: 2, weight: 15 }, { value: 3, weight: 65 }, { value: 4, weight: 20 }]);
  if (L < 200) return chooseWeighted([{ value: 3, weight: 35 }, { value: 4, weight: 55 }, { value: 5, weight: 10 }]);
  return Number(quality) >= 4
    ? chooseWeighted([{ value: 4, weight: 35 }, { value: 5, weight: 50 }, { value: 6, weight: 15 }])
    : chooseWeighted([{ value: 3, weight: 35 }, { value: 4, weight: 50 }, { value: 5, weight: 15 }]);
}

function resolveBatchStatCount(shell, ilvl, quality, blueprintKey, currentRows) {
  const mode = byId("bDensityMode").value;
  if (mode === "manual") return Math.max(1, Math.min(6, numberValue("bManualStatCount") || 1));
  if (mode === "progressive") return progressiveBatchStatCount(ilvl, quality);
  if (mode === "reference") return referenceStatCountForShell(shell, ilvl, quality, blueprintKey) || progressiveBatchStatCount(ilvl, quality);
  return Math.max(1, Math.min(6, currentRows.filter(r => Number(r.share) > 0).length || selectedForwardPresetStatCount()));
}

function applyBatchDistribution(rows, distributionMode, skewPercent, weights = {}) {
  const clean = rows.filter(r => STATS[Number(r.statId)]).slice(0, 6);
  if (!clean.length) return [{ statId: 4, share: 100 }];
  let rawShares;
  if (distributionMode === "even") {
    rawShares = clean.map(() => 1);
  } else {
    rawShares = clean.map(row => Math.max(1, Number(weights[row.statId] || row.share || 50)));
    if (distributionMode === "varied") {
      const skew = Math.max(0, Math.min(100, Number(skewPercent || 0))) / 100;
      rawShares = rawShares.map(share => Math.max(0.01, share * randomBetween(1 - skew, 1 + skew)));
    }
  }
  const total = rawShares.reduce((sum, share) => sum + share, 0) || 1;
  return clean.map((row, index) => ({ statId: Number(row.statId), share: rawShares[index] / total * 100 }));
}

function fillBatchRowsFromBlueprint(rows, blueprint, targetCount) {
  const chosenIds = new Set(rows.map(r => Number(r.statId)));
  const target = Math.max(1, Math.min(6, Number(targetCount || 1)));
  for (const statId of blueprint.anchors || []) {
    if (rows.length >= target) break;
    if (!chosenIds.has(statId) && blueprint.pool.includes(statId)) {
      rows.push({ statId });
      chosenIds.add(statId);
    }
  }
  while (rows.length < target) {
    const remaining = (blueprint.pool || []).filter(id => !chosenIds.has(id));
    if (!remaining.length) break;
    const next = chooseWeighted(remaining.map(id => ({ value: id, weight: blueprint.weights[id] || 50 })));
    if (!next) break;
    rows.push({ statId: next });
    chosenIds.add(next);
  }
  return rows;
}

function batchRowsFromCurrent(currentRows, statCount, distributionMode, skewPercent, ilvl, shell) {
  const rows = [...currentRows]
    .filter(r => Number(r.share) > 0 && STATS[Number(r.statId)])
    .sort((a, b) => Number(b.share) - Number(a.share))
    .slice(0, Math.max(1, Math.min(6, statCount)));
  if (rows.length < Math.min(6, statCount)) {
    const fillerKey = inferBatchBlueprintForShell(shell);
    const fillerBlueprint = filteredBatchBlueprint(fillerKey, ilvl, shell);
    fillBatchRowsFromBlueprint(rows, fillerBlueprint, statCount);
  }
  const currentWeights = Object.fromEntries(currentRows.map(r => [Number(r.statId), Number(r.share)]));
  return applyBatchDistribution(rows, distributionMode, skewPercent, currentWeights);
}

function batchRowsFromBlueprint(blueprint, statCount, distributionMode, skewPercent) {
  const targetCount = Math.max(1, Math.min(6, statCount, blueprint.pool.length || 1));
  const chosen = [];
  for (const statId of blueprint.anchors) {
    if (blueprint.pool.includes(statId) && !chosen.includes(statId)) chosen.push(statId);
    if (chosen.length >= targetCount) break;
  }
  while (chosen.length < targetCount) {
    const remaining = blueprint.pool.filter(id => !chosen.includes(id));
    if (!remaining.length) break;
    const next = chooseWeighted(remaining.map(id => ({ value: id, weight: blueprint.weights[id] || 50 })));
    if (!next) break;
    chosen.push(next);
  }
  return applyBatchDistribution(chosen.map(statId => ({ statId })), distributionMode, skewPercent, blueprint.weights);
}

function buildBatchStatRows(shell, ilvl, quality, currentRows) {
  const blueprintKey = resolveBatchBlueprint(shell);
  const statCount = resolveBatchStatCount(shell, ilvl, quality, blueprintKey, currentRows);
  const distributionMode = byId("bDistributionMode").value;
  const skewPercent = numberValue("bSkew");
  if (blueprintKey === "current") {
    return {
      blueprintKey,
      blueprintLabel: "Forward stat rows",
      rows: batchRowsFromCurrent(currentRows, statCount, distributionMode, skewPercent, ilvl, shell),
      statCount,
    };
  }
  const blueprint = filteredBatchBlueprint(blueprintKey, ilvl, shell);
  return {
    blueprintKey,
    blueprintLabel: blueprint.label || BATCH_BLUEPRINTS[blueprintKey]?.label || blueprintKey,
    rows: batchRowsFromBlueprint(blueprint, statCount, distributionMode, skewPercent),
    statCount,
  };
}

function referenceMetadataCandidates(shell, ilvl, quality) {
  const classId = dbClassForShell(shell);
  const subclass = dbSubclassForShell(shell);
  return referenceItems
    .filter(item => item.classId === classId && item.subclass === subclass)
    .map(item => {
      let score = Math.abs(item.ilvl - ilvl);
      if (item.quality !== quality) score += 18;
      if (item.invType !== shell.slot) score += isJewelrySlot(item.invType) && isJewelrySlot(shell.slot) ? 8 : 18;
      if (shell.family !== "weapon" && shell.armorType !== "none" && item.armorType !== shell.armorType) score += 12;
      return { item, score };
    })
    .sort((a, b) => a.score - b.score);
}

function interpolateReferenceField(candidates, ilvl, field, fallback = 0) {
  const nodes = candidates
    .map(c => c.item)
    .filter(item => Number(item[field]) > 0)
    .sort((a, b) => a.ilvl - b.ilvl);
  if (!nodes.length) return fallback;
  const exact = nodes.find(item => item.ilvl === ilvl);
  if (exact) return Number(exact[field]);
  const low = [...nodes].reverse().find(item => item.ilvl <= ilvl);
  const high = nodes.find(item => item.ilvl >= ilvl);
  if (low && high && low !== high) {
    const t = (ilvl - low.ilvl) / Math.max(1, high.ilvl - low.ilvl);
    return Number(low[field]) + t * (Number(high[field]) - Number(low[field]));
  }
  return Number((low || high || nodes[0])[field]);
}

function estimateRequiredLevel(shell, ilvl, quality) {
  const candidates = referenceMetadataCandidates(shell, ilvl, quality).filter(c => Number(c.item.reqLevel) > 1);
  const weighted = candidates.slice(0, 30).map(c => ({ value: c.item.reqLevel, weight: 1 / (1 + Math.pow(c.score / 8, 2)) }));
  let req = Math.round(weightedAverage(weighted, Math.max(1, Math.floor(Number(ilvl) * 0.75))));
  if (Number(ilvl) <= 90) req = Math.min(req, 60);
  else if (Number(ilvl) <= 154) req = Math.min(req, 70);
  return Math.max(1, Math.min(80, req));
}

function estimateSellPrice(shell, ilvl, quality) {
  const candidates = referenceMetadataCandidates(shell, ilvl, quality);
  const exact = candidates.filter(c => c.item.invType === shell.slot && c.item.quality === quality);
  const value = interpolateReferenceField(exact.length ? exact : candidates, ilvl, "sellPrice", 0);
  if (value > 0) return Math.max(1, Math.round(value));
  const slotFactor = Math.max(0.2, slotMod(shell.slot, quality, ilvl, shell.family));
  return Math.max(500, Math.round(Math.pow(Math.max(1, ilvl), 1.45) * Math.max(1, quality - 1) * slotFactor * 12));
}

function inferSheath(shell) {
  if (Number(shell.slot) === 14) return 4;
  if (shell.family !== "weapon") return 0;
  if (["staff", "caster_staff"].includes(shell.weaponSubclass)) return 2;
  if (Number(shell.slot) === 17 || ["axe2h", "mace2h", "polearm", "sword2h"].includes(shell.weaponSubclass)) return 1;
  if (Number(shell.slot) === 23) return 7;
  return 3;
}

function chooseVisualMetadata(shell, ilvl, quality) {
  let candidates = referenceMetadataCandidates(shell, ilvl, quality);
  if (!candidates.length && referenceItems.length) {
    candidates = referenceItems
      .filter(item => item.invType === shell.slot)
      .map(item => {
        let score = Math.abs(item.ilvl - ilvl) + Math.abs(item.quality - quality) * 18;
        if (item.family !== shell.family) score += 20;
        if (shell.family !== "weapon" && shell.armorType !== "none" && item.armorType !== shell.armorType) score += 12;
        return { item, score };
      })
      .sort((a, b) => a.score - b.score);
  }
  const displayPool = candidates.filter(c =>
    Number(c.item.displayId) > 0 &&
    c.item.invType === shell.slot &&
    Math.abs(c.item.ilvl - ilvl) <= Math.max(12, ilvl * 0.25) &&
    Math.abs(c.item.quality - quality) <= 1
  );
  const displayChoice = chooseWeighted((displayPool.length ? displayPool : candidates.filter(c => Number(c.item.displayId) > 0)).slice(0, 30)
    .map(c => ({ value: c, weight: 1 / (1 + Math.pow(c.score / 10, 2)) })));
  const materialChoice = chooseWeighted(candidates.filter(c => c.item.material !== null && c.item.material !== undefined)
    .slice(0, 30)
    .map(c => ({ value: c.item.material, weight: 1 / (1 + Math.pow(c.score / 10, 2)) })));
  const sheathChoice = chooseWeighted(candidates.filter(c => c.item.sheath !== null && c.item.sheath !== undefined)
    .slice(0, 30)
    .map(c => ({ value: c.item.sheath, weight: 1 / (1 + Math.pow(c.score / 10, 2)) })));
  return {
    displayId: displayChoice?.item?.displayId || 0,
    displaySource: displayChoice ? referenceMini(displayChoice.item, displayChoice.score) : null,
    material: materialChoice ?? 1,
    sheath: sheathChoice ?? inferSheath(shell),
  };
}

function cleanNamePart(value) {
  return String(value || "")
    .replace(/["]/g, "")
    .replace(/[_]+/g, " ")
    .replace(/[^A-Za-z0-9' -]/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function uniqueNameParts(...groups) {
  const seen = new Set();
  const out = [];
  for (const group of groups) {
    for (const value of group || []) {
      const clean = cleanNamePart(value);
      const key = clean.toLowerCase();
      if (!clean || seen.has(key)) continue;
      seen.add(key);
      out.push(clean);
    }
  }
  return out;
}

function chooseNamePart(groups) {
  const seen = new Set();
  const entries = [];
  for (const group of groups || []) {
    const weight = Number(group.weight ?? 1);
    if (weight <= 0) continue;
    for (const value of group.values || []) {
      const clean = cleanNamePart(value);
      const key = clean.toLowerCase();
      if (!clean || seen.has(key)) continue;
      seen.add(key);
      entries.push({ value: clean, weight });
    }
  }
  return chooseWeighted(entries);
}

function shellNounPool(shell) {
  if (shell.family === "weapon") {
    const weaponPool = uniqueNameParts(BATCH_WEAPON_NOUNS[shell.weaponSubclass]);
    return weaponPool.length ? weaponPool : ["Weapon"];
  }
  const typed = BATCH_ARMOR_TYPE_NOUNS[shell.armorType]?.[Number(shell.slot)] || [];
  if (typed.length) return uniqueNameParts(typed);
  const armorPool = uniqueNameParts(typed, BATCH_ARMOR_NOUNS[Number(shell.slot)]);
  return armorPool.length ? armorPool : ["Item"];
}

function fallbackNounForShell(shell) {
  return chooseRandom(shellNounPool(shell)) || "Item";
}

function materialPoolForShell(shell, ilvl) {
  const tier = BATCH_MATERIAL_TIERS.find(t => Number(ilvl) <= t.maxIlvl) || BATCH_MATERIAL_TIERS[BATCH_MATERIAL_TIERS.length - 1];
  const metal = new Set(["Iron", "Steel", "Mithril", "Thorium", "Dark Iron", "Fel Iron", "Adamantite", "Khorium", "Cobalt", "Saronite", "Titansteel", "Titanium", "Nethersteel", "Obsidian"]);
  const cloth = new Set(["Mooncloth", "Netherweave", "Frostwoven"]);
  const hide = new Set(["Dragonscale", "Wyrmhide", "Nerubian"]);
  const universal = new Set(["Primal", "Runed"]);
  const values = uniqueNameParts(tier.values, BATCH_FALLBACK_MATERIALS);
  if (shell.family === "weapon" || shell.armorType === "plate" || shell.armorType === "shield" || Number(shell.slot) === 14) {
    return values.filter(v => metal.has(v) || universal.has(v));
  }
  if (shell.armorType === "cloth") {
    return values.filter(v => cloth.has(v) || universal.has(v));
  }
  if (shell.armorType === "leather") {
    return values.filter(v => hide.has(v) || universal.has(v));
  }
  if (shell.armorType === "mail") {
    return values.filter(v => metal.has(v) || hide.has(v) || universal.has(v));
  }
  return values.filter(v => metal.has(v) || cloth.has(v) || universal.has(v));
}

function referenceNameParts(shell, ilvl, quality) {
  const candidates = referenceMetadataCandidates(shell, ilvl, quality).slice(0, 50);
  const adjectives = new Set();
  const materials = new Set();
  const nouns = new Set();
  const suffixes = new Set();
  const stopWords = new Set(["of", "the", "and", "a", "an"]);
  for (const c of candidates) {
    const rawName = cleanNamePart(c.item.name);
    if (!rawName) continue;
    const suffixMatch = rawName.match(/\b(of(?: the)? [A-Za-z0-9' -]+)$/i);
    if (suffixMatch) {
      const suffix = cleanNamePart(suffixMatch[1]);
      if (suffix.split(/\s+/).length <= 5) suffixes.add(suffix);
    }
    for (const material of BATCH_FALLBACK_MATERIALS) {
      const materialPattern = new RegExp(`\\b${String(material).replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\b`, "i");
      if (materialPattern.test(rawName)) materials.add(material);
    }
    const nameStem = cleanNamePart(rawName.replace(/\bof\b.+$/i, ""));
    const words = nameStem.replace(/[-_]/g, " ").split(/\s+/).filter(Boolean);
    if (!words.length) continue;
    const first = words[0];
    const last = words[words.length - 1];
    if (first.length > 3 && !/^\d+$/.test(first) && !stopWords.has(first.toLowerCase())) adjectives.add(first);
    if (last.length > 2 && !/^\d+$/.test(last) && !stopWords.has(last.toLowerCase())) nouns.add(last);
  }
  return {
    adjectives: [...adjectives],
    materials: [...materials],
    nouns: [...nouns],
    suffixes: [...suffixes],
  };
}

function batchNameThemeKey(shell, statPlan = {}) {
  const text = `${statPlan.blueprintKey || ""} ${statPlan.blueprintLabel || ""}`.toLowerCase();
  if (text.includes("block")) return "tankBlock";
  if (text.includes("avoid") || text.includes("dodge") || text.includes("parry")) return "tankAvoidance";
  if (text.includes("threat")) return "tankThreat";
  if (text.includes("tank") || shell.weaponSubclass === "tank_1h" || shell.armorType === "shield" || Number(shell.slot) === 14) return "tank";
  if (text.includes("hunter") || ["bow", "gun", "crossbow", "thrown"].includes(shell.weaponSubclass)) return "hunter";
  if (text.includes("healer")) return "healer";
  if (text.includes("caster") || ["caster_mh", "caster_staff", "wand"].includes(shell.weaponSubclass) || shell.armorType === "cloth" || Number(shell.slot) === 23) return "caster";
  if (text.includes("pvp")) return "pvp";
  return "melee";
}

function cleanGeneratedItemName(name, fallbackNoun) {
  let clean = cleanNamePart(name)
    .replace(/\b(of the|of)\s*$/i, "")
    .replace(/\s+/g, " ")
    .trim();
  clean = clean.replace(/\b([A-Za-z]+)\s+\1\b/gi, "$1");
  clean = clean.replace(/^([A-Za-z']+)\s+(.+)\s+of(?: the)? \1$/i, "$2 of the $1");
  if (clean.split(/\s+/).length < 2) clean = `${chooseRandom(BATCH_FALLBACK_ADJECTIVES)} ${fallbackNoun}`;
  if (clean.length > 70) clean = clean.split(/\s+/).slice(0, 7).join(" ");
  return clean;
}

function generateBatchItemName(shell, ilvl, quality, statPlan = {}) {
  const parts = referenceNameParts(shell, ilvl, quality);
  const themeKey = batchNameThemeKey(shell, statPlan);
  const theme = BATCH_NAME_THEMES[themeKey] || BATCH_NAME_THEMES.melee;
  const tankTheme = themeKey.startsWith("tank");
  const qualityParts = BATCH_QUALITY_NAME_PARTS[Number(quality)] || BATCH_QUALITY_NAME_PARTS[3];
  const fallbackNoun = fallbackNounForShell(shell);
  const noun = chooseNamePart([
    { values: parts.nouns, weight: 4 },
    { values: shellNounPool(shell), weight: 6 },
  ]) || fallbackNoun;
  const adjective = chooseNamePart([
    { values: parts.adjectives, weight: 3 },
    { values: theme.adjectives, weight: 5 },
    { values: qualityParts.adjectives, weight: 3 },
    { values: BATCH_FALLBACK_ADJECTIVES, weight: 1 },
  ]);
  const material = chooseNamePart([
    { values: parts.materials, weight: 4 },
    { values: materialPoolForShell(shell, ilvl), weight: 3 },
  ]);
  const suffix = chooseNamePart([
    { values: parts.suffixes, weight: tankTheme ? 1 : 4 },
    { values: theme.suffixes, weight: tankTheme ? 8 : 5 },
    { values: qualityParts.suffixes, weight: tankTheme ? 0 : 2 },
    { values: BATCH_FALLBACK_SUFFIXES, weight: tankTheme ? 0 : 1 },
  ]);
  const pattern = chooseWeighted([
    { value: "adjective-noun", weight: 24 },
    { value: "material-noun", weight: 18 },
    { value: "adjective-material-noun", weight: 16 },
    { value: "noun-suffix", weight: 20 },
    { value: "adjective-noun-suffix", weight: Number(quality) >= 3 ? 22 : 12 },
    { value: "material-noun-suffix", weight: Number(quality) >= 4 ? 16 : 8 },
  ]);
  let name;
  switch (pattern) {
    case "material-noun":
      name = `${material || adjective} ${noun}`;
      break;
    case "adjective-material-noun":
      name = `${adjective} ${material} ${noun}`;
      break;
    case "noun-suffix":
      name = `${noun} ${suffix}`;
      break;
    case "adjective-noun-suffix":
      name = `${adjective} ${noun} ${suffix}`;
      break;
    case "material-noun-suffix":
      name = `${material} ${noun} ${suffix}`;
      break;
    default:
      name = `${adjective} ${noun}`;
      break;
  }
  return cleanGeneratedItemName(name, fallbackNoun);
}

function generateUniqueBatchItemName(shell, ilvl, quality, statPlan, usedNames) {
  for (let i = 0; i < 12; i++) {
    const name = generateBatchItemName(shell, ilvl, quality, statPlan);
    const key = name.toLowerCase();
    if (!usedNames.has(key)) {
      usedNames.add(key);
      return name;
    }
  }
  const noun = fallbackNounForShell(shell);
  const suffix = chooseNamePart([
    { values: BATCH_NAME_THEMES[batchNameThemeKey(shell, statPlan)]?.suffixes, weight: 4 },
    { values: BATCH_FALLBACK_SUFFIXES, weight: 1 },
  ]) || "of Victory";
  const name = cleanGeneratedItemName(`${chooseRandom(BATCH_FALLBACK_ADJECTIVES)} ${noun} ${suffix}`, noun);
  usedNames.add(name.toLowerCase());
  return name;
}

function randomizedBatchSpeed(shell) {
  if (shell.family !== "weapon") return 0;
  const mode = byId("bWeaponSpeedMode").value;
  if (mode !== "random") return shell.speed || defaultWeaponSpeed(shell.weaponSubclass, shell.slot);
  const base = shell.speed || defaultWeaponSpeed(shell.weaponSubclass, shell.slot);
  return Math.max(0.5, Math.min(5, Math.round(randomBetween(base * 0.85, base * 1.15) * 10) / 10));
}

function batchStatSummary(item) {
  return item.stats.map(stat => `+${stat.value} ${STATS[stat.statId]?.label || stat.statId}`).join(", ");
}

function generateBatch() {
  const seed = currentForwardShell();
  const quantity = Math.max(1, Math.min(200, numberValue("bQuantity") || 1));
  const minLevel = Math.max(1, Math.min(MAX_ILVL, numberValue("bMinItemLevel") || numberValue("fItemLevel")));
  const maxLevel = Math.max(minLevel, Math.min(MAX_ILVL, numberValue("bMaxItemLevel") || minLevel));
  const qualityPool = batchQualityPool();
  const currentRows = collectForwardStats();
  const startEntry = Math.max(1, Math.round(numberValue("bEntryStart") || 91000));
  const variance = Math.max(0, Math.min(25, numberValue("bBudgetVariance") || 0)) / 100;
  let levelDeck = shuffleArray(Array.from({ length: maxLevel - minLevel + 1 }, (_, i) => minLevel + i));
  const items = [];
  const usedNames = new Set();

  for (let i = 0; i < quantity; i++) {
    if (!levelDeck.length) levelDeck = shuffleArray(Array.from({ length: maxLevel - minLevel + 1 }, (_, n) => minLevel + n));
    const ilvl = levelDeck.pop();
    const quality = chooseRandom(qualityPool) || numberValue("fQuality");
    const shell = { ...seed, ilvl, quality, speed: randomizedBatchSpeed(seed) };
    const statPlan = buildBatchStatRows(shell, ilvl, quality, currentRows);
    const budgetFuzz = randomBetween(1 - variance, 1 + variance);
    const refPassive = wantReferenceMode()
      ? referencePassiveBudgetValue(ilvl, quality, shell.slot, shell.family, shell.weaponSubclass, shell.armorType, null, statPlan.rows, statPlan.blueprintKey === "current" ? "auto" : statPlan.blueprintKey)
      : null;
    const allocation = allocateStats(statPlan.rows, ilvl, quality, shell.slot, shell.family, shell.sockets, shell.budgetUsage * budgetFuzz, refPassive ? refPassive.budget : null, refPassive);
    const refArmor = wantReferenceMode() ? referenceArmorValue(shell.slot, shell.armorType, quality, ilvl, shell.bonusArmor) : null;
    const armor = refArmor ? refArmor.armor : expectedArmor(shell.slot, shell.armorType, quality, ilvl, shell.bonusArmor);
    const refWeapon = shell.family === "weapon" && wantReferenceMode() ? referenceWeaponDamage(quality, ilvl, shell.slot, shell.weaponSubclass, shell.speed) : null;
    const weapon = shell.family === "weapon" ? (refWeapon || expectedWeaponDamage(quality, ilvl, shell.slot, shell.weaponSubclass, shell.speed)) : null;
    if (weapon && !weapon.source) weapon.source = "formula";
    const visual = chooseVisualMetadata(shell, ilvl, quality);
    const classId = dbClassForShell(shell);
    const subclass = dbSubclassForShell(shell);
    const item = {
      entry: startEntry + i,
      name: generateUniqueBatchItemName(shell, ilvl, quality, statPlan, usedNames),
      classId,
      subclass,
      quality,
      invType: shell.slot,
      family: shell.family,
      armorType: shell.armorType,
      weaponSubclass: shell.weaponSubclass,
      ilvl,
      reqLevel: estimateRequiredLevel(shell, ilvl, quality),
      displayId: visual.displayId,
      displaySource: visual.displaySource,
      material: visual.material,
      sheath: visual.sheath,
      sellPrice: estimateSellPrice(shell, ilvl, quality),
      sockets: shell.sockets,
      gemModel: shell.gemModel,
      socketAudit: socketModelAudit(shell.sockets, quality, ilvl, shell.slot, shell.gemModel),
      stats: allocation.stats,
      allocation,
      armor,
      armorReference: refArmor,
      weapon,
      damageType: damageTypeForShell(shell),
      blueprint: statPlan.blueprintKey,
      blueprintLabel: statPlan.blueprintLabel,
      statCount: statPlan.statCount,
      randomProperty: 0,
      randomSuffix: 0,
      description: `iLvl ${ilvl} ${SLOTS[shell.slot] || "item"} generated by Item Level Calculator batch generator.`,
    };
    items.push(item);
  }

  lastBatchItems = items;
  lastBatchSql = buildBatchSql(items);
  lastBatchCsvRows = buildBatchWdbxCsvRows(items);
  byId("batchResult").innerHTML = renderBatchResult(items);
  byId("batchPreview").innerHTML = renderBatchPreview(items);
  byId("batchSqlOutput").value = lastBatchSql;
}

function renderBatchResult(items) {
  if (!items.length) return `<span class="pill warn">No items generated.</span>`;
  const avgBudget = items.reduce((sum, item) => sum + Number(item.allocation?.usedBudget || 0), 0) / items.length;
  const rows = items.map(item => {
    const q = QUALITY[item.quality] || QUALITY[1];
    const visual = item.displayId ? `Display ${item.displayId}${item.displaySource ? ` from #${item.displaySource.entry}` : ""}` : "No display match";
    const combat = item.weapon ? `${item.weapon.min}-${item.weapon.max}, ${item.weapon.dps.toFixed(1)} DPS` : item.armor ? `${item.armor} armor` : "-";
    return `
      <tr>
        <td><strong>${escapeHtml(item.name)}</strong><br><small class="muted">#${item.entry}, ${escapeHtml(item.blueprintLabel)}</small></td>
        <td><span style="color:${q.color}">${q.name}</span><br><small class="muted">ilvl ${item.ilvl}, req ${item.reqLevel}</small></td>
        <td>${escapeHtml(SLOTS[item.invType] || item.invType)}<br><small class="muted">${escapeHtml(visual)}</small></td>
        <td>${escapeHtml(batchStatSummary(item) || "No passive stats")}</td>
        <td>${escapeHtml(combat)}</td>
      </tr>`;
  }).join("");
  return `
    <div class="summary-grid">
      <div class="metric"><small>Generated items</small><strong>${items.length}</strong></div>
      <div class="metric"><small>Entry range</small><strong>${items[0].entry}-${items[items.length - 1].entry}</strong></div>
      <div class="metric"><small>Average used budget</small><strong>${formatNumber(avgBudget)}</strong></div>
      <div class="metric"><small>Reference metadata</small><strong>${referenceItems.length ? "Loaded" : "Fallback"}</strong></div>
    </div>
    <table class="result-table">
      <thead><tr><th>Item</th><th>Quality / level</th><th>Slot / visual</th><th>Stats</th><th>Armor / damage</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
}

function formatCopper(copper) {
  const value = Math.max(0, Math.round(Number(copper || 0)));
  const gold = Math.floor(value / 10000);
  const silver = Math.floor((value % 10000) / 100);
  const copperOnly = value % 100;
  const parts = [];
  if (gold) parts.push(`${gold}g`);
  if (silver || gold) parts.push(`${silver}s`);
  parts.push(`${copperOnly}c`);
  return parts.join(" ");
}

function previewStatLine(stat) {
  const info = STATS[stat.statId];
  const value = Math.round(Number(stat.value || 0));
  const label = info?.label || `Unknown Stat ${stat.statId}`;
  const greenGroups = new Set(["rating", "ap", "spellpower", "mp5", "regen", "spellpen", "blockvalue", "bonusarmor"]);
  if (info && greenGroups.has(info.group) && info.group !== "ap") {
    return `<div class="preview-stat green">Equip: Increases ${escapeHtml(label)} by ${value}.</div>`;
  }
  return `<div class="preview-stat">+${value} ${escapeHtml(label)}</div>`;
}

function socketPreviewText(sockets) {
  const parts = [];
  for (const [key, count] of Object.entries(sockets || {})) {
    for (let i = 0; i < Number(count || 0); i++) {
      parts.push(`<span class="socket-dot ${key}"></span>${SOCKET_COLORS[key]?.label || key}`);
    }
  }
  return parts.length ? parts.join(" ") : "";
}

function renderBatchPreviewItem(item) {
  const q = QUALITY[item.quality] || QUALITY[1];
  const slotLabel = SLOTS[item.invType] || item.invType;
  const classLabel = item.family === "weapon"
    ? (WEAPON_SUBCLASSES[item.weaponSubclass] || item.weaponSubclass)
    : (ARMOR_TYPES[item.armorType] || item.armorType || "Armor");
  const damageRows = item.weapon ? `
    <div class="preview-row split"><span>${item.weapon.min} - ${item.weapon.max} Damage</span><span>Speed ${item.weapon.speed.toFixed(2)}</span></div>
    <div class="preview-row muted">(${item.weapon.dps.toFixed(1)} damage per second)</div>` : "";
  const armorRow = item.armor > 0 ? `<div class="preview-row">${Math.round(item.armor)} Armor</div>` : "";
  const statRows = item.stats.map(previewStatLine).join("");
  const socketText = socketPreviewText(item.sockets);
  const socketRow = socketText ? `<div class="preview-row sockets">${socketText}</div>` : "";
  const sourceText = item.displaySource
    ? `Display ${item.displayId} from #${item.displaySource.entry} ${item.displaySource.name || ""}`
    : item.displayId
      ? `Display ${item.displayId}`
      : "No display match";
  const budgetText = item.allocation?.referenceInfo
    ? `Reference-scaled budget, ${item.allocation.referenceInfo.confidence} confidence`
    : "Formula budget";

  return `
    <article class="item-preview">
      <div class="preview-name" style="color:${q.color}">${escapeHtml(item.name)}</div>
      <div class="preview-row ilvl">Item Level ${item.ilvl}</div>
      <div class="preview-row">Binds when equipped</div>
      <div class="preview-row split"><span>${escapeHtml(slotLabel)}</span><span>${escapeHtml(classLabel)}</span></div>
      ${damageRows}
      ${armorRow}
      ${statRows || `<div class="preview-stat muted">No passive stats</div>`}
      ${socketRow}
      <div class="preview-row">Requires Level ${item.reqLevel}</div>
      <div class="preview-row muted">Sell Price: ${escapeHtml(formatCopper(item.sellPrice))}</div>
      <div class="preview-meta">
        <span>#${item.entry}</span>
        <span>${escapeHtml(item.blueprintLabel)}</span>
        <span>${escapeHtml(budgetText)}</span>
        <span>${escapeHtml(sourceText)}</span>
        <span>Material ${Math.round(Number(item.material || 0))}, sheath ${Math.round(Number(item.sheath || 0))}</span>
      </div>
    </article>`;
}

function renderBatchPreview(items) {
  if (!items.length) return `<span class="pill warn">No generated items to preview.</span>`;
  return `
    <p class="hint">Preview only. Nothing is written to item_template until you run the generated SQL yourself.</p>
    <div class="preview-grid">${items.map(renderBatchPreviewItem).join("")}</div>`;
}

function batchSqlString(value) {
  return `'${String(value ?? "").replace(/\\/g, "\\\\").replace(/'/g, "''")}'`;
}

function batchStatAssignments(stats, limit = 10) {
  const out = [];
  for (let i = 1; i <= limit; i++) {
    const stat = stats[i - 1];
    out.push(stat ? Number(stat.statId) : 0);
    out.push(stat ? Math.round(Number(stat.value || 0)) : 0);
  }
  return out;
}

function buildBatchSql(items) {
  if (!items.length) return "-- Generate a batch first.";
  const statColumns = [];
  for (let i = 1; i <= 10; i++) {
    statColumns.push(`stat_type${i}`, `stat_value${i}`);
  }
  const columns = [
    "entry", "class", "subclass", "name", "displayid", "Quality", "InventoryType", "ItemLevel", "RequiredLevel",
    "armor", "delay", "dmg_min1", "dmg_max1", "dmg_type1",
    ...statColumns,
    "socketColor_1", "socketColor_2", "socketColor_3",
    "RandomProperty", "RandomSuffix", "Material", "sheath", "SellPrice", "description",
  ];
  const chunks = [`-- Generated by Wrath 3.3.5 Item Level Calculator - Batch generator ${MODEL_VERSION}`];
  for (const item of items) {
    const statValues = batchStatAssignments(item.stats);
    const weapon = item.weapon || {};
    const values = [
      item.entry,
      item.classId,
      item.subclass,
      batchSqlString(item.name),
      Math.round(Number(item.displayId || 0)),
      item.quality,
      item.invType,
      item.ilvl,
      item.reqLevel,
      Math.round(Number(item.armor || 0)),
      item.weapon ? Math.round(Number(weapon.speed || 0) * 1000) : 0,
      item.weapon ? Math.round(Number(weapon.min || 0)) : 0,
      item.weapon ? Math.round(Number(weapon.max || 0)) : 0,
      item.damageType || 0,
      ...statValues,
      socketSqlColor(item.sockets, 1),
      socketSqlColor(item.sockets, 2),
      socketSqlColor(item.sockets, 3),
      item.randomProperty || 0,
      item.randomSuffix || 0,
      Math.round(Number(item.material || 1)),
      Math.round(Number(item.sheath || 0)),
      Math.round(Number(item.sellPrice || 0)),
      batchSqlString(item.description),
    ];
    chunks.push(`DELETE FROM item_template WHERE entry = ${item.entry};`);
    chunks.push(`INSERT INTO item_template (${columns.map(c => `\`${c}\``).join(", ")})\nVALUES (${values.join(", ")});`);
  }
  return `${chunks.join("\n")}\n`;
}

function buildBatchWdbxCsvRows(items) {
  const rows = [["ID", "ClassID", "SubclassID", "Sound_Override_Subclassid", "Material", "DisplayInfoID", "InventoryType", "SheatheType"]];
  for (const item of items) {
    rows.push([
      item.entry,
      item.classId,
      item.subclass,
      -1,
      Math.round(Number(item.material || 1)),
      Math.round(Number(item.displayId || 0)),
      item.invType,
      Math.round(Number(item.sheath || 0)),
    ]);
  }
  return rows;
}

function copyBatchJson() {
  if (!lastBatchItems.length) generateBatch();
  copyText(JSON.stringify(lastBatchItems, null, 2));
}

function copyBatchSql() {
  if (!lastBatchSql) generateBatch();
  copyText(lastBatchSql);
}

function exportBatchCsv() {
  if (!lastBatchCsvRows.length) generateBatch();
  downloadCsv("generated-items-wdbx.csv", lastBatchCsvRows);
}

function calculateForward() {
  const ilvl = numberValue("fItemLevel");
  const quality = numberValue("fQuality");
  const slot = numberValue("fSlot");
  const family = byId("fFamily").value;
  const armorType = byId("fArmorType").value;
  const rawWeaponSubclass = byId("fWeaponSubclass").value;
  const weaponSubclass = family === "weapon" ? (usableWeaponSubclass(rawWeaponSubclass) ? rawWeaponSubclass : defaultWeaponSubclassForSlot(slot)) : "none";
  const speed = family === "weapon" ? (numberValue("fSpeed") || defaultWeaponSpeed(weaponSubclass, slot)) : 0;
  const bonusArmor = numberValue("fBonusArmor");
  const budgetUsage = Math.max(1, Math.min(125, numberValue("fBudgetUsage") || 100)) / 100;
  const sockets = socketCounts("f");
  const gemModel = byId("fGemModel").value;
  const statRows = collectForwardStats();
  const refPassive = wantReferenceMode() ? referencePassiveBudgetValue(ilvl, quality, slot, family, weaponSubclass, armorType, null, statRows, byId("fReferenceProfileMode").value) : null;
  const allocation = allocateStats(statRows, ilvl, quality, slot, family, sockets, budgetUsage, refPassive ? refPassive.budget : null, refPassive);
  const refArmor = wantReferenceMode() ? referenceArmorValue(slot, armorType, quality, ilvl, bonusArmor) : null;
  const armor = refArmor ? refArmor.armor : expectedArmor(slot, armorType, quality, ilvl, bonusArmor);
  const refWeapon = family === "weapon" && wantReferenceMode() ? referenceWeaponDamage(quality, ilvl, slot, weaponSubclass, speed) : null;
  const weapon = family === "weapon" ? (refWeapon || expectedWeaponDamage(quality, ilvl, slot, weaponSubclass, speed)) : null;
  if (weapon && !weapon.source) weapon.source = "formula";

  lastForwardResult = {
    ilvl, quality, slot, family, armorType, weaponSubclass, speed, bonusArmor, budgetUsage, sockets, gemModel,
    allocation, armor, armorReference: refArmor, weapon, referenceProfile: activeForwardReferenceProfile,
    socketAudit: socketModelAudit(sockets, quality, ilvl, slot, gemModel),
  };

  byId("forwardResult").innerHTML = renderForwardResult(lastForwardResult);
}

function renderForwardResult(result) {
  const { ilvl, quality, slot, family, armorType, weaponSubclass, sockets, allocation, armor, weapon } = result;
  const q = QUALITY[quality];
  const statPills = allocation.stats.map(s => `<span class="pill">+${s.value} ${escapeHtml(s.label)}</span>`).join("");
  const socketsText = Object.entries(sockets)
    .filter(([, v]) => Number(v) > 0)
    .map(([k, v]) => `${v} ${SOCKET_COLORS[k].label}`)
    .join(", ") || "No sockets";
  const warnings = forwardWarnings(result);
  const refBudget = allocation.referenceInfo;
  const budgetArchetype = refBudget && refBudget.archetype && refBudget.archetype !== "any" ? `, ${ARCHETYPE_LABELS[refBudget.archetype] || refBudget.archetype} references` : "";
  const budgetSource = refBudget ? `Reference-scaled from ${refBudget.referenceCount}/${refBudget.candidateCount} nearby item(s)${budgetArchetype}, scale ${round(refBudget.scale, 3)}, ${refBudget.outlierCount} outlier(s) trimmed, ${refBudget.confidence} confidence` : "Formula fallback";
  const weaponSource = weapon && weapon.source === "reference" ? `Reference damage from ${weapon.referenceCount}/${weapon.candidateCount} nearby item(s), ${weapon.outlierCount} outlier(s) trimmed, ${weapon.confidence} confidence` : "Formula damage";
  const armorSource = result.armorReference ? `Reference armor from ${result.armorReference.referenceCount}/${result.armorReference.candidateCount} nearby item(s), ${result.armorReference.outlierCount} outlier(s) trimmed, ${result.armorReference.confidence} confidence` : "Formula armor";

  return `
    <div class="summary-grid">
      <div class="metric"><small>Generated ilvl</small><strong>${ilvl}</strong></div>
      <div class="metric"><small>Quality / slot</small><strong style="color:${q.color}">${q.name}</strong><span class="muted">${SLOTS[slot]}</span></div>
      <div class="metric"><small>Passive budget</small><strong>${formatNumber(allocation.remainingBudget)}</strong><span class="muted">${Math.round(allocation.budgetUsage * 1000) / 10}% used, after ${formatNumber(allocation.socketBudget)} socket budget</span><span class="muted">${escapeHtml(budgetSource)}</span></div>
      <div class="metric"><small>Slot coefficient</small><strong>${slotMod(slot, quality, ilvl, family).toFixed(4)}</strong></div>
    </div>
    <h3>Stat line</h3>
    <div class="pill-row">${statPills || `<span class="pill warn">No passive stats generated</span>`}</div>
    <h3>Sockets</h3>
    <div class="pill-row"><span class="pill">${escapeHtml(socketsText)}</span><span class="pill">${allocation.socketCostEach} cost each</span></div>
    ${renderSocketModelAudit(result.socketAudit)}
    ${armor > 0 ? `<h3>Armor</h3><div class="pill-row"><span class="pill good">${armor} Armor</span><span class="pill">${escapeHtml(ARMOR_TYPES[armorType])}</span><span class="pill">${escapeHtml(armorSource)}</span></div>${result.armorReference ? renderReferencePills(result.armorReference.references) : ""}` : ""}
    ${weapon ? `<h3>Weapon damage</h3><div class="pill-row"><span class="pill good">${weapon.min} - ${weapon.max} Damage</span><span class="pill">Speed ${weapon.speed.toFixed(2)}</span><span class="pill">${weapon.dps.toFixed(2)} DPS</span><span class="pill">${escapeHtml(WEAPON_SUBCLASSES[weaponSubclass])}</span><span class="pill">${escapeHtml(weaponSource)}</span></div>${weapon.source === "reference" ? renderReferencePills(weapon.references) : ""}` : ""}
    ${result.referenceProfile ? `<h3>Stat-profile references</h3><div class="pill-row"><span class="pill good">${escapeHtml(result.referenceProfile.archetypeLabel)}</span><span class="pill">${result.referenceProfile.referenceCount} nearby item(s)</span><span class="pill">Filter: ${escapeHtml(result.referenceProfile.profileFilter || "none")}</span></div>${renderReferencePills(result.referenceProfile.references)}${renderReferenceStatDiagnostics(result.referenceProfile)}` : ""}
    ${refBudget ? `<h3>Passive-budget references</h3>${renderReferencePills(refBudget.references)}` : ""}
    ${warnings.length ? `<h3>Warnings</h3><div class="pill-row">${warnings.map(w => `<span class="pill warn">${escapeHtml(w)}</span>`).join("")}</div>` : ""}
    <h3>Generated item JSON</h3>
    <div class="codebox">${escapeHtml(JSON.stringify(toExportObject(result), null, 2))}</div>
  `;
}

function renderSocketModelAudit(audit) {
  if (!audit || !audit.socketCount) return "";
  const netClass = audit.netExternalBudget >= 0 ? "good" : "warn";
  return `
    <h3>Socket and gem budget</h3>
    <div class="summary-grid">
      <div class="metric"><small>Item socket cost</small><strong>${formatNumber(audit.itemSocketCost)}</strong><span class="muted">${audit.socketCount} socket(s)</span></div>
      <div class="metric"><small>Estimated gem value</small><strong>${formatNumber(audit.gemBudget)}</strong><span class="muted">${escapeHtml(audit.gemModel)}</span></div>
      <div class="metric"><small>Estimated socket bonus</small><strong>${formatNumber(audit.socketBonusBudget)}</strong><span class="muted">${escapeHtml(audit.socketBonusModel)}</span></div>
      <div class="metric"><small>Net after gemming</small><strong class="${netClass}">${formatNumber(audit.netExternalBudget)}</strong><span class="muted">gem value + bonus - item socket cost</span></div>
    </div>`;
}

function forwardWarnings(result) {
  const warnings = [];
  if (["trinket", "relic"].includes(result.family)) warnings.push("Passive budget only: hidden use/equip effects are not modeled as full power.");
  if (result.ilvl >= 260 && result.quality >= 4) warnings.push("Late Wrath outlier zone; expect broader variance.");
  if (socketTotal(result.sockets) >= 3) warnings.push("Socket-heavy item; socket bonus is intentionally not charged as full stat budget.");
  if (result.family === "weapon" && ["caster_mh", "caster_staff", "wand"].includes(result.weaponSubclass)) warnings.push("Caster weapon physical DPS is reduced by family prior.");
  if (result.referenceProfile && result.referenceProfile.archetype === "any") warnings.push("Stat profile is using any nearby slot archetype; choose Melee, Caster, Healer, Tank, etc. for cleaner generated weights.");
  if (result.allocation.referenceInfo && result.allocation.referenceInfo.confidence === "low") warnings.push("Passive-budget reference confidence is low; inspect the shown DB neighbors before trusting the scale.");
  if (result.armorReference && result.armorReference.confidence === "low") warnings.push("Armor reference confidence is low; nearby same-slot DB armor was thin or noisy.");
  if (result.weapon && result.weapon.source === "reference" && result.weapon.confidence === "low") warnings.push("Weapon reference confidence is low; nearby same-subclass DB weapons were thin or noisy.");
  return warnings;
}

function calculateReverse() {
  const quality = numberValue("rQuality");
  const slot = numberValue("rSlot");
  const family = byId("rFamily").value;
  const armorType = byId("rArmorType").value;
  const rawWeaponSubclass = byId("rWeaponSubclass").value;
  const weaponSubclass = family === "weapon" ? (usableWeaponSubclass(rawWeaponSubclass) ? rawWeaponSubclass : defaultWeaponSubclassForSlot(slot)) : "none";
  const sockets = socketCounts("r");
  const input = {
    quality,
    slot,
    family,
    armorType,
    weaponSubclass,
    sockets,
    stats: collectReverseStats(),
    armor: numberValue("rArmor"),
    minDamage: numberValue("rMinDamage"),
    maxDamage: numberValue("rMaxDamage"),
    speed: numberValue("rSpeed"),
  };

  const candidates = reverseEstimate(input);
  const best = candidates[0];
  input.bestIlvl = best.ilvl;
  const tol = TOLERANCES[byId("rTolerance").value] || TOLERANCES.normal;
  const band = candidates.filter(c => Math.abs(c.ilvl - best.ilvl) <= tol.ilvl || c.score <= best.score + tol.score).slice(0, 12);
  const referenceMatches = referenceReverseMatches(input);
  byId("reverseResult").innerHTML = renderReverseResult(input, best, band, tol, referenceMatches);
}

function renderReverseResult(input, best, band, tol, referenceMatches = []) {
  const conf = confidenceForScore(best.score, input);
  const q = QUALITY[input.quality];
  const topRows = band.map(c => `
    <tr>
      <td>${c.ilvl}</td>
      <td>${(c.score * 100).toFixed(2)}%</td>
      <td>${(c.statsErr * 100).toFixed(2)}%</td>
      <td>${input.armor > 0 ? (c.armorErr * 100).toFixed(2) + "%" : "—"}</td>
      <td>${input.minDamage > 0 && input.maxDamage > 0 ? (c.dpsErr * 100).toFixed(2) + "%" : "—"}</td>
    </tr>`).join("");

  const statPills = input.stats.map(s => `<span class="pill">+${s.value} ${escapeHtml(STATS[s.statId].label)}</span>`).join("") || `<span class="pill warn">No passive stats entered</span>`;
  const warnings = conf.warnings.map(w => `<span class="pill warn">${escapeHtml(w)}</span>`).join("");
  const referenceRows = referenceMatches.map(match => {
    const item = match.item;
    return `
      <tr>
        <td><strong>${escapeHtml(item.name)}</strong><br><small class="muted">#${item.entry || "?"} - ${escapeHtml(referenceFactFlags(item).join(", "))}</small></td>
        <td>${item.ilvl}</td>
        <td>${escapeHtml(SLOTS[item.invType] || item.invType)}</td>
        <td>${(match.score * 100).toFixed(2)}%</td>
        <td>${(match.budgetErr * 100).toFixed(2)}%</td>
        <td>${(match.profileErr * 100).toFixed(2)}%</td>
        <td>${input.armor > 0 ? (match.armorErr * 100).toFixed(2) + "%" : "-"}</td>
        <td>${input.minDamage > 0 && input.maxDamage > 0 ? (match.dpsErr * 100).toFixed(2) + "%" : "-"}</td>
      </tr>`;
  }).join("");

  return `
    <div class="summary-grid">
      <div class="metric"><small>Best estimate</small><strong>${best.ilvl}</strong></div>
      <div class="metric"><small>Confidence</small><strong class="${conf.cls}">${conf.confidence}</strong><span class="muted">${(best.score * 100).toFixed(2)}% combined residual</span></div>
      <div class="metric"><small>Quality / slot</small><strong style="color:${q.color}">${q.name}</strong><span class="muted">${SLOTS[input.slot]}</span></div>
      <div class="metric"><small>Tolerance profile</small><strong>${tol.label}</strong></div>
    </div>
    <h3>Observed stat payload</h3>
    <div class="pill-row">${statPills}</div>
    ${warnings ? `<h3>Warnings</h3><div class="pill-row">${warnings}</div>` : ""}
    <h3>Candidate band</h3>
    <table class="result-table">
      <thead><tr><th>ilvl</th><th>Combined residual</th><th>Stat residual</th><th>Armor residual</th><th>DPS residual</th></tr></thead>
      <tbody>${topRows}</tbody>
    </table>
    ${referenceRows ? `
      <h3>Closest loaded DB items</h3>
      <table class="result-table">
        <thead><tr><th>Item</th><th>ilvl</th><th>Slot</th><th>Match residual</th><th>Budget residual</th><th>Profile residual</th><th>Armor residual</th><th>DPS residual</th></tr></thead>
        <tbody>${referenceRows}</tbody>
      </table>
      <p class="hint">These are actual rows from the loaded item_template reference set. Set items and spell-payload items stay visible, but receive a small penalty because their true power is not fully represented by passive stat columns.</p>` : ""}
    <p class="hint">Reverse itemization is a ranked estimate. Identical stat payloads can map to different slots/qualities once armor, damage, source, proc effects, or handcrafted outliers are involved.</p>
  `;
}

function findReferenceItemByEntry(entry) {
  const id = Number(entry || 0);
  if (!id) return null;
  return referenceItems.find(item => Number(item.entry) === id) || null;
}

function itemStatOnlyBudget(item) {
  return (item.stats || []).reduce((sum, stat) => sum + statBudgetContribution(stat.statId, stat.value, item.quality, item.ilvl, item.invType), 0);
}

function itemToReverseInput(item) {
  return {
    quality: item.quality,
    slot: item.invType,
    family: item.family,
    armorType: item.armorType,
    weaponSubclass: item.weaponSubclass || "none",
    sockets: item.sockets,
    stats: item.stats,
    armor: item.armor,
    minDamage: item.dmgMin,
    maxDamage: item.dmgMax,
    speed: item.speed,
    excludeEntry: item.entry,
  };
}

function formatPct(value) {
  const n = Number(value || 0);
  const sign = n > 0 ? "+" : "";
  return `${sign}${(n * 100).toFixed(2)}%`;
}

function normalizeItemStats(item, targetChargedBudget, targetSource) {
  const stats = item.stats || [];
  const currentStatBudget = itemStatOnlyBudget(item);
  const socketBudgetValue = socketBudget(item.sockets, item.quality, item.ilvl, item.invType);
  const source = targetSource || "budget";
  const targetBudget = Number(targetChargedBudget || 0);
  const targetStatBudget = Math.max(0, targetBudget - socketBudgetValue);

  if (targetBudget <= 0) {
    return {
      source,
      targetChargedBudget: targetBudget,
      currentStatBudget,
      targetStatBudget,
      socketBudget: socketBudgetValue,
      scale: 0,
      rows: [],
      note: `No ${source} budget is available for this item.`,
    };
  }

  if (!stats.length || currentStatBudget <= 0 || targetStatBudget <= 0) {
    return {
      source,
      targetChargedBudget: targetBudget,
      currentStatBudget,
      targetStatBudget,
      socketBudget: socketBudgetValue,
      scale: 0,
      rows: [],
      note: "No passive stat normalization is available for this item.",
    };
  }

  const scale = targetStatBudget / currentStatBudget;
  const rows = stats.map(stat => {
    const statId = Number(stat.statId);
    const currentValue = Number(stat.value || 0);
    const currentBudget = statBudgetContribution(statId, currentValue, item.quality, item.ilvl, item.invType);
    const targetBudget = currentBudget * scale;
    const mod = statMod(statId, item.quality, item.ilvl, item.invType);
    const suggestedValue = Math.max(0, Math.round(Math.pow(targetBudget, 1 / EXPONENT) / Math.max(mod, 0.00001)));
    return {
      statId,
      label: STATS[statId]?.label || `Unknown stat ${statId}`,
      currentValue,
      suggestedValue,
      delta: suggestedValue - currentValue,
      currentBudget,
      targetBudget,
    };
  });

  return {
    source,
    targetChargedBudget: targetBudget,
    currentStatBudget,
    targetStatBudget,
    socketBudget: socketBudgetValue,
    scale,
    rows,
    note: `Preserves current stat-budget shares and scales passive stats to the ${source} expected budget.`,
  };
}

function renderNormalizationSuggestion(normalization, heading = "Normalization suggestion") {
  if (!normalization || !normalization.rows.length) {
    return `
      <h3>${escapeHtml(heading)}</h3>
      <div class="pill-row"><span class="pill warn">${escapeHtml(normalization?.note || "No normalization suggestion is available.")}</span></div>`;
  }

  const scaleClass = Math.abs(Number(normalization.scale || 1) - 1) <= 0.08 ? "good" : "warn";
  const rows = normalization.rows.map(row => `
    <tr>
      <td>${row.statId}</td>
      <td>${escapeHtml(row.label)}</td>
      <td>${row.currentValue}</td>
      <td>${row.suggestedValue}</td>
      <td>${row.delta > 0 ? "+" : ""}${row.delta}</td>
      <td>${formatNumber(row.currentBudget)}</td>
      <td>${formatNumber(row.targetBudget)}</td>
    </tr>`).join("");

  return `
    <h3>${escapeHtml(heading)}</h3>
    <div class="summary-grid">
      <div class="metric"><small>Target source</small><strong>${escapeHtml(normalization.source)}</strong></div>
      <div class="metric"><small>Current stat budget</small><strong>${formatNumber(normalization.currentStatBudget)}</strong></div>
      <div class="metric"><small>Target stat budget</small><strong>${formatNumber(normalization.targetStatBudget)}</strong><span class="muted">after ${formatNumber(normalization.socketBudget)} socket budget</span></div>
      <div class="metric"><small>Passive scale</small><strong class="${scaleClass}">${formatPct(normalization.scale - 1)}</strong><span class="muted">${escapeHtml(normalization.note)}</span></div>
    </div>
    <table class="result-table">
      <thead><tr><th>Type</th><th>Stat</th><th>Current</th><th>Suggested</th><th>Delta</th><th>Current budget</th><th>Target budget</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
}

const COMPARE_DESIGN_POLICY = {
  acceptDelta: 0.03,
  minMeaningfulNormalizeDelta: 0.05,
  maxAutoNormalizeDelta: 0.20,
  weakReferenceNearestProfileErr: 0.30,
  weakReferenceAverageProfileErr: 0.35,
  weakReferenceSmallDelta: 0.08,
  formulaReferenceDisagreement: 0.08,
};

function referenceQualityFromNeighbors(neighbors) {
  if (!neighbors || !neighbors.length) {
    return {
      quality: "None",
      cls: "bad",
      nearestProfileErr: null,
      averageProfileErr: null,
      note: "No loaded DB neighbors passed the active reference filters.",
    };
  }

  const sample = neighbors.slice(0, Math.min(5, neighbors.length));
  const nearestProfileErr = Number(neighbors[0].profileErr || 0);
  const averageProfileErr = sample.reduce((sum, match) => sum + Number(match.profileErr || 0), 0) / sample.length;
  const nearestScore = Number(neighbors[0].score || 0);

  let quality = "Weak";
  let cls = "bad";
  if (nearestProfileErr <= 0.18 && averageProfileErr <= 0.25 && nearestScore <= 0.12) {
    quality = "Strong";
    cls = "good";
  } else if (nearestProfileErr <= COMPARE_DESIGN_POLICY.weakReferenceNearestProfileErr
    && averageProfileErr <= COMPARE_DESIGN_POLICY.weakReferenceAverageProfileErr) {
    quality = "Usable";
    cls = "warn";
  }

  return {
    quality,
    cls,
    nearestProfileErr,
    averageProfileErr,
    nearestScore,
    note: `Nearest profile residual ${formatPct(nearestProfileErr)}; top-${sample.length} average ${formatPct(averageProfileErr)}.`,
  };
}

function normalizationStatsSummary(normalization) {
  if (!normalization || !normalization.rows || !normalization.rows.length) return "";
  return normalization.rows
    .map(row => `${row.currentValue} ${row.label} -> ${row.suggestedValue}`)
    .join("; ");
}

function normalizationChanged(normalization) {
  return !!(normalization && normalization.rows && normalization.rows.some(row => Number(row.delta || 0) !== 0));
}

function itemHasUseOrProcEffect(item) {
  return (item?.spellPayloads || []).some(payload => {
    const trigger = Number(payload.trigger || 0);
    return trigger === 0 || trigger === 1 || trigger === 2 || trigger === 6;
  });
}

function compareDesignFlags(item, data) {
  const flags = [];
  if (!item) return flags;
  const name = String(item.name || "");
  if (/^\s*(test|\d+)/i.test(name)) flags.push("Excluded-looking name");
  if (Number(item.quality || 0) >= 5) flags.push("Legendary/artifact quality");
  if (["trinket", "relic"].includes(item.family)) flags.push(`${item.family} slot hides contextual power`);
  if (Number(item.itemset || 0) > 0) flags.push(`Set/tier item ${item.itemset}`);
  if (socketTotal(item.sockets) > 0) flags.push(`${socketTotal(item.sockets)} socket(s); socket assumptions affect budget`);
  if (Number(item.socketBonus || 0) > 0) flags.push(`Socket bonus spell ${item.socketBonus}`);
  if (item.spellPayloads && item.spellPayloads.length) flags.push(`${item.spellPayloads.length} spell payload(s)`);
  if (itemHasUseOrProcEffect(item)) flags.push("Use/proc payload");
  if (data.referenceQuality?.quality === "Weak") flags.push("Weak profile-neighbor similarity");
  if (data.referenceDelta !== null && Math.abs(Number(data.formulaDelta || 0) - Number(data.referenceDelta || 0)) >= COMPARE_DESIGN_POLICY.formulaReferenceDisagreement) {
    flags.push("Formula/reference disagreement");
  }
  return flags;
}

function compareVerdictClass(verdict) {
  if (!verdict) return "warn";
  if (verdict === "Accept") return "good";
  if (verdict === "Exclude") return "bad";
  return "warn";
}

function buildCompareDesignVerdict(data) {
  const item = data.item;
  const referenceQuality = referenceQualityFromNeighbors(data.neighbors);
  const designFlags = compareDesignFlags(item, { ...data, referenceQuality });
  const formulaAbs = Math.abs(Number(data.formulaDelta || 0));
  const referenceAbs = data.referenceDelta === null ? null : Math.abs(Number(data.referenceDelta || 0));
  const bestDelta = referenceAbs !== null ? referenceAbs : formulaAbs;
  const manualReasons = [];
  let verdict = "Manual review";
  let action = "Do not auto-apply";
  let targetSource = "none";
  let autoApplyAllowed = false;

  if (/^\s*(test|\d+)/i.test(String(item.name || "")) || Number(item.quality || 0) >= 5 || ["trinket", "relic"].includes(item.family)) {
    verdict = "Exclude";
    action = "Exclude from generic passive normalization";
    manualReasons.push("This item is not safe for a generic passive-stat normalization pass.");
  }

  if (verdict !== "Exclude") {
    if (Number(item.itemset || 0) > 0) manualReasons.push("Set/tier bonuses are progression context, not passive budget columns.");
    if (item.spellPayloads && item.spellPayloads.length) manualReasons.push("Spell payloads can carry hidden use/proc/equip power.");
    if (referenceQuality.quality === "Weak") manualReasons.push("Reference neighbors are budget-adjacent but not profile-adjacent.");
    if (data.referenceDelta !== null && referenceQuality.quality === "Weak" && referenceAbs <= COMPARE_DESIGN_POLICY.weakReferenceSmallDelta) {
      manualReasons.push("Reference delta is small, so weak neighbors should not create an automatic trim.");
    }
    if (socketTotal(item.sockets) > 0) manualReasons.push("Socketed items need socket/gem design context before changing visible stats.");
    if (data.referenceDelta !== null && Math.abs(Number(data.formulaDelta || 0) - Number(data.referenceDelta || 0)) >= COMPARE_DESIGN_POLICY.formulaReferenceDisagreement) {
      manualReasons.push("Formula and reference models disagree enough that the authority is not settled.");
    }

    if (!manualReasons.length && bestDelta <= COMPARE_DESIGN_POLICY.acceptDelta) {
      verdict = "Accept";
      action = "No stat change recommended";
      targetSource = "none";
    } else if (!manualReasons.length && bestDelta >= COMPARE_DESIGN_POLICY.minMeaningfulNormalizeDelta && bestDelta <= COMPARE_DESIGN_POLICY.maxAutoNormalizeDelta) {
      targetSource = data.referenceDelta !== null && referenceQuality.quality !== "Weak" ? "reference" : "formula";
      verdict = targetSource === "reference" ? "Reference normalization candidate" : "Formula normalization candidate";
      action = "Candidate only; review before applying";
      autoApplyAllowed = false;
    } else if (!manualReasons.length) {
      manualReasons.push(bestDelta < COMPARE_DESIGN_POLICY.minMeaningfulNormalizeDelta
        ? "Delta is too small to justify a normalization edit."
        : "Delta is large enough to require manual item-design review.");
    }
  }

  const recommendedStats = targetSource === "reference"
    ? normalizationStatsSummary(data.referenceNormalization)
    : targetSource === "formula"
      ? normalizationStatsSummary(data.formulaNormalization)
      : "";

  return {
    formulaVerdict: `${formatPct(data.formulaDelta)} vs formula`,
    referenceVerdict: data.referenceDelta === null ? "No reference estimate" : `${formatPct(data.referenceDelta)} vs reference`,
    referenceQuality,
    designFlags,
    designVerdict: verdict,
    autoApplyAllowed,
    recommendedAction: action,
    recommendedTargetSource: targetSource,
    recommendedStats,
    recommendationReason: manualReasons.length ? manualReasons.join(" ") : "Budget fit is close enough and no design-confidence flags were detected.",
  };
}

function renderCompareDesignVerdict(decision) {
  if (!decision) return "";
  const flags = decision.designFlags.length
    ? decision.designFlags.map(flag => `<span class="pill warn">${escapeHtml(flag)}</span>`).join("")
    : `<span class="pill good">No design-confidence flags</span>`;

  return `
    <h3>Design verdict</h3>
    <div class="summary-grid">
      <div class="metric"><small>Formula verdict</small><strong>${escapeHtml(decision.formulaVerdict)}</strong></div>
      <div class="metric"><small>Reference verdict</small><strong>${escapeHtml(decision.referenceVerdict)}</strong></div>
      <div class="metric"><small>Reference quality</small><strong class="${decision.referenceQuality.cls}">${escapeHtml(decision.referenceQuality.quality)}</strong><span class="muted">${escapeHtml(decision.referenceQuality.note)}</span></div>
      <div class="metric"><small>Recommended action</small><strong class="${compareVerdictClass(decision.designVerdict)}">${escapeHtml(decision.recommendedAction)}</strong><span class="muted">${escapeHtml(decision.designVerdict)}</span></div>
    </div>
    <div class="pill-row">${flags}</div>
    <p class="hint">${escapeHtml(decision.recommendationReason)}</p>
    ${decision.recommendedStats ? `<p class="hint"><strong>Candidate stats:</strong> ${escapeHtml(decision.recommendedStats)}</p>` : ""}`;
}

function compareLoadedItem() {
  if (!referenceItems.length) {
    byId("compareResult").innerHTML = `<span class="pill bad">Load reference data first.</span>`;
    lastCompareCsvRows = [];
    return;
  }

  const item = findReferenceItemByEntry(numberValue("compareEntry"));
  if (!item) {
    byId("compareResult").innerHTML = `<span class="pill bad">Item entry was not found in loaded reference data.</span>`;
    lastCompareCsvRows = [];
    return;
  }

  byId("compareResult").innerHTML = renderCompareResult(item);
}

function renderCompareResult(item) {
  const q = QUALITY[item.quality] || QUALITY[1];
  const statBudget = itemStatOnlyBudget(item);
  const chargedBudget = observedBudget(item.stats, item.quality, item.ilvl, item.invType, item.sockets);
  const expectedBudget = passiveBudget(item.ilvl, item.quality, item.invType, item.family);
  const refBudget = referencePassiveBudgetValue(item.ilvl, item.quality, item.invType, item.family, item.weaponSubclass, item.armorType, item.entry, item.stats, "auto");
  const expectedRefBudget = refBudget ? refBudget.budget : null;
  const formulaDelta = expectedBudget > 0 ? (chargedBudget - expectedBudget) / expectedBudget : 0;
  const referenceDelta = expectedRefBudget > 0 ? (chargedBudget - expectedRefBudget) / expectedRefBudget : null;
  const socketAudit = socketModelAudit(item.sockets, item.quality, item.ilvl, item.invType, byId("compareGemModel").value);
  const armorReference = item.armor > 0 ? referenceArmorValue(item.invType, item.armorType, item.quality, item.ilvl, 0, item.entry) : null;
  const weaponReference = item.family === "weapon" && item.dps > 0 ? referenceWeaponDamage(item.quality, item.ilvl, item.invType, item.weaponSubclass, item.speed, item.entry) : null;
  const expectedArmorValue = armorReference ? armorReference.armor : expectedArmor(item.invType, item.armorType, item.quality, item.ilvl, 0);
  const expectedWeaponValue = weaponReference || (item.family === "weapon" ? expectedWeaponDamage(item.quality, item.ilvl, item.invType, item.weaponSubclass, item.speed) : null);
  const armorDelta = item.armor > 0 && expectedArmorValue > 0 ? (item.armor - expectedArmorValue) / expectedArmorValue : null;
  const dpsDelta = item.dps > 0 && expectedWeaponValue && expectedWeaponValue.dps > 0 ? (item.dps - expectedWeaponValue.dps) / expectedWeaponValue.dps : null;
  const neighbors = referenceReverseMatches(itemToReverseInput(item), 10);
  const referenceNormalization = normalizeItemStats(item, expectedRefBudget, "reference");
  const formulaNormalization = normalizeItemStats(item, expectedBudget, "formula");
  const designDecision = buildCompareDesignVerdict({
    item,
    formulaDelta,
    referenceDelta,
    neighbors,
    referenceNormalization,
    formulaNormalization,
  });
  lastCompareCsvRows = compareItemCsvRows({
    item,
    statBudget,
    chargedBudget,
    expectedBudget,
    expectedRefBudget,
    formulaDelta,
    referenceDelta,
    socketAudit,
    armorDelta,
    dpsDelta,
    expectedArmorValue,
    expectedWeaponValue,
    referenceNormalization,
    formulaNormalization,
    designDecision,
    neighbors,
  });
  lastCompareCsvFilename = `compare-item-${item.entry || "unknown"}.csv`;
  const statPills = item.stats.map(s => `<span class="pill">+${s.value} ${escapeHtml(STATS[s.statId]?.label || s.statId)}</span>`).join("") || `<span class="pill warn">No passive stats</span>`;
  const neighborRows = neighbors.map(match => `
    <tr>
      <td><strong>${escapeHtml(match.item.name)}</strong><br><small class="muted">#${match.item.entry || "?"} - ${escapeHtml(referenceFactFlags(match.item).join(", "))}</small></td>
      <td>${match.item.ilvl}</td>
      <td>${escapeHtml(SLOTS[match.item.invType] || match.item.invType)}</td>
      <td>${(match.score * 100).toFixed(2)}%</td>
      <td>${(match.budgetErr * 100).toFixed(2)}%</td>
      <td>${(match.profileErr * 100).toFixed(2)}%</td>
    </tr>`).join("");

  return `
    <div class="summary-grid">
      <div class="metric"><small>Item</small><strong>${escapeHtml(item.name)}</strong><span class="muted">#${item.entry}</span></div>
      <div class="metric"><small>Quality / ilvl</small><strong style="color:${q.color}">${q.name}</strong><span class="muted">ilvl ${item.ilvl}</span></div>
      <div class="metric"><small>Slot</small><strong>${escapeHtml(SLOTS[item.invType] || item.invType)}</strong><span class="muted">${escapeHtml(item.weaponSubclass ? WEAPON_SUBCLASSES[item.weaponSubclass] || item.weaponSubclass : ARMOR_TYPES[item.armorType] || item.armorType)}</span></div>
      <div class="metric"><small>Formula delta</small><strong class="${Math.abs(formulaDelta) <= 0.08 ? "good" : "warn"}">${formatPct(formulaDelta)}</strong><span class="muted">charged passive budget vs formula</span></div>
    </div>
    <h3>Passive budget audit</h3>
    <div class="summary-grid">
      <div class="metric"><small>Stat-only budget</small><strong>${formatNumber(statBudget)}</strong></div>
      <div class="metric"><small>Charged budget</small><strong>${formatNumber(chargedBudget)}</strong><span class="muted">stats + socket cost</span></div>
      <div class="metric"><small>Formula expected</small><strong>${formatNumber(expectedBudget)}</strong></div>
      <div class="metric"><small>Reference expected</small><strong>${expectedRefBudget ? formatNumber(expectedRefBudget) : "No match"}</strong><span class="muted">${referenceDelta === null ? "No reference delta" : formatPct(referenceDelta)}</span></div>
    </div>
    <h3>Stats</h3>
    <div class="pill-row">${statPills}</div>
    ${renderCompareDesignVerdict(designDecision)}
    ${renderNormalizationSuggestion(formulaNormalization, "Formula-normalized candidate")}
    ${renderNormalizationSuggestion(referenceNormalization, "Reference-normalized candidate")}
    ${renderSocketModelAudit(socketAudit)}
    ${(item.armor > 0 || item.dps > 0) ? `
      <h3>Armor / DPS audit</h3>
      <div class="summary-grid">
        <div class="metric"><small>Armor</small><strong>${item.armor || "-"}</strong><span class="muted">${armorDelta === null ? "No armor comparison" : `expected ${expectedArmorValue}, ${formatPct(armorDelta)}`}</span></div>
        <div class="metric"><small>DPS</small><strong>${item.dps ? item.dps.toFixed(2) : "-"}</strong><span class="muted">${dpsDelta === null ? "No DPS comparison" : `expected ${expectedWeaponValue.dps.toFixed(2)}, ${formatPct(dpsDelta)}`}</span></div>
      </div>` : ""}
    ${renderPayloadAudit(item)}
    ${neighborRows ? `
      <h3>Nearest loaded DB neighbors</h3>
      <table class="result-table">
        <thead><tr><th>Item</th><th>ilvl</th><th>Slot</th><th>Match residual</th><th>Budget residual</th><th>Profile residual</th></tr></thead>
        <tbody>${neighborRows}</tbody>
      </table>` : `<h3>Nearest loaded DB neighbors</h3><div class="pill-row"><span class="pill warn">No neighbors passed the active reference filters</span></div>`}
    <p class="hint">Reference quality controls on the Reference data tab affect this neighbor list and the reference-expected budget.</p>`;
}

function generateSql() {
  if (!lastForwardResult) {
    byId("sqlOutput").value = "-- Generate a forward result first.";
    return;
  }
  const entry = numberValue("sqlEntry");
  const r = lastForwardResult;
  const assignments = [];
  assignments.push(`ItemLevel = ${sqlNumber(r.ilvl)}`);
  assignments.push(`Quality = ${sqlNumber(r.quality)}`);
  assignments.push(`InventoryType = ${sqlNumber(r.slot)}`);
  assignments.push(`socketColor_1 = ${socketSqlColor(r.sockets, 1)}`);
  assignments.push(`socketColor_2 = ${socketSqlColor(r.sockets, 2)}`);
  assignments.push(`socketColor_3 = ${socketSqlColor(r.sockets, 3)}`);

  for (let i = 1; i <= 10; i++) {
    const stat = r.allocation.stats[i - 1];
    if (stat && STATS[stat.statId].sql !== null) {
      assignments.push(`stat_type${i} = ${STATS[stat.statId].sql}`);
      assignments.push(`stat_value${i} = ${sqlNumber(stat.value)}`);
    } else {
      assignments.push(`stat_type${i} = 0`);
      assignments.push(`stat_value${i} = 0`);
    }
  }

  if (r.armor > 0) assignments.push(`armor = ${sqlNumber(r.armor)}`);
  if (r.weapon) {
    assignments.push(`dmg_min1 = ${r.weapon.min.toFixed(1)}`);
    assignments.push(`dmg_max1 = ${r.weapon.max.toFixed(1)}`);
    assignments.push(`delay = ${Math.round(r.weapon.speed * 1000)}`);
  }

  lastSql = `-- Generated by Wrath 3.3.5 Item Level Calculator - Reference Data Build ${MODEL_VERSION}\nUPDATE item_template\nSET\n  ${assignments.join(",\n  ")}\nWHERE entry = ${sqlNumber(entry)};\n`;
  byId("sqlOutput").value = lastSql;
}

function socketSqlColor(sockets, index) {
  const expanded = [];
  for (const [key, count] of Object.entries(sockets)) {
    for (let i = 0; i < Number(count || 0); i++) expanded.push(SOCKET_COLORS[key].sql);
  }
  return expanded[index - 1] || 0;
}

function toExportObject(result) {
  return {
    itemLevel: result.ilvl,
    quality: QUALITY[result.quality].name,
    inventoryType: { id: result.slot, label: SLOTS[result.slot] },
    family: result.family,
    sockets: result.sockets,
    budget: {
      exponent: EXPONENT,
      base: round(result.allocation.baseBudget, 3),
      socket: round(result.allocation.socketBudget, 3),
      passiveAfterSockets: round(result.allocation.remainingBudget, 3),
      slotMod: round(slotMod(result.slot, result.quality, result.ilvl, result.family), 6),
      qualityCurve: round(qualityCurve(result.quality, result.ilvl), 3),
    },
    socketModel: result.socketAudit ? {
      gemModel: result.socketAudit.gemModel,
      itemSocketCost: round(result.socketAudit.itemSocketCost, 3),
      estimatedGemBudget: round(result.socketAudit.gemBudget, 3),
      estimatedSocketBonusBudget: round(result.socketAudit.socketBonusBudget, 3),
      socketBonusModel: result.socketAudit.socketBonusModel,
      netAfterGemming: round(result.socketAudit.netExternalBudget, 3),
    } : undefined,
    stats: result.allocation.stats.map(s => ({ type: s.statId, label: s.label, value: s.value, mod: round(s.mod, 6), share: round(s.share, 4) })),
    armor: result.armor || undefined,
    weapon: result.weapon ? {
      subclass: WEAPON_SUBCLASSES[result.weaponSubclass],
      min: result.weapon.min,
      max: result.weapon.max,
      speed: round(result.weapon.speed, 2),
      dps: round(result.weapon.dps, 2),
    } : undefined,
  };
}

function compareItemCsvRows(data) {
  const item = data.item;
  const rows = [[
    "section", "entry", "item", "field", "stat_type", "stat_label",
    "current_value", "suggested_value", "delta", "value", "note"
  ]];

  const add = (section, field, value, note = "", statType = "", statLabel = "", currentValue = "", suggestedValue = "", delta = "") => {
    rows.push([
      section,
      item.entry || "",
      item.name || "",
      field,
      statType,
      statLabel,
      currentValue,
      suggestedValue,
      delta,
      value,
      note,
    ]);
  };

  add("summary", "quality", QUALITY[item.quality]?.name || item.quality);
  add("summary", "item_level", item.ilvl);
  add("summary", "slot", SLOTS[item.invType] || item.invType);
  add("summary", "stat_only_budget", round(data.statBudget, 3));
  add("summary", "charged_budget", round(data.chargedBudget, 3), "stats plus socket item cost");
  add("summary", "formula_expected_budget", round(data.expectedBudget, 3));
  add("summary", "reference_expected_budget", data.expectedRefBudget ? round(data.expectedRefBudget, 3) : "", data.expectedRefBudget ? "archetype-filtered reference estimate" : "no reference estimate");
  add("summary", "formula_delta", formatPct(data.formulaDelta));
  add("summary", "reference_delta", data.referenceDelta === null ? "" : formatPct(data.referenceDelta));
  add("summary", "formula_delta_pct", round(Number(data.formulaDelta || 0) * 100, 3));
  add("summary", "reference_delta_pct", data.referenceDelta === null ? "" : round(Number(data.referenceDelta || 0) * 100, 3));

  const decision = data.designDecision;
  if (decision) {
    add("recommendation", "formula_verdict", decision.formulaVerdict);
    add("recommendation", "reference_verdict", decision.referenceVerdict);
    add("recommendation", "reference_quality", decision.referenceQuality.quality, decision.referenceQuality.note);
    add("recommendation", "design_flags", decision.designFlags.join(" | "));
    add("recommendation", "design_verdict", decision.designVerdict);
    add("recommendation", "auto_apply_allowed", decision.autoApplyAllowed ? 1 : 0);
    add("recommendation", "recommended_action", decision.recommendedAction);
    add("recommendation", "recommended_target_source", decision.recommendedTargetSource);
    add("recommendation", "recommended_stats", decision.recommendedStats);
    add("recommendation", "recommendation_reason", decision.recommendationReason);
    add("recommendation", "formula_normalized_stats", normalizationStatsSummary(data.formulaNormalization));
    add("recommendation", "reference_normalized_stats", normalizationStatsSummary(data.referenceNormalization));
  }

  if (data.socketAudit) {
    add("socket", "gem_model", data.socketAudit.gemModel);
    add("socket", "item_socket_cost", round(data.socketAudit.itemSocketCost, 3));
    add("socket", "estimated_gem_budget", round(data.socketAudit.gemBudget, 3));
    add("socket", "estimated_socket_bonus_budget", round(data.socketAudit.socketBonusBudget, 3), data.socketAudit.socketBonusModel);
    add("socket", "net_after_gemming", round(data.socketAudit.netExternalBudget, 3));
  }

  for (const stat of item.stats || []) {
    add("current_stat", "current", stat.value, "", stat.statId, STATS[stat.statId]?.label || stat.statId, stat.value);
  }

  for (const n of [data.referenceNormalization, data.formulaNormalization]) {
    if (n) {
      const section = `${n.source}_normalization`;
      add(section, "target_source", n.source, n.note);
      add(section, "target_charged_budget", round(n.targetChargedBudget, 3));
      add(section, "current_stat_budget", round(n.currentStatBudget, 3));
      add(section, "target_stat_budget", round(n.targetStatBudget, 3));
      add(section, "passive_scale", round(n.scale, 6));
      for (const stat of n.rows || []) {
        add(`${section}_stat`, "suggested", "", "", stat.statId, stat.label, stat.currentValue, stat.suggestedValue, stat.delta);
        add(`${section}_budget`, "stat_budget", round(stat.targetBudget, 3), "target budget for this stat", stat.statId, stat.label, round(stat.currentBudget, 3), round(stat.targetBudget, 3), round(stat.targetBudget - stat.currentBudget, 3));
      }
    }
  }

  for (const match of data.neighbors || []) {
    const neighbor = match.item;
    rows.push([
      "neighbor",
      neighbor.entry || "",
      neighbor.name || "",
      "nearest_loaded_db_item",
      "",
      "",
      "",
      "",
      "",
      round(match.score, 6),
      `ilvl=${neighbor.ilvl}; slot=${SLOTS[neighbor.invType] || neighbor.invType}; budgetErr=${round(match.budgetErr, 6)}; profileErr=${round(match.profileErr, 6)}; flags=${referenceFactFlags(neighbor).join("|")}`,
    ]);
  }

  return rows;
}

function copyForwardJson() {
  if (!lastForwardResult) return;
  copyText(JSON.stringify(toExportObject(lastForwardResult), null, 2));
}

function byId(id) { return document.getElementById(id); }
function numberValue(id) { return Number(byId(id).value || 0); }
function round(n, places = 2) { const p = Math.pow(10, places); return Math.round(Number(n) * p) / p; }
function formatNumber(n) { return Number(n).toLocaleString(undefined, { maximumFractionDigits: 2 }); }
function sqlNumber(n) { return Number.isFinite(Number(n)) ? String(Math.round(Number(n))) : "0"; }
function escapeHtml(value) {
  return String(value).replace(/[&<>'"]/g, ch => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;" }[ch]));
}
function csvCell(value) {
  const text = value === null || value === undefined ? "" : String(value);
  return /[",\r\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}
function rowsToCsv(rows) {
  return (rows || []).map(row => row.map(csvCell).join(",")).join("\r\n");
}
function downloadCsv(filename, rows) {
  if (!rows || rows.length <= 1) return;
  const blob = new Blob([rowsToCsv(rows)], { type: "text/csv;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename || "item-calculator.csv";
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}
function copyText(text) {
  if (!text) return;
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).catch(() => fallbackCopy(text));
  } else {
    fallbackCopy(text);
  }
}

function exportCompareCsv() {
  if (!lastCompareCsvRows.length) {
    compareLoadedItem();
  }

  if (!lastCompareCsvRows.length) {
    byId("compareResult").innerHTML = `<span class="pill bad">Compare an item before exporting CSV.</span>`;
    return;
  }

  downloadCsv(lastCompareCsvFilename, lastCompareCsvRows);
}
function fallbackCopy(text) {
  const ta = document.createElement("textarea");
  ta.value = text;
  document.body.appendChild(ta);
  ta.select();
  document.execCommand("copy");
  ta.remove();
}

function initEvents() {
  document.querySelectorAll(".tab").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach(b => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach(p => p.classList.remove("active"));
      btn.classList.add("active");
      byId(btn.dataset.tab).classList.add("active");
    });
  });

  document.body.addEventListener("click", e => {
    if (e.target.classList.contains("remove-row")) {
      if (e.target.closest("#forwardStatsBody")) activeForwardReferenceProfile = null;
      e.target.closest("tr").remove();
    }
  });

  byId("addForwardStat").addEventListener("click", () => { activeForwardReferenceProfile = null; addForwardStat(32, 10); });
  byId("loadReferenceProfile").addEventListener("click", applyReferenceStatProfile);
  byId("addReverseStat").addEventListener("click", () => addReverseStat(32, 50));
  byId("clearReverseStats").addEventListener("click", () => { byId("reverseStatsBody").innerHTML = ""; });
  byId("forwardStatsBody").addEventListener("input", () => { activeForwardReferenceProfile = null; });
  byId("forwardStatsBody").addEventListener("change", () => { activeForwardReferenceProfile = null; });
  byId("loadPresetMelee").addEventListener("click", () => loadForwardPreset("melee"));
  byId("loadPresetCaster").addEventListener("click", () => loadForwardPreset("caster"));
  byId("loadPresetTank").addEventListener("click", () => loadForwardPreset("tank"));
  byId("calculateForward").addEventListener("click", calculateForward);
  byId("copyForwardJson").addEventListener("click", copyForwardJson);
  byId("generateBatch").addEventListener("click", generateBatch);
  byId("copyBatchJson").addEventListener("click", copyBatchJson);
  byId("copyBatchSql").addEventListener("click", copyBatchSql);
  byId("exportBatchCsv").addEventListener("click", exportBatchCsv);
  byId("calculateReverse").addEventListener("click", calculateReverse);
  byId("compareItem").addEventListener("click", compareLoadedItem);
  byId("exportCompareCsv").addEventListener("click", exportCompareCsv);
  byId("generateSql").addEventListener("click", generateSql);
  byId("copySql").addEventListener("click", () => copyText(byId("sqlOutput").value));
  byId("loadReferenceData").addEventListener("click", loadReferenceDataFromUi);
  byId("clearReferenceData").addEventListener("click", clearReferenceData);
  byId("loadLocalMysqlReferenceData").addEventListener("click", loadReferenceDataFromLocalMySql);
  byId("copyReferenceSql").addEventListener("click", () => copyText(byId("referenceSql").value));

  byId("fSlot").addEventListener("change", syncForwardDefaults);
  byId("fFamily").addEventListener("change", () => { activeForwardReferenceProfile = null; syncShellControlState("f"); });
  byId("fArmorType").addEventListener("change", () => { activeForwardReferenceProfile = null; });
  byId("fWeaponSubclass").addEventListener("change", syncForwardDefaults);
  byId("fGemModel").addEventListener("change", () => { if (lastForwardResult) calculateForward(); });
  byId("rSlot").addEventListener("change", syncReverseDefaults);
  byId("rFamily").addEventListener("change", () => syncShellControlState("r"));
  byId("rWeaponSubclass").addEventListener("change", syncReverseDefaults);

  ["refExcludePvp", "refExcludeItemSets", "refExcludeSpellPayloads", "refExactSlot", "refExactArmorType"].forEach(id => {
    byId(id).addEventListener("change", () => {
      activeForwardReferenceProfile = null;
      renderReferenceStatus();
    });
  });
}


function syncShellControlState(prefix) {
  const slot = numberValue(`${prefix}Slot`);
  const family = byId(`${prefix}Family`).value;
  const weaponSelect = byId(`${prefix}WeaponSubclass`);
  const speedInput = byId(`${prefix}Speed`);
  const armorSelect = byId(`${prefix}ArmorType`);
  const weaponShell = family === "weapon" || isWeaponSlot(slot);

  if (weaponSelect) {
    weaponSelect.disabled = !weaponShell;
    if (!weaponShell) weaponSelect.value = "none";
    else if (!usableWeaponSubclass(weaponSelect.value)) weaponSelect.value = defaultWeaponSubclassForSlot(slot);
  }

  if (speedInput) {
    speedInput.disabled = !weaponShell;
    if (!weaponShell) speedInput.value = "";
  }

  if (armorSelect) {
    armorSelect.disabled = family === "weapon" || family === "trinket" || family === "relic" || isPassiveNonArmorSlot(slot);
  }
}

function syncForwardDefaults() {
  activeForwardReferenceProfile = null;
  const slot = numberValue("fSlot");
  if (isWeaponSlot(slot)) {
    byId("fFamily").value = "weapon";
    if (!usableWeaponSubclass(byId("fWeaponSubclass").value)) byId("fWeaponSubclass").value = defaultWeaponSubclassForSlot(slot);
    byId("fSpeed").value = defaultWeaponSpeed(byId("fWeaponSubclass").value, slot).toFixed(1);
  } else if (slot === 12) {
    byId("fFamily").value = "trinket";
    byId("fArmorType").value = "none";
    byId("fWeaponSubclass").value = "none";
  } else if (slot === 28) {
    byId("fFamily").value = "relic";
    byId("fArmorType").value = "none";
    byId("fWeaponSubclass").value = "none";
  } else if ([2, 11, 23].includes(slot)) {
    byId("fFamily").value = "armor";
    byId("fArmorType").value = "none";
    byId("fWeaponSubclass").value = "none";
  } else if (slot === 14) {
    byId("fFamily").value = "armor";
    byId("fArmorType").value = "shield";
    byId("fWeaponSubclass").value = "none";
  } else {
    if (byId("fFamily").value !== "weapon") byId("fWeaponSubclass").value = "none";
  }
  syncShellControlState("f");
}

function syncReverseDefaults() {
  const slot = numberValue("rSlot");
  if (isWeaponSlot(slot)) {
    byId("rFamily").value = "weapon";
    if (!usableWeaponSubclass(byId("rWeaponSubclass").value)) byId("rWeaponSubclass").value = defaultWeaponSubclassForSlot(slot);
    byId("rSpeed").value = defaultWeaponSpeed(byId("rWeaponSubclass").value, slot).toFixed(1);
  } else if (slot === 12) {
    byId("rFamily").value = "trinket";
    byId("rArmorType").value = "none";
    byId("rWeaponSubclass").value = "none";
  } else if (slot === 28) {
    byId("rFamily").value = "relic";
    byId("rArmorType").value = "none";
    byId("rWeaponSubclass").value = "none";
  } else if ([2, 11, 23].includes(slot)) {
    byId("rFamily").value = "armor";
    byId("rArmorType").value = "none";
    byId("rWeaponSubclass").value = "none";
  } else if (slot === 14) {
    byId("rFamily").value = "armor";
    byId("rArmorType").value = "shield";
    byId("rWeaponSubclass").value = "none";
  } else {
    if (byId("rFamily").value !== "weapon") byId("rWeaponSubclass").value = "none";
  }
  syncShellControlState("r");
}

function boot() {
  populateSelects();
  byId("referenceSql").value = REFERENCE_EXPORT_SQL;
  renderReferenceStatus();
  loadForwardPreset("melee");
  syncForwardDefaults();
  syncReverseDefaults();
  addReverseStat(4, 153);
  addReverseStat(7, 129);
  addReverseStat(32, 68);
  addReverseStat(31, 68);
  initEvents();
  calculateForward();
}

document.addEventListener("DOMContentLoaded", boot);
