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
#include "Containers.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "SpellMgr.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "sunwell_plateau.h"
#include "VMapFactory.h"
#include "VMapMgr2.h"

#include <vector>

enum Spells
{
    SPELL_ENRAGE                        = 26662,
    SPELL_NEGATIVE_ENERGY               = 46009,
    SPELL_SUMMON_BLOOD_ELVES_PERIODIC   = 46041,
    SPELL_OPEN_PORTAL_PERIODIC          = 45994,
    SPELL_DARKNESS_PERIODIC             = 45998,
    SPELL_SUMMON_BERSERKER1             = 46037,
    SPELL_SUMMON_FURY_MAGE1             = 46038,
    SPELL_SUMMON_FURY_MAGE2             = 46039,
    SPELL_SUMMON_BERSERKER2             = 46040,
    SPELL_SUMMON_DARK_FIEND             = 46000, // till 46007
    SPELL_OPEN_ALL_PORTALS              = 46177,
    SPELL_SUMMON_ENTROPIUS              = 46217,

    // Entropius's spells
    SPELL_ENTROPIUS_COSMETIC_SPAWN      = 46223,
    SPELL_NEGATIVE_ENERGY_PERIODIC      = 46284,
    SPELL_BLACK_HOLE                    = 46282,
    SPELL_DARKNESS                      = 46269,
    SPELL_SUMMON_DARK_FIEND_ENTROPIUS   = 46263,

    //Black Hole Spells
    SPELL_BLACK_HOLE_SUMMON_VISUAL      = 46242,
    SPELL_BLACK_HOLE_SUMMON_VISUAL2     = 46247,
    SPELL_BLACK_HOLE_VISUAL2            = 46235,
    SPELL_BLACK_HOLE_PASSIVE            = 46228,
    SPELL_BLACK_HOLE_EFFECT             = 46230,

    // Dark Fiend Spells
    SPELL_DARK_FIEND_APPEARANCE         = 45934,
    SPELL_DARK_FIEND_SECONDARY          = 45936,
    SPELL_DARK_FIEND_TRIGGER            = 45944,
    // It is currently unkown why Dark Fiend Casts this or what it should do
    //SPELL_DARK_FIEND_TRIGGER_SINGLE   = 45943

    // NPCBot opening consumables
    SPELL_FLASK_OF_CHROMATIC_WONDER     = 42735,
    SPELL_SCROLL_OF_STAMINA             = 48101,

    // Custom Ashbringer support
    SPELL_ASHBRINGER                    = 28282,
    SPELL_ENDLESS_VOID                  = 600467
};

namespace
{
    constexpr float MURU_BOT_CONSUMABLE_RANGE = 150.0f;

    bool IsOwnedMuruNpcBot(Unit const* unit)
    {
        Creature const* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
    }

    bool IsMuruPlayerOrOwnedBot(Unit const* unit)
    {
        return unit && (unit->IsPlayer() || IsOwnedMuruNpcBot(unit));
    }

    bool IsMuruConsumableBot(Unit const* unit)
    {
        return unit && unit->IsAlive() && unit->IsInWorld() && IsOwnedMuruNpcBot(unit);
    }

    bool HasMuruFlaskAura(Unit const* unit)
    {
        if (!unit)
            return false;

        for (Unit::AuraApplicationMap::value_type const& pair : unit->GetAppliedAuras())
        {
            Aura const* aura = pair.second ? pair.second->GetBase() : nullptr;
            if (!aura)
                continue;

            uint32 const spellId = aura->GetId();
            if (sSpellMgr->IsSpellMemberOfSpellGroup(spellId, SPELL_GROUP_ELIXIR_BATTLE) &&
                sSpellMgr->IsSpellMemberOfSpellGroup(spellId, SPELL_GROUP_ELIXIR_GUARDIAN))
                return true;
        }

        return false;
    }

