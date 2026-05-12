/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * Blackheart the Inciter rewrite:
 * - Removed Incite Chaos / Incite Chaos trigger phase.
 * - Replaced it with Maddening Command.
 * - Maddening Command summons hostile reflections of every valid player/NPCBot encounter target.
 * - NPCBot-aware target collection adapted from the Exarch Maladaar Stolen Soul pattern.
 *
 * Reflection behavior:
 * - Reuses creature entry 18441, the original Stolen Soul.
 * - This version intentionally overrides the old npc_stolen_soul AI.
 * - Each reflection copies its source target's display, level, class, and attacks that source.
 * - Each class reflection now has the original class ability plus 3 additional class-flavored abilities.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Map.h"
#include "Containers.h"
#include "botmgr.h"
#include "bot_ai.h"
#include "shadow_labyrinth.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;

enum Text
{
    SAY_INTRO = 0,
    SAY_AGGRO = 1,
    SAY_SLAY = 2,
    SAY_HELP = 3,
    SAY_DEATH = 4
};

enum Spells
{
    // Blackheart
    SPELL_CHARGE = 33709,
    SPELL_WAR_STOMP = 33707,
    SPELL_LAUGHTER = 33722,

    // Maddening Command / Reflection visuals
    SPELL_MADDENING_COMMAND_MARK = 32346,
    SPELL_MADDENED_REFLECTION_VISUAL = 32395,

    // Warrior Reflection
    SPELL_WARRIOR_MORTAL_STRIKE = 37335,
    SPELL_WARRIOR_THUNDER_CLAP = 15588,
    SPELL_WARRIOR_SUNDER_ARMOR = 25225,
    SPELL_WARRIOR_CLEAVE = 15496,

    // Paladin Reflection
    SPELL_PALADIN_HAMMER_OF_JUSTICE = 37369,
    SPELL_PALADIN_CONSECRATION = 27173,
    SPELL_PALADIN_HOLY_STRIKE = 17143,
    SPELL_PALADIN_HOLY_LIGHT = 25263,

    // Hunter Reflection
    SPELL_HUNTER_FREEZING_TRAP = 37368,
    SPELL_HUNTER_ARCANE_SHOT = 27019,
    SPELL_HUNTER_MULTI_SHOT = 27021,
    SPELL_HUNTER_SERPENT_STING = 27016,

    // Rogue Reflection
    SPELL_ROGUE_HEMORRHAGE = 37331,
    SPELL_ROGUE_GOUGE = 1776,
    SPELL_ROGUE_SINISTER_STRIKE = 26862,
    SPELL_ROGUE_EVISCERATE = 26865,

    // Priest Reflection
    SPELL_PRIEST_MIND_FLAY = 37330,
    SPELL_PRIEST_SHADOW_WORD_PAIN = 25368,
    SPELL_PRIEST_PSYCHIC_SCREAM = 10890,
    SPELL_PRIEST_SMITE = 25364,

    // Shaman Reflection
    SPELL_SHAMAN_FROST_SHOCK = 37332,
    SPELL_SHAMAN_LIGHTNING_BOLT = 15208,
    SPELL_SHAMAN_CHAIN_LIGHTNING = 25439,
    SPELL_SHAMAN_EARTH_SHOCK = 25454,

    // Mage Reflection
    SPELL_MAGE_FIREBALL = 37329,
    SPELL_MAGE_FROSTBOLT = 25304,
    SPELL_MAGE_ARCANE_EXPLOSION = 27080,
    SPELL_MAGE_COUNTERSPELL = 2139,

    // Warlock Reflection
    SPELL_WARLOCK_CURSE_OF_AGONY = 27218,
    SPELL_WARLOCK_SHADOW_BOLT = 11661,
    SPELL_WARLOCK_IMMOLATE = 25308,
    SPELL_WARLOCK_FEAR = 5782,

    // Druid Reflection
    SPELL_DRUID_MOONFIRE = 26988,
    SPELL_DRUID_WRATH = 26984,
    SPELL_DRUID_ENTANGLING_ROOTS = 26989,
    SPELL_DRUID_REJUVENATION = 26982,

    // Death Knight Reflection
    SPELL_DEATH_KNIGHT_PLAGUE_STRIKE = 58839,
    SPELL_DEATH_KNIGHT_ICY_TOUCH = 49909,
    SPELL_DEATH_KNIGHT_DEATH_COIL = 29893,
    SPELL_DEATH_KNIGHT_BLOOD_BOIL = 49940
};

enum Npc
{
    // Reuses Exarch Maladaar's Stolen Soul creature template.
    // This script intentionally overrides the old npc_stolen_soul AI.
    NPC_MADDENED_REFLECTION = 18441
};

