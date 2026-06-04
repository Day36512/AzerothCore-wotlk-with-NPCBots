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

#include "Containers.h"
#include "CreatureScript.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "botmgr.h"
#include "mechanar.h"

#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceDebuffOnMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
    void AnnounceCustomForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& message, uint32 cooldownMs = 5000);
}

enum Says
{
    SAY_AGGRO                       = 0,
    SAY_DOMINATION                  = 1,
    SAY_SUMMON                      = 2,
    SAY_ENRAGE                      = 3,
    SAY_SLAY                        = 4,
    SAY_DEATH                       = 5,
    SAY_APPEAR                      = 6
};

enum Spells
{
    SPELL_ARCANE_EXPLOSION          = 15453,
    SPELL_DISGRUNTLED_ANGER         = 35289,
    SPELL_ARCANE_TORRENT            = 36022,
    SPELL_MANA_TAP                  = 36021,
    SPELL_POLYMORPH_RANK_1          = 118,
    SPELL_DOMINATION                = 35280,
    SPELL_FRENZY                    = 36992,
    SPELL_SUICIDE                   = 35301,
    SPELL_ETHEREAL_TELEPORT         = 34427,
    SPELL_GREATER_INVISIBILITY      = 34426,
    SPELL_SUMMON_NETHER_WRAITH_1    = 35285,
    SPELL_SUMMON_NETHER_WRAITH_2    = 35286,
    SPELL_SUMMON_NETHER_WRAITH_3    = 35287,
    SPELL_SUMMON_NETHER_WRAITH_4    = 35288,
};

enum Misc
{
    ACTION_BRIDGE_MOB_DEATH = 1, // Used by SAI
    EQUIPMENT_NORMAL        = 1,
    EQUIPMENT_FRENZY        = 2,
};

enum PathaleonMindControlReplacementTargetMode : uint8
{
    PATHALEON_MC_REPLACEMENT_TARGET_TANK          = 0,
    PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP  = 1,
    PATHALEON_MC_REPLACEMENT_TARGET_SELF          = 2,
    PATHALEON_MC_REPLACEMENT_TARGET_EVERYONE      = 3
};

struct PathaleonMindControlReplacementConfig
{
    bool Enabled = false;
    uint32 SpellId = SPELL_POLYMORPH_RANK_1;
    PathaleonMindControlReplacementTargetMode TargetMode = PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP;
};

static constexpr float PATHALEON_DOMINATION_RANGE = 50.0f;

static bool IsPathaleonValidGroupTarget(Creature const* source, Unit* unit, float range)
{
    if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    if (!unit->IsPlayer() && !unit->IsNPCBot())
        return false;

    if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    if (!unit->IsInMap(source) || !unit->InSamePhase(source))
        return false;

    if (!source->IsWithinDistInMap(unit, range))
        return false;

    if (!source->CanSeeOrDetect(unit) || !source->IsWithinLOSInMap(unit))
        return false;

    return true;
}

static void AddPathaleonTarget(Creature const* source, std::vector<Unit*>& targets, GuidSet& seen, Unit* unit, float range)
{
    if (!IsPathaleonValidGroupTarget(source, unit, range))
        return;

    if (!seen.insert(unit->GetGUID()).second)
        return;

    targets.push_back(unit);
}

static Player* GetPathaleonOwnerPlayer(Unit* unit)
{
    if (!unit)
        return nullptr;

    if (Player* player = unit->ToPlayer())
        return player;

    Creature* creature = unit->ToCreature();
    return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
}

static Group const* GetPathaleonGroup(Unit* unit)
{
    if (!unit)
        return nullptr;

    if (Player* player = unit->ToPlayer())
        return player->GetGroup();

    Creature* creature = unit->ToCreature();
    return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
}

static void AddPathaleonOwnedBots(Creature const* source, std::vector<Unit*>& targets, GuidSet& seen, Player* player, float range)
{
    if (!player || !player->HaveBot())
        return;

    BotMgr* botMgr = player->GetBotMgr();
    if (!botMgr)
        return;

    if (BotMap const* botMap = botMgr->GetBotMap())
        for (BotMap::value_type const& pair : *botMap)
            AddPathaleonTarget(source, targets, seen, pair.second, range);
}

static Unit* GetPathaleonTank(Creature* source)
{
    if (!source)
        return nullptr;

    if (Unit* tank = source->GetThreatMgr().GetCurrentVictim())
        return tank;

    return source->GetVictim();
}

static void AddPathaleonThreatTargets(Creature* source, std::vector<Unit*>& targets, GuidSet& seen, float range)
{
    if (!source)
        return;

    for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
    {
        if (!ref || !ref->IsAvailable())
            continue;

        AddPathaleonTarget(source, targets, seen, ref->GetVictim(), range);
    }
}