    void CastMuruKnownSelfSpell(Creature* bot, uint32 spellId)
    {
        if (bot && sSpellMgr->GetSpellInfo(spellId))
            bot->CastSpell(bot, spellId, true);
    }

    void AddMuruConsumableBot(Creature* source, std::vector<Creature*>& bots, GuidSet& seen, Unit* unit)
    {
        if (!source || !IsMuruConsumableBot(unit))
            return;

        Creature* bot = unit->ToCreature();
        if (!bot || !bot->IsInMap(source) || !bot->InSamePhase(source))
            return;

        if (!source->IsWithinDistInMap(bot, MURU_BOT_CONSUMABLE_RANGE))
            return;

        if (!seen.insert(bot->GetGUID()).second)
            return;

        bots.push_back(bot);
    }

    Player* GetMuruOwnerPlayer(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player;

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotOwner() : nullptr;
    }

    Group* GetMuruGroup(Unit* unit)
    {
        if (!unit)
            return nullptr;

        if (Player* player = unit->ToPlayer())
            return player->GetGroup();

        Creature* creature = unit->ToCreature();
        return creature && creature->IsNPCBot() ? creature->GetBotGroup() : nullptr;
    }

    void AddMuruOwnedBots(Creature* source, std::vector<Creature*>& bots, GuidSet& seen, Player* player)
    {
        if (!player || !player->HaveBot())
            return;

        BotMgr* botMgr = player->GetBotMgr();
        if (!botMgr)
            return;

        for (BotMap::value_type const& pair : *botMgr->GetBotMap())
            AddMuruConsumableBot(source, bots, seen, pair.second);
    }

    void AddMuruThreatBots(Creature* source, std::vector<Creature*>& bots, GuidSet& seen)
    {
        if (!source)
            return;

        for (ThreatReference const* ref : source->GetThreatMgr().GetSortedThreatList())
        {
            if (!ref || !ref->IsAvailable())
                continue;

            AddMuruConsumableBot(source, bots, seen, ref->GetVictim());
        }
    }

    std::vector<Creature*> GatherMuruConsumableBots(Creature* source, Unit* seed)
    {
        std::vector<Creature*> bots;
        GuidSet seen;

        if (!source)
            return bots;

        if (!seed)
            seed = source->GetThreatMgr().GetCurrentVictim();

        if (!seed)
            seed = source->GetVictim();

        if (Group* group = GetMuruGroup(seed))
        {
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
            {
                AddMuruConsumableBot(source, bots, seen, member);

                if (Player* player = member ? member->ToPlayer() : nullptr)
                    AddMuruOwnedBots(source, bots, seen, player);
            }
        }
        else
        {
            AddMuruConsumableBot(source, bots, seen, seed);

            if (Player* player = GetMuruOwnerPlayer(seed))
            {
                AddMuruConsumableBot(source, bots, seen, player);
                AddMuruOwnedBots(source, bots, seen, player);
            }
        }

        AddMuruThreatBots(source, bots, seen);
        return bots;
    }

    void CastMuruOpeningBotConsumables(Creature* source, Unit* seed)
    {
        for (Creature* bot : GatherMuruConsumableBots(source, seed))
        {
            if (HasMuruFlaskAura(bot))
                continue;

            CastMuruKnownSelfSpell(bot, SPELL_FLASK_OF_CHROMATIC_WONDER);
            CastMuruKnownSelfSpell(bot, SPELL_SCROLL_OF_STAMINA);
        }
    }

    bool IsValidSingularityTarget(Creature* source, Unit* unit)
    {
        if (!source || !unit || !unit->IsAlive() || !unit->IsInWorld() || unit->HasAura(SPELL_BLACK_HOLE_EFFECT))
            return false;

        if (!IsMuruPlayerOrOwnedBot(unit))
            return false;

        if (!unit->IsInMap(source) || !unit->InSamePhase(source))
            return false;

        if (!source->IsWithinDistInMap(unit, 90.0f) || !source->IsWithinLOSInMap(unit))
            return false;

        return true;
    }