struct boss_blackheart_the_inciter : public BossAI
{
    boss_blackheart_the_inciter(Creature* creature) : BossAI(creature, DATA_BLACKHEARTTHEINCITEREVENT)
    {
        _talked = false;

        scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    bool IsEncounterPlayer(Player const* player, bool aliveOnly = true) const
    {
        if (!player)
            return false;

        if (player->GetMap() != me->GetMap())
            return false;

        if (player->IsGameMaster())
            return false;

        if (aliveOnly && !player->IsAlive())
            return false;

        if (!me->IsWithinDistInMap(player, 150.0f))
            return false;

        return true;
    }

    bool IsEncounterBot(Creature const* bot, bool aliveOnly = true) const
    {
        if (!bot)
            return false;

        if (!bot->IsNPCBot())
            return false;

        if (bot->GetMap() != me->GetMap())
            return false;

        if (aliveOnly && !bot->IsAlive())
            return false;

        if (!me->IsWithinDistInMap(bot, 150.0f))
            return false;

        return true;
    }

    std::vector<Player*> GetEncounterPlayers(bool aliveOnly) const
    {
        std::vector<Player*> players;

        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player, aliveOnly))
                continue;

            players.push_back(player);
        }

        return players;
    }

    std::vector<Creature*> GetEncounterBots(bool aliveOnly) const
    {
        std::vector<Creature*> bots;
        std::unordered_set<ObjectGuid::LowType> seen;

        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player, false))
                continue;

            for (Unit* member : BotMgr::GetAllGroupMembers(player))
            {
                if (!member || !member->IsInWorld())
                    continue;

                if (member->FindMap() != me->GetMap())
                    continue;

                Creature* bot = member->ToCreature();
                if (!bot || !IsEncounterBot(bot, aliveOnly))
                    continue;

                if (!seen.insert(bot->GetGUID().GetCounter()).second)
                    continue;

                bots.push_back(bot);
            }
        }

        return bots;
    }

    std::vector<Unit*> GetEncounterTargets(bool aliveOnly) const
    {
        std::vector<Unit*> targets;

        std::vector<Player*> players = GetEncounterPlayers(aliveOnly);
        targets.reserve(players.size() + 8);

        for (Player* player : players)
            targets.push_back(player);

        for (Creature* bot : GetEncounterBots(aliveOnly))
            targets.push_back(bot);

        return targets;
    }

    Unit* SelectRandomEncounterTarget(float maxRange, bool mustBeAlive = true, Unit const* exclude = nullptr, bool preferOutsideMelee = false)
    {
        std::vector<Unit*> candidates;
        candidates.reserve(16);

        for (Unit* unit : GetEncounterTargets(mustBeAlive))
        {
            if (!unit)
                continue;

            if (exclude && unit == exclude)
                continue;

            if (!me->IsWithinDistInMap(unit, maxRange))
                continue;

            if (preferOutsideMelee && me->IsWithinDistInMap(unit, 8.0f))
                continue;

            candidates.push_back(unit);
        }

        if (candidates.empty() && preferOutsideMelee)
        {
            for (Unit* unit : GetEncounterTargets(mustBeAlive))
            {
                if (!unit)
                    continue;

                if (exclude && unit == exclude)
                    continue;

                if (!me->IsWithinDistInMap(unit, maxRange))
                    continue;

                candidates.push_back(unit);
            }
        }

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    void SummonMaddenedReflection(Unit* target)
    {
        if (!target || !target->IsAlive())
            return;

        me->CastSpell(target, SPELL_MADDENING_COMMAND_MARK, true);

        if (Creature* reflection = me->SummonCreature(
            NPC_MADDENED_REFLECTION,
            *target,
            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
            30 * IN_MILLISECONDS))
        {
            reflection->CastSpell(reflection, SPELL_MADDENED_REFLECTION_VISUAL, true);
            reflection->SetDisplayId(target->GetDisplayId());
            reflection->SetLevel(target->GetLevel());
            reflection->SetReactState(REACT_AGGRESSIVE);

            reflection->AI()->SetGUID(target->GetGUID());
            reflection->AI()->DoAction(target->getClass());
            reflection->AI()->AttackStart(target);
        }
    }

    void DoMaddeningCommand()
    {
        DoCastSelf(SPELL_LAUGHTER, true);

        std::vector<Unit*> candidates = GetEncounterTargets(true);

        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [this](Unit* unit)
            {
                if (!unit)
                    return true;

                if (!unit->IsAlive())
                    return true;

                if (!me->IsWithinDistInMap(unit, 100.0f))
                    return true;

                return false;
            }), candidates.end());

        if (candidates.empty())
            return;

        for (Unit* target : candidates)
            SummonMaddenedReflection(target);
    }

    void Reset() override
    {
        _Reset();

        scheduler.CancelAll();

        me->SetImmuneToPC(false);
        _talked = false;
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!_talked && who && (who->IsPlayer() || who->IsNPCBot()) && me->IsWithinDistInMap(who, 150.0f))
        {
            _talked = true;
            Talk(SAY_INTRO);
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && (victim->IsPlayer() || victim->IsNPCBot()) && urand(0, 1))
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        _JustDied();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _JustEngagedWith();

        me->CallForHelp(100.0f);

        scheduler.Schedule(24s, [this](TaskContext context)
            {
                DoMaddeningCommand();
                context.Repeat(50s, 70s);
            }).Schedule(30s, 50s, [this](TaskContext context)
                {
                    Unit* currentVictim = me->GetVictim();

                    if (Unit* target = SelectRandomEncounterTarget(100.0f, true, currentVictim, true))
                        DoCast(target, SPELL_CHARGE);

                    context.Repeat(30s, 50s);
                }).Schedule(16950ms, 26350ms, [this](TaskContext context)
                    {
                        DoCastAOE(SPELL_WAR_STOMP);
                        context.Repeat(16950ms, 26350ms);
                    });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _talked;
};

