/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "molten_core.h"

 // Nearby search helpers
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"

// Optional NPCBots (safe to include if present in your tree)
#include "bot_ai.h"
#include "botmgr.h"
#include "botdatamgr.h"

#include <list>
#include <vector>
#include <cmath>

enum Emotes
{
    EMOTE_SERVICE = 0
};

enum Spells
{
    SPELL_INFERNO = 19695,
    SPELL_INFERNO_DUMMY_EFFECT = 19698, // server-side damage
    SPELL_IGNITE_MANA = 19659,
    SPELL_LIVING_BOMB = 20475,
    SPELL_ARMAGEDDON = 20478,
};

enum Events
{
    EVENT_INFERNO = 1,
    EVENT_IGNITE_MANA,
    EVENT_LIVING_BOMB,
};

// Simple NPCBot checker
static inline bool IsNPCBot(Unit* u)
{
    return u && u->GetTypeId() == TYPEID_UNIT && u->ToCreature() && u->ToCreature()->IsNPCBot();
}

static inline void BotSay(Unit* u, char const* text)
{
    if (!u || !text || !*text) return;
    if (IsNPCBot(u))
        u->ToCreature()->Say(text, LANG_UNIVERSAL);
}

class boss_baron_geddon : public CreatureScript
{
public:
    boss_baron_geddon() : CreatureScript("boss_baron_geddon") {}

    struct boss_baron_geddonAI : public BossAI
    {
        boss_baron_geddonAI(Creature* creature) : BossAI(creature, DATA_GEDDON), armageddonCasted(false) {}

        void Reset() override
        {
            _Reset();
            armageddonCasted = false;
        }

        void JustEngagedWith(Unit* /*attacker*/) override
        {
            _JustEngagedWith();
            events.ScheduleEvent(EVENT_INFERNO, 13s, 15s);
            events.ScheduleEvent(EVENT_IGNITE_MANA, 7s, 19s);
            events.ScheduleEvent(EVENT_LIVING_BOMB, 11s, 16s);
        }

        // Random player or NPCBot (alive/hostile/in range). Uses Cell visitor (AzerothCore pattern).
        Unit* SelectRandomPlayerOrNPCBot(float range)
        {
            std::list<Unit*> nearby;
            Acore::AnyUnitInObjectRangeCheck check(me, range);
            Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, nearby, check);
            Cell::VisitObjects(me, searcher, range);

            std::vector<Unit*> pool;
            pool.reserve(nearby.size());
            for (Unit* u : nearby)
            {
                if (!u || !u->IsAlive())
                    continue;
                if (!u->IsHostileTo(me))
                    continue;
                if (!(u->IsPlayer() || IsNPCBot(u)))
                    continue;
                pool.push_back(u);
            }

            if (pool.empty())
                return nullptr;

            return pool[urand(0u, uint32(pool.size() - 1))];
        }

        void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*dmgType*/, SpellSchoolMask /*school*/) override
        {
            // If boss is below 2% hp - cast Armageddon
            if (!armageddonCasted && damage < me->GetHealth() && me->HealthBelowPctDamaged(2, damage))
            {
                me->RemoveAurasDueToSpell(SPELL_INFERNO);
                me->StopMoving();
                if (me->CastSpell(me, SPELL_ARMAGEDDON, TRIGGERED_FULL_MASK) == SPELL_CAST_OK)
                {
                    Talk(EMOTE_SERVICE);
                    armageddonCasted = true;
                }
            }
        }

        void ExecuteEvent(uint32 eventId) override
        {
            switch (eventId)
            {
            case EVENT_INFERNO:
            {
                DoCastAOE(SPELL_INFERNO);
                events.Repeat(21s, 26s);
                break;
            }
            case EVENT_IGNITE_MANA:
            {
                if (Unit* target = SelectRandomPlayerOrNPCBot(100.0f))
                    DoCast(target, SPELL_IGNITE_MANA);

                events.Repeat(27s, 32s);
                break;
            }
            case EVENT_LIVING_BOMB:
            {
                if (Unit* target = SelectRandomPlayerOrNPCBot(100.0f))
                    DoCast(target, SPELL_LIVING_BOMB);

                events.Repeat(11s, 16s);
                break;
            }
            }
        }

    private:
        bool armageddonCasted;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetMoltenCoreAI<boss_baron_geddonAI>(creature);
    }
};