    Unit* SelectSingularityTarget(Creature* source)
    {
        if (!source)
            return nullptr;

        std::list<Unit*> units;
        Bcore::AnyUnitInObjectRangeCheck check(source, 90.0f);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(source, units, check);
        Cell::VisitObjects(source, searcher, 90.0f);

        std::vector<Unit*> targets;
        for (Unit* unit : units)
            if (IsValidSingularityTarget(source, unit))
                targets.push_back(unit);

        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    bool IsValidEndlessVoidUnit(Creature* source, Unit* unit)
    {
        return source && unit && unit->IsAlive() && unit->IsInWorld() && IsMuruPlayerOrOwnedBot(unit) &&
            unit->IsInMap(source) && unit->InSamePhase(source);
    }

    void MaintainMuruEndlessVoidAura(Creature* source, GuidSet& trackedTargets)
    {
        if (!source || !sSpellMgr->GetSpellInfo(SPELL_ENDLESS_VOID))
            return;

        std::list<Unit*> units;
        Bcore::AnyUnitInObjectRangeCheck check(source, 100.0f);
        Bcore::UnitListSearcher<Bcore::AnyUnitInObjectRangeCheck> searcher(source, units, check);
        Cell::VisitObjects(source, searcher, 100.0f);

        for (Unit* unit : units)
        {
            if (!IsValidEndlessVoidUnit(source, unit))
                continue;

            if (!unit->HasAura(SPELL_ASHBRINGER))
            {
                unit->RemoveAurasDueToSpell(SPELL_ENDLESS_VOID);
                trackedTargets.erase(unit->GetGUID());
                continue;
            }

            if (!source->IsWithinLOSInMap(unit))
                continue;

            if (!unit->HasAura(SPELL_ENDLESS_VOID))
                unit->AddAura(SPELL_ENDLESS_VOID, unit);

            trackedTargets.insert(unit->GetGUID());
        }

        for (GuidSet::iterator itr = trackedTargets.begin(); itr != trackedTargets.end();)
        {
            Unit* unit = ObjectAccessor::GetUnit(*source, *itr);
            if (!unit || !unit->IsInWorld())
            {
                itr = trackedTargets.erase(itr);
                continue;
            }

            if (!unit->HasAura(SPELL_ASHBRINGER))
            {
                unit->RemoveAurasDueToSpell(SPELL_ENDLESS_VOID);
                itr = trackedTargets.erase(itr);
                continue;
            }

            ++itr;
        }
    }
}

struct boss_muru : public BossAI
{
    boss_muru(Creature* creature) : BossAI(creature, DATA_MURU) { }

