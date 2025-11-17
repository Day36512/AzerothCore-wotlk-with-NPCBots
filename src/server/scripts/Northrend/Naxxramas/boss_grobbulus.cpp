/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "naxxramas.h"
#include <cmath>
#include <chrono>
#include <vector>
#include <limits>
#include <unordered_map>

 // NPCBots
#include "botmgr.h"
#include "bot_ai.h"

enum Spells
{
    SPELL_POISON_CLOUD                      = 28240,
    SPELL_MUTATING_INJECTION                = 28169,
    SPELL_MUTATING_EXPLOSION                = 28206,
    SPELL_SLIME_SPRAY                       = 28157,
    SPELL_POISON_CLOUD_DAMAGE_AURA          = 28158,
    SPELL_BERSERK                           = 26662,
    SPELL_BOMBARD_SLIME                     = 28280
};

enum Emotes
{
    EMOTE_SLIME                             = 0
};

enum Events
{
    EVENT_BERSERK                           = 1,
    EVENT_POISON_CLOUD                      = 2,
    EVENT_SLIME_SPRAY                       = 3,
    EVENT_MUTATING_INJECTION                = 4
};

enum Misc
{
    NPC_FALLOUT_SLIME                       = 16290,
    NPC_SEWAGE_SLIME                        = 16375,
    NPC_STICHED_GIANT                       = 16025
};

class boss_grobbulus : public CreatureScript
{
public:
    boss_grobbulus() : CreatureScript("boss_grobbulus") { }

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetNaxxramasAI<boss_grobbulusAI>(pCreature);
    }

    struct boss_grobbulusAI : public BossAI
    {
        explicit boss_grobbulusAI(Creature* c) : BossAI(c, BOSS_GROBBULUS), summons(me)
        {}

        EventMap events;
        SummonList summons;
        uint32 dropSludgeTimer{};

        void Reset() override
        {
            BossAI::Reset();
            events.Reset();
            summons.DespawnAll();
            dropSludgeTimer = 0;
        }

        void PullChamberAdds()
        {
            std::list<Creature*> StichedGiants;
            me->GetCreaturesWithEntryInRange(StichedGiants, 300.0f, NPC_STICHED_GIANT);
            for (std::list<Creature*>::const_iterator itr = StichedGiants.begin(); itr != StichedGiants.end(); ++itr)
            {
                (*itr)->ToCreature()->AI()->AttackStart(me->GetVictim());
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            BossAI::JustEngagedWith(who);
            PullChamberAdds();
            me->SetInCombatWithZone();
            events.ScheduleEvent(EVENT_POISON_CLOUD, 15s);
            events.ScheduleEvent(EVENT_MUTATING_INJECTION, 20s);
            events.ScheduleEvent(EVENT_SLIME_SPRAY, 10s);
            events.ScheduleEvent(EVENT_BERSERK, RAID_MODE(720s, 540s));
        }

        void JustSummoned(Creature* cr) override
        {
            if (cr->GetEntry() == NPC_FALLOUT_SLIME)
            {
                cr->SetInCombatWithZone();
            }
            summons.Summon(cr);
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            summons.Despawn(summon);
        }

        void JustDied(Unit*  killer) override
        {
            BossAI::JustDied(killer);
            summons.DespawnAll();
        }

        void KilledUnit(Unit* who) override
        {
            if (who->IsPlayer())
                instance->StorePersistentData(PERSISTENT_DATA_IMMORTAL_FAIL, 1);
        }

        void UpdateAI(uint32 diff) override
        {
            dropSludgeTimer += diff;
            if (!me->IsInCombat() && dropSludgeTimer >= 5000)
            {
                if (me->IsWithinDist3d(3178, -3305, 319, 5.0f) && !summons.HasEntry(NPC_SEWAGE_SLIME))
                {
                    me->CastSpell(3128.96f + irand(-20, 20), -3312.96f + irand(-20, 20), 293.25f, SPELL_BOMBARD_SLIME, false);
                }
                dropSludgeTimer = 0;
            }

            if (!UpdateVictim())
                return;

            events.Update(diff);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            switch (events.ExecuteEvent())
            {
                case EVENT_POISON_CLOUD:
                    me->CastSpell(me, SPELL_POISON_CLOUD, true);
                    events.Repeat(15s);
                    break;
                case EVENT_BERSERK:
                    me->CastSpell(me, SPELL_BERSERK, true);
                    break;
                case EVENT_SLIME_SPRAY:
                    Talk(EMOTE_SLIME);
                    me->CastSpell(me->GetVictim(), SPELL_SLIME_SPRAY, false);
                    events.Repeat(20s);
                    break;
                case EVENT_MUTATING_INJECTION:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true, false, -SPELL_MUTATING_INJECTION))
                    {
                        me->CastSpell(target, SPELL_MUTATING_INJECTION, false);
                    }
                    events.Repeat(Milliseconds(6000 + uint32(120 * me->GetHealthPct())));
                    break;
            }
            DoMeleeAttackIfReady();
        }
    };
};