static std::vector<Unit*> GatherPathaleonGroupTargets(Creature* source, float range)
{
    std::vector<Unit*> targets;
    GuidSet seen;

    if (!source)
        return targets;

    Unit* seed = GetPathaleonTank(source);
    if (!seed)
        seed = source->GetVictim();

    if (Group const* group = GetPathaleonGroup(seed))
    {
        for (Unit* member : BotMgr::GetAllGroupMembers(group))
        {
            AddPathaleonTarget(source, targets, seen, member, range);

            if (Player* player = member ? member->ToPlayer() : nullptr)
                AddPathaleonOwnedBots(source, targets, seen, player, range);
        }
    }
    else
    {
        AddPathaleonTarget(source, targets, seen, seed, range);

        if (Player* player = GetPathaleonOwnerPlayer(seed))
        {
            AddPathaleonTarget(source, targets, seen, player, range);
            AddPathaleonOwnedBots(source, targets, seen, player, range);
        }
    }

    if (targets.empty())
        AddPathaleonThreatTargets(source, targets, seen, range);

    return targets;
}

static PathaleonMindControlReplacementTargetMode GetPathaleonMindControlReplacementTargetMode()
{
    switch (sConfigMgr->GetOption<uint32>("Pathaleon.MindControlReplacement.TargetMode", PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP))
    {
        case PATHALEON_MC_REPLACEMENT_TARGET_TANK:
            return PATHALEON_MC_REPLACEMENT_TARGET_TANK;
        case PATHALEON_MC_REPLACEMENT_TARGET_SELF:
            return PATHALEON_MC_REPLACEMENT_TARGET_SELF;
        case PATHALEON_MC_REPLACEMENT_TARGET_EVERYONE:
            return PATHALEON_MC_REPLACEMENT_TARGET_EVERYONE;
        case PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP:
        default:
            return PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP;
    }
}

static PathaleonMindControlReplacementConfig GetPathaleonMindControlReplacementConfig()
{
    PathaleonMindControlReplacementConfig config;
    config.Enabled = sConfigMgr->GetOption<bool>("Pathaleon.MindControlReplacement.Enabled", false);
    config.SpellId = sConfigMgr->GetOption<uint32>("Pathaleon.MindControlReplacement.SpellId", SPELL_POLYMORPH_RANK_1);

    if (!config.SpellId || !sSpellMgr->GetSpellInfo(config.SpellId))
        config.SpellId = SPELL_POLYMORPH_RANK_1;

    config.TargetMode = GetPathaleonMindControlReplacementTargetMode();
    return config;
}

struct boss_pathaleon_the_calculator : public BossAI
{
    boss_pathaleon_the_calculator(Creature* creature) : BossAI(creature, DATA_PATHALEON_THE_CALCULATOR) { }

    bool _isEnraged;

