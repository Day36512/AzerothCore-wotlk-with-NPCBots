/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Cell.h"
#include "CellImpl.h"
#include "Config.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "RaceMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "SpellScriptLoader.h"
#include "black_temple.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
    void AnnounceCustomForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& message, uint32 cooldownMs = 5000);
}

enum Says
{
    SAY_ILLIDAN_MINION         = 0,
    SAY_ILLIDAN_KILL           = 1,
    SAY_ILLIDAN_TAKEOFF        = 2,
    SAY_ILLIDAN_SUMMONFLAMES   = 3,
    SAY_ILLIDAN_EYE_BLAST      = 4,
    SAY_ILLIDAN_MORPH          = 5,
    SAY_ILLIDAN_ENRAGE         = 6,
    SAY_ILLIDAN_TAUNT          = 7,
    SAY_ILLIDAN_DUPLICITY      = 8,
    SAY_ILLIDAN_UNCONVINCED    = 9,
    SAY_ILLIDAN_PREPARED       = 10,
    SAY_ILLIDAN_SHADOW_PRISON  = 11,
    SAY_ILLIDAN_CONFRONT_MAIEV = 12,
    SAY_ILLIDAN_FRENZY         = 13,
    SAY_ILLIDAN_DEFEATED       = 14,

    EMOTE_AZZINOTH_GAZE        = 0
};

enum Spells
{
    SPELL_ILLIDAN_KNEEL_INTRO           = 39656, // Aura removal does not play the full animation, using StandState instead
    SPELL_DUAL_WIELD                    = 42459,
    SPELL_BERSERK                       = 45078,
    SPELL_EMOTE_TALK_QUESTION           = 41616,
    SPELL_CLEAR_ALL_DEBUFFS             = 34098,
    SPELL_HIT_CHANCE                    = 43689,

    // Phase 1
    SPELL_FLAME_CRASH                   = 40832,
    SPELL_DRAW_SOUL                     = 40904,
    SPELL_DRAW_SOUL_HEAL                = 40903,
    SPELL_PARASITIC_SHADOWFIEND         = 41917,
    SPELL_PARASITIC_SHADOWFIEND_TRIGGER = 41914,
    SPELL_SUMMON_PARASITIC_SHADOWFIENDS = 41915,

    // Phase 2
    SPELL_THROW_GLAIVE                  = 39635,
    SPELL_THROW_GLAIVE2                 = 39849,
    SPELL_GLAIVE_RETURNS                = 39873,
    SPELL_SUMMON_GLAIVE                 = 41466,
    SPELL_FIREBALL                      = 40598,
    SPELL_DARK_BARRAGE                  = 40585,
    SPELL_EYE_BLAST                     = 39908,

    // Phase 3
    SPELL_AGONIZING_FLAMES              = 40932,
    SPELL_SUMMON_MAIEV                  = 40403,
    SPELL_SHADOW_PRISON                 = 40647,

    // Phase 4
    SPELL_DEMON_TRANSFORM_1             = 40511,
    SPELL_DEMON_TRANSFORM_2             = 40398,
    SPELL_DEMON_TRANSFORM_3             = 40510,
    SPELL_DEMON_FORM                    = 40506,
    SPELL_SHADOW_BLAST                  = 41078,
    SPELL_AURA_OF_DREAD                 = 41142,
    SPELL_FLAME_BURST                   = 41126,
    SPELL_FLAME_BURST_EFFECT            = 41131,
    SPELL_SUMMON_SHADOW_DEMON           = 41117,
    SPELL_CONSUME_SOUL                  = 41080,
    SPELL_FIND_TARGET                   = 41081,

    // Phase 5
    SPELL_FRENZY                        = 40683,
    SPELL_TELEPORT_MAIEV                = 41221,
    SPELL_DEATH                         = 41218,

    // Cage
    SPELL_CAGE_TRAP                     = 40693,
    SPELL_CAGE_TRAP_PERIODIC            = 40760,
    SPELL_CAGE_TRAP_DUMMY               = 40761,
    SPELL_CAGED_DEBUFF                  = 40695,
    SPELL_CAGED_SUMMON1                 = 40696,
    SPELL_CAGED_SUMMON8                 = 40703
};

enum Misc
{
    EQUIPMENT_UNARMED                   = 0,
    EQUIPMENT_GLAIVES                   = 1,

    // Illidan
    ACTION_START_EVENT                  = 1,
    ACTION_ILLIDAN_LIFTOFF              = 2,
    ACTION_ILLIDAN_CAGED                = 3,
    ACTION_SHADOW_PRISON                = 4,
    ACTION_ILLIDAN_DIE                  = 5,
    ACTION_ILLIDAN_DEMON_TRANSFORM      = 6,
    ACTION_ILLIDAN_DEMON_TRANSFORM_BACK = 7,

    // Akama
    ACTION_ILLIDARI_COUNCIL_DONE        = 0,
    ACTION_AKAMA_MINIONS                = 1,
    ACTION_AKAMA_ENDING                 = 2,
    ACTION_AKAMA_MAIEV_DESPAWN          = 3,

    // Summons
    ACTION_MAIEV_ENDING                 = 1,
    ACTION_RETURN_BLADE                 = 2, // Sent to 22996 (Blade of Azzinoth)

    MAX_EYE_BEAM_POS                    = 4,

    POINT_ILLIDAN_TAKEOFF               = 1,
    POINT_ILLIDAN_HOVER                 = 2,
    POINT_ILLIDAN_LAND                  = 3,

    GROUP_BERSERK                       = 1,
    GROUP_PHASE_FLYING                  = 2,
    GROUP_DEMON_FORM                    = 3,

    NPC_WORLD_TRIGGER                   = 22515,
    NPC_ILLIDAN_DB_TARGET               = 23070,
    NPC_MAIEV_SHADOWSONG                = 23197,
    NPC_SHADOW_DEMON                    = 23375,

    GO_CAGE_TRAP                        = 185916,

    PHASE_INITIAL                       = 1,
    PHASE_FLYING                        = 2,
    PHASE_LANDING                       = 3,
    PHASE_DEMON                         = 4,
    PHASE_MAIEV                         = 5
};

const Position eyeBeamPos[MAX_EYE_BEAM_POS * 2] =
{
    {639.97f, 301.63f, 354.0f, 0.0f},
    {658.83f, 265.10f, 354.0f, 0.0f},
    {656.86f, 344.07f, 354.0f, 0.0f},
    {640.70f, 310.47f, 354.0f, 0.0f},

    {706.22f, 273.26f, 354.0f, 0.0f},
    {717.55f, 328.33f, 354.0f, 0.0f},
    {718.06f, 286.08f, 354.0f, 0.0f},
    {705.92f, 337.14f, 354.0f, 0.0f}
};

const Position airHoverPos[MAX_EYE_BEAM_POS] =
{
    {658.83f, 265.10f, 356.0f, 0.0f},
    {706.22f, 273.26f, 356.0f, 0.0f},
    {705.92f, 337.14f, 356.0f, 0.0f},
    {656.86f, 344.07f, 356.0f, 0.0f}
};

Position illidanTakeoffPoint = { 727.6356f, 305.62753, 359.1486f };
Position illidanLand = { 676.648f, 304.76074f, 354.18906f, 6.230825424194335937f };
Position roomCenter = { 676.021f, 305.455f, 353.582f, 3.82227f };

Position const BladesPositions[2] =
{
    { 676.226013f, 325.230988f },
    { 678.059998f, 285.220001f }
};

namespace
{
    static constexpr char const* DBM_BLACK_TEMPLE_MODULE_FOLDER = "DBM-BlackTemple";
    static constexpr char const* DBM_ILLIDAN_MODULE_ID = "Illidan";
    static constexpr char const* CONFIG_ILLIDAN_FLAME_TETHER_EXTRA_DISTANCE = "BlackTemple.Illidan.FlameOfAzzinoth.TetherExtraDistance";
    static constexpr char const* CONFIG_ILLIDAN_RANGED_DPS_BOT_HAZARD_DAMAGE_MULTIPLIER = "BlackTemple.Illidan.RangedDpsBotHazardDamageMultiplier";
    static constexpr float FLAME_OF_AZZINOTH_BASE_TETHER_DISTANCE = 30.0f;
    static constexpr float PARASITIC_SHADOWFIEND_SAFE_MIN_RADIUS = 14.0f;
    static constexpr float PARASITIC_SHADOWFIEND_SAFE_MAX_RADIUS = 24.0f;
    static constexpr float PARASITIC_SHADOWFIEND_SAFE_STEP = 4.0f;
    static constexpr float ILLIDAN_PI = 3.14159265358979323846f;
    static constexpr uint32 SPELL_BLOODLUST = 2825;
    static constexpr uint32 SPELL_HEROISM = 32182;
    static constexpr uint32 SPELL_EXHAUSTION = 57723;
    static constexpr uint32 SPELL_SATED = 57724;
    static constexpr uint32 SPELL_DRUMS_OF_WAR = 35475;
    static constexpr uint32 SPELL_ELIXIR_OF_DEMONSLAYING = 11406;
    static constexpr float ILLIDAN_INITIAL_TANK_BOT_THREAT = 1000000.0f;
    static constexpr float ILLIDAN_DEMON_WARLOCK_TANK_THREAT = 10000000.0f;
    static constexpr float FLAME_ASSIGNED_TANK_BOT_THREAT = 3500000.0f;
    static constexpr float FLAME_BACKUP_TANK_BOT_THREAT = 900000.0f;
    static constexpr float ILLIDAN_SUMMON_CLEANUP_RANGE = 220.0f;

    bool IsOwnedNPCBot(Unit const* target)
    {
        Creature const* creature = target ? target->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && const_cast<Creature*>(creature)->GetBotAI();
    }

    bool IsPlayerOrOwnedNPCBot(Unit const* target)
    {
        return target && (target->IsPlayer() || IsOwnedNPCBot(target));
    }

    bool IsOwnedTankNPCBot(Unit const* target)
    {
        Creature const* creature = target ? target->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = const_cast<Creature*>(creature)->GetBotAI();
        return ai && (ai->IsTank(creature) || ai->IsOffTank(creature));
    }

    bool IsIllidanProtectedTankTarget(Creature const* illidan, Unit const* target)
    {
        if (!illidan || !target)
            return false;

        if (IsOwnedTankNPCBot(target))
            return true;

        Creature* mutableIllidan = const_cast<Creature*>(illidan);
        return target == mutableIllidan->GetVictim() ||
            target == mutableIllidan->GetThreatMgr().GetCurrentVictim() ||
            target == mutableIllidan->GetThreatMgr().GetLastVictim();
    }

    bool HasParasiticShadowfiendAura(Unit const* target)
    {
        return target && (target->HasAura(SPELL_PARASITIC_SHADOWFIEND) || target->HasAura(SPELL_PARASITIC_SHADOWFIEND_TRIGGER));
    }

    float GetFlameOfAzzinothTetherDistance()
    {
        float extraDistance = sConfigMgr->GetOption<float>(CONFIG_ILLIDAN_FLAME_TETHER_EXTRA_DISTANCE, 5.0f);
        if (extraDistance < 0.0f)
            extraDistance = 0.0f;

        return FLAME_OF_AZZINOTH_BASE_TETHER_DISTANCE + extraDistance;
    }