struct npc_stolen_soul : public ScriptedAI
{
    npc_stolen_soul(Creature* creature) : ScriptedAI(creature)
    {
        _scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
    }

    void Reset() override
    {
        _myClass = CLASS_WARRIOR;
        _targetGuid.Clear();

        _scheduler.CancelAll();

        _scheduler.Schedule(1s, [this](TaskContext /*context*/)
            {
                ScheduleClassAbilitySet();
                ScheduleFixateCheck();
            });
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _targetGuid = guid;
    }

    void DoAction(int32 pClass) override
    {
        _myClass = static_cast<uint8>(pClass);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Unit* target = ObjectAccessor::GetUnit(*me, _targetGuid))
            target->RemoveAurasDueToSpell(SPELL_MADDENING_COMMAND_MARK);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            TryFixateOriginalTarget();

            if (!UpdateVictim())
                return;
        }

        _scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    void TryFixateOriginalTarget()
    {
        if (_targetGuid.IsEmpty())
            return;

        Unit* target = ObjectAccessor::GetUnit(*me, _targetGuid);
        if (!target || !target->IsAlive())
            return;

        if (target->GetMap() != me->GetMap())
            return;

        if (!me->IsWithinDistInMap(target, 120.0f))
            return;

        AttackStart(target);
    }

    void ScheduleFixateCheck()
    {
        _scheduler.Schedule(2s, [this](TaskContext context)
            {
                TryFixateOriginalTarget();
                context.Repeat(3s);
            });
    }

    void ScheduleClassAbilitySet()
    {
        switch (_myClass)
        {
        case CLASS_WARRIOR:
            ScheduleWarriorAbilities();
            break;
        case CLASS_PALADIN:
            SchedulePaladinAbilities();
            break;
        case CLASS_HUNTER:
            ScheduleHunterAbilities();
            break;
        case CLASS_ROGUE:
            ScheduleRogueAbilities();
            break;
        case CLASS_PRIEST:
            SchedulePriestAbilities();
            break;
        case CLASS_SHAMAN:
            ScheduleShamanAbilities();
            break;
        case CLASS_MAGE:
            ScheduleMageAbilities();
            break;
        case CLASS_WARLOCK:
            ScheduleWarlockAbilities();
            break;
        case CLASS_DRUID:
            ScheduleDruidAbilities();
            break;
        case CLASS_DEATH_KNIGHT:
            ScheduleDeathKnightAbilities();
            break;
        default:
            ScheduleWarriorAbilities();
            break;
        }
    }

    void ScheduleWarriorAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARRIOR_MORTAL_STRIKE);
                context.Repeat(6s, 9s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastAOE(SPELL_WARRIOR_THUNDER_CLAP);
                context.Repeat(10s, 14s);
            });

        _scheduler.Schedule(4500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARRIOR_SUNDER_ARMOR);
                context.Repeat(8s, 12s);
            });

        _scheduler.Schedule(6500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARRIOR_CLEAVE);
                context.Repeat(7s, 10s);
            });
    }

    void SchedulePaladinAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PALADIN_HAMMER_OF_JUSTICE);
                context.Repeat(10s, 15s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastAOE(SPELL_PALADIN_CONSECRATION);
                context.Repeat(12s, 18s);
            });

        _scheduler.Schedule(4500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PALADIN_HOLY_STRIKE);
                context.Repeat(7s, 11s);
            });

        _scheduler.Schedule(7500ms, [this](TaskContext context)
            {
                if (me->HealthBelowPct(55))
                    DoCastSelf(SPELL_PALADIN_HOLY_LIGHT);

                context.Repeat(9s, 14s);
            });
    }

    void ScheduleHunterAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_HUNTER_FREEZING_TRAP);
                context.Repeat(18s, 24s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_HUNTER_ARCANE_SHOT);
                context.Repeat(6s, 9s);
            });

        _scheduler.Schedule(4500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_HUNTER_MULTI_SHOT);
                context.Repeat(10s, 14s);
            });

        _scheduler.Schedule(6500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_HUNTER_SERPENT_STING);
                context.Repeat(14s, 20s);
            });
    }

    void ScheduleRogueAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ROGUE_HEMORRHAGE);
                context.Repeat(7s, 10s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ROGUE_GOUGE);
                context.Repeat(12s, 18s);
            });

        _scheduler.Schedule(4500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ROGUE_SINISTER_STRIKE);
                context.Repeat(5s, 8s);
            });

        _scheduler.Schedule(7500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_ROGUE_EVISCERATE);
                context.Repeat(9s, 13s);
            });
    }

    void SchedulePriestAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PRIEST_MIND_FLAY);
                context.Repeat(7s, 11s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PRIEST_SHADOW_WORD_PAIN);
                context.Repeat(14s, 20s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastAOE(SPELL_PRIEST_PSYCHIC_SCREAM);
                context.Repeat(18s, 26s);
            });

        _scheduler.Schedule(7500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_PRIEST_SMITE);
                context.Repeat(5s, 8s);
            });
    }

    void ScheduleShamanAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_SHAMAN_FROST_SHOCK);
                context.Repeat(8s, 12s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_SHAMAN_LIGHTNING_BOLT);
                context.Repeat(5s, 8s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_SHAMAN_CHAIN_LIGHTNING);
                context.Repeat(10s, 15s);
            });

        _scheduler.Schedule(7500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_SHAMAN_EARTH_SHOCK);
                context.Repeat(8s, 12s);
            });
    }

    void ScheduleMageAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_MAGE_FIREBALL);
                context.Repeat(5s, 8s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_MAGE_FROSTBOLT);
                context.Repeat(7s, 11s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastAOE(SPELL_MAGE_ARCANE_EXPLOSION);
                context.Repeat(10s, 14s);
            });

        _scheduler.Schedule(8500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_MAGE_COUNTERSPELL);
                context.Repeat(14s, 22s);
            });
    }

    void ScheduleWarlockAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARLOCK_CURSE_OF_AGONY);
                context.Repeat(14s, 20s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARLOCK_SHADOW_BOLT);
                context.Repeat(5s, 8s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARLOCK_IMMOLATE);
                context.Repeat(12s, 18s);
            });

        _scheduler.Schedule(8500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_WARLOCK_FEAR);
                context.Repeat(18s, 26s);
            });
    }

    void ScheduleDruidAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DRUID_MOONFIRE);
                context.Repeat(8s, 12s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DRUID_WRATH);
                context.Repeat(5s, 8s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DRUID_ENTANGLING_ROOTS);
                context.Repeat(14s, 20s);
            });

        _scheduler.Schedule(8500ms, [this](TaskContext context)
            {
                if (me->HealthBelowPct(55))
                    DoCastSelf(SPELL_DRUID_REJUVENATION);

                context.Repeat(10s, 16s);
            });
    }

    void ScheduleDeathKnightAbilities()
    {
        _scheduler.Schedule(0ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DEATH_KNIGHT_PLAGUE_STRIKE);
                context.Repeat(6s, 9s);
            });

        _scheduler.Schedule(2500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DEATH_KNIGHT_ICY_TOUCH);
                context.Repeat(7s, 11s);
            });

        _scheduler.Schedule(5500ms, [this](TaskContext context)
            {
                DoCastVictim(SPELL_DEATH_KNIGHT_DEATH_COIL);
                context.Repeat(8s, 12s);
            });

        _scheduler.Schedule(8500ms, [this](TaskContext context)
            {
                DoCastAOE(SPELL_DEATH_KNIGHT_BLOOD_BOIL);
                context.Repeat(10s, 15s);
            });
    }

private:
    TaskScheduler _scheduler;
    ObjectGuid _targetGuid;
    uint8 _myClass = CLASS_WARRIOR;
};

void AddSC_boss_blackheart_the_inciter()
{
    RegisterShadowLabyrinthCreatureAI(boss_blackheart_the_inciter);
    RegisterShadowLabyrinthCreatureAI(npc_stolen_soul);
}