class spell_geddon_inferno_aura : public AuraScript
{
    PrepareAuraScript(spell_geddon_inferno_aura);

    // Read once and cache the multiplier from worldserver.conf
    static float GetInfernoDamageMult()
    {
        static float sMult = []() -> float
            {
                float v = sConfigMgr->GetOption<float>("MoltenCore.BaronGeddon.InfernoDamageMult", 1.0f);
                if (v <= 0.0f)
                    v = 0.01f; // clamp to a sane minimum
                return v;
            }();
        return sMult;
    }

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_INFERNO_DUMMY_EFFECT });
    }

    void HandleAfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* pCreatureTarget = GetTarget()->ToCreature())
        {
            pCreatureTarget->SetReactState(REACT_PASSIVE);
            pCreatureTarget->AttackStop();
        }
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* pCreatureTarget = GetTarget()->ToCreature())
        {
            pCreatureTarget->SetReactState(REACT_AGGRESSIVE);
        }
    }

    void PeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        if (Unit* caster = GetUnitOwner())
        {
            int32 tickMult = 1;
            switch (aurEff->GetTickNumber())
            {
            case 3:
            case 4: tickMult = 2;  break;
            case 5:
            case 6: tickMult = 4;  break;
            case 7: tickMult = 6;  break;
            case 8: tickMult = 10; break;
            default: break;
            }

            float const cfg = GetInfernoDamageMult();
            int32 const basePoints = int32(std::lround(500.0f * tickMult * cfg));

            caster->CastCustomSpell(
                SPELL_INFERNO_DUMMY_EFFECT,
                SPELLVALUE_BASE_POINT0,
                basePoints,
                (Unit*)nullptr,
                TRIGGERED_NONE,
                nullptr,
                aurEff
            );
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_geddon_inferno_aura::HandleAfterApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_geddon_inferno_aura::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_geddon_inferno_aura::PeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_geddon_armageddon_aura : public AuraScript
{
    PrepareAuraScript(spell_geddon_armageddon_aura);

    void HandleAfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* pCreatureTarget = GetTarget()->ToCreature())
        {
            pCreatureTarget->SetReactState(REACT_PASSIVE);
            pCreatureTarget->AttackStop();
        }
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* pCreatureTarget = GetTarget()->ToCreature())
        {
            pCreatureTarget->SetReactState(REACT_AGGRESSIVE);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_geddon_armageddon_aura::HandleAfterApply, EFFECT_1, SPELL_AURA_MOD_PACIFY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_geddon_armageddon_aura::HandleAfterRemove, EFFECT_1, SPELL_AURA_MOD_PACIFY, AURA_EFFECT_HANDLE_REAL);
    }
};

// ---------------------- Living Bomb (NPCBots) ----------------------
// When Living Bomb is applied to an NPCBot:
//  - The bot says a line.
//  - It finds a destination that has no OTHER units (players/creatures/NPCBots) within 25y
//    (ignores its own pet/guardian to avoid hunter/warlock deadlocks).
//  - It moves there immediately; if it has a pet/guardian, the pet is ordered to move too.
//  - After the aura expires, we restore FOLLOW bot command state.
class spell_geddon_living_bomb_aura : public AuraScript
{
    PrepareAuraScript(spell_geddon_living_bomb_aura);

    static constexpr float SAFE_CLEAR_RADIUS = 25.0f;
    static constexpr float SEARCH_RADIUS_MAX = 120.0f;

    bool Validate(SpellInfo const* /*spell*/) override { return true; }

    void CollectNearbyUnits(Unit* center, std::vector<Unit*>& out)
    {
        out.clear();
        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck chk(center, SEARCH_RADIUS_MAX);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> srch(center, nearby, chk);
        Cell::VisitObjects(center, srch, SEARCH_RADIUS_MAX);
        for (Unit* u : nearby)
            if (u && u->IsAlive())
                out.push_back(u);
    }

    void MovePetIfOwned(Unit* owner, Position const& dest)
    {
        if (!owner) return;

        ObjectGuid ownerGuid = owner->GetGUID();

        std::list<Unit*> nearby;
        Acore::AnyUnitInObjectRangeCheck chk(owner, 40.0f);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> srch(owner, nearby, chk);
        Cell::VisitObjects(owner, srch, 40.0f);

        for (Unit* u : nearby)
        {
            if (!u->IsAlive()) continue;
            if (u->GetOwnerGUID() != ownerGuid) continue;

            if (Creature* c = u->ToCreature())
            {
                c->AttackStop();
                c->GetMotionMaster()->Clear();
                c->GetMotionMaster()->MovePoint(0, dest);
            }
        }
    }

