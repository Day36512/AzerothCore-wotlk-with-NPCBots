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

#include "Creature.h"
#include "CreatureScript.h"
#include "GameObjectScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "blood_furnace.h"
#include "ObjectAccessor.h" 

 // NPCBots
#include "bot_ai.h"
#include "botmgr.h"

using namespace std::chrono_literals;

enum Say
{
    SAY_AGGRO = 0
};

enum Spells
{
    SPELL_SLIME_SPRAY = 30913,
    SPELL_POISON_CLOUD = 30916,
    SPELL_POISON_BOLT = 30917,
    SPELL_POISON = 30914,

    // Teleport visual for players/bots and now for boss intro blink
    SPELL_TELEPORT_VISUAL = 41232
};

// ---------- custom IDs for healer attendants ----------
enum CustomEntries
{
    NPC_BROGGOK_ATTENDANT_N = 816947,  // Normal-mode healer 
    NPC_BROGGOK_ATTENDANT_H = 816948   // Heroic-mode healer 
};

enum CustomSpells
{
    SPELL_ATTENDANT_HEAL_N = 300378,   // Channel heal to Broggok (Normal)
    SPELL_ATTENDANT_HEAL_H = 300378    // Channel heal to Broggok (Heroic) - adjust later 
};

// Boss entries (fixed)
static constexpr uint32 BROGGOK_NORMAL_ENTRY = 17380;
static constexpr uint32 BROGGOK_HEROIC_ENTRY = 18601;

// ---------- Attendant spawn spots (now 4 unique points) ----------
static Position const kHealerSpawn[4] =
{
    { 458.57f, 140.95f, 9.62f, 0.00f },
    { 454.05f, 141.00f, 9.62f, 0.00f },
    { 453.25f,  42.01f, 9.62f, 0.00f },
    { 459.01f,  41.32f, 9.62f, 0.00f }
};

// ---------- TELEPORT GATE (positions & radius) ----------
static Position const kTeleportTrigger = { 456.09f,  37.01f, 9.59f, 0.0f };
static Position const kTeleportDest = { 455.17f,  91.55f, 9.61f, 0.0f };
static constexpr float kTeleportRadius = 7.0f;

// ---------- BOSS INTRO TELEPORT DEST ----------
static Position const kBossIntroBlink = { 455.26f, 44.83f, 9.62f, 0.0f };

enum Events
{
    EVENT_SPRAY = 1,
    EVENT_POISON_BOLT,
    EVENT_CLOUD_SELF,
    EVENT_GAS_CANISTER
};

static inline bool IsPlayerOrNPCBot(Unit* u)
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

// ============================================================================
// Boss: Broggok
// ============================================================================
struct boss_broggok : public BossAI
{
    boss_broggok(Creature* creature) : BossAI(creature, DATA_BROGGOK) {}

    void Reset() override
    {
        _Reset();
        // ---------- DESPAWN ON RESET (safety) ----------
        summons.DespawnAll();

        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        me->SetImmuneToAll(true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // ---------- DESPAWN ON DEATH ----------
        summons.DespawnAll();
        _JustDied();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);

        // Intro blink to the specified pad with visual
        me->CastSpell(me, SPELL_TELEPORT_VISUAL, true);
        me->NearTeleportTo(kBossIntroBlink.GetPositionX(),
            kBossIntroBlink.GetPositionY(),
            kBossIntroBlink.GetPositionZ(),
            me->GetOrientation());

        me->SetInCombatWithZone();
        _JustEngagedWith();
    }

    void JustSummoned(Creature* summoned) override
    {
        // Track attendants and other summons
        summons.Summon(summoned);

        // Skip special setup for attendants; they run/channel via their own AI
        if (summoned->GetEntry() == NPC_BROGGOK_ATTENDANT_N || summoned->GetEntry() == NPC_BROGGOK_ATTENDANT_H)
            return;

        // Original poison cloud helper behavior
        summoned->SetFaction(FACTION_MONSTER_2);
        summoned->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        summoned->CastSpell(summoned, SPELL_POISON, true, nullptr, nullptr, me->GetGUID());
    }

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