    float GetIllidanRangedDpsBotHazardDamageMultiplier()
    {
        return std::clamp(sConfigMgr->GetOption<float>(CONFIG_ILLIDAN_RANGED_DPS_BOT_HAZARD_DAMAGE_MULTIPLIER, 0.7f), 0.0f, 5.0f);
    }

    struct OwnedIllidanBotRef
    {
        Creature* Bot = nullptr;
        Player* Owner = nullptr;
        bot_ai* AI = nullptr;
    };

    bool IsValidIllidanSupportOwner(Creature const* source, Player const* player, float range)
    {
        return source && player && player->IsInWorld() && player->IsInMap(source) && player->InSamePhase(source) && player->IsWithinDistInMap(source, range);
    }

    bool IsValidIllidanSupportBot(Creature const* source, Creature const* bot, float range)
    {
        return source && bot && bot->IsNPCBot() && bot->IsAlive() && bot->IsInWorld() &&
            bot->IsInMap(source) && bot->InSamePhase(source) && bot->IsWithinDistInMap(source, range);
    }

    void AddIllidanOwnedBotRef(Creature* source, Player* owner, Creature* bot, float range, std::vector<OwnedIllidanBotRef>& bots)
    {
        if (!IsValidIllidanSupportBot(source, bot, range))
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        for (OwnedIllidanBotRef const& existing : bots)
            if (existing.Bot && existing.Bot->GetGUID() == bot->GetGUID())
                return;

        bots.push_back({ bot, owner, ai });
    }

    std::vector<OwnedIllidanBotRef> GatherNearbyOwnedIllidanBots(Creature* source, float range = 180.0f)
    {
        std::vector<OwnedIllidanBotRef> bots;
        if (!source || !source->GetMap())
            return bots;

        for (MapReference const& ref : source->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!IsValidIllidanSupportOwner(source, player, range) || !player->HaveBot() || !player->GetBotMgr())
                continue;

            for (BotMap::value_type const& pair : *player->GetBotMgr()->GetBotMap())
                AddIllidanOwnedBotRef(source, player, pair.second, range, bots);
        }