    void Reset() override
    {
        BossAI::Reset();
        me->SetReactState(REACT_PASSIVE);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        me->SetVisible(true);
        me->m_Events.KillAllEvents(false);
        _endlessVoidTimer = 0;
        _endlessVoidTargets.clear();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // Radius of room is ~38.5f this might need adjusting a bit
        // Radius ~36.0 is right inside
        // Radius 20.0 is outer circle
        if (!me->IsInCombat() && who->IsPlayer() && who->GetPositionZ() > 69.0f && me->IsWithinDistInMap(who, 25.0f))
        {
            me->SetInCombatWithZone();
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        CastMuruOpeningBotConsumables(me, who);

        DoCastSelf(SPELL_NEGATIVE_ENERGY, true);
        DoCastSelf(SPELL_SUMMON_BLOOD_ELVES_PERIODIC, true);
        DoCastSelf(SPELL_OPEN_PORTAL_PERIODIC, true);
        DoCastSelf(SPELL_DARKNESS_PERIODIC, true);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_ENRAGE, true);

            if (Creature* entropius = summons.GetCreatureWithEntry(NPC_ENTROPIUS))
                entropius->CastSpell(entropius, SPELL_ENRAGE, true);
        }, 10min);
    }

    void JustSummoned(Creature* creature) override
    {
        if (creature->GetEntry() == NPC_ENTROPIUS)
            creature->SetInCombatWithZone();
        else
            BossAI::JustSummoned(creature);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (damage >= me->GetHealth())
        {
            damage = me->GetHealth() - 1;
            if (!me->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            {
                me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                me->RemoveAllAuras();
                DoCastSelf(SPELL_OPEN_ALL_PORTALS, true);

                me->m_Events.AddEventAtOffset([&]
                {
                    DoCastAOE(SPELL_SUMMON_ENTROPIUS);
                }, 7s);

                me->m_Events.AddEventAtOffset([&]
                {
                    me->SetVisible(false);
                }, 8s);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (_endlessVoidTimer <= diff)
        {
            MaintainMuruEndlessVoidAura(me, _endlessVoidTargets);
            _endlessVoidTimer = 15000;
        }
        else
            _endlessVoidTimer -= diff;

        BossAI::UpdateAI(diff);
    }

private:
    uint32 _endlessVoidTimer = 0;
    GuidSet _endlessVoidTargets;
};

struct boss_entropius : public ScriptedAI
{
    boss_entropius(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        scheduler.CancelAll();

        DoCastSelf(SPELL_ENTROPIUS_COSMETIC_SPAWN);
        DoCastSelf(SPELL_NEGATIVE_ENERGY_PERIODIC, true);

        me->SetReactState(REACT_PASSIVE);

        me->m_Events.AddEventAtOffset([&] {
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetInCombatWithZone();
            AttackStart(SelectTarget(SelectTargetMethod::Random, 0, 50.0f, false, true));
        }, 3s);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* muru = instance->GetCreature(DATA_MURU))
                if (!muru->IsInEvadeMode())
                    muru->AI()->EnterEvadeMode(why);

        me->DespawnOrUnsummon();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ScheduleTimedEvent(8s, 29s, [this]() {
            DoCastRandomTarget(SPELL_DARKNESS, 0, 50.0f, false, true);
        }, 8s, 29s);

        ScheduleTimedEvent(14s, 29s, [this]() {
            DoCastRandomTarget(SPELL_BLACK_HOLE, 0, 50.0f, false, true);
        }, 14s, 29s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* muru = instance->GetCreature(DATA_MURU))
                muru->KillSelf();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
        scheduler.Update(diff);
    }
};

struct npc_dark_fiend : public ScriptedAI
{
    npc_dark_fiend(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _lastVictimGUID.Clear();
        _spellCast = false;

        me->SetReactState(REACT_PASSIVE);
        DoCast(me, SPELL_DARK_FIEND_APPEARANCE);
        DoCast(me, SPELL_DARK_FIEND_SECONDARY);

        me->m_Events.AddEventAtOffset([this]() {
            me->SetReactState(REACT_AGGRESSIVE);

            Unit* target = nullptr;
            if (InstanceScript* instance = me->GetInstanceScript())
                if (Creature* muru = instance->GetCreature(DATA_MURU))
                    target = muru->GetAI()->SelectTarget(SelectTargetMethod::Random, 0, RangeSelector(me, 50.0f, false, true));

            if (target)
            {
                AttackStart(target);
                me->AddThreat(target, 100000.0f);
            }
        }, 1s, 2s);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damagetype*/, SpellSchoolMask /*schoolMask*/) override
    {
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - me->GetMaxHealth() * 0.01f;
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!me->HasAura(SPELL_DARK_FIEND_APPEARANCE))
        {
            me->DespawnOrUnsummon();
            return;
        }

        // Check if victim has changed or disappeared
        Unit* currentVictim = me->GetVictim();
        ObjectGuid currentVictimGUID = currentVictim ? currentVictim->GetGUID() : ObjectGuid::Empty;

        if (_lastVictimGUID != currentVictimGUID)
        {
            // If had a victim before but now it's gone
            if (!_lastVictimGUID.IsEmpty() && currentVictimGUID.IsEmpty())
                me->DespawnOrUnsummon();

            _lastVictimGUID = currentVictimGUID;
        }

        if (!UpdateVictim())
            return;

        if (!_spellCast && currentVictim && me->IsWithinMeleeRange(currentVictim, 2.0f))
        {
            DoCast(me, SPELL_DARK_FIEND_TRIGGER);
            _spellCast = true;
            me->m_Events.AddEventAtOffset([this]() {
                me->DespawnOrUnsummon();
            }, 1s);
        }
    }

private:
    ObjectGuid _lastVictimGUID;
    bool _spellCast;
};

