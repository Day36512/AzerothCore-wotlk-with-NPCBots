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
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "naxxramas.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "Config.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <vector>

 // NPCBots
#include "botmgr.h"
#include "bot_ai.h"

using namespace std::chrono_literals;

enum Spells
{
    SPELL_POISON_CLOUD = 28240,
    SPELL_MUTATING_INJECTION = 28169,
    SPELL_MUTATING_EXPLOSION = 28206,
    SPELL_SLIME_SPRAY = 28157,
    SPELL_POISON_CLOUD_DAMAGE_AURA = 28158,
    SPELL_BERSERK = 26662,
    SPELL_BOMBARD_SLIME = 28280
};

enum Emotes
{
    EMOTE_SLIME = 0
};

enum Events
{
    EVENT_BERSERK = 1,
    EVENT_POISON_CLOUD = 2,
    EVENT_SLIME_SPRAY = 3,
    EVENT_MUTATING_INJECTION = 4
};

enum Misc
{
    NPC_FALLOUT_SLIME = 16290,
    NPC_SEWAGE_SLIME = 16375,
    NPC_STICHED_GIANT = 16025
};

class boss_grobbulus : public CreatureScript
{
public:
    boss_grobbulus() : CreatureScript("boss_grobbulus") {}

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetNaxxramasAI<boss_grobbulusAI>(pCreature);
    }

    struct boss_grobbulusAI : public BossAI
    {
        explicit boss_grobbulusAI(Creature* c) : BossAI(c, BOSS_GROBBULUS), summons(me) {}

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

        void PullChamberAdds(Unit* target)
        {
            if (!target)
                return;

            std::list<Creature*> stitchedGiants;
            me->GetCreaturesWithEntryInRange(stitchedGiants, 300.0f, NPC_STICHED_GIANT);
            for (Creature* giant : stitchedGiants)
                giant->AI()->AttackStart(target);
        }

        void JustEngagedWith(Unit* who) override
        {
            BossAI::JustEngagedWith(who);
            PullChamberAdds(who);
            me->SetInCombatWithZone();
            events.ScheduleEvent(EVENT_POISON_CLOUD, 15s);
            events.ScheduleEvent(EVENT_MUTATING_INJECTION, 20s);
            events.ScheduleEvent(EVENT_SLIME_SPRAY, 10s);
            events.ScheduleEvent(EVENT_BERSERK, RAID_MODE(720s, 540s));
        }

        void JustSummoned(Creature* cr) override
        {
            if (cr->GetEntry() == NPC_FALLOUT_SLIME)
                cr->SetInCombatWithZone();

            summons.Summon(cr);
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            summons.Despawn(summon);
        }

        void JustDied(Unit* killer) override
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
                if (me->IsWithinDist3d(3178.0f, -3305.0f, 319.0f, 5.0f) && !summons.HasEntry(NPC_SEWAGE_SLIME))
                    me->CastSpell(3128.96f + irand(-20, 20), -3312.96f + irand(-20, 20), 293.25f, SPELL_BOMBARD_SLIME, false);

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
                    me->CastSpell(target, SPELL_MUTATING_INJECTION, false);

                events.Repeat(Milliseconds(6000 + uint32(120 * me->GetHealthPct())));
                break;
            default:
                break;
            }

            DoMeleeAttackIfReady();
        }
    };
};

class boss_grobbulus_poison_cloud : public CreatureScript
{
public:
    boss_grobbulus_poison_cloud() : CreatureScript("boss_grobbulus_poison_cloud") {}

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetNaxxramasAI<boss_grobbulus_poison_cloudAI>(pCreature);
    }

    struct boss_grobbulus_poison_cloudAI : public NullCreatureAI
    {
        explicit boss_grobbulus_poison_cloudAI(Creature* pCreature) : NullCreatureAI(pCreature) {}

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
            if (auraVisualTimer)
            {
                auraVisualTimer += diff;
                if (auraVisualTimer >= 1000)
                {
                    me->CastSpell(me, SPELL_POISON_CLOUD_DAMAGE_AURA, true);
                    auraVisualTimer = 0;
                }
            }

            sizeTimer += diff;
            me->SetFloatValue(UNIT_FIELD_COMBATREACH, 2.0f + (0.00025f * sizeTimer));
        }
    };
};