        return bots;
    }

    bool IsIllidanTankBot(OwnedIllidanBotRef const& botRef)
    {
        return botRef.Bot && botRef.AI && (botRef.AI->IsTank(botRef.Bot) || botRef.AI->IsOffTank(botRef.Bot));
    }

    bool IsIllidanRangedDpsBot(Unit const* target)
    {
        Creature const* bot = target ? target->ToCreature() : nullptr;
        if (!bot || !bot->IsNPCBot())
            return false;

        bot_ai* ai = const_cast<Creature*>(bot)->GetBotAI();
        return ai && !ai->IAmFree() && ai->HasRole(BOT_ROLE_DPS) && ai->HasRole(BOT_ROLE_RANGED) &&
            !ai->HasRole(BOT_ROLE_HEAL) && !IsOwnedTankNPCBot(target);
    }

    bool IsIllidanPhysicalDpsBot(OwnedIllidanBotRef const& botRef)
    {
        if (!botRef.Bot || !botRef.AI || botRef.AI->IAmFree())
            return false;

        if (!botRef.AI->HasRole(BOT_ROLE_DPS) || botRef.AI->HasRole(BOT_ROLE_HEAL) || IsIllidanTankBot(botRef))
            return false;

        if (!botRef.AI->HasRole(BOT_ROLE_RANGED))
            return true;

        switch (botRef.AI->GetBotClass())
        {
            case BOT_CLASS_HUNTER:
            case BOT_CLASS_DARK_RANGER:
                return true;
            default:
                return false;
        }
    }

    uint8 GetIllidanTankBotSlot(OwnedIllidanBotRef const& botRef)
    {
        if (botRef.Owner && botRef.Owner->GetBotMgr())
            return uint8(botRef.Owner->GetBotMgr()->GetNpcBotSlotByRole(BOT_ROLE_TANK | BOT_ROLE_TANK_OFF, botRef.Bot));

        return uint8(botRef.Bot ? botRef.Bot->GetGUID().GetCounter() % 2 : 0);
    }

    bool IllidanTankBotPrefersEast(OwnedIllidanBotRef const& botRef)
    {
        return (GetIllidanTankBotSlot(botRef) % 2) == 0;
    }

    bool IsEastFlameOfAzzinoth(Creature const* flame)
    {
        return flame && flame->GetExactDist2d(BladesPositions[1].GetPositionX(), BladesPositions[1].GetPositionY()) <=
            flame->GetExactDist2d(BladesPositions[0].GetPositionX(), BladesPositions[0].GetPositionY());
    }

    void AddIllidanThreat(Unit* target, Unit* tank, float threat)
    {
        Creature* creatureTarget = target ? target->ToCreature() : nullptr;
        if (!creatureTarget || !tank || !creatureTarget->CanHaveThreatList())
            return;

        tank->SetInCombatWith(creatureTarget);
        creatureTarget->SetInCombatWith(tank);

        float const currentThreat = creatureTarget->GetThreatMgr().GetThreat(tank, true);
        if (currentThreat < threat)
            creatureTarget->GetThreatMgr().AddThreat(tank, threat - currentThreat, nullptr, true, true);
    }

    void SeedIllidanInitialTankBotThreat(Creature* illidan)
    {
        if (!illidan || !illidan->CanHaveThreatList())
            return;

        for (OwnedIllidanBotRef const& botRef : GatherNearbyOwnedIllidanBots(illidan))
            if (IsIllidanTankBot(botRef))
                AddIllidanThreat(illidan, botRef.Bot, ILLIDAN_INITIAL_TANK_BOT_THREAT);
    }

    Unit* SelectHighestHealthIllidanWarlock(Creature* illidan)
    {
        if (!illidan || !illidan->GetMap())
            return nullptr;

        Unit* bestWarlock = nullptr;
        uint32 bestHealth = 0;

        auto consider = [&](Unit* unit)
        {
            if (!unit || !unit->IsAlive() || !unit->IsInWorld() || unit->GetMap() != illidan->GetMap() ||
                !unit->InSamePhase(illidan) || !unit->IsWithinDistInMap(illidan, 180.0f) || unit->HasUnitState(UNIT_STATE_ISOLATED))
                return;

            uint32 const maxHealth = unit->GetMaxHealth();
            if (!bestWarlock || maxHealth > bestHealth)
            {
                bestWarlock = unit;
                bestHealth = maxHealth;
            }
        };

        for (MapReference const& ref : illidan->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (IsValidIllidanSupportOwner(illidan, player, 180.0f) && player->IsAlive() && player->getClass() == CLASS_WARLOCK)
                consider(player);
        }

        for (OwnedIllidanBotRef const& botRef : GatherNearbyOwnedIllidanBots(illidan))
            if (botRef.Bot && botRef.AI && botRef.AI->GetBotClass() == BOT_CLASS_WARLOCK)
                consider(botRef.Bot);

        return bestWarlock;
    }

    void SeedIllidanDemonWarlockTankThreat(Creature* illidan)
    {
        if (!illidan || !illidan->CanHaveThreatList())
            return;

        Unit* warlock = SelectHighestHealthIllidanWarlock(illidan);
        if (!warlock)
            return;

        AddIllidanThreat(illidan, warlock, ILLIDAN_DEMON_WARLOCK_TANK_THREAT);
        if (illidan->AI())
            illidan->AI()->AttackStart(warlock);
    }

    void SeedFlameOfAzzinothTankBotThreat(Creature* flame)
    {
        if (!flame || !flame->CanHaveThreatList())
            return;

        bool const eastFlame = IsEastFlameOfAzzinoth(flame);
        Unit* preferredTank = nullptr;
        uint8 preferredTankSlot = std::numeric_limits<uint8>::max();

        for (OwnedIllidanBotRef const& botRef : GatherNearbyOwnedIllidanBots(flame))
        {
            if (!IsIllidanTankBot(botRef))
                continue;

            bool const preferred = IllidanTankBotPrefersEast(botRef) == eastFlame;
            AddIllidanThreat(flame, botRef.Bot, preferred ? FLAME_ASSIGNED_TANK_BOT_THREAT : FLAME_BACKUP_TANK_BOT_THREAT);

            uint8 const tankSlot = GetIllidanTankBotSlot(botRef);
            if (preferred && (!preferredTank || tankSlot < preferredTankSlot))
            {
                preferredTank = botRef.Bot;
                preferredTankSlot = tankSlot;
            }
        }

        if (!preferredTank)
            for (OwnedIllidanBotRef const& botRef : GatherNearbyOwnedIllidanBots(flame))
                if (IsIllidanTankBot(botRef))
                {
                    preferredTank = botRef.Bot;
                    break;
                }

        if (preferredTank && flame->AI())
            flame->AI()->AttackStart(preferredTank);
    }

    bool HasIllidanHeroismActive(Unit const* unit)
    {
        return unit && (unit->HasAura(SPELL_BLOODLUST) || unit->HasAura(SPELL_HEROISM) ||
            unit->GetAuraEffect(SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELLFAMILY_SHAMAN, 0x0, 0x40, 0x0));
    }

    bool HasIllidanDrumsOfWarActive(Unit const* unit)
    {
        return unit && unit->HasAura(SPELL_DRUMS_OF_WAR);
    }

    bool HasAnyIllidanHeroismActive(Creature* source, std::vector<OwnedIllidanBotRef> const& bots)
    {
        for (OwnedIllidanBotRef const& botRef : bots)
        {
            if (HasIllidanHeroismActive(botRef.Bot) || HasIllidanHeroismActive(botRef.Owner))
                return true;
        }

        if (source && source->GetMap())
            for (MapReference const& ref : source->GetMap()->GetPlayers())
                if (Player* player = ref.GetSource())
                    if (IsValidIllidanSupportOwner(source, player, 180.0f) && HasIllidanHeroismActive(player))
                        return true;

        return false;
    }

    bool HasAnyIllidanDrumsOfWarActive(Creature* source, std::vector<OwnedIllidanBotRef> const& bots)
    {
        for (OwnedIllidanBotRef const& botRef : bots)
        {
            if (HasIllidanDrumsOfWarActive(botRef.Bot) || HasIllidanDrumsOfWarActive(botRef.Owner))
                return true;
        }

        if (source && source->GetMap())
            for (MapReference const& ref : source->GetMap()->GetPlayers())
                if (Player* player = ref.GetSource())
                    if (IsValidIllidanSupportOwner(source, player, 180.0f) && HasIllidanDrumsOfWarActive(player))
                        return true;

        return false;
    }

    uint8 GetActiveFlameOfAzzinothCount(Creature* source)
    {
        if (!source)
            return 0;

        uint8 count = 0;
        std::list<Creature*> flames;
        source->GetCreatureListWithEntryInGrid(flames, NPC_FLAME_OF_AZZINOTH, 180.0f);
        for (Creature* flame : flames)
            if (flame && flame->IsAlive() && flame->IsInCombat())
                ++count;

        return count;
    }

    void TriggerIllidanPhaseTwoBotBuffFallback(Creature* source)
    {
        if (!source || GetActiveFlameOfAzzinothCount(source) < 2)
            return;

        std::vector<OwnedIllidanBotRef> bots = GatherNearbyOwnedIllidanBots(source);
        if (!HasAnyIllidanHeroismActive(source, bots))
        {
            for (OwnedIllidanBotRef const& botRef : bots)
            {
                if (!botRef.Bot || !botRef.AI || botRef.AI->GetBotClass() != BOT_CLASS_SHAMAN)
                    continue;

                uint32 const spellId = (botRef.Bot->GetRaceMask() & sRaceMgr->GetAllianceRaceMask()) ? SPELL_HEROISM : SPELL_BLOODLUST;
                if (!sSpellMgr->GetSpellInfo(spellId) || botRef.Bot->HasAura(SPELL_EXHAUSTION) || botRef.Bot->HasAura(SPELL_SATED))
                    continue;

                botRef.Bot->InterruptNonMeleeSpells(true);
                botRef.Bot->CastSpell(botRef.Bot, spellId, true);
                break;
            }
        }

        if (!HasAnyIllidanDrumsOfWarActive(source, bots) && sSpellMgr->GetSpellInfo(SPELL_DRUMS_OF_WAR))
        {
            for (OwnedIllidanBotRef const& botRef : bots)
            {
                if (!botRef.Bot || !botRef.Bot->IsAlive())
                    continue;

                botRef.Bot->CastSpell(botRef.Bot, SPELL_DRUMS_OF_WAR, true);
                break;
            }
        }
    }

    void CastIllidanPhysicalDpsBotDemonSlayingElixir(Creature* source)
    {
        if (!source || !sSpellMgr->GetSpellInfo(SPELL_ELIXIR_OF_DEMONSLAYING))
            return;

        for (OwnedIllidanBotRef const& botRef : GatherNearbyOwnedIllidanBots(source))
        {
            if (!IsIllidanPhysicalDpsBot(botRef) || botRef.Bot->HasAura(SPELL_ELIXIR_OF_DEMONSLAYING))
                continue;

            botRef.Bot->CastSpell(botRef.Bot, SPELL_ELIXIR_OF_DEMONSLAYING, true);
        }
    }

    void DespawnNearbyIllidanCreature(Creature* source, uint32 entry)
    {
        if (!source)
            return;

        std::list<Creature*> creatures;
        source->GetCreatureListWithEntryInGrid(creatures, entry, ILLIDAN_SUMMON_CLEANUP_RANGE);
        for (Creature* creature : creatures)
            if (creature && creature->IsInWorld())
                creature->DespawnOrUnsummon();
    }

    void CleanupIllidanSummons(Creature* illidan, SummonList& summons)
    {
        summons.DespawnEntry(NPC_SHADOW_DEMON);
        summons.DespawnEntry(NPC_PARASITIC_SHADOWFIEND);

        DespawnNearbyIllidanCreature(illidan, NPC_SHADOW_DEMON);
        DespawnNearbyIllidanCreature(illidan, NPC_PARASITIC_SHADOWFIEND);
    }

    void RestoreIllidanFromDemonForm(Creature* illidan)
    {
        if (!illidan)
            return;

        illidan->SetReactState(REACT_AGGRESSIVE);
        illidan->SetCombatMovement(true);
        illidan->SetControlled(false, UNIT_STATE_ROOT);
        illidan->RemoveAurasDueToSpell(SPELL_DEMON_TRANSFORM_1);
        illidan->RemoveAurasDueToSpell(SPELL_DEMON_TRANSFORM_2);
        illidan->RemoveAurasDueToSpell(SPELL_DEMON_TRANSFORM_3);
        illidan->RemoveAurasDueToSpell(SPELL_DEMON_FORM);
        illidan->LoadEquipment(EQUIPMENT_GLAIVES, true);
    }

    Unit* ResolveParasiticShadowfiendTarget(Creature* shadowfiend, WorldObject* summoner)
    {
        if (Unit* summonerUnit = summoner ? summoner->ToUnit() : nullptr)
            if (IsPlayerOrOwnedNPCBot(summonerUnit) && !IsIllidanProtectedTankTarget(shadowfiend ? shadowfiend->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 160.0f) : nullptr, summonerUnit))
                return summonerUnit;

        if (!shadowfiend)
            return nullptr;

        std::list<Unit*> candidates;
        Bcore::AnyUnitInObjectRangeCheck check(shadowfiend, 18.0f);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(shadowfiend, candidates, check);
        Cell::VisitObjects(shadowfiend, searcher, 18.0f);

        Unit* best = nullptr;
        float bestDistance = std::numeric_limits<float>::max();
        for (Unit* candidate : candidates)
        {
            if (!IsPlayerOrOwnedNPCBot(candidate) || !candidate->IsAlive() || !HasParasiticShadowfiendAura(candidate) ||
                IsIllidanProtectedTankTarget(shadowfiend->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 160.0f), candidate))
                continue;

            float const distance = shadowfiend->GetExactDist(candidate);
            if (!best || distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }

        return best;
    }

    Player* GetParasiticShadowfiendBotOwner(Creature* bot)
    {
        bot_ai* ai = bot && bot->IsNPCBot() ? bot->GetBotAI() : nullptr;
        return ai && !ai->IAmFree() ? ai->GetBotOwner() : nullptr;
    }

    void AddParasiticShadowfiendObstacle(Creature* bot, std::vector<Unit*>& obstacles, Unit* unit)
    {
        if (!bot || !unit || unit == bot || !unit->IsAlive())
            return;

        if (!unit->IsPlayer() && !IsOwnedNPCBot(unit))
            return;

        if (!unit->IsInMap(bot) || !unit->InSamePhase(bot) || !unit->IsWithinDistInMap(bot, 90.0f))
            return;

        for (Unit* existing : obstacles)
            if (existing && existing->GetGUID() == unit->GetGUID())
                return;

        obstacles.push_back(unit);
    }

    void AddOwnedParasiticShadowfiendBotObstacles(Creature* bot, std::vector<Unit*>& obstacles, Player* owner)
    {
        if (!owner || !owner->HaveBot() || !owner->GetBotMgr())
            return;

        for (BotMap::value_type const& pair : *owner->GetBotMgr()->GetBotMap())
            AddParasiticShadowfiendObstacle(bot, obstacles, pair.second);
    }

    std::vector<Unit*> GatherParasiticShadowfiendObstacles(Creature* bot)
    {
        std::vector<Unit*> obstacles;
        Player* owner = GetParasiticShadowfiendBotOwner(bot);

        if (owner && owner->GetGroup())
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(owner->GetGroup()))
            {
                AddParasiticShadowfiendObstacle(bot, obstacles, member);
                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddOwnedParasiticShadowfiendBotObstacles(bot, obstacles, player);
            }
        }

        AddParasiticShadowfiendObstacle(bot, obstacles, owner);
        AddOwnedParasiticShadowfiendBotObstacles(bot, obstacles, owner);
        return obstacles;
    }

    float GetParasiticShadowfiendPositionScore(Position const& candidate, std::vector<Unit*> const& obstacles)
    {
        if (obstacles.empty())
            return 1000.0f;

        float score = std::numeric_limits<float>::max();
        for (Unit* unit : obstacles)
        {
            if (!unit)
                continue;

            float const distance = unit->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
            score = std::min(score, distance);
        }

        return score;
    }

    bool TryGetParasiticShadowfiendSafePosition(Creature* bot, Position& destination)
    {
        if (!bot)
            return false;

        Creature* illidan = bot->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 160.0f);
        Position const& anchor = illidan ? illidan->GetHomePosition() : bot->GetPosition();
        std::vector<Unit*> obstacles = GatherParasiticShadowfiendObstacles(bot);

        float bestScore = -1.0f;
        Position bestPosition;
        bool found = false;

        float const baseAngle = std::atan2(bot->GetPositionY() - anchor.GetPositionY(), bot->GetPositionX() - anchor.GetPositionX());
        for (float radius = PARASITIC_SHADOWFIEND_SAFE_MIN_RADIUS; radius <= PARASITIC_SHADOWFIEND_SAFE_MAX_RADIUS; radius += PARASITIC_SHADOWFIEND_SAFE_STEP)
        {
            for (uint8 step = 0; step < 16; ++step)
            {
                float const angle = baseAngle + (2.0f * ILLIDAN_PI * float(step) / 16.0f);
                float x = anchor.GetPositionX() + std::cos(angle) * radius;
                float y = anchor.GetPositionY() + std::sin(angle) * radius;
                float z = anchor.GetPositionZ();

                if (!bot->CanFly())
                    bot->UpdateAllowedPositionZ(x, y, z);

                if (!bot->IsWithinLOS(x, y, z))
                    continue;

                Position candidate;
                candidate.Relocate(x, y, z, illidan ? bot->GetAngle(illidan) : bot->GetOrientation());

                if (bot->GetExactDist(candidate) > 32.0f)
                    continue;

                float score = GetParasiticShadowfiendPositionScore(candidate, obstacles);
                score -= bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY()) * 0.04f;

                if (!found || score > bestScore)
                {
                    found = true;
                    bestScore = score;
                    bestPosition = candidate;
                }
            }
        }

        if (!found)
            return false;

        destination = bestPosition;
        return true;
    }

    void MoveParasiticShadowfiendBot(Creature* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai || ai->IAmFree())
            return;

        Position destination;
        if (!TryGetParasiticShadowfiendSafePosition(bot, destination))
            return;

        bot->InterruptNonMeleeSpells(false);
        bot->AttackStop();
        bot->BotStopMovement();
        ai->RemoveBotCommandState(BOT_COMMAND_FOLLOW | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);
        ai->SetBotCommandState(BOT_COMMAND_INACTION);
        ai->SetBotCommandState(BOT_COMMAND_STAY);
        ai->BotMovement(BOT_MOVE_POINT, &destination, nullptr, true);
    }

    void AnnounceParasiticShadowfiendBot(Creature* bot, int32 auraDurationMs)
    {
        if (!bot)
            return;

        DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_PARASITIC_SHADOWFIEND, DBM_BLACK_TEMPLE_MODULE_FOLDER, DBM_ILLIDAN_MODULE_ID, "Parasitic Shadowfiends", 1);

        ObjectGuid const botGuid = bot->GetGUID();
        uint32 const firstCountdownMs = auraDurationMs > 3000 ? uint32(auraDurationMs - 3000) : 1;
        for (uint8 i = 0; i < 3; ++i)
        {
            uint8 const count = 3 - i;
            bot->m_Events.AddEventAtOffset([bot, botGuid, count]()
            {
                if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid || !bot->IsAlive() || !HasParasiticShadowfiendAura(bot))
                    return;

                DBMFTABotCallouts::AnnounceCustomForModule(bot, SPELL_PARASITIC_SHADOWFIEND, DBM_BLACK_TEMPLE_MODULE_FOLDER, DBM_ILLIDAN_MODULE_ID,
                    "Shadowfiends on me in " + std::to_string(count) + "!", 1);
            }, Milliseconds(firstCountdownMs + i * 1000));
        }
    }

    bool BeginParasiticShadowfiendBotHandling(Unit* /*target*/, int32 /*auraDurationMs*/, uint32& originalCommandState)
    {
        originalCommandState = 0;
        return false;
    }

    void EndParasiticShadowfiendBotHandling(Unit* target, bool handledBot, uint32 originalCommandState)
    {
        if (!handledBot)
            return;

        Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target);
        if (!bot || !bot->IsNPCBot())
            return;

        ObjectGuid const botGuid = bot->GetGUID();
        bot->m_Events.AddEventAtOffset([bot, botGuid, originalCommandState]()
        {
            if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid || !bot->IsNPCBot())
                return;

            if (HasParasiticShadowfiendAura(bot))
                return;

            bot_ai* ai = bot->GetBotAI();
            if (!ai)
                return;

            if (!(originalCommandState & BOT_COMMAND_INACTION))
                ai->RemoveBotCommandState(BOT_COMMAND_INACTION);

            if (!((originalCommandState) & BOT_COMMAND_MASK_NOCAST_ANY))
                ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY);
            else
                ai->SetBotCommandState(originalCommandState & BOT_COMMAND_MASK_NOCAST_ANY);

            ai->RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);

            if (bot->IsAlive() && !ai->IAmFree())
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }, 2500ms);
    }
}

