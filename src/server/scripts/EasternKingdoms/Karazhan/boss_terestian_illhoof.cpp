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

#include "CreatureScript.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "karazhan.h"

#include <string>
#include <vector>

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceDebuffOnMe(Creature* bot, uint32 spellId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Text
{
    SAY_SLAY                    = 0,
    SAY_DEATH                   = 1,
    SAY_AGGRO                   = 2,
    SAY_SACRIFICE               = 3,
    SAY_SUMMON                  = 4
};

enum Spells
{
    SPELL_SUMMON_DEMONCHAINS    = 30120,
    SPELL_DEMON_CHAINS          = 30206,
    SPELL_ENRAGE                = 23537,
    SPELL_SHADOW_BOLT           = 30055,
    SPELL_SACRIFICE             = 30115,
    SPELL_BERSERK               = 32965,
    SPELL_SUMMON_FIENDISIMP     = 30184,
    SPELL_SUMMON_IMP            = 30066,

    SPELL_FIENDISH_PORTAL       = 30171,
    SPELL_FIENDISH_PORTAL_1     = 30179,

    SPELL_FIREBOLT              = 30050,
    SPELL_BROKEN_PACT           = 30065,
    SPELL_AMPLIFY_FLAMES        = 30053
};

enum Creatures
{
    NPC_DEMONCHAINS             = 17248,
    NPC_PORTAL                  = 17265
};

namespace
{
    bool IsNPCBotUnit(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && creature->GetBotAI();
    }

    bool IsValidIllhoofEncounterUnit(Unit* unit, Creature const* source, float range)
    {
        if (!unit || !source)
            return false;

        if (!unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !source->IsWithinDistInMap(unit, range))
            return false;

        if (unit->IsPlayer())
            return true;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }
}

struct npc_kilrek : public ScriptedAI
{
    npc_kilrek(Creature* creature) : ScriptedAI(creature)
    {
        instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        _scheduler.CancelAll();
        TerestianGUID.Clear();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _scheduler.Schedule(2s, [this](TaskContext context)
        {
            me->InterruptNonMeleeSpells(false);
            DoCastVictim(SPELL_AMPLIFY_FLAMES);
            context.Repeat(10s, 20s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        ObjectGuid TerestianGuid = instance->GetGuidData(DATA_TERESTIAN);
        if (TerestianGuid)
        {
            Unit* Terestian = ObjectAccessor::GetUnit(*me, TerestianGuid);
            if (Terestian && Terestian->IsAlive())
            {
                DoCast(Terestian, SPELL_BROKEN_PACT, true);
            }
        }
        me->DespawnOrUnsummon(15s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        DoMeleeAttackIfReady();
    }

private:
    TaskScheduler _scheduler;
    InstanceScript* instance;
    ObjectGuid TerestianGUID;
};

struct npc_demon_chain : public ScriptedAI
{
    npc_demon_chain(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        sacrificeGUID.Clear();
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        sacrificeGUID = summoner->GetGUID();
        DoCastSelf(SPELL_DEMON_CHAINS, true);
    }

    void JustEngagedWith(Unit* /*who*/) override { }
    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }

    void JustDied(Unit* /*killer*/) override
    {
        if (sacrificeGUID)
        {
            Unit* Sacrifice = ObjectAccessor::GetUnit(*me, sacrificeGUID);
            if (Sacrifice)
            {
                Sacrifice->RemoveAurasDueToSpell(SPELL_SACRIFICE);
            }
        }
    }

private:
    ObjectGuid sacrificeGUID;
};

struct boss_terestian_illhoof : public BossAI
{
    boss_terestian_illhoof(Creature* creature) : BossAI(creature, DATA_TERESTIAN)
    {    }

    void Reset() override
    {
        RemoveSacrificeFromEncounterUnits();
        _Reset();
        SummonKilrek();
    }

    void SummonKilrek()
    {
        me->RemoveAurasDueToSpell(SPELL_BROKEN_PACT);
        DoCastSelf(SPELL_SUMMON_IMP);
    }

    void SpellHit(Unit* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_BROKEN_PACT)
        {
            scheduler.Schedule(45s, [this](TaskContext /*context*/) {
                SummonKilrek();
                });
        }
    }

    std::vector<Unit*> GatherIllhoofEncounterUnits(float range = 120.0f) const
    {
        std::vector<Unit*> units;
        GuidSet seen;

        auto addUnit = [&](Unit* unit)
        {
            if (!IsValidIllhoofEncounterUnit(unit, me, range))
                return;

            if (seen.count(unit->GetGUID()))
                return;

            seen.insert(unit->GetGUID());
            units.push_back(unit);
        };

        if (!me->GetMap())
            return units;

        Map::PlayerList const& players = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            addUnit(player);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    addUnit(member);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    addUnit(pair.second);
            }
        }

        return units;
    }

    Unit* SelectSacrificeTarget()
    {
        std::vector<Unit*> candidates;

        for (Unit* unit : GatherIllhoofEncounterUnits(100.0f))
        {
            if (!unit || unit == me->GetVictim())
                continue;

            if (unit->HasAura(SPELL_SACRIFICE))
                continue;

            candidates.push_back(unit);
        }

        if (candidates.empty())
            return nullptr;

        return candidates[urand(0, candidates.size() - 1)];
    }

    void RemoveSacrificeFromEncounterUnits()
    {
        for (Unit* unit : GatherIllhoofEncounterUnits(120.0f))
        {
            if (unit)
                unit->RemoveAurasDueToSpell(SPELL_SACRIFICE);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        DoZoneInCombat();
        scheduler.Schedule(30s, [this](TaskContext context)
        {
            if (Unit* target = SelectSacrificeTarget())
            {
                DoCast(target, SPELL_SACRIFICE, true);
                if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target))
                    DBMFTABotCallouts::AnnounceDebuffOnMe(bot, SPELL_SACRIFICE, "Sacrifice", DBMFTABotCallouts::GetCooldownMs());

                target->m_Events.AddEventAtOffset([target] {
                    target->CastSpell(target, SPELL_SUMMON_DEMONCHAINS, true);
                }, 1s);

                Talk(SAY_SACRIFICE);
            }

            context.Repeat(30s);
        }).Schedule(5s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_SHADOW_BOLT);
            context.Repeat(10s);
        }).Schedule(10s, [this](TaskContext)
        {
            DoCastAOE(SPELL_FIENDISH_PORTAL);
        }).Schedule(11s, [this](TaskContext)
        {
            DoCastAOE(SPELL_FIENDISH_PORTAL_1);
        }).Schedule(10min, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_BERSERK);
        });
    }

    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_PORTAL)
        {
            summoned->SetReactState(REACT_PASSIVE);
            if (summoned->GetUInt32Value(UNIT_CREATED_BY_SPELL) == SPELL_FIENDISH_PORTAL_1)
            {
                Talk(SAY_SUMMON);
            }
        }

        summons.Summon(summoned);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() || IsNPCBotUnit(victim))
        {
            Talk(SAY_SLAY);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        RemoveSacrificeFromEncounterUnits();
        _JustDied();
        Talk(SAY_DEATH);
    }
};

void AddSC_boss_terestian_illhoof()
{
    RegisterKarazhanCreatureAI(boss_terestian_illhoof);
    RegisterKarazhanCreatureAI(npc_kilrek);
    RegisterKarazhanCreatureAI(npc_demon_chain);
}