class spell_grobbulus_poison : public SpellScript
{
    PrepareSpellScript(spell_grobbulus_poison);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        std::list<WorldObject*> filtered;
        for (WorldObject* target : targets)
        {
            if (GetCaster()->IsWithinDist3d(target, 0.0f))
                filtered.push_back(target);
        }

        targets.clear();
        for (WorldObject* itr : filtered)
            targets.push_back(itr);
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
            if (Unit* caster = GetCaster())
                caster->CastSpell(GetTarget(), SPELL_MUTATING_EXPLOSION, true);
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

namespace GrobDirector
{
    constexpr uint32 MAP_NAXX = 533;
    constexpr uint32 SPELL_SLIME_10 = 28157;
    constexpr uint32 SPELL_SLIME_25 = 54364;

    constexpr int   kSubStepsPerSegment = 3;
    constexpr auto  kStepPeriod = 700ms;
    constexpr float kMaxMoveDistance = 80.0f;
    constexpr float kMinAdvanceDist = 1.5f;
    constexpr float kTankScanRange = 120.0f;

    inline bool IsOurSpray(uint32 id)
    {
        return id == SPELL_SLIME_10 || id == SPELL_SLIME_25;
    }

    inline float GetVictimThreatBonus()
    {
        return std::max(0.0f, sConfigMgr->GetOption<float>("Grobbulus.SlimeSpray.VictimThreatBonus", 2500.0f));
    }

    inline float GetNpcBotFalloutHealthScale()
    {
        return std::clamp(sConfigMgr->GetOption<float>("Grobbulus.FalloutSlime.NPCBotTarget.HealthScale", 0.67f), 0.01f, 1.0f);
    }

    inline bool OfftankThreatEnabled()
    {
        return sConfigMgr->GetOption<bool>("Grobbulus.FalloutSlime.OfftankThreat.Enable", true);
    }

    inline float GetOfftankThreatAmount()
    {
        return std::max(0.0f, sConfigMgr->GetOption<float>("Grobbulus.FalloutSlime.OfftankThreat.Amount", 3500.0f));
    }

    inline uint32 GetOfftankThreatTimes()
    {
        return std::min<uint32>(sConfigMgr->GetOption<uint32>("Grobbulus.FalloutSlime.OfftankThreat.Times", 3u), 10u);
    }

    inline uint32 GetOfftankThreatPeriodMs()
    {
        return std::max<uint32>(sConfigMgr->GetOption<uint32>("Grobbulus.FalloutSlime.OfftankThreat.PeriodMs", 600u), 100u);
    }

    inline float GetOfftankThreatRange()
    {
        return std::clamp(sConfigMgr->GetOption<float>("Grobbulus.FalloutSlime.OfftankThreat.Range", 100.0f), 5.0f, 200.0f);
    }

    inline float Dist2D(Position const& a, Position const& b)
    {
        float dx = a.GetPositionX() - b.GetPositionX();
        float dy = a.GetPositionY() - b.GetPositionY();
        return std::sqrt(dx * dx + dy * dy);
    }

    inline Position Lerp(Position const& a, Position const& b, float t)
    {
        Position out;
        out.Relocate(
            a.GetPositionX() + (b.GetPositionX() - a.GetPositionX()) * t,
            a.GetPositionY() + (b.GetPositionY() - a.GetPositionY()) * t,
            a.GetPositionZ() + (b.GetPositionZ() - a.GetPositionZ()) * t,
            0.0f);
        return out;
    }

    inline bool IsNPCBotUnit(Unit* u)
    {
        if (!u || u->GetTypeId() != TYPEID_UNIT)
            return false;

        if (Creature* c = u->ToCreature())
            return c->IsAlive() && c->IsNPCBot();

        return false;
    }

    inline bool IsNPCTankBot(Creature* c)
    {
        if (!c || !c->IsAlive() || !c->IsNPCBot())
            return false;

        if (bot_ai* ai = c->GetBotAI())
            return ai->HasRole(BOT_ROLE_TANK);

        return false;
    }