class ChargeTargetSelector
{
public:
    ChargeTargetSelector() { }

    bool operator()(Unit* unit) const
    {
        return unit && unit->IsPlayer()
            && unit->GetDistance2d(BladesPositions[0].GetPositionX(), BladesPositions[0].GetPositionY()) > 25.0f
            && unit->GetDistance2d(BladesPositions[1].GetPositionX(), BladesPositions[1].GetPositionY()) > 25.0f;
    }
};

struct boss_illidan_stormrage : public BossAI
{
    boss_illidan_stormrage(Creature* creature) : BossAI(creature, DATA_ILLIDAN_STORMRAGE), _canTalk(true), _dying(false), _inCutscene(false), beamPosId(0) { }

    void Reset() override
    {
        BossAI::Reset();
        CleanupIllidanSummons(me, summons);
        me->m_Events.CancelEventGroup(GROUP_BERSERK);
        me->m_Events.CancelEventGroup(GROUP_PHASE_FLYING);
        me->m_Events.CancelEventGroup(GROUP_DEMON_FORM);
        _canTalk = true;
        _dying = false;
        _inCutscene = false;
        beamPosId = urand(0, MAX_EYE_BEAM_POS - 1);
        me->ReplaceAllUnitFlags(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
        me->SetDisableGravity(false);
        me->SetHover(false);
        DoCastSelf(SPELL_DUAL_WIELD, true);
        me->LoadEquipment(EQUIPMENT_GLAIVES, true);
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        me->SetSheath(SHEATH_STATE_UNARMED);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetCombatMovement(true);

        ScheduleHealthCheckEvent(90, [&] {
            // Call for minions
            Talk(SAY_ILLIDAN_MINION);
            if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
                akama->AI()->DoAction(ACTION_AKAMA_MINIONS);
        });
        ScheduleHealthCheckEvent(65, [&] {
            // Phase 2
            scheduler.CancelAll();
            DoAction(ACTION_ILLIDAN_LIFTOFF);
        });
        ScheduleHealthCheckEvent(30, [&] {
            // Maiev Spawn Scene
            scheduler.CancelAll();
            if (me->HasAura(SPELL_DEMON_FORM))
                DoAction(ACTION_ILLIDAN_DEMON_TRANSFORM_BACK);
            me->m_Events.CancelEventGroup(GROUP_DEMON_FORM);
            DoAction(ACTION_SHADOW_PRISON);
        });
    }

    void DoAction(int32 param) override
    {
        scheduler.CancelAll();
        switch (param)
        {
            case ACTION_START_EVENT:
            {
                me->SetStandState(UNIT_STAND_STATE_STAND);

                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_DUPLICITY);
                }, 2210ms);
                me->m_Events.AddEventAtOffset([&] {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                }, 5670ms); // 3460ms
                me->m_Events.AddEventAtOffset([&] {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                }, 8100ms); // 2430ms
                me->m_Events.AddEventAtOffset([&] {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                }, 11750ms); // 3650ms
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_UNCONVINCED);
                }, 26950ms); // 15200ms
                me->m_Events.AddEventAtOffset([&] {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                }, 29980ms); // 3030ms
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_PREPARED);
                }, 39710ms); // 9730ms
                me->m_Events.AddEventAtOffset([&] {
                    me->SetSheath(SHEATH_STATE_MELEE);
                }, 40920ms); // 1210ms
                me->m_Events.AddEventAtOffset([&] {
                    me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
                    me->SetInCombatWithZone();
                }, 43370ms); // 2450ms
            }
            break;
            case ACTION_ILLIDAN_LIFTOFF:
            {
                me->SetReactState(REACT_PASSIVE);
                DoStopAttack();
                me->GetMotionMaster()->Clear();
                me->StopMovingOnCurrentPos();
                DoCastSelf(SPELL_CLEAR_ALL_DEBUFFS, true);
                me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
                me->SetDisableGravity(true);
                me->SetHover(true);
                me->SetOrientation(2.837961435317993164f);
                me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_TAKEOFF);
                    }, 3640ms);
                me->m_Events.AddEventAtOffset([&] {
                    me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_TAKEOFF, illidanTakeoffPoint);
                    }, 7290ms); // 3650ms
            }
            break;
            case ACTION_SHADOW_PRISON:
            {
                scheduler.CancelAll();
                _inCutscene = true;
                DoCastSelf(SPELL_SHADOW_PRISON, true);
                DoStopAttack();
                me->SetReactState(REACT_PASSIVE);
                me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

                me->m_Events.AddEventAtOffset([&] {
                    DoCastSelf(SPELL_EMOTE_TALK_QUESTION);
                }, 1200ms);
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_SHADOW_PRISON);
                }, 1430ms); // 230ms
                me->m_Events.AddEventAtOffset([&] {
                    DoCastSelf(SPELL_SUMMON_MAIEV);
                }, 10940ms); // 9510ms
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_CONFRONT_MAIEV);
                }, 19490ms); // 8550ms
                me->m_Events.AddEventAtOffset([&] {
                    me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                }, 23080ms); // 3590ms
                me->m_Events.AddEventAtOffset([&] {
                    me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    me->SetReactState(REACT_AGGRESSIVE);
                    ScheduleAbilities(PHASE_MAIEV);
                    _inCutscene = false;
                }, 32830ms); // 9750ms
            }
            break;
            case ACTION_ILLIDAN_DEMON_TRANSFORM:
            {
                scheduler.CancelAll();
                me->SetReactState(REACT_PASSIVE);
                DoStopAttack();
                me->SetControlled(true, UNIT_STATE_ROOT);
                me->SetCombatMovement(false);
                DoResetThreatList();
                DoCastSelf(SPELL_DEMON_TRANSFORM_1, true);

                Talk(SAY_ILLIDAN_MORPH, 2630ms);

                me->m_Events.AddEventAtOffset([&] {
                    // me->SetControlled(false, UNIT_STATE_ROOT);
                    me->SetReactState(REACT_AGGRESSIVE);
                    SeedIllidanDemonWarlockTankThreat(me);
                    ScheduleAbilities(PHASE_DEMON);
                }, 12230ms, GROUP_DEMON_FORM);
            }
            break;
            case ACTION_ILLIDAN_DEMON_TRANSFORM_BACK:
            {
                scheduler.CancelAll();
                RestoreIllidanFromDemonForm(me);
            }
            break;
            case ACTION_ILLIDAN_CAGED:
            {
                scheduler.CancelAll();
                me->RemoveAurasDueToSpell(SPELL_FRENZY);
                DoCastSelf(SPELL_CAGE_TRAP_PERIODIC, true);

                me->m_Events.AddEventAtOffset([&] {
                    DoAction(ACTION_ILLIDAN_DEMON_TRANSFORM);
                }, 15s, GROUP_DEMON_FORM);
            }
            break;
            case ACTION_ILLIDAN_DIE:
            {
                me->m_Events.CancelEventGroup(GROUP_DEMON_FORM);
                scheduler.CancelAll();

                if (me->HasAura(SPELL_DEMON_FORM))
                    DoAction(ACTION_ILLIDAN_DEMON_TRANSFORM_BACK);

                _dying = true;
                if (Creature* maiev = summons.GetCreatureWithEntry(NPC_MAIEV_SHADOWSONG))
                    maiev->AI()->DoAction(ACTION_MAIEV_ENDING);
                if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
                    akama->AI()->DoAction(ACTION_AKAMA_ENDING);
                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
                        akama->AI()->DoAction(ACTION_AKAMA_ENDING);
                }, 1s);

                me->SetControlled(true, UNIT_STATE_ROOT);
                DoCastSelf(SPELL_CLEAR_ALL_DEBUFFS, true);
                DoCastSelf(SPELL_DEATH, true);
                me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);

                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_ILLIDAN_DEFEATED);
                }, 7490ms);
                me->m_Events.AddEventAtOffset([&] {
                    Unit::Kill(nullptr, me);
                }, 25530ms); // 18040ms
            }
            break;
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE)
        {
            switch (id)
            {
                case POINT_ILLIDAN_TAKEOFF:
                {
                    me->SetFacingTo(me->GetAngle(&roomCenter));
                    Talk(SAY_ILLIDAN_SUMMONFLAMES);

                    me->m_Events.AddEventAtOffset([&] {
                        DoCastSelf(SPELL_THROW_GLAIVE2);
                    }, 1210ms);
                    me->m_Events.AddEventAtOffset([&] {
                        DoCastSelf(SPELL_THROW_GLAIVE);
                        me->LoadEquipment(EQUIPMENT_UNARMED, true);
                    }, 2430ms); // 1220ms
                    me->m_Events.AddEventAtOffset([&] {
                        ScheduleAbilities(PHASE_FLYING);
                    }, 3090ms); // 660ms
                }
                break;
                case POINT_ILLIDAN_HOVER:
                {
                    me->SetControlled(true, UNIT_STATE_ROOT);
                    scheduler.CancelAll();
                    ScheduleAbilities(PHASE_FLYING);
                }
                break;
                case POINT_ILLIDAN_LAND:
                {
                    EntryCheckPredicate pred(NPC_BLADE_OF_AZZINOTH);
                    summons.DoAction(ACTION_RETURN_BLADE, pred);

                    me->m_Events.AddEventAtOffset([&] {
                        me->LoadEquipment(EQUIPMENT_GLAIVES);
                    }, 1215ms);
                    me->m_Events.AddEventAtOffset([&] {
                        me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
                        me->SetDisableGravity(false);
                        me->SetHover(false);
                    }, 3665ms); // 2450ms
                    me->m_Events.AddEventAtOffset([&] {
                        DoResetThreatList();
                        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                        me->SetReactState(REACT_AGGRESSIVE);
                        me->SetControlled(false, UNIT_STATE_ROOT);
                        ScheduleAbilities(PHASE_LANDING);
                    }, 6095ms); // 2430ms
                    me->m_Events.AddEventAtOffset([&] {
                        DoCastSelf(SPELL_HIT_CHANCE, true);
                    }, 7305ms); // 1210ms
                }
                break;
            }
        }
    }

    void ScheduleAbilities(uint8 phase)
    {
        switch (phase)
        {
            case PHASE_INITIAL:
            {
                ScheduleTimedEvent(25s, 30s, [&] {
                    DoCastVictim(SPELL_FLAME_CRASH);
                }, 26s, 35s);

                ScheduleTimedEvent(32s, [&] {
                    DoCastVictim(SPELL_DRAW_SOUL);
                }, 32s);

                ScheduleTimedEvent(25s, 30s, [&] {
                    DoCastParasiticShadowfiendTarget();
                }, 25s, 30s);

                // Custom from SunwellCore?
                ScheduleTimedEvent(30s, 60s, [&] {
                    Talk(SAY_ILLIDAN_TAUNT);
                }, 30s, 60s);
            }
            break;
            case PHASE_FLYING:
            {
                me->SetFacingTo(me->GetAngle(&roomCenter));

                scheduler.Schedule(0s, [this](TaskContext context)
                {
                    // Do not repeat if interrupted (Eye Beam is cast)
                    if (DoCastRandomPlayerOrBotTarget(SPELL_FIREBALL, 0U, 50000.f) == SPELL_CAST_OK)
                        context.Repeat(2400ms);
                }).Schedule(25s, 45s, [this](TaskContext /*context*/)
                {
                    // Eye Blast
                    me->InterruptNonMeleeSpells(false);
                    Talk(SAY_ILLIDAN_EYE_BLAST);
                    me->SummonCreature(NPC_ILLIDAN_DB_TARGET, eyeBeamPos[beamPosId], TEMPSUMMON_TIMED_DESPAWN, 30000);
                    if (Creature* trigger = summons.GetCreatureWithEntry(NPC_ILLIDAN_DB_TARGET))
                        trigger->GetMotionMaster()->MovePoint(0, eyeBeamPos[beamPosId + MAX_EYE_BEAM_POS], FORCED_MOVEMENT_NONE, 0.f, false, true);

                    // Reposition
                    me->m_Events.AddEventAtOffset([&] {
                        scheduler.CancelAll();
                        me->InterruptNonMeleeSpells(false);
                        me->SetControlled(false, UNIT_STATE_ROOT);
                        CycleBeamPos(beamPosId);
                        me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_HOVER, airHoverPos[beamPosId], FORCED_MOVEMENT_NONE, 0.f, false, true);
                    }, 20s, GROUP_PHASE_FLYING);
                });
                // Check for Phase Transition
                scheduler.Schedule(5s, [this](TaskContext context) {
                    if (!SelectTargetFromPlayerList(150.0f))
                    {
                        EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
                        return;
                    }

                    summons.RemoveNotExisting();
                    if (!summons.HasEntry(NPC_FLAME_OF_AZZINOTH))
                    {
                        me->InterruptNonMeleeSpells(false);
                        me->SetControlled(false, UNIT_STATE_ROOT);
                        me->m_Events.CancelEventGroup(GROUP_PHASE_FLYING);
                        me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_LAND, illidanLand);
                        scheduler.CancelAll();
                    }
                    else
                        context.Repeat(3s);
                });
            }
            break;
            case PHASE_LANDING:
            {
                scheduler.CancelAll();

                ScheduleTimedEvent(25s, 30s, [&] {
                    DoCastVictim(SPELL_FLAME_CRASH);
                }, 26s, 35s);

                ScheduleTimedEvent(32s, [&] {
                    DoCastVictim(SPELL_DRAW_SOUL);
                }, 32s);

                ScheduleTimedEvent(25s, 30s, [&] {
                    DoCastParasiticShadowfiendTarget();
                }, 25s, 30s);

                ScheduleTimedEvent(25s, [&] {
                    DoCastSelf(SPELL_AGONIZING_FLAMES);
                }, 24s);

                ScheduleTimedEvent(60s, [&] {
                    if (!_inCutscene)
                        DoAction(ACTION_ILLIDAN_DEMON_TRANSFORM);
                }, 60s);
            }
            break;
            case PHASE_DEMON:
            {
                scheduler.CancelAll();
                SeedIllidanDemonWarlockTankThreat(me);

                ScheduleTimedEvent(30s, [&] {
                    DoCastSelf(SPELL_SUMMON_SHADOW_DEMON, true);
                }, 100s);

                ScheduleTimedEvent(1s, 2500ms, [&] {
                    DoCastVictim(SPELL_SHADOW_BLAST);
                }, 2500ms);

                ScheduleTimedEvent(7s, [&] {
                    DoCastSelf(SPELL_FLAME_BURST);
                }, 19500ms);

                scheduler.Schedule(5s, [this](TaskContext context) {
                    if (!SelectTargetFromPlayerList(150.0f))
                    {
                        EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
                        return;
                    }

                    context.Repeat(3s);
                });

                me->m_Events.AddEventAtOffset([&] {
                    DoAction(ACTION_ILLIDAN_DEMON_TRANSFORM_BACK);
                    if (summons.GetCreatureWithEntry(NPC_MAIEV_SHADOWSONG))
                        ScheduleAbilities(PHASE_MAIEV);
                    else
                        ScheduleAbilities(PHASE_LANDING);
                    DoResetThreatList();
                }, 60s);
            }
            break;
            case PHASE_MAIEV:
            {
                ScheduleAbilities(PHASE_LANDING);

                ScheduleTimedEvent(40s, [&] {
                    Talk(SAY_ILLIDAN_FRENZY);
                    DoCastSelf(SPELL_FRENZY, true);
                }, 100s);

                ScheduleTimedEvent(30s, [&] {
                    if (Creature* maiev = summons.GetCreatureWithEntry(NPC_MAIEV_SHADOWSONG))
                        DoCast(maiev, SPELL_CAGE_TRAP, true);
                }, 45s);
            }
            break;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        CastIllidanPhysicalDpsBotDemonSlayingElixir(me);
        SeedIllidanInitialTankBotThreat(me);
        ScheduleAbilities(PHASE_INITIAL);
        if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
            akama->AI()->AttackStart(me);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BERSERK, true);
        }, 25min, GROUP_BERSERK);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (_inCutscene)
            return;

        scheduler.CancelAll();
        me->m_Events.CancelEventGroup(GROUP_BERSERK);
        me->m_Events.CancelEventGroup(GROUP_PHASE_FLYING);
        me->m_Events.CancelEventGroup(GROUP_DEMON_FORM);
        CleanupIllidanSummons(me, summons);
        RestoreIllidanFromDemonForm(me);
        DoStopAttack();

        if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
            akama->DespawnOnEvade();

        BossAI::EnterEvadeMode(why);
        me->DespawnOnEvade();
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_ILLIDAN_DB_TARGET)
            DoCast(summon, SPELL_EYE_BLAST);
        else if (summon->GetEntry() == NPC_MAIEV_SHADOWSONG)
        {
            me->SetTarget(summon->GetGUID());
            me->SetFacingToObject(summon);
        }
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (_canTalk)
        {
            Talk(SAY_ILLIDAN_KILL);
            _canTalk = false;

            me->m_Events.AddEventAtOffset([&] {
                _canTalk = true;
            }, 6s); // 3590ms
        }
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask) override
    {
        if (damage >= me->GetHealth())
        {
            damage = me->GetHealth() - 1;

            if (!_dying)
                DoAction(ACTION_ILLIDAN_DIE);
        }

        if (!_dying)
            BossAI::DamageTaken(attacker, damage, damagetype, damageSchoolMask);
    }

    void JustDied(Unit* killer) override
    {
        CleanupIllidanSummons(me, summons);
        summons.clear();
        BossAI::JustDied(killer);
    }

    bool CanAIAttack(Unit const* target) const override
    {
        return target->GetEntry() != NPC_AKAMA_ILLIDAN && target->GetEntry() != NPC_MAIEV_SHADOWSONG;
    }