            if (IsPlayerOrNPCBot(u))
                pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0u, static_cast<uint32>(pool.size() - 1))];
    }

    Unit* SelectRandomPlayerOrNPCBotBeyond(float minDist, bool excludeVictim = true)
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

            if (!IsPlayerOrNPCBot(u))
                continue;

            if (me->GetExactDist(u) > minDist)
                pool.push_back(u);
        }

        if (pool.empty())
            return nullptr;

        return pool[urand(0u, static_cast<uint32>(pool.size() - 1))];
    }

    // ---------- helper: spawn attendants at unique points (2 on Normal, 3 on Heroic) ----------
    void SpawnHealerAttendants()
    {
        const bool heroic = me->GetMap()->IsHeroic();
        const uint32 entry = heroic ? NPC_BROGGOK_ATTENDANT_H : NPC_BROGGOK_ATTENDANT_N;
        const uint8 count = heroic ? 3 : 2;

        // Pick 'count' unique indices from {0,1,2,3} using a partial Fisher–Yates shuffle
        int idx[4] = { 0, 1, 2, 3 };
        for (uint8 i = 0; i < count; ++i)
        {
            uint8 r = urand(i, 3);
            // swap idx[i] and idx[r] (manual swap to avoid requiring headers)
            int tmp = idx[i];
            idx[i] = idx[r];
            idx[r] = tmp;

            me->SummonCreature(entry, kHealerSpawn[idx[i]], TEMPSUMMON_MANUAL_DESPAWN);
        }
    }

    // ---------- TELEPORT GATE: scan threat list and warp intruders (with visual) ----------
    void TeleportGateScan()
    {
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
        {
            if (!ref || ref->IsOffline())
                continue;

            Unit* u = ref->GetVictim();
            if (!u || !u->IsAlive())
                continue;

            // Only players and NPCBots are affected
            if (!IsPlayerOrNPCBot(u))
                continue;

            if (u->GetExactDist(kTeleportTrigger.GetPositionX(),
                kTeleportTrigger.GetPositionY(),
                kTeleportTrigger.GetPositionZ()) <= kTeleportRadius)
            {
                // Teleport visual, then move them
                u->CastSpell(u, SPELL_TELEPORT_VISUAL, true);
                u->NearTeleportTo(kTeleportDest.GetPositionX(),
                    kTeleportDest.GetPositionY(),
                    kTeleportDest.GetPositionZ(),
                    u->GetOrientation());
            }
        }
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
        case ACTION_PREPARE_BROGGOK:
        {
            me->SetInCombatWithZone();
            instance->SetBossState(DATA_BROGGOK, IN_PROGRESS);
            break;
        }
        case ACTION_ACTIVATE_BROGGOK:
        {
            // Initial attendants at pull (now mode-aware and unique points)
            SpawnHealerAttendants();

            // Core kit + orientation fix + teleport scan + recurring attendants
            scheduler
                .Schedule(10s, [this](TaskContext ctx)
                    {
                        if (Unit* v = me->GetVictim())
                        {
                            me->SetFacingToObject(v);
                            me->SetInFront(v);
                            me->CastSpell(v, SPELL_SLIME_SPRAY, false);
                        }
                        ctx.Repeat(7s, 12s);
                    })
                .Schedule(5s, [this](TaskContext ctx)
                    {
                        if (Unit* tgt = SelectRandomPlayerOrNPCBotFromThreat(true))
                            me->CastSpell(tgt, SPELL_POISON_BOLT, false);
                        ctx.Repeat(6s, 11s);
                    })
                .Schedule(7s, [this](TaskContext ctx)
                    {
                        me->CastSpell(me, SPELL_POISON_CLOUD, false);
                        ctx.Repeat(20s);
                    })
                // TELEPORT GATE: run while the fight is active
                .Schedule(1s, [this](TaskContext ctx)
                    {
                        TeleportGateScan();
                        ctx.Repeat(1s);
                    })
                // Recurring waves of healer attendants (mode-aware, unique each time)
                .Schedule(30s, [this](TaskContext ctx)
                    {
                        SpawnHealerAttendants();
                        ctx.Repeat(28s, 38s);
                    });

            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->SetImmuneToAll(false);
            break;
        }
        }
    }
};

// ============================================================================
// Healer Attendant (Normal)
// ============================================================================
class npc_broggok_attendant_normal : public CreatureScript
{
public:
    npc_broggok_attendant_normal() : CreatureScript("npc_broggok_attendant_normal") {}

    struct npc_broggok_attendant_normalAI : public ScriptedAI
    {
        explicit npc_broggok_attendant_normalAI(Creature* c) : ScriptedAI(c) {}

        ObjectGuid _bossGuid;
        EventMap   _events;
        bool       _channeling = false;

        enum { POINT_TO_BOSS = 1 };

        Creature* GetBoss()
        {
            if (_bossGuid)
                if (Creature* b = ObjectAccessor::GetCreature(*me, _bossGuid))
                    return b;

            if (Creature* b = me->FindNearestCreature(BROGGOK_NORMAL_ENTRY, 200.0f, true))
                return b;

            if (InstanceScript* inst = me->GetInstanceScript())
                if (Creature* b = inst->GetCreature(DATA_BROGGOK))
                    return b;

            return nullptr;
        }

        void IsSummonedBy(WorldObject* summoner) override
        {
            _bossGuid = summoner ? summoner->GetGUID() : ObjectGuid::Empty;

            me->SetReactState(REACT_PASSIVE);
            me->SetWalk(false); // run
            me->Yell("For the master’s renewal!", LANG_UNIVERSAL);

            if (Creature* boss = GetBoss())
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
            }
            else
            {
                _events.ScheduleEvent(1, 1s);
            }
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE || id != POINT_TO_BOSS)
                return;