class boss_grobbulus_poison_cloud : public CreatureScript
{
public:
    boss_grobbulus_poison_cloud() : CreatureScript("boss_grobbulus_poison_cloud") { }

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetNaxxramasAI<boss_grobbulus_poison_cloudAI>(pCreature);
    }

    struct boss_grobbulus_poison_cloudAI : public NullCreatureAI
    {
        explicit boss_grobbulus_poison_cloudAI(Creature* pCreature) : NullCreatureAI(pCreature) { }

        uint32 sizeTimer{};
        uint32 auraVisualTimer{};

        void Reset() override
        {
            sizeTimer = 0;
            auraVisualTimer = 1;
            me->SetFloatValue(UNIT_FIELD_COMBATREACH, 2.0f);
            me->SetFaction(FACTION_BOOTY_BAY);
        }

        void KilledUnit(Unit* who) override
        {
            if (who->IsPlayer())
                me->GetInstanceScript()->StorePersistentData(PERSISTENT_DATA_IMMORTAL_FAIL, 1);
        }

        void UpdateAI(uint32 diff) override
        {
            if (auraVisualTimer) // this has to be delayed to be visible
            {
                auraVisualTimer += diff;
                if (auraVisualTimer >= 1000)
                {
                    me->CastSpell(me, SPELL_POISON_CLOUD_DAMAGE_AURA, true);
                    auraVisualTimer = 0;
                }
            }
            sizeTimer += diff; // increase size to 15yd in 60 seconds, 0.00025 is the growth of size in 1ms
            me->SetFloatValue(UNIT_FIELD_COMBATREACH, 2.0f + (0.00025f * sizeTimer));
        }
    };
};