private:
    Unit* SelectRandomPlayerOrBotTarget(uint32 threatTablePosition = 0, float dist = 0.0f, bool withTank = true)
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, threatTablePosition, [this, dist, withTank](Unit* target)
        {
            if (!target || !target->IsInWorld() || !target->IsAlive() || !target->IsInCombat())
                return false;

            if (target->GetMap() != me->GetMap() || !target->InSamePhase(me))
                return false;

            if (target->HasUnitState(UNIT_STATE_ISOLATED))
                return false;

            if (!withTank && IsIllidanProtectedTankTarget(me, target))
                return false;

            if (dist > 0.0f && !me->IsWithinCombatRange(target, dist))
                return false;

            if (!me->IsValidAttackTarget(target))
                return false;

            return IsPlayerOrOwnedNPCBot(target);
        });

        return targets.empty() ? nullptr : targets.front();
    }

    SpellCastResult DoCastRandomPlayerOrBotTarget(uint32 spellId, uint32 threatTablePosition = 0, float dist = 0.0f, bool triggered = false, bool withTank = true)
    {
        if (Unit* target = SelectRandomPlayerOrBotTarget(threatTablePosition, dist, withTank))
            return DoCast(target, spellId, triggered);

        SpellCastResult result = DoCastRandomTarget(spellId, threatTablePosition, dist, true, triggered, withTank);
        if (result != SPELL_FAILED_BAD_TARGETS)
            return result;

        if (Unit* victim = me->GetVictim())
            return DoCast(victim, spellId, triggered);

        return result;
    }

    SpellCastResult DoCastParasiticShadowfiendTarget()
    {
        std::list<Unit*> targets;
        SelectTargetList(targets, 1, SelectTargetMethod::Random, 0, [this](Unit* target)
        {
            if (!target || !target->IsInWorld() || !target->IsAlive() || !target->IsInCombat())
                return false;

            if (target->GetMap() != me->GetMap() || !target->InSamePhase(me))
                return false;

            if (target->HasUnitState(UNIT_STATE_ISOLATED) || HasParasiticShadowfiendAura(target))
                return false;

            if (IsIllidanProtectedTankTarget(me, target))
                return false;

            if (!me->IsWithinCombatRange(target, 100.0f) || !me->IsValidAttackTarget(target))
                return false;

            return IsPlayerOrOwnedNPCBot(target);
        });

        if (!targets.empty())
            return DoCast(targets.front(), SPELL_PARASITIC_SHADOWFIEND);

        return SPELL_FAILED_BAD_TARGETS;
    }

    bool _canTalk;
    bool _dying;
    bool _inCutscene;
    uint8 beamPosId;

    void CycleBeamPos(uint8 &beamPosId)
    {
        uint8 newPos;
        do {
            newPos = urand(0, MAX_EYE_BEAM_POS - 1);
        } while (newPos == beamPosId);
        beamPosId = newPos;
    }
};

enum Akama
{
    POINT_FACE_ILLIDAN            = 1,
    POINT_ILLIDAN_DEFEATED_1      = 2,
    POINT_ILLIDAN_DEFEATED_2      = 3,