            if (Creature* boss = GetBoss())
                StartChannel(boss);
        }

        void StartChannel(Creature* boss)
        {
            if (!boss || !boss->IsAlive())
                return;

            me->AttackStop();
            me->SetFacingToObject(boss);
            me->CastSpell(boss, SPELL_ATTENDANT_HEAL_N, false);
            _channeling = true;
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            if (uint32 id = _events.ExecuteEvent())
            {
                if (id == 1)
                {
                    if (Creature* boss = GetBoss())
                        me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
                }
            }

            if (Creature* boss = GetBoss())
            {
                if (!_channeling || !me->HasUnitState(UNIT_STATE_CASTING))
                {
                    if (me->GetDistance(boss) > 24.0f)
                        me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
                    else
                        StartChannel(boss);
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* c) const override { return new npc_broggok_attendant_normalAI(c); }
};

// ============================================================================
// Healer Attendant (Heroic)
// ============================================================================
class npc_broggok_attendant_heroic : public CreatureScript
{
public:
    npc_broggok_attendant_heroic() : CreatureScript("npc_broggok_attendant_heroic") {}

    struct npc_broggok_attendant_heroicAI : public ScriptedAI
    {
        explicit npc_broggok_attendant_heroicAI(Creature* c) : ScriptedAI(c) {}

        ObjectGuid _bossGuid;
        EventMap   _events;
        bool       _channeling = false;

        enum { POINT_TO_BOSS = 1 };

        Creature* GetBoss()
        {
            if (_bossGuid)
                if (Creature* b = ObjectAccessor::GetCreature(*me, _bossGuid))
                    return b;

            if (Creature* b = me->FindNearestCreature(BROGGOK_HEROIC_ENTRY, 200.0f, true))
                return b;

            if (InstanceScript* inst = me->GetInstanceScript())
                if (Creature* b = inst->GetCreature(DATA_BROGGOK))
                    return b;

            return nullptr;
        }

        void IsSummonedBy(WorldObject* summoner) override
        {
            _bossGuid = summoner ? summoner->GetGUID() : ObjectGuid::Empty;

            me->SetReactState(REACT_PASSIVE);
            me->SetWalk(false); // run
            me->Yell("His vitality shall be restored!", LANG_UNIVERSAL);

            if (Creature* boss = GetBoss())
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
            }
            else
            {
                _events.ScheduleEvent(1, 1s);
            }
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE || id != POINT_TO_BOSS)
                return;

            if (Creature* boss = GetBoss())
                StartChannel(boss);
        }

        void StartChannel(Creature* boss)
        {
            if (!boss || !boss->IsAlive())
                return;

            me->AttackStop();
            me->SetFacingToObject(boss);
            me->CastSpell(boss, SPELL_ATTENDANT_HEAL_H, false);
            _channeling = true;
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            if (uint32 id = _events.ExecuteEvent())
            {
                if (id == 1)
                {
                    if (Creature* boss = GetBoss())
                        me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
                }
            }

            if (Creature* boss = GetBoss())
            {
                if (!_channeling || !me->HasUnitState(UNIT_STATE_CASTING))
                {
                    if (me->GetDistance(boss) > 24.0f)
                        me->GetMotionMaster()->MovePoint(POINT_TO_BOSS, boss->GetPosition());
                    else
                        StartChannel(boss);
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* c) const override { return new npc_broggok_attendant_heroicAI(c); }
};

// ============================================================================
// Poison (Broggok) aura helper 
// ============================================================================
class spell_broggok_poison_cloud : public AuraScript
{
    PrepareAuraScript(spell_broggok_poison_cloud);

    bool Validate(SpellInfo const* spellInfo) override
    {
        if (!sSpellMgr->GetSpellInfo(spellInfo->Effects[EFFECT_0].TriggerSpell))
            return false;

        return true;
    }

    void PeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        uint32 triggerSpell = GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell;
        int32 mod = int32(((float(aurEff->GetTickNumber()) / aurEff->GetTotalTicks()) * 0.9f + 0.1f) * 10000 * 2 / 3);
        GetTarget()->CastCustomSpell(triggerSpell, SPELLVALUE_RADIUS_MOD, mod, (Unit*)nullptr, TRIGGERED_FULL_MASK, nullptr, aurEff);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_broggok_poison_cloud::PeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class go_broggok_lever : public GameObjectScript
{
public:
    go_broggok_lever() : GameObjectScript("go_broggok_lever") {}

    bool OnGossipHello(Player* /*player*/, GameObject* go) override
    {
        if (InstanceScript* instance = go->GetInstanceScript())
            if (instance->GetBossState(DATA_BROGGOK) == NOT_STARTED)
                if (Creature* broggok = instance->GetCreature(DATA_BROGGOK))
                    broggok->AI()->DoAction(ACTION_PREPARE_BROGGOK);

        go->UseDoorOrButton();
        return false;
    }
};

void AddSC_boss_broggok()
{
    RegisterBloodFurnaceCreatureAI(boss_broggok);

    new npc_broggok_attendant_normal();
    new npc_broggok_attendant_heroic();
    RegisterSpellScript(spell_broggok_poison_cloud);

    new go_broggok_lever();
}