struct npc_singularity : public NullCreatureAI
{
    npc_singularity(Creature* creature) : NullCreatureAI(creature) { }

    void Reset() override
    {
        me->DespawnOrUnsummon(18s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BLACK_HOLE_SUMMON_VISUAL, true);
        }, 2s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BLACK_HOLE_SUMMON_VISUAL2, true);
        }, 4s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BLACK_HOLE_SUMMON_VISUAL, true);
        }, 6s);

        me->m_Events.AddEventAtOffset([&] {
            DoCastSelf(SPELL_BLACK_HOLE_VISUAL2, true);
            DoCastSelf(SPELL_BLACK_HOLE_PASSIVE, true);

            // Start following players after visuals are complete
            FindAndFollowTarget();
        }, 8s);

        me->m_Events.AddEventAtOffset([&] {
            me->KillSelf();
        }, 17s);
    }

    void FindAndFollowTarget()
    {
        scheduler.Schedule(1s, [this](TaskContext context)
        {
            Unit* target = SelectSingularityTarget(me);

            if (target)
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveFollow(target, 0.0f, 0.0f);

                scheduler.Schedule(6s, [this](TaskContext)
                {
                    FindAndFollowTarget();
                });
            }
            else
            {
                // No valid target found, check again soon
                context.Repeat();
            }
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

class spell_muru_summon_blood_elves_periodic_aura : public AuraScript
{
    PrepareAuraScript(spell_muru_summon_blood_elves_periodic_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_FURY_MAGE1, SPELL_SUMMON_FURY_MAGE2, SPELL_SUMMON_BERSERKER1, SPELL_SUMMON_BERSERKER2 });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        // first tick after 10 seconds
        GetAura()->GetEffect(aurEff->GetEffIndex())->SetPeriodicTimer(10000);
    }

    void OnPeriodic(AuraEffect const*  /*aurEff*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_FURY_MAGE1, true);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_FURY_MAGE2, true);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_BERSERKER1, true);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_BERSERKER2, true);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_BERSERKER1, true);
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_BERSERKER2, true);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_muru_summon_blood_elves_periodic_aura::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_muru_summon_blood_elves_periodic_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_muru_darkness_aura : public AuraScript
{
    PrepareAuraScript(spell_muru_darkness_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_DARK_FIEND });
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        if (aurEff->GetTickNumber() == 2)
            for (uint8 i = 0; i < 8; ++i)
                GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_DARK_FIEND + i, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_muru_darkness_aura::OnPeriodic, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_entropius_void_zone_visual_aura : public AuraScript
{
    PrepareAuraScript(spell_entropius_void_zone_visual_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_DARK_FIEND_ENTROPIUS });
    }

    void HandleApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        SetDuration(3000);
    }

    void HandleRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_SUMMON_DARK_FIEND_ENTROPIUS, true);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_entropius_void_zone_visual_aura::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_entropius_void_zone_visual_aura::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_entropius_black_hole_effect : public SpellScript
{
    PrepareSpellScript(spell_entropius_black_hole_effect);
    float RaycastToObstacle(Unit* unit, float angle, float z, float maxDist = 20.0f)
    {
        float baseX = unit->GetPositionX();
        float baseY = unit->GetPositionY();
        float targetX = baseX + maxDist * cos(angle);
        float targetY = baseY + maxDist * sin(angle);
        float hitX, hitY, hitZ;
        if (unit->GetMap()->GetMapCollisionData().GetStaticTree().GetObjectHitPos(
                baseX, baseY, z,
                targetX, targetY, z,
                hitX, hitY, hitZ,
                0.0f))
        {
            return std::sqrt(
                std::pow(hitX - baseX, 2) +
                std::pow(hitY - baseY, 2) +
                std::pow(hitZ - z, 2)
            );
        }
        return maxDist;
    }

    void HandlePull(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* target = GetHitUnit();
        if (!target)
            return;
        Position pos;
        if (target->GetDistance(GetCaster()) < 5.0f)
        {
            float o = frand(0, 2 * M_PI);
            float z = GetCaster()->GetPositionZ() + frand(1.0f, 2.0f);
            float safeDistance = RaycastToObstacle(GetCaster(), o, z, 10.0f);
            float actualDistance = std::min(8.0f, safeDistance * 0.8f);

            pos.Relocate(
                GetCaster()->GetPositionX() + actualDistance * cos(o),
                GetCaster()->GetPositionY() + actualDistance * sin(o),
                z
            );
        }
        else
            pos.Relocate(GetCaster()->GetPositionX(), GetCaster()->GetPositionY(), GetCaster()->GetPositionZ() + 1.0f);

        float speedXY = float(GetSpellInfo()->Effects[effIndex].MiscValue) * 0.1f;
        float speedZ = target->GetDistance(pos) / speedXY * 0.5f * Movement::gravity;
        target->GetMotionMaster()->MoveJump(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), speedXY, speedZ);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_entropius_black_hole_effect::HandlePull, EFFECT_0, SPELL_EFFECT_PULL_TOWARDS_DEST);
    }
};

// 46284 - Negative Energy Periodic
class spell_entropius_negative_energy_periodic : public AuraScript
{
    PrepareAuraScript(spell_entropius_negative_energy_periodic);

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ spellInfo->Effects[EFFECT_0].TriggerSpell });
    }

    void PeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        uint32 targetCount = (aurEff->GetTickNumber() + 11) / 12;
        GetTarget()->CastCustomSpell(aurEff->GetSpellInfo()->Effects[EFFECT_0].TriggerSpell, SPELLVALUE_MAX_TARGETS, targetCount);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_entropius_negative_energy_periodic::PeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

class spell_gen_summon_target_floor : public SpellScript
{
    PrepareSpellScript(spell_gen_summon_target_floor);

    void ChangeSummonPos(SpellEffIndex /*effIndex*/)
    {
        if (!GetCaster())
            return;

        WorldLocation summonPos = *GetExplTargetDest();
        float destZ = summonPos.GetPositionZ() - GetCaster()->GetMapWaterOrGroundLevel(summonPos);
        Position offset = { 0.0f, 0.0f, -destZ, 0.0f};
        summonPos.RelocateOffset(offset);
        SetExplTargetDest(summonPos);
        GetHitDest()->RelocateOffset(offset);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_gen_summon_target_floor::ChangeSummonPos, EFFECT_0, SPELL_EFFECT_SUMMON);
    }
};

void AddSC_boss_muru()
{
    RegisterSunwellPlateauCreatureAI(boss_muru);
    RegisterSunwellPlateauCreatureAI(boss_entropius);
    RegisterSunwellPlateauCreatureAI(npc_singularity);
    RegisterSunwellPlateauCreatureAI(npc_dark_fiend);

    RegisterSpellScript(spell_muru_summon_blood_elves_periodic_aura);
    RegisterSpellScript(spell_muru_darkness_aura);
    RegisterSpellScript(spell_entropius_void_zone_visual_aura);
    RegisterSpellScript(spell_entropius_black_hole_effect);
    RegisterSpellScript(spell_entropius_negative_energy_periodic);
    RegisterSpellScript(spell_gen_summon_target_floor);
}