    SPELL_AKAMA_DOOR_OPEN         = 41268,
    SPELL_AKAMA_DOOR_FAIL         = 41271,
    SPELL_DEATHSWORN_DOOR_OPEN    = 41269,
    SPELL_ARCANE_EXPLOSION_VIS    = 35426,
    SPELL_HEALING_POTION          = 40535,
    SPELL_CHAIN_LIGHTNING         = 40536,
    SPELL_REDUCED_THREAT          = 41000,
    SPELL_AKAMA_TELEPORT          = 41077,
    SPELL_AKAMA_DESPAWN           = 41242,

    NPC_ILLIDAN_DOOR_TRIGGER      = 23412,
    NPC_SPIRIT_OF_OLUM            = 23411,
    NPC_SPIRIT_OF_UDALO           = 23410,
    NPC_ILLIDARI_ELITE            = 23226,

    PATH_AKAMA_ILLIDARI_COUNCIL_1 = 230891,
    PATH_AKAMA_ILLIDARI_COUNCIL_2 = 230892,
    PATH_AKAMA_ILLIDARI_COUNCIL_3 = 230893,
    PATH_AKAMA_MINIONS            = 230894,

    SAY_UDALO                     = 0,
    SAY_OLUM                      = 0,

    SAY_AKAMA_DOOR                = 0,
    SAY_AKAMA_ALONE               = 1,
    SAY_AKAMA_SALUTE              = 2,
    SAY_AKAMA_BETRAYER            = 3,
    SAY_AKAMA_FREE                = 4,
    SAY_AKAMA_TIME_HAS_COME       = 5,
    SAY_AKAMA_MINIONS             = 6,
    SAY_AKAMA_LIGHT               = 7,
    SAY_AKAMA_COUNCIL_1           = 8,
    SAY_AKAMA_COUNCIL_2           = 9
};

Position AkamaIllidariCouncilTeleport = { 609.772f, 308.456f, 271.826f, 6.1972566f };
Position SpiritUdaloPos = { 751.4565f, 311.01065f, 312.18997f, 0.f };
Position SpiritOlumPos = { 751.6437f, 297.2233f, 312.20825f, 6.038839340209960937f };
Position FaceIllidan = { 745.225f, 304.946f, 352.98593f };
Position IllidanDefeated = { 753.04553f, 369.30273f, 353.1165f };
Position IllidariMinionPos[10] =
{
{ 750.0472f , 282.32742f, 309.4353f , 3.071779489517211914f },
{ 747.0576f , 326.42682f, 309.06885f, 0.0f                  },
{ 754.0332f , 325.81363f, 310.31952f, 2.914699792861938476f },
{ 745.25525f, 322.15738f, 310.45963f, 6.038839340209960937f },
{ 748.8422f , 288.06195f, 310.9782f , 1.884955525398254394f },
{ 745.3237f , 283.986f  , 309.2765f , 0.628318548202514648f },
{ 743.9686f , 289.64468f, 311.18066f, 6.056292533874511718f },
{ 751.08777f, 327.6505f , 309.45758f, 6.17846536636352539f  },
{ 750.03217f, 323.60635f, 310.27567f, 5.497786998748779296f },
{ 753.8425f , 286.56195f, 310.9353f , 1.029744267463684082f }
};

struct npc_akama_illidan : public ScriptedAI
{
    npc_akama_illidan(Creature* creature) : ScriptedAI(creature), summons(me)
    {
        instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        scheduler.CancelAll();
        me->m_Events.KillAllEvents(false);
        me->SetReactState(REACT_AGGRESSIVE);
        if (instance->GetBossState(DATA_ILLIDAN_STORMRAGE) == DONE)
            me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        else
            me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        me->setActive(false);
        summons.DespawnAll();
        DoCastSelf(SPELL_REDUCED_THREAT, true);
        me->SetControlled(false, UNIT_STATE_ROOT);
    }

    void sGossipSelect(Player* player, uint32 /*sender*/, uint32  /*action*/) override
    {
        CloseGossipMenuFor(player);
        me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);
        me->setActive(true);

        if (instance->GetBossState(DATA_AKAMA_ILLIDAN) != DONE)
        {
            me->GetMotionMaster()->MoveWaypoint(PATH_AKAMA_ILLIDARI_COUNCIL_2, false);
        }
        else
        {
            me->SetSheath(SHEATH_STATE_UNARMED);
            me->GetMotionMaster()->MovePoint(POINT_FACE_ILLIDAN, FaceIllidan);
        }
    }

    void DoAction(int32 param) override
    {
        switch (param)
        {
            case ACTION_ILLIDARI_COUNCIL_DONE:
            {
                me->NearTeleportTo(AkamaIllidariCouncilTeleport);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                me->GetMotionMaster()->MoveWaypoint(PATH_AKAMA_ILLIDARI_COUNCIL_1, false);
            }
            break;
            case ACTION_AKAMA_MINIONS:
            {
                EnterEvadeMode(EVADE_REASON_OTHER);
            }
            break;
            case ACTION_AKAMA_ENDING:
            {
                if (me->IsEngaged())
                {
                    summons.DespawnAll();
                    me->SetReactState(REACT_PASSIVE);
                    me->SetControlled(false, UNIT_STATE_ROOT);
                }
                else
                {
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_DEFEATED_1, IllidanDefeated);
                }
            }
            break;
            case ACTION_AKAMA_MAIEV_DESPAWN:
            {
                Talk(SAY_AKAMA_LIGHT);
                me->m_Events.AddEventAtOffset([&] {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                }, 3490ms);
                me->m_Events.AddEventAtOffset([&] {
                    me->SetRespawnDelay(WEEK);
                    DoCastSelf(SPELL_AKAMA_DESPAWN);
                }, 8340ms); // 4850ms
                me->m_Events.AddEventAtOffset([&] {
                    me->DespawnOnEvade();
                }, 8740ms);
            }
            break;
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE)
        {
            switch (id)
            {
                case POINT_FACE_ILLIDAN:
                {
                    if (Creature* illidan = instance->GetCreature(DATA_ILLIDAN_STORMRAGE))
                    {
                        me->SetFacingToObject(illidan);
                        illidan->AI()->DoAction(ACTION_START_EVENT);
                        me->SetHomePosition(me->GetPosition());
                    }
                    me->m_Events.AddEventAtOffset([&] {
                        Talk(SAY_AKAMA_FREE);
                    }, 15400ms);
                    me->m_Events.AddEventAtOffset([&] {
                        me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                    }, 19440ms); // 4040ms
                    me->m_Events.AddEventAtOffset([&] {
                        me->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                    }, 23080ms); // 3640ms
                    me->m_Events.AddEventAtOffset([&] {
                        Talk(SAY_AKAMA_TIME_HAS_COME);
                    }, 33840ms); // 10760ms
                    me->m_Events.AddEventAtOffset([&] {
                        me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                        me->SetSheath(SHEATH_STATE_MELEE);
                    }, 35210ms); // 1370ms
                    me->m_Events.AddEventAtOffset([&] {
                        me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                    }, 37640ms); // 2430ms
                }
                break;
                case POINT_ILLIDAN_DEFEATED_1:
                {
                    me->SetWalk(true);
                    if (Creature* illidan = instance->GetCreature(DATA_ILLIDAN_STORMRAGE))
                    {
                        float x, y, z;
                        me->GetNearPoint(illidan, x, y, z, 15.f, 0, me->GetAngle(illidan));
                        me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_DEFEATED_2, x, y, z);  // Maiev starts Akama's Ending scene
                    }
                }
                break;
            }
        }
        else if (type == WAYPOINT_MOTION_TYPE)
        {
            if (me->GetCurrentWaypointID() == PATH_AKAMA_MINIONS)
                if (id == 3)
                    DoCastSelf(SPELL_AKAMA_TELEPORT);
        }
    }

    void PathEndReached(uint32 pathId) override
    {
        switch (pathId)
        {
            // Talk to Open Door
            case PATH_AKAMA_ILLIDARI_COUNCIL_1:
            {
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_AKAMA_COUNCIL_1);
                }, 200ms);

                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_AKAMA_COUNCIL_2);
                    me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                }, 8s); // 7800ms
            }
            break;
            // Reached Door
            case PATH_AKAMA_ILLIDARI_COUNCIL_2:
            {
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_AKAMA_DOOR);
                }, 4600ms);
                me->m_Events.AddEventAtOffset([&] {
                    DoCastSelf(SPELL_AKAMA_DOOR_FAIL);
                }, 8120ms); // 3520ms
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_AKAMA_ALONE);
                }, 17860ms); // 9740ms
                me->m_Events.AddEventAtOffset([&] {
                    me->SummonCreature(NPC_SPIRIT_OF_UDALO, SpiritUdaloPos, TEMPSUMMON_TIMED_DESPAWN, 60000);
                    me->SummonCreature(NPC_SPIRIT_OF_OLUM, SpiritOlumPos, TEMPSUMMON_TIMED_DESPAWN, 60000);
                }, 23930ms); // 6070ms
                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* udalo = me->FindNearestCreature(NPC_SPIRIT_OF_UDALO, 15.0f))
                        udalo->AI()->Talk(SAY_UDALO);
                }, 25190ms); // 1260ms
                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* olum = me->FindNearestCreature(NPC_SPIRIT_OF_OLUM, 15.0f))
                        olum->AI()->Talk(SAY_OLUM);
                }, 31370ms); // 6180ms
                me->m_Events.AddEventAtOffset([&] {
                    DoCastSelf(SPELL_AKAMA_DOOR_OPEN);
                    if (Creature* udalo = me->FindNearestCreature(NPC_SPIRIT_OF_UDALO, 15.0f))
                        udalo->AI()->DoCastSelf(SPELL_DEATHSWORN_DOOR_OPEN);
                    if (Creature* olum = me->FindNearestCreature(NPC_SPIRIT_OF_OLUM, 15.0f))
                        olum->AI()->DoCastSelf(SPELL_DEATHSWORN_DOOR_OPEN);
                }, 39710ms); // 8340ms
                me->m_Events.AddEventAtOffset([&] {
                    if (Creature* door = me->FindNearestCreature(NPC_ILLIDAN_DOOR_TRIGGER, 15.0f))
                        door->AI()->DoCastSelf(SPELL_ARCANE_EXPLOSION_VIS);
                }, 50660ms); // 10950ms
                me->m_Events.AddEventAtOffset([&] {
                    me->InterruptNonMeleeSpells(false);
                    if (Creature* udalo = me->FindNearestCreature(NPC_SPIRIT_OF_UDALO, 15.0f))
                        udalo->InterruptNonMeleeSpells(false);
                    if (Creature* olum = me->FindNearestCreature(NPC_SPIRIT_OF_OLUM, 15.0f))
                        olum->InterruptNonMeleeSpells(false);
                    instance->SetBossState(DATA_AKAMA_ILLIDAN, NOT_STARTED);
                    instance->SetBossState(DATA_AKAMA_ILLIDAN, DONE);
                }, 50680ms); // 20ms
                me->m_Events.AddEventAtOffset([&] {
                    Talk(SAY_AKAMA_SALUTE);
                }, 56955ms); // 6275ms
                me->m_Events.AddEventAtOffset([&] {
                    me->GetMotionMaster()->MoveWaypoint(PATH_AKAMA_ILLIDARI_COUNCIL_3, false);
                }, 64030ms); // 7075ms
            }
            break;
            // Talk to Initiate Fight
            case PATH_AKAMA_ILLIDARI_COUNCIL_3:
            {
                Talk(SAY_AKAMA_BETRAYER);
                me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            }
            break;
            case PATH_AKAMA_MINIONS:
            {
                me->SetControlled(true, UNIT_STATE_ROOT);
                me->SetReactState(REACT_AGGRESSIVE);

                for (int i = 0; i < 10; ++i)
                    me->SummonCreature(NPC_ILLIDARI_ELITE, IllidariMinionPos[i], TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1200);
            }
            break;
        }
    }

    void JustReachedHome() override
    {
        // Minions Event
        Creature* illidan = instance->GetCreature(DATA_ILLIDAN_STORMRAGE);
        if (illidan && instance->GetBossState(DATA_ILLIDAN_STORMRAGE) == IN_PROGRESS && !illidan->HasAura(SPELL_DEATH))
        {
            me->SetReactState(REACT_PASSIVE);
            me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);

            me->m_Events.AddEventAtOffset([&] {
                Talk(SAY_AKAMA_MINIONS);
            }, 6700ms);
            me->m_Events.AddEventAtOffset([&] {
                me->HandleEmoteCommand(EMOTE_ONESHOT_EXCLAMATION);
            }, 9530ms); // 2830ms
            me->m_Events.AddEventAtOffset([&] {
                me->GetMotionMaster()->MoveWaypoint(PATH_AKAMA_MINIONS, false);
            }, 14400ms); // 4870ms
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        summon->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);
        if (summon->GetEntry() == NPC_ILLIDARI_ELITE)
        {
            me->AddThreat(summon, 1000000.0f);
            summon->AddThreat(me, 1000000.0f);
            summon->AI()->AttackStart(me);
            AttackStart(summon);
        }
    }

    void SummonedCreatureDies(Creature* summon, Unit*) override
    {
        summons.Despawn(summon);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->GetEntry() == NPC_ILLIDARI_ELITE)
            me->SummonCreature(NPC_ILLIDARI_ELITE, IllidariMinionPos[urand(0, 9)], TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1200);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ScheduleTimedEvent(12s, 18s, [&] {
            DoCastVictim(SPELL_CHAIN_LIGHTNING);
        }, 16s, 24s);

        ScheduleTimedEvent(5s, 10s, [&] {
            if (me->HealthBelowPct(20))
                DoCastSelf(SPELL_HEALING_POTION);
        }, 5s, 10s);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    InstanceScript* instance;
    SummonList summons;
};