    bool IsCandidateClear(Position const& p, Unit* carrier, std::vector<Unit*> const& crowd)
    {
        ObjectGuid ownerGuid = carrier->GetGUID();

        for (Unit* u : crowd)
        {
            if (u == carrier) continue;
            if (u->GetOwnerGUID() == ownerGuid) continue;

            float d2d = u->GetDistance2d(p.GetPositionX(), p.GetPositionY());
            if (d2d < SAFE_CLEAR_RADIUS)
                return false;
        }
        return true;
    }

    bool IsPathableAndInLOS(Unit* carrier, Position& cand)
    {
        Map* map = carrier->GetMap();
        float dx = cand.GetPositionX();
        float dy = cand.GetPositionY();
        float dz = cand.GetPositionZ();

        if (!map->CanReachPositionAndGetValidCoords(carrier, dx, dy, dz, /*failOnCollision*/true, /*failOnSlopes*/true)) // Map.h
            return false;                                                                                                 // :contentReference[oaicite:3]{index=3}

        cand.Relocate(dx, dy, dz, cand.GetOrientation());

        if (!carrier->IsWithinLOS(dx, dy, dz))
            return false;

        if (!map->isInLineOfSight(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ(),
            dx, dy, dz, carrier->GetPhaseMask(),
            LineOfSightChecks::LINEOFSIGHT_ALL_CHECKS,
            VMAP::ModelIgnoreFlags::M2))                                                          // :contentReference[oaicite:4]{index=4}
        {
            return false;
        }

        return true;
    }

    bool FindSafeSpot(Unit* carrier, Position& out)
    {
        std::vector<Unit*> crowd;
        CollectNearbyUnits(carrier, crowd);

        float bx = carrier->GetPositionX();
        float by = carrier->GetPositionY();
        float bz = carrier->GetPositionZ();

        static constexpr float radii[] = { 30.f, 35.f, 40.f, 45.f, 50.f, 55.f, 60.f, 65.f, 70.f };
        for (float r : radii)
        {
            uint32 const N = 24; 
            for (uint32 i = 0; i < N; ++i)
            {
                float ang = float(M_PI) * 2.f * (float(i) / float(N));
                float nx = bx + r * std::cos(ang);
                float ny = by + r * std::sin(ang);
                float nz = bz;

                carrier->UpdateAllowedPositionZ(nx, ny, nz);

                Position cand; cand.Relocate(nx, ny, nz, ang);

                if (IsCandidateClear(cand, carrier, crowd) && IsPathableAndInLOS(carrier, cand))
                {
                    out = cand;
                    return true;
                }
            }
        }
        return false;
    }

    void SafeMove(Unit* carrier, Position const& dest)
    {
        if (carrier->IsPlayer())
            return;

        if (IsNPCBot(carrier))
        {
            if (bot_ai* ai = carrier->ToCreature()->GetBotAI())
            {
                ai->MoveToSendPosition(dest);
                return;
            }
        }

        if (Creature* c = carrier->ToCreature())
        {
            c->AttackStop();
            c->GetMotionMaster()->Clear();
            c->GetMotionMaster()->MovePoint(0, dest);
        }
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target || !target->IsAlive())
            return;

        if (target->IsPlayer())
            return;

        if (IsNPCBot(target))
            BotSay(target, "Living Bomb on me!");

        Position dest;
        if (FindSafeSpot(target, dest))
        {
            SafeMove(target, dest);

            if (IsNPCBot(target))
                MovePetIfOwned(target, dest);
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* c = GetTarget()->ToCreature())
        {
            if (c->IsNPCBot())
            {
                if (bot_ai* ai = c->GetBotAI())
                {
                    ai->SetBotCommandState(BOT_COMMAND_FOLLOW);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_geddon_living_bomb_aura::OnApply, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_geddon_living_bomb_aura::OnRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_baron_geddon()
{
    new boss_baron_geddon();

    RegisterSpellScript(spell_geddon_inferno_aura);
    RegisterSpellScript(spell_geddon_armageddon_aura);
    RegisterSpellScript(spell_geddon_living_bomb_aura);
}