    void Reset() override
    {
        _Reset();
        _isEnraged = false;
        me->LoadEquipment(EQUIPMENT_NORMAL);

        if (instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT) < 4)
        {
            DoCastSelf(SPELL_GREATER_INVISIBILITY);
        }
    }

    bool CanAIAttack(Unit const* /*target*/) const override
    {
        return instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT) >= 4;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        ScheduleHealthCheckEvent(20, [&]()
        {
            DoCastSelf(SPELL_SUICIDE, true);
            DoCastSelf(SPELL_FRENZY, true);
            Talk(SAY_ENRAGE);
            _isEnraged = true;
            me->LoadEquipment(EQUIPMENT_FRENZY);
        });

        scheduler.Schedule(10s, 16s, [this](TaskContext context)
        {
            if (!_isEnraged)
            {
                for (uint8 i = 0; i < DUNGEON_MODE(3, 4); ++i)
                    me->CastSpell(me, SPELL_SUMMON_NETHER_WRAITH_1 + i, true);

                Talk(SAY_SUMMON);
            }
            context.Repeat(45s, 50s);
        }).Schedule(12s, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, PowerUsersSelector(me, POWER_MANA, 40.0f, false)))
            {
                DoCast(target, SPELL_MANA_TAP);
            }
            context.Repeat(18s);
        }).Schedule(16s, [this](TaskContext context)
        {
            me->RemoveAurasDueToSpell(SPELL_MANA_TAP);
            me->ModifyPower(POWER_MANA, 5000);
            DoCastSelf(SPELL_ARCANE_TORRENT);
            context.Repeat(15s);
        }).Schedule(10s, 16s, [this](TaskContext context)
        {
            if (CastDominationOrReplacement() == SPELL_CAST_OK)
            {
                Talk(SAY_DOMINATION);
            }
            context.Repeat(27s, 40s);
        }).Schedule(25s, [this](TaskContext context)
        {
            DoCast(SPELL_DISGRUNTLED_ANGER);
            context.Repeat(40s, 90s);
        });

        if (IsHeroic())
        {
            scheduler.Schedule(8s, [this](TaskContext context)
            {
                DoCastAOE(SPELL_ARCANE_EXPLOSION);
                context.Repeat(12s);
            });
        }

        Talk(SAY_AGGRO);
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_BRIDGE_MOB_DEATH)
        {
            uint8 mobCount = instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT);
            instance->StorePersistentData(DATA_BRIDGE_MOB_DEATH_COUNT, ++mobCount);

            if (mobCount >= 4)
            {
                DoCastSelf(SPELL_ETHEREAL_TELEPORT);
                Talk(SAY_APPEAR);

                scheduler.Schedule(2s, [this](TaskContext)
                {
                    me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                }).Schedule(25s, [this](TaskContext)
                {
                    DoZoneInCombat();
                });
            }
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
        {
            Talk(SAY_SLAY);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

private:
    void AnnounceDominationTarget(Unit* target, uint32 spellId)
    {
        Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target);
        if (!bot)
            return;

        if (spellId == SPELL_DOMINATION)
        {
            DBMFTABotCallouts::AnnounceCustomForModule(bot, spellId, "DBM-Party-BC", "565", "Domination on me!", DBMFTABotCallouts::GetCooldownMs());
            return;
        }

        DBMFTABotCallouts::AnnounceDebuffOnMeForModule(bot, spellId, "DBM-Party-BC", "565", "Polymorph", DBMFTABotCallouts::GetCooldownMs());
    }

    Unit* SelectDefaultMindControlTarget()
    {
        Unit* tank = GetPathaleonTank(me);
        std::vector<Unit*> candidates;

        for (Unit* unit : GatherPathaleonGroupTargets(me, PATHALEON_DOMINATION_RANGE))
        {
            if (!unit->IsPlayer() || unit == tank)
                continue;

            candidates.push_back(unit);
        }

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    Unit* SelectFallbackPolymorphBotTarget()
    {
        Unit* tank = GetPathaleonTank(me);
        std::vector<Unit*> candidates;

        for (Unit* unit : GatherPathaleonGroupTargets(me, PATHALEON_DOMINATION_RANGE))
        {
            if (!unit->IsNPCBot() || unit == tank)
                continue;

            candidates.push_back(unit);
        }

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    Unit* SelectRandomReplacementGroupTarget()
    {
        std::vector<Unit*> candidates = GatherPathaleonGroupTargets(me, PATHALEON_DOMINATION_RANGE);
        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    SpellCastResult CastDefaultDomination()
    {
        if (Unit* target = SelectDefaultMindControlTarget())
        {
            SpellCastResult result = me->CastSpell(target, SPELL_DOMINATION, false);
            if (result == SPELL_CAST_OK)
                AnnounceDominationTarget(target, SPELL_DOMINATION);
            return result;
        }

        if (Unit* target = SelectFallbackPolymorphBotTarget())
        {
            SpellCastResult result = me->CastSpell(target, SPELL_POLYMORPH_RANK_1, false);
            if (result == SPELL_CAST_OK)
                AnnounceDominationTarget(target, SPELL_POLYMORPH_RANK_1);
            return result;
        }

        return SPELL_FAILED_BAD_TARGETS;
    }

    SpellCastResult CastReplacementOnEveryone(uint32 spellId)
    {
        SpellCastResult firstFailure = SPELL_FAILED_BAD_TARGETS;
        bool castStarted = false;

        for (Unit* target : GatherPathaleonGroupTargets(me, PATHALEON_DOMINATION_RANGE))
        {
            SpellCastResult result = me->CastSpell(target, spellId, false);
            if (result == SPELL_CAST_OK)
            {
                castStarted = true;
                AnnounceDominationTarget(target, spellId);
                continue;
            }

            if (firstFailure == SPELL_FAILED_BAD_TARGETS)
                firstFailure = result;
        }

        return castStarted ? SPELL_CAST_OK : firstFailure;
    }

    SpellCastResult CastConfiguredMindControlReplacement(PathaleonMindControlReplacementConfig const& config)
    {
        switch (config.TargetMode)
        {
            case PATHALEON_MC_REPLACEMENT_TARGET_TANK:
                if (Unit* target = GetPathaleonTank(me))
                {
                    SpellCastResult result = me->CastSpell(target, config.SpellId, false);
                    if (result == SPELL_CAST_OK)
                        AnnounceDominationTarget(target, config.SpellId);
                    return result;
                }
                return SPELL_FAILED_BAD_TARGETS;
            case PATHALEON_MC_REPLACEMENT_TARGET_SELF:
                return me->CastSpell(me, config.SpellId, false);
            case PATHALEON_MC_REPLACEMENT_TARGET_EVERYONE:
                return CastReplacementOnEveryone(config.SpellId);
            case PATHALEON_MC_REPLACEMENT_TARGET_RANDOM_GROUP:
            default:
                if (Unit* target = SelectRandomReplacementGroupTarget())
                {
                    SpellCastResult result = me->CastSpell(target, config.SpellId, false);
                    if (result == SPELL_CAST_OK)
                        AnnounceDominationTarget(target, config.SpellId);
                    return result;
                }
                return SPELL_FAILED_BAD_TARGETS;
        }
    }

    SpellCastResult CastDominationOrReplacement()
    {
        PathaleonMindControlReplacementConfig config = GetPathaleonMindControlReplacementConfig();
        if (config.Enabled)
            return CastConfiguredMindControlReplacement(config);

        return CastDefaultDomination();
    }
};

void AddSC_boss_pathaleon_the_calculator()
{
    RegisterMechanarCreatureAI(boss_pathaleon_the_calculator);
}