enum Maiev
{
    SPELL_MAIEV_DOWN              = 40409,
    SPELL_THROW_DAGGER            = 41152,
    SPELL_SHADOW_STRIKE           = 40685,
    SPELL_CAGE_TRAP_SUMMON        = 40694,
    SPELL_TELEPORT_VISUAL         = 41236,

    SAY_MAIEV_SHADOWSONG_TAUNT    = 0,
    SAY_MAIEV_SHADOWSONG_APPEAR   = 1,
    SAY_MAIEV_SHADOWSONG_JUSTICE  = 2,
    SAY_MAIEV_SHADOWSONG_TRAP     = 3,
    SAY_MAIEV_SHADOWSONG_DOWN     = 4,
    SAY_MAIEV_SHADOWSONG_FINISHED = 5,
    SAY_MAIEV_SHADOWSONG_OUTRO    = 6,
    SAY_MAIEV_SHADOWSONG_FAREWELL = 7
};

struct npc_maiev_illidan : public ScriptedAI
{
    npc_maiev_illidan(Creature* creature) : ScriptedAI(creature)
    {
        instance = creature->GetInstanceScript();
    }

    bool _outroActive{ false };

    void Reset() override
    {
        if (_outroActive)
            return;
        scheduler.CancelAll();
        me->m_Events.KillAllEvents(false);
    }

    void JustExitedCombat() override
    {
        EngagementOver();
        if (_outroActive)
            return;
        EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_MAIEV_ENDING)
        {
            _outroActive = true;
            scheduler.CancelAll();
            me->SetReactState(REACT_PASSIVE);
            DoStopAttack();
            me->CombatStop();
            me->StopMovingOnCurrentPos();
            me->RemoveAurasDueToSpell(SPELL_MAIEV_DOWN);
            me->SetWalk(true);
            if (Creature* illidan = instance->GetCreature(DATA_ILLIDAN_STORMRAGE))
            {
                if (!me->IsWithinDist(illidan, 15.f))
                {
                    float x, y, z;
                    me->GetNearPoint(illidan, x, y, z, 15.f, 0, me->GetAngle(illidan));
                    me->GetMotionMaster()->MovePoint(POINT_ILLIDAN_DEFEATED_2, x, y, z);
                }
            }

            me->m_Events.AddEventAtOffset([&] {
                Talk(SAY_MAIEV_SHADOWSONG_FINISHED);
            }, 1420ms);
            me->m_Events.AddEventAtOffset([&] {
                Talk(SAY_MAIEV_SHADOWSONG_OUTRO);
            }, 28550ms); // 27130ms
            me->m_Events.AddEventAtOffset([&] {
                Talk(SAY_MAIEV_SHADOWSONG_FAREWELL);
            }, 39510ms); // 10960ms
            me->m_Events.AddEventAtOffset([&] {
                DoCastSelf(SPELL_TELEPORT_VISUAL);
            }, 41700ms); // 2190ms
            me->m_Events.AddEventAtOffset([&] {
                if (Creature* akama = instance->GetCreature(DATA_AKAMA_ILLIDAN))
                    akama->AI()->DoAction(ACTION_AKAMA_MAIEV_DESPAWN);
                me->DespawnOnEvade();
            }, 43320ms); // 1620ms
        }
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetFacingToObject(summoner);

        me->m_Events.AddEventAtOffset([&] {
            Talk(SAY_MAIEV_SHADOWSONG_APPEAR);
        }, 25ms);
        me->m_Events.AddEventAtOffset([&] {
            me->HandleEmoteCommand(EMOTE_ONESHOT_EXCLAMATION);
        }, 2415ms); // 2390ms
        me->m_Events.AddEventAtOffset([&] {
            Talk(SAY_MAIEV_SHADOWSONG_JUSTICE);
        }, 14815ms); // 12400ms
        me->m_Events.AddEventAtOffset([&] {
            me->HandleEmoteCommand(EMOTE_ONESHOT_YES);
        }, 17015ms); // 2200ms
        me->m_Events.AddEventAtOffset([&] {
            me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
        }, 19445ms); // 2430ms
        me->m_Events.AddEventAtOffset([&] {
            me->SetReactState(REACT_AGGRESSIVE);
        }, 21920ms); // 2475ms
        me->m_Events.AddEventAtOffset([&] {
            if (Creature* illidan = me->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 15.0f))
                AttackStart(illidan);
        }, 23095ms); // 1175ms
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ScheduleTimedEvent(7s, [&] {
            DoCastVictim(SPELL_THROW_DAGGER);
        }, 30s);

        ScheduleTimedEvent(22s, [&] {
            DoCastVictim(SPELL_SHADOW_STRIKE);
            if (roll_chance_i(50))
                Talk(SAY_MAIEV_SHADOWSONG_TAUNT);
        }, 30s);

        ScheduleTimedEvent(1200ms, [&] {
            if (me->HealthBelowPct(20))
            {
                Talk(SAY_MAIEV_SHADOWSONG_DOWN);
                DoCastSelf(SPELL_MAIEV_DOWN);
            }
        }, 1200ms);
    }

    void SpellHit(Unit* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_CAGE_TRAP)
        {
            DoCastSelf(SPELL_CAGE_TRAP_SUMMON, true);
            Talk(SAY_MAIEV_SHADOWSONG_TRAP);
        }
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!me->HasUnitState(UNIT_STATE_STUNNED))
            scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    InstanceScript* instance;
};

struct npc_parasitic_shadowfiend : public ScriptedAI
{
    npc_parasitic_shadowfiend(Creature* creature) : ScriptedAI(creature) { }

    bool CanAIAttack(Unit const* who) const override
    {
        if (!who || !IsPlayerOrOwnedNPCBot(who))
            return false;

        if (IsIllidanProtectedTankTarget(me->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 160.0f), who))
            return false;

        if (_preferredTargetGuid && who->GetGUID() == _preferredTargetGuid)
            return true;

        return !HasParasiticShadowfiendAura(who);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->DespawnOrUnsummon();
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Unit* target = ResolveParasiticShadowfiendTarget(me, summoner))
            _preferredTargetGuid = target->GetGUID();

        // Simulate blizz-like AI delay to avoid extreme overpopulation of adds
        me->SetReactState(REACT_DEFENSIVE);

        scheduler.Schedule(2400ms, [this](TaskContext context)
        {
            me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* target = ObjectAccessor::GetUnit(*me, _preferredTargetGuid))
            {
                me->SetInCombatWith(target);
                target->SetInCombatWith(me);
                me->GetThreatMgr().AddThreat(target, 50000.0f, nullptr, true, true);
                AttackStart(target);
            }

            me->SetInCombatWithZone();
            context.Repeat();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    ObjectGuid _preferredTargetGuid;
};

enum WarbladeTear
{
    SOUND_WARBLADE_SPAWN = 11689,

    SPELL_SUMMON_TEAR    = 39855,
    SPELL_TEAR_CHANNEL   = 39857,

    MODEL_INVISIBLE      = 11686
};

struct npc_blade_of_azzinoth : public ScriptedAI
{
    npc_blade_of_azzinoth(Creature* creature) : ScriptedAI(creature)
    {
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->PlayRadiusSound(SOUND_WARBLADE_SPAWN, 150.f);
        me->SetInCombatWithZone();

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_SUMMON_TEAR);
        }, 2700ms);
    }

    void JustSummoned(Creature* summon) override
    {
        DoCast(summon, SPELL_TEAR_CHANNEL, true);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_RETURN_BLADE)
        {
            if (Creature* illidan = me->FindNearestCreature(NPC_ILLIDAN_STORMRAGE, 100.0f))
                DoCast(illidan, SPELL_GLAIVE_RETURNS, true);

            me->m_Events.AddEventAtOffset([&] {
                me->SetDisplayId(MODEL_INVISIBLE);
            }, 10ms);
            me->m_Events.AddEventAtOffset([&] {
                me->DespawnOrUnsummon();
            }, 2020ms);
        }
    }
};

enum FlameAzzinoth
{
    NPC_BLAZE           = 23259,

    SPELL_BLAZE_EFFECT  = 40610,
    SPELL_FLAME_BLAST   = 40631,
    SPELL_CHARGE        = 42003,
    SPELL_UNCAGED_WRATH = 39869,
    SPELL_BLAZE         = 40637
};

