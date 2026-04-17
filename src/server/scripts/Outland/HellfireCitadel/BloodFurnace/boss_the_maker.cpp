/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Unit.h"
#include "blood_furnace.h"

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

using namespace std::chrono_literals;

namespace Maker
{
    // ---------- Configuration / IDs ----------
    enum Says
    {
        SAY_AGGRO = 0,
        SAY_KILL = 1,
        SAY_DIE = 2
    };

    enum Spells
    {
        SPELL_EXPLODING_BEAKER = 30925,      // Original random-target beaker
        SPELL_TRANSMOGRIFY = 118,            // Polymorph (non-MC CC) – "Mutagenic Transmogrify"
        SPELL_SLIME_SPRAY = 300370,          // Frontal cone "Acid Spray"
        SPELL_VOLATILE_CLOUD = 300371,       // Ground hazard
        SPELL_VOLATILE_MARK_VISUAL = 300372, // Short visual mark on the chosen target
        SPELL_BERSERK = 26662,               // Fail-safe enrage (optional, not scheduled here)

        // Cage ritual
        SPELL_CAGE_CHANNEL = 300374,         // Channeled spell on cage
        SPELL_ORC_HARVEST_BUFF = 300376,     // Small buff
        SPELL_ORC_HARVEST_HEAL = 300375      // Small self-heal
    };

    enum Events
    {
        EVENT_BEAKER = 1,
        EVENT_TRANSMOGRIFY,
        EVENT_ACID_SPRAY,
        EVENT_VOLATILE_CLOUD,
        EVENT_BERSERK,
        EVENT_ORC_RITUAL_START
    };

    enum Npcs
    {
        NPC_CAGED_ORC = 17416
    };

    enum Points
    {
        POINT_RITUAL_0 = 100,
        POINT_RITUAL_1,
        POINT_RITUAL_2,
        POINT_RITUAL_3
    };

    // Four cage anchor positions
    static Position const kCagePos[4] =
    {
        { 296.02f,  99.86f, 9.62f, 3.12f },
        { 295.38f,  69.08f, 9.62f, 3.12f },
        { 359.20f,  69.07f, 9.62f, 0.00f },
        { 359.37f, 100.09f, 9.62f, 0.00f }
    };

    inline bool IsPlayerOrNPCBot(Unit* u)
    {
        if (!u || !u->IsAlive())
            return false;

        if (u->GetTypeId() == TYPEID_PLAYER)
            return true;

        if (u->GetTypeId() == TYPEID_UNIT)
            if (Creature* c = u->ToCreature())
                return c->IsNPCBot();

        return false;
    }
} // namespace Maker

struct boss_the_maker : public BossAI
{
    boss_the_maker(Creature* creature) : BossAI(creature, DATA_THE_MAKER)
    {
    }