    inline bool IsEncounterTankBot(Creature* boss, Creature* bot)
    {
        if (!boss || !bot)
            return false;

        if (!IsNPCTankBot(bot))
            return false;

        if (!bot->IsInWorld() || bot->GetMap() != boss->GetMap())
            return false;

        if (!bot->IsInCombat())
            return false;

        if (!boss->IsWithinDistInMap(bot, kTankScanRange))
            return false;

        return true;
    }

    inline void SafeMoveBotTo(Creature* bot, Position const& dest)
    {
        if (!bot || !bot->IsAlive())
            return;

        float x = dest.GetPositionX();
        float y = dest.GetPositionY();
        float z = dest.GetPositionZ();
        if (!bot->CanFly())
            bot->UpdateAllowedPositionZ(x, y, z);

        Position finalPos;
        finalPos.Relocate(x, y, z, 0.0f);

        if (finalPos.GetExactDist(*bot) > kMaxMoveDistance)
            return;

        if (bot_ai* ai = bot->GetBotAI())
            ai->MoveToSendPosition(finalPos);
    }

    inline void SetNonVictimTanksToFollow(Creature* boss, Unit* victim)
    {
        if (!boss || !boss->IsInWorld())
            return;

        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck checker(boss, kTankScanRange);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(boss, units, checker);
        Cell::VisitObjects(boss, searcher, kTankScanRange);

        for (Unit* unit : units)
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!IsEncounterTankBot(boss, bot))
                continue;

            if (bot == victim)
                continue;

            if (bot_ai* ai = bot->GetBotAI())
            {
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);