struct npc_flame_of_azzinoth : public ScriptedAI
{
    npc_flame_of_azzinoth(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        // Flame is set to be Illidan's summon, so we check for nearest blade
        if (Creature* _blade = me->FindNearestCreature(NPC_BLADE_OF_AZZINOTH, 15.0f))
            _bladeGUID = _blade->GetGUID();

        me->SetCorpseDelay(2);
        me->SetReactState(REACT_DEFENSIVE);
        me->m_Events.AddEventAtOffset([&] {
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetInCombatWithZone();
            SeedFlameOfAzzinothTankBotThreat(me);
            TriggerIllidanPhaseTwoBotBuffFallback(me);
        }, 2020ms);

        for (uint8 i = 1; i <= 3; ++i)
        {
            me->m_Events.AddEventAtOffset([this]()
            {
                if (!me || !me->IsAlive() || !me->IsInCombat())
                    return;

                SeedFlameOfAzzinothTankBotThreat(me);
                TriggerIllidanPhaseTwoBotBuffFallback(me);
            }, Milliseconds(3000 + i * 2000));
        }
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_BLAZE)
        {
            summon->SetReactState(REACT_PASSIVE);
            summon->AI()->DoCastSelf(SPELL_BLAZE_EFFECT, true);
            summon->SetCombatMovement(false);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ScheduleTimedEvent(10s, [&] {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, ChargeTargetSelector()))
                DoCast(target, SPELL_CHARGE);
        }, 5s, 20s);

        ScheduleTimedEvent(10s, 20s, [&] {
            DoCastVictim(SPELL_FLAME_BLAST);

            me->m_Events.AddEventAtOffset([&] {
                if (Unit* victim = me->GetVictim())
                    victim->CastSpell(victim, SPELL_BLAZE, true);
            }, 1s);
        }, 15s, 20s);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    ObjectGuid _bladeGUID;
};

class spell_illidan_draw_soul : public SpellScript
{
    PrepareSpellScript(spell_illidan_draw_soul);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRAW_SOUL_HEAL });
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(GetCaster(), SPELL_DRAW_SOUL_HEAL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_draw_soul::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_illidan_parasitic_shadowfiend_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_parasitic_shadowfiend_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_PARASITIC_SHADOWFIENDS });
    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        _handledBot = BeginParasiticShadowfiendBotHandling(GetTarget(), GetAura() ? GetAura()->GetDuration() : 3000, _originalCommandState);
    }

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!GetTarget()->HasAura(SPELL_SHADOW_PRISON) && GetTarget()->GetInstanceScript() && GetTarget()->GetInstanceScript()->IsEncounterInProgress())
            GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_PARASITIC_SHADOWFIENDS, true);

        EndParasiticShadowfiendBotHandling(GetTarget(), _handledBot, _originalCommandState);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_illidan_parasitic_shadowfiend_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_illidan_parasitic_shadowfiend_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }

private:
    bool _handledBot = false;
    uint32 _originalCommandState = 0;
};

class spell_illidan_parasitic_shadowfiend_trigger : public SpellScript
{
    PrepareSpellScript(spell_illidan_parasitic_shadowfiend_trigger);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Creature* target = GetHitCreature())
            target->DespawnOrUnsummon(1ms);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_parasitic_shadowfiend_trigger::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_illidan_parasitic_shadowfiend_trigger_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_parasitic_shadowfiend_trigger_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_PARASITIC_SHADOWFIENDS });
    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        _handledBot = BeginParasiticShadowfiendBotHandling(GetTarget(), GetAura() ? GetAura()->GetDuration() : 3000, _originalCommandState);
    }

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!GetTarget()->HasAura(SPELL_SHADOW_PRISON) && GetTarget()->GetInstanceScript() && GetTarget()->GetInstanceScript()->IsEncounterInProgress())
            GetTarget()->CastSpell(GetTarget(), SPELL_SUMMON_PARASITIC_SHADOWFIENDS, true);

        EndParasiticShadowfiendBotHandling(GetTarget(), _handledBot, _originalCommandState);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_illidan_parasitic_shadowfiend_trigger_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_illidan_parasitic_shadowfiend_trigger_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }

private:
    bool _handledBot = false;
    uint32 _originalCommandState = 0;
};

class spell_illidan_glaive_throw : public SpellScript
{
    PrepareSpellScript(spell_illidan_glaive_throw);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_GLAIVE });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_SUMMON_GLAIVE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_glaive_throw::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_illidan_tear_of_azzinoth_summon_channel_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_tear_of_azzinoth_summon_channel_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_UNCAGED_WRATH, SPELL_CHARGE });
    }

    void OnPeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();
        if (Unit* caster = GetCaster())
        {
            if (GetTarget()->GetDistance2d(caster) > GetFlameOfAzzinothTetherDistance())
            {
                SetDuration(0);
                GetTarget()->CastSpell(GetTarget(), SPELL_UNCAGED_WRATH, true);
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_illidan_tear_of_azzinoth_summon_channel_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_illidan_shadow_prison : public SpellScript
{
    PrepareSpellScript(spell_illidan_shadow_prison);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(PlayerOrPetCheck());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_illidan_shadow_prison::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_illidan_shadow_prison_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_shadow_prison_aura);

    void HandleOnEffectApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_SCHOOL, aurEff->GetMiscValue(), true);
    }

    void HandleOnEffectRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_SCHOOL, aurEff->GetMiscValue(), false);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_illidan_shadow_prison_aura::HandleOnEffectApply, EFFECT_1, SPELL_AURA_DAMAGE_IMMUNITY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_illidan_shadow_prison_aura::HandleOnEffectRemove, EFFECT_1, SPELL_AURA_DAMAGE_IMMUNITY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_illidan_demon_transform1_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_demon_transform1_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEMON_TRANSFORM_2 });
    }

    bool Load() override
    {
        return GetUnitOwner()->IsCreature();
    }

    void OnPeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();
        SetDuration(0);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_DEMON_TRANSFORM_2, true);
        GetUnitOwner()->ToCreature()->LoadEquipment(0, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_illidan_demon_transform1_aura::OnPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_illidan_demon_transform2_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_demon_transform2_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEMON_FORM, SPELL_DEMON_TRANSFORM_3 });
    }

    bool Load() override
    {
        return GetUnitOwner()->IsCreature();
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        if (aurEff->GetTickNumber() == 1)
        {
            if (GetUnitOwner()->GetDisplayId() == GetUnitOwner()->GetNativeDisplayId())
                GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_DEMON_FORM, true);
            else
                GetUnitOwner()->RemoveAurasDueToSpell(SPELL_DEMON_FORM);
        }
        else if (aurEff->GetTickNumber() == 2)
        {
            SetDuration(0);
            GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_DEMON_TRANSFORM_3, true);
            if (Aura* aura = GetUnitOwner()->GetAura(SPELL_DEMON_TRANSFORM_3))
                aura->SetDuration(4500);

            if (!GetUnitOwner()->HasAura(SPELL_DEMON_FORM))
                GetUnitOwner()->ToCreature()->LoadEquipment(1, true);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_illidan_demon_transform2_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_illidan_shadow_blast : public SpellScript
{
    PrepareSpellScript(spell_illidan_shadow_blast);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        if (IsIllidanRangedDpsBot(GetHitUnit()))
            SetHitDamage(int32(float(GetHitDamage()) * GetIllidanRangedDpsBotHazardDamageMultiplier()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_shadow_blast::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_illidan_flame_burst : public SpellScript
{
    PrepareSpellScript(spell_illidan_flame_burst);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FLAME_BURST_EFFECT });
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_FLAME_BURST_EFFECT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_flame_burst::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_illidan_found_target : public SpellScript
{
    PrepareSpellScript(spell_illidan_found_target);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CONSUME_SOUL, SPELL_FIND_TARGET });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Unit* target = GetHitUnit())
            if (GetCaster()->GetDistance(target) < 2.0f)
            {
                GetCaster()->CastSpell(target, SPELL_CONSUME_SOUL, true);
                GetCaster()->CastSpell(GetCaster(), SPELL_FIND_TARGET, true);
            }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_found_target::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_illidan_cage_trap : public SpellScript
{
    PrepareSpellScript(spell_illidan_cage_trap);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CAGE_TRAP });
    }

    bool Load() override
    {
        return GetCaster()->IsCreature();
    }

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (Creature* target = GetHitCreature())
            if (GetCaster()->GetExactDist2d(target) < 4.0f)
            {
                target->AI()->DoAction(ACTION_ILLIDAN_CAGED);
                GetCaster()->ToCreature()->DespawnOrUnsummon(1ms);
                if (GameObject* gobject = GetCaster()->FindNearestGameObject(GO_CAGE_TRAP, 10.0f))
                    gobject->SetLootState(GO_JUST_DEACTIVATED);
            }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_illidan_cage_trap::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_illidan_cage_trap_stun_aura : public AuraScript
{
    PrepareAuraScript(spell_illidan_cage_trap_stun_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CAGED_DEBUFF, SPELL_CAGED_SUMMON1, SPELL_CAGED_SUMMON1+1, SPELL_CAGED_SUMMON1+2, SPELL_CAGED_SUMMON1+3, SPELL_CAGED_SUMMON1+4, SPELL_CAGED_SUMMON1+5, SPELL_CAGED_SUMMON1+6, SPELL_CAGED_SUMMON8 });
    }

    void OnPeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();
        SetDuration(0);

        for (uint32 i = SPELL_CAGED_SUMMON1; i <= SPELL_CAGED_SUMMON8; ++i)
            GetTarget()->CastSpell(GetTarget(), i, true);
        GetTarget()->CastSpell(GetTarget(), SPELL_CAGED_DEBUFF, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_illidan_cage_trap_stun_aura::OnPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_illidan()
{
    RegisterBlackTempleCreatureAI(boss_illidan_stormrage);
    RegisterBlackTempleCreatureAI(npc_maiev_illidan);
    RegisterBlackTempleCreatureAI(npc_akama_illidan);
    RegisterBlackTempleCreatureAI(npc_parasitic_shadowfiend);
    RegisterBlackTempleCreatureAI(npc_blade_of_azzinoth);
    RegisterBlackTempleCreatureAI(npc_flame_of_azzinoth);
    RegisterSpellScript(spell_illidan_draw_soul);
    RegisterSpellScript(spell_illidan_parasitic_shadowfiend_aura);
    RegisterSpellAndAuraScriptPair(spell_illidan_parasitic_shadowfiend_trigger, spell_illidan_parasitic_shadowfiend_trigger_aura);
    RegisterSpellScript(spell_illidan_glaive_throw);
    RegisterSpellScript(spell_illidan_tear_of_azzinoth_summon_channel_aura);
    RegisterSpellAndAuraScriptPair(spell_illidan_shadow_prison, spell_illidan_shadow_prison_aura);
    RegisterSpellScript(spell_illidan_demon_transform1_aura);
    RegisterSpellScript(spell_illidan_demon_transform2_aura);
    RegisterSpellScript(spell_illidan_shadow_blast);
    RegisterSpellScript(spell_illidan_flame_burst);
    RegisterSpellScript(spell_illidan_found_target);
    RegisterSpellScript(spell_illidan_cage_trap);
    RegisterSpellScript(spell_illidan_cage_trap_stun_aura);
}