    //Dinkle custom start
    Unit* SelectRandomPlayerOrNPCBotFromThreat(bool excludeVictim = true)
    {
        std::vector<Unit*> pool;
        pool.reserve(me->GetThreatMgr().GetThreatListSize());

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (excludeVictim && u == me->GetVictim())
                continue;

            if (Maker::IsPlayerOrNPCBot(u))
                pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0, uint32(pool.size() - 1))];
    }
    //Dinkle custom end

    // Select a random player/NPCBot beyond a minimum distance; optionally exclude current victim.
    Unit* SelectRandomPlayerOrNPCBotBeyond(float minDistance, bool excludeVictim = true)
    {
        std::vector<Unit*> pool;
        pool.reserve(me->GetThreatMgr().GetThreatListSize());

        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            if (excludeVictim && u == me->GetVictim())
                continue;

            if (!Maker::IsPlayerOrNPCBot(u))
                continue;

            if (me->GetExactDist(u) > minDistance)
                pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0, uint32(pool.size() - 1))];
    }

    // Ritual state
    bool  _ritualActive = false;
    bool  _visited[4] = { false, false, false, false };
    uint8 _visitedCount = 0;
    int8  _activeCage = -1;

    int8 GetFurthestUnvisitedCageIndex() const
    {
        float bestDist = -1.f;
        int8 bestIdx = -1;

        for (int8 i = 0; i < 4; ++i)
        {
            if (_visited[i])
                continue;

            float d = me->GetExactDist(
                Maker::kCagePos[i].GetPositionX(),
                Maker::kCagePos[i].GetPositionY(),
                Maker::kCagePos[i].GetPositionZ());

            if (d > bestDist)
            {
                bestDist = d;
                bestIdx = i;
            }
        }

        return bestIdx;
    }

    void FaceRitualTarget()
    {
        if (Creature* orc = me->FindNearestCreature(Maker::NPC_CAGED_ORC, 25.0f))
        {
            me->SetFacingToObject(orc);
            return;
        }

        if (_activeCage >= 0 && _activeCage < 4)
            me->SetFacingTo(Maker::kCagePos[_activeCage].GetOrientation());
    }

    void Reset() override
    {
        BossAI::Reset();
        events.Reset();

        _ritualActive = false;
        _visited[0] = _visited[1] = _visited[2] = _visited[3] = false;
        _visitedCount = 0;
        _activeCage = -1;

        std::list<Creature*> orcs;
        me->GetCreaturesWithEntryInRange(orcs, 200.0f, Maker::NPC_CAGED_ORC);
        for (Creature* c : orcs)
            if (!c->IsAlive())
                c->Respawn(true);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(Maker::SAY_AGGRO);

        events.ScheduleEvent(Maker::EVENT_BEAKER, 6s);
        events.ScheduleEvent(Maker::EVENT_TRANSMOGRIFY, 20s);
        events.ScheduleEvent(Maker::EVENT_ACID_SPRAY, 9s);
        events.ScheduleEvent(Maker::EVENT_VOLATILE_CLOUD, 15s);
        // events.ScheduleEvent(Maker::EVENT_BERSERK, 10min);

        events.ScheduleEvent(Maker::EVENT_ORC_RITUAL_START, 20s);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && victim->IsPlayer() && urand(0, 1))
            Talk(Maker::SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(Maker::SAY_DIE);
        BossAI::JustDied(nullptr);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (!_ritualActive)
            return;

        if (id == Maker::POINT_RITUAL_0 || id == Maker::POINT_RITUAL_1 ||
            id == Maker::POINT_RITUAL_2 || id == Maker::POINT_RITUAL_3)
        {
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            FaceRitualTarget();
            me->CastSpell(me, Maker::SPELL_CAGE_CHANNEL, false);
        }
    }

    enum Actions
    {
        ACTION_RITUAL_DONE = 1
    };

    void DoAction(int32 action) override
    {
        if (action == ACTION_RITUAL_DONE)
        {
            if (_activeCage >= 0 && _activeCage < 4 && !_visited[_activeCage])
            {
                _visited[_activeCage] = true;
                ++_visitedCount;
            }

            _activeCage = -1;
            _ritualActive = false;

            me->Yell("Their souls empower my flesh!", LANG_UNIVERSAL);

            me->SetReactState(REACT_AGGRESSIVE);
            me->SetInCombatWithZone();

            if (_visitedCount < 4)
                events.ScheduleEvent(Maker::EVENT_ORC_RITUAL_START, 25s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (_ritualActive)
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
        case Maker::EVENT_BEAKER:
        {
            if (Unit* tgt = SelectRandomPlayerOrNPCBotFromThreat(true))
                me->CastSpell(tgt, Maker::SPELL_EXPLODING_BEAKER, false);

            events.Repeat(7s, 11s);
            break;
        }

        case Maker::EVENT_TRANSMOGRIFY:
        {
            if (Unit* tgt = SelectRandomPlayerOrNPCBotFromThreat(true))
                me->CastSpell(tgt, Maker::SPELL_TRANSMOGRIFY, false);

            events.Repeat(22s, 30s);
            break;
        }

        case Maker::EVENT_ACID_SPRAY:
        {
            if (Unit* victim = me->GetVictim())
            {
                me->SetFacingToObject(victim);
                me->SetInFront(victim);
                me->CastSpell(victim, Maker::SPELL_SLIME_SPRAY, false);
            }

            events.Repeat(12s, 16s);
            break;
        }

        case Maker::EVENT_VOLATILE_CLOUD:
        {
            Unit* tgt = SelectRandomPlayerOrNPCBotBeyond(10.0f, false);
            if (!tgt)
                tgt = SelectRandomPlayerOrNPCBotFromThreat(false);

            if (tgt)
            {
                me->CastSpell(tgt, Maker::SPELL_VOLATILE_MARK_VISUAL, true);
                me->CastSpell(tgt, Maker::SPELL_VOLATILE_CLOUD, false);
            }
            else
            {
                me->CastSpell(me, Maker::SPELL_VOLATILE_CLOUD, false);
            }

            events.Repeat(18s, 24s);
            break;
        }

        case Maker::EVENT_BERSERK:
        {
            me->CastSpell(me, Maker::SPELL_BERSERK, true);
            break;
        }

        case Maker::EVENT_ORC_RITUAL_START:
        {
            if (_visitedCount >= 4)
                break;

            int8 idx = GetFurthestUnvisitedCageIndex();
            if (idx < 0)
                break;

            _ritualActive = true;
            _activeCage = idx;

            me->Yell("Subjects prepared... I will harvest your souls!", LANG_UNIVERSAL);

            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MovePoint(Maker::POINT_RITUAL_0 + idx, Maker::kCagePos[idx]);
            break;
        }

        default:
            break;
        }

        DoMeleeAttackIfReady();
    }
};

// 300374 - Cage Channel
class aura_maker_cage_channel : public AuraScript
{
    PrepareAuraScript(aura_maker_cage_channel);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            Maker::SPELL_CAGE_CHANNEL,
            Maker::SPELL_ORC_HARVEST_BUFF,
            Maker::SPELL_ORC_HARVEST_HEAL,
            300377
            });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsAlive())
            return;

        std::list<Creature*> found;
        caster->GetCreaturesWithEntryInRange(found, 15.0f, Maker::NPC_CAGED_ORC);

        for (Creature* c : found)
        {
            if (!c || !c->IsAlive())
                continue;

            c->CastSpell(c, 300377, true);

            Unit::Kill(caster, c, true);
            caster->CastSpell(caster, Maker::SPELL_ORC_HARVEST_BUFF, true);
            caster->CastSpell(caster, Maker::SPELL_ORC_HARVEST_HEAL, true);
        }

        if (Creature* cr = caster->ToCreature())
            if (cr->IsAIEnabled)
                cr->AI()->DoAction(boss_the_maker::ACTION_RITUAL_DONE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(aura_maker_cage_channel::AfterRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_the_maker()
{
    RegisterBloodFurnaceCreatureAI(boss_the_maker);
    RegisterSpellScript(aura_maker_cage_channel);
}