                LOG_DEBUG("npcbots.grobbulus",
                    "Director: Slime Spray released non-victim tank bot [{}] back to FOLLOW.",
                    bot->GetGUID().ToString().c_str());
            }
        }
    }

    inline void AddThreatToCurrentVictim(Creature* boss, Unit* victim)
    {
        if (!boss || !victim || !victim->IsAlive())
            return;

        float threat = GetVictimThreatBonus();
        if (threat <= 0.0f)
            return;

        boss->AddThreat(victim, threat);

        LOG_DEBUG("npcbots.grobbulus",
            "Director: Slime Spray added {:.1f} threat to victim [{}].",
            threat, victim->GetGUID().ToString().c_str());
    }

    inline void ApplyNpcBotSpawnHealthTuning(Creature* slime)
    {
        if (!slime || !slime->IsAlive())
            return;

        uint32 originalMaxHealth = slime->GetMaxHealth();
        if (originalMaxHealth == 0)
            return;

        uint32 tunedHealth = std::max<uint32>(1u, uint32(std::lround(double(originalMaxHealth) * GetNpcBotFalloutHealthScale())));

        slime->SetCreateHealth(tunedHealth);
        slime->SetMaxHealth(tunedHealth);
        slime->SetHealth(tunedHealth);

        LOG_DEBUG("npcbots.grobbulus",
            "Director: tuned Fallout Slime [{}] spawned from NPCBot target to {} max health (from {}).",
            slime->GetGUID().ToString().c_str(), tunedHealth, originalMaxHealth);
    }

    inline Creature* FindNearestOfftankBot(Unit* anchor, float radius)
    {
        if (!anchor || !anchor->IsInWorld())
            return nullptr;

        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck checker(anchor, radius);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(anchor, units, checker);
        Cell::VisitObjects(anchor, searcher, radius);

        Creature* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();

        for (Unit* unit : units)
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
                continue;

            if (bot_ai* ai = bot->GetBotAI())
            {
                if (!ai->IsOffTank())
                    continue;

                float dist = anchor->GetDistance(bot);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = bot;
                }
            }
        }

        return best;
    }

    inline void ScheduleOfftankThreatPings(Creature* slime)
    {
        if (!slime || !slime->IsAlive() || !slime->IsInWorld())
            return;

        if (!OfftankThreatEnabled())
            return;

        float threatAmount = GetOfftankThreatAmount();
        uint32 times = GetOfftankThreatTimes();
        uint32 periodMs = GetOfftankThreatPeriodMs();
        float range = GetOfftankThreatRange();

        if (threatAmount <= 0.0f || times == 0)
            return;

        for (uint32 i = 0; i < times; ++i)
        {
            auto at = std::chrono::milliseconds(periodMs * (i + 1));
            slime->m_Events.AddEventAtOffset([slime, threatAmount, range]()
                {
                    if (!slime || !slime->IsAlive() || !slime->IsInWorld())
                        return;

                    Creature* offtank = FindNearestOfftankBot(slime, range);
                    if (!offtank)
                        return;

                    slime->AddThreat(offtank, threatAmount);

                    if (!slime->GetVictim() && slime->AI())
                        slime->AI()->AttackStart(offtank);

                    LOG_DEBUG("npcbots.grobbulus",
                        "Director: Fallout Slime [{}] added {:.1f} threat to offtank bot [{}].",
                        slime->GetGUID().ToString().c_str(), threatAmount, offtank->GetGUID().ToString().c_str());
                }, at);
        }
    }

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
                    Position p;
                    p.Relocate(f[0].Get<float>(), f[1].Get<float>(), f[2].Get<float>(), 0.0f);
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
                LOG_INFO("npcbots.grobbulus",
                    "Director: loaded perimeter (%zu points) from DB.",
                    pts.size());
            }
        }

        int FindNearestIndex(Position const& from) const
        {
            float best = std::numeric_limits<float>::max();
            int idx = 0;

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
    static std::unordered_map<ObjectGuid::LowType, int> g_lastIndex;

    inline int GetBossKey(Creature* boss)
    {
        return static_cast<int>(boss->GetGUID().GetCounter());
    }

    inline int ResolveFromIndex(Creature* boss, Creature* bot)
    {
        int n = static_cast<int>(g_edge.pts.size());
        if (n < 3)
            return 0;

        int key = GetBossKey(boss);
        auto itr = g_lastIndex.find(key);
        if (itr != g_lastIndex.end())
            return itr->second % n;

        return g_edge.FindNearestIndex(bot->GetPosition());
    }

    inline int ComputeNextIndex(int fromIdx, int n, Position const& botPos)
    {
        int toIdx = (fromIdx - 1 + n) % n;
        if (Dist2D(botPos, g_edge.pts[toIdx]) < kMinAdvanceDist)
            toIdx = (toIdx - 1 + n) % n;

        return toIdx;
    }

    inline void ScheduleAdvanceOneSegment(Creature* bot, int fromIdx, int toIdx)
    {
        if (!bot || !bot->IsInWorld())
            return;

        Position const from = g_edge.pts[fromIdx];
        Position const to = g_edge.pts[toIdx];

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

        GrobDirector::SetNonVictimTanksToFollow(boss, victim);
        GrobDirector::AddThreatToCurrentVictim(boss, victim);

        Creature* botTank = victim ? victim->ToCreature() : nullptr;
        if (!GrobDirector::IsEncounterTankBot(boss, botTank))
            return;

        GrobDirector::g_edge.EnsureLoaded();
        int n = static_cast<int>(GrobDirector::g_edge.pts.size());
        if (n < 3)
            return;

        if (bot_ai* ai = botTank->GetBotAI())
            ai->SetBotCommandState(BOT_COMMAND_STAY, true);

        int fromIdx = GrobDirector::ResolveFromIndex(boss, botTank);
        int toIdx = GrobDirector::ComputeNextIndex(fromIdx, n, botTank->GetPosition());

        GrobDirector::g_lastIndex[GrobDirector::GetBossKey(boss)] = toIdx;

        LOG_INFO("npcbots.grobbulus",
            "Director: BEFORE CAST Slime Spray -> victim [{}] fromIdx={} toIdx={} (N={})",
            botTank->GetGUID().ToString().c_str(), fromIdx, toIdx, n);

        GrobDirector::ScheduleAdvanceOneSegment(botTank, fromIdx, toIdx);
    }

    void HandleHit()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (Map* map = caster->GetMap())
        {
            if (map->IsRaid() && map->IsHeroic() && !map->Is25ManRaid())
                return;
        }

        Unit* target = GetHitUnit();
        if (!target)
            return;

        Creature* slime = caster->SummonCreature(
            NPC_FALLOUT_SLIME,
            target->GetPositionX(),
            target->GetPositionY(),
            target->GetPositionZ());

        if (!slime)
            return;

        if (GrobDirector::IsNPCBotUnit(target))
            GrobDirector::ApplyNpcBotSpawnHealthTuning(slime);

        GrobDirector::ScheduleOfftankThreatPings(slime);
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