class spell_grobbulus_poison : public SpellScript
{
    PrepareSpellScript(spell_grobbulus_poison);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        std::list<WorldObject*> tmplist;
        for (auto& target : targets)
        {
            if (GetCaster()->IsWithinDist3d(target, 0.0f))
            {
                tmplist.push_back(target);
            }
        }
        targets.clear();
        for (auto& itr : tmplist)
        {
            targets.push_back(itr);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_grobbulus_poison::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_grobbulus_mutating_injection_aura : public AuraScript
{
    PrepareAuraScript(spell_grobbulus_mutating_injection_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MUTATING_EXPLOSION });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        switch (GetTargetApplication()->GetRemoveMode())
        {
            case AURA_REMOVE_BY_ENEMY_SPELL:
            case AURA_REMOVE_BY_EXPIRE:
                if (auto caster = GetCaster())
                {
                    caster->CastSpell(GetTarget(), SPELL_MUTATING_EXPLOSION, true);
                }
                break;
            default:
                return;
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_grobbulus_mutating_injection_aura::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

using namespace std::chrono_literals;

namespace GrobDirector
{
    // -------------------------------------------------------
    // Map / Spells (Slime Spray 10/25)
    // -------------------------------------------------------
    constexpr uint32 MAP_NAXX = 533;
    constexpr uint32 SPELL_SLIME_10 = 28157;
    constexpr uint32 SPELL_SLIME_25 = 54364;

    inline bool IsOurSpray(uint32 id) { return id == SPELL_SLIME_10 || id == SPELL_SLIME_25; }

    // -------------------------------------------------------
    // Movement tuning – ONE segment per spray (clockwise)
    // -------------------------------------------------------
    constexpr int   kSubStepsPerSegment = 3;       // micro-steps within the single segment
    constexpr auto  kStepPeriod = 700ms;   // 3 sub-steps * 0.5s ≈ 1.5s per spray movement
    constexpr float kMaxMoveDistance = 80.0f;   // sanity guard for MoveToSendPosition
    constexpr float kMinAdvanceDist = 1.5f;    // if next point is closer than this, skip to the next one again

    // -----------------------
    // Helpers
    // -----------------------
    inline float Dist2D(Position const& a, Position const& b)
    {
        float dx = a.GetPositionX() - b.GetPositionX();
        float dy = a.GetPositionY() - b.GetPositionY();
        return std::sqrt(dx * dx + dy * dy);
    }

    inline Position Lerp(Position const& a, Position const& b, float t)
    {
        Position out;
        out.Relocate(a.GetPositionX() + (b.GetPositionX() - a.GetPositionX()) * t,
            a.GetPositionY() + (b.GetPositionY() - a.GetPositionY()) * t,
            a.GetPositionZ() + (b.GetPositionZ() - a.GetPositionZ()) * t,
            0.0f);
        return out;
    }

    inline bool IsNPCTankBot(Creature* c)
    {
        if (!c || !c->IsAlive() || !c->IsNPCBot())
            return false;
        if (bot_ai* ai = c->GetBotAI())
            return ai->HasRole(BOT_ROLE_TANK);
        return false;
    }

    inline void SafeMoveBotTo(Creature* bot, Position const& dest)
    {
        if (!bot || !bot->IsAlive())
            return;

        float x = dest.GetPositionX();
        float y = dest.GetPositionY();
        float z = dest.GetPositionZ();
        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z); // see Creature/Unit movement helpers

        Position finalPos;
        finalPos.Relocate(x, y, z, 0.0f);

        if (finalPos.GetExactDist(*bot) > kMaxMoveDistance)
            return;

        if (bot_ai* ai = bot->GetBotAI())
            ai->MoveToSendPosition(finalPos);
    }

    // -----------------------
    // Waypoint source: REQUIRED DB table (no fallback)
    // Table: custom_grobbulus_edge_points(idx INT PK, x FLOAT, y FLOAT, z FLOAT), ordered CCW.
    // We traverse CLOCKWISE by decrementing index with wrap.
    // -----------------------
    struct EdgeCache
    {
        std::vector<Position> pts;
        bool loaded = false;

        void EnsureLoaded()
        {
            if (loaded)
                return;
            loaded = true;
            pts.clear();

            if (QueryResult res = WorldDatabase.Query(
                "SELECT `x`,`y`,`z` FROM `custom_grobbulus_edge_points` ORDER BY `idx` ASC"))
            {
                do
                {
                    Field* f = res->Fetch();
                    float x = f[0].Get<float>();
                    float y = f[1].Get<float>();
                    float z = f[2].Get<float>();
                    Position p; p.Relocate(x, y, z, 0.0f);
                    pts.push_back(p);
                } while (res->NextRow());
            }

            if (pts.size() < 3)
            {
                LOG_ERROR("npcbots.grobbulus",
                    "Director: perimeter table `custom_grobbulus_edge_points` returned < 3 points. Movement disabled.");
            }
            else
            {
                LOG_INFO("npcbots.grobbulus", "Director: loaded perimeter (%zu points) from DB.", pts.size());
            }
        }

        int FindNearestIndex(Position const& from) const
        {
            float best = std::numeric_limits<float>::max();
            int   idx = 0;
            for (int i = 0; i < static_cast<int>(pts.size()); ++i)
            {
                float d = Dist2D(from, pts[i]);
                if (d < best)
                {
                    best = d;
                    idx = i;
                }
            }
            return idx;
        }
    };

    static EdgeCache g_edge;

    // Persist progress per boss across casts (map-thread only usage).
    static std::unordered_map<ObjectGuid::LowType, int> g_lastIndex;

    inline int GetBossKey(Creature* boss)
    {
        // Use Low GUID counter as the key; stable for lifetime of the creature.
        return static_cast<int>(boss->GetGUID().GetCounter());
    }

    // Decide the "from" index: first time -> nearest; thereafter -> last stored.
    inline int ResolveFromIndex(Creature* boss, Creature* bot)
    {
        int N = static_cast<int>(g_edge.pts.size());
        if (N < 3)
            return 0;

        int key = GetBossKey(boss);
        auto it = g_lastIndex.find(key);
        if (it != g_lastIndex.end())
            return it->second % N;

        // First time for this boss: snap to nearest edge point to the bot
        return g_edge.FindNearestIndex(bot->GetPosition());
    }

    // Compute the next clockwise index, with a tiny-distance guard to avoid puddle stacking.
    inline int ComputeNextIndex(int fromIdx, int N, Position const& botPos)
    {
        int toIdx = (fromIdx - 1 + N) % N;
        if (Dist2D(botPos, g_edge.pts[toIdx]) < kMinAdvanceDist)
            toIdx = (toIdx - 1 + N) % N; // skip one more if we’re basically already there
        return toIdx;
    }

    // Schedule movement along ONE segment (from -> to) with micro-steps.
    inline void ScheduleAdvanceOneSegment(Creature* bot, int fromIdx, int toIdx)
    {
        if (!bot || !bot->IsInWorld())
            return;

        Position from = g_edge.pts[fromIdx];
        Position to = g_edge.pts[toIdx];

        for (int s = 1; s <= kSubStepsPerSegment; ++s)
        {
            float t = float(s) / float(kSubStepsPerSegment);
            Position stepPos = Lerp(from, to, t);

            auto at = kStepPeriod * s;
            bot->m_Events.AddEventAtOffset([bot, stepPos]()
                {
                    if (!bot || !bot->IsAlive() || !bot->IsInWorld())
                        return;
                    SafeMoveBotTo(bot, stepPos);
                }, at);
        }
    }
} // namespace GrobDirector

// Spell hook: BEFORE CAST of Slime Spray → move tank bot ONE waypoint clockwise (persist progress)
class spell_grobbulus_slime_spray_director : public SpellScript
{
    PrepareSpellScript(spell_grobbulus_slime_spray_director);

    void HandleBeforeCast()
    {
        Unit* caster = GetCaster();
        if (!caster || caster->GetMapId() != GrobDirector::MAP_NAXX)
            return;

        uint32 spellId = GetSpellInfo() ? GetSpellInfo()->Id : 0;
        if (!GrobDirector::IsOurSpray(spellId))
            return;

        Creature* boss = caster->ToCreature();
        if (!boss || !boss->IsAlive())
            return;

        Unit* victim = boss->GetVictim();
        Creature* botTank = victim ? victim->ToCreature() : nullptr;
        if (!GrobDirector::IsNPCTankBot(botTank))
            return;

        GrobDirector::g_edge.EnsureLoaded();
        int N = static_cast<int>(GrobDirector::g_edge.pts.size());
        if (N < 3)
            return;

        // Resolve from-index (persisted per boss), then compute next clockwise index.
        int fromIdx = GrobDirector::ResolveFromIndex(boss, botTank);
        int toIdx = GrobDirector::ComputeNextIndex(fromIdx, N, botTank->GetPosition());

        // Store progress for the next spray.
        GrobDirector::g_lastIndex[GrobDirector::GetBossKey(boss)] = toIdx;

        LOG_INFO("npcbots.grobbulus",
            "Director: BEFORE CAST Slime Spray -> bot [{}] fromIdx={} toIdx={} (N={})",
            botTank->GetGUID().ToString().c_str(), fromIdx, toIdx, N);

        // Schedule the single advance with micro-steps.
        GrobDirector::ScheduleAdvanceOneSegment(botTank, fromIdx, toIdx);
    }

    void HandleHit()
    {
        if (Unit* caster = GetCaster())
        {
            if (Map* map = caster->GetMap())
            {
                if (map->IsRaid() && map->IsHeroic() && !map->Is25ManRaid())
                    return;
            }

            if (Unit* target = GetHitUnit())
                caster->SummonCreature(NPC_FALLOUT_SLIME, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_grobbulus_slime_spray_director::HandleBeforeCast);
        OnHit += SpellHitFn(spell_grobbulus_slime_spray_director::HandleHit);
    }
};

void AddSC_boss_grobbulus()
{
    new boss_grobbulus();
    new boss_grobbulus_poison_cloud();
    RegisterSpellScript(spell_grobbulus_mutating_injection_aura);
    RegisterSpellScript(spell_grobbulus_poison);
    RegisterSpellScript(spell_grobbulus_slime_spray_director);
}
