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

#include "CreatureGroups.h"
#include "CreatureScript.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "zulaman.h"
#include <cmath>
#include <limits>
#include <vector>

enum Says
{
    SAY_INTRO                    = 0,
    SAY_AGGRO                    = 1,
    SAY_TRANSFORM_TO_BEAR        = 2,
    SAY_TRANSFORM_TO_EAGLE       = 3,
    SAY_TRANSFORM_TO_LYNX        = 4,
    SAY_TRANSFORM_TO_DRAGONHAWK  = 5,
    SAY_FIRE_BREATH              = 6,
    SAY_BERSERK                  = 7,
    SAY_KILL                     = 8,
    SAY_DEATH                    = 9
};

enum Spells
{
    // Troll Form
    SPELL_WHIRLWIND               = 17207,
    SPELL_GRIEVOUS_THROW          = 43093, // remove debuff after full healed
    // Bear Form
    SPELL_CREEPING_PARALYSIS      = 43095, // should cast on the whole raid
    SPELL_OVERPOWER               = 43456, // use after melee attack dodged
    // Eagle Form
    SPELL_ENERGY_STORM            = 43983, // enemy area aura, trigger 42577
    SPELL_ZAP_INFORM              = 42577,
    SPELL_ZAP_DAMAGE              = 43137, // 1250 damage
    SPELL_SUMMON_CYCLONE          = 43112, // summon four feather vortex
    CREATURE_FEATHER_VORTEX       = 24136,
    SPELL_CYCLONE_VISUAL          = 43119, // trigger 43147 visual
    SPELL_CYCLONE_PASSIVE         = 43120, // trigger 43121 (4y aoe) every second
    // Lynx Form
    SPELL_CLAW_RAGE_CHARGE        = 42583,
    SPELL_CLAW_RAGE_AURA          = 43149,
    SPELL_CLAW_RAGE_DAMAGE        = 43150,
    SPELL_LYNX_RUSH_HASTE         = 43152,
    SPELL_LYNX_RUSH_DAMAGE        = 43153,
    // Dragonhawk Form
    SPELL_FLAME_WHIRL             = 43213, // trigger two spells
    SPELL_FLAME_BREATH            = 43215,
    SPELL_SUMMON_PILLAR           = 43216, // summon 24187
    CREATURE_COLUMN_OF_FIRE       = 24187,
    SPELL_PILLAR_TRIGGER          = 43218, // trigger 43217
    // Cosmetic
    SPELL_SPIRIT_AURA             = 42466,
    SPELL_SPIRIT_DRAIN            = 42542,
    // Transforms:
    SPELL_SHAPE_OF_THE_BEAR       = 42594, // 15% dmg
    SPELL_SHAPE_OF_THE_EAGLE      = 42606,
    SPELL_SHAPE_OF_THE_LYNX       = 42607, // haste melee 30%
    SPELL_SHAPE_OF_THE_DRAGONHAWK = 42608,

    SPELL_BERSERK                 = 45078
};

enum Phase
{
    PHASE_BEAR                    = 0,
    PHASE_EAGLE                   = 1,
    PHASE_LYNX                    = 2,
    PHASE_DRAGONHAWK              = 3,
    PHASE_TROLL                   = 4
};

enum Misc
{
    GUID_CHARGE_TARGET            = 0,
    GROUP_LYNX                    = 1,
    GROUP_BOT_DRAGONHAWK_DIRECTOR = 11,
    GROUP_BOT_ZULJIN_PRIORITY     = 12,
    POINT_CENTER                  = 0
};

enum CreatureEntries
{
    NPC_SPIRIT_BEAR       = 23878,
    NPC_SPIRIT_EAGLE      = 23880,
    NPC_SPIRIT_LYNX_ZJ    = 23877,
    NPC_SPIRIT_DRAGONHAWK = 23879
};

const Position CenterPosition = { 120.148811f, 703.713684f, 45.111477f };

static bool IsNPCBotUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsNPCBot() && creature->GetBotAI();
}

static bool IsValidEncounterNPCBot(Creature* creature)
{
    return creature && creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();
}

static bool IsEncounterParticipantFor(Creature const* source, Unit* unit, float range)
{
    if (!source || !unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != source->GetMap())
        return false;

    if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(source, range))
        return false;

    if (unit->IsPlayer())
        return true;

    return IsValidEncounterNPCBot(unit->ToCreature());
}

static std::vector<Unit*> GatherEncounterUnitsForCreature(Creature const* source, float range = 150.0f)
{
    std::vector<Unit*> units;
    GuidSet seen;

    auto addUnit = [&](Unit* unit)
    {
        if (!unit || seen.count(unit->GetGUID()) || !IsEncounterParticipantFor(source, unit, range))
            return;

        seen.insert(unit->GetGUID());
        units.push_back(unit);
    };

    if (!source || !source->GetMap())
        return units;

    Map::PlayerList const& players = source->GetMap()->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* player = itr->GetSource();
        if (!player)
            continue;

        addUnit(player);

        if (Group* group = player->GetGroup())
            for (Unit* member : BotMgr::GetAllGroupMembers(group))
                addUnit(member);

        if (BotMgr* botMgr = player->GetBotMgr())
            if (BotMap const* botMap = botMgr->GetBotMap())
                for (BotMap::value_type const& pair : *botMap)
                    addUnit(pair.second);
    }

    return units;
}

static std::vector<Creature*> GatherEncounterBotsForCreature(Creature const* source, float range = 150.0f)
{
    std::vector<Creature*> bots;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
        if (Creature* creature = unit->ToCreature())
            if (IsValidEncounterNPCBot(creature))
                bots.push_back(creature);

    return bots;
}

static Unit* SelectRandomEncounterTargetForCreature(Creature* source, float range, bool includeVictim = true)
{
    std::vector<Unit*> candidates;

    for (Unit* unit : GatherEncounterUnitsForCreature(source, range))
    {
        if (!includeVictim && unit == source->GetVictim())
            continue;

        candidates.push_back(unit);
    }

    if (candidates.empty() && includeVictim && source->GetVictim() && source->IsWithinDistInMap(source->GetVictim(), range))
        return source->GetVictim();

    if (candidates.empty())
        return nullptr;

    return Acore::Containers::SelectRandomContainerElement(candidates);
}

static bool IsBotCastingUninterruptible(Creature* bot)
{
    if (!bot)
        return false;

    if (Spell* spell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (SpellInfo const* spellInfo = spell->GetSpellInfo())
            if (!(spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT))
                return true;

    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        if (SpellInfo const* spellInfo = spell->GetSpellInfo())
            if (!(spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))
                return true;

    return false;
}

static void SafeBotMoveTo(Creature* bot, Position const& destination)
{
    if (!bot || !bot->IsAlive() || !bot->IsNPCBot())
        return;

    bot_ai* ai = bot->GetBotAI();
    if (!ai)
        return;

    float x = destination.GetPositionX();
    float y = destination.GetPositionY();
    float z = destination.GetPositionZ();

    if (!bot->CanFly())
        bot->UpdateAllowedPositionZ(x, y, z);

    Position finalPosition;
    finalPosition.Relocate(x, y, z, destination.GetOrientation());

    ai->MoveToSendPosition(finalPosition);
}

struct SpiritInfoStruct
{
    uint32 entry;
    Position pos;
};

static SpiritInfoStruct SpiritInfo[4] =
{
    { NPC_SPIRIT_LYNX_ZJ,    { 147.87f, 706.51f, 45.11f, 3.04f } },
    { NPC_SPIRIT_DRAGONHAWK, { 88.95f,  705.49f, 45.11f, 6.11f } },
    { NPC_SPIRIT_BEAR,       { 137.23f, 725.98f, 45.11f, 3.71f } },
    { NPC_SPIRIT_EAGLE,      { 104.29f, 726.43f, 45.11f, 5.43f } }
};

struct TransformStruct
{
    uint8  text;
    uint32 spell;
    uint32 unaura;
    uint32 spiritEntry;
};

static TransformStruct Transform[4] =
{
    { SAY_TRANSFORM_TO_BEAR,       SPELL_SHAPE_OF_THE_BEAR,       SPELL_WHIRLWIND,          NPC_SPIRIT_BEAR       },
    { SAY_TRANSFORM_TO_EAGLE,      SPELL_SHAPE_OF_THE_EAGLE,      SPELL_SHAPE_OF_THE_BEAR,  NPC_SPIRIT_EAGLE      },
    { SAY_TRANSFORM_TO_LYNX,       SPELL_SHAPE_OF_THE_LYNX,       SPELL_SHAPE_OF_THE_EAGLE, NPC_SPIRIT_LYNX_ZJ    },
    { SAY_TRANSFORM_TO_DRAGONHAWK, SPELL_SHAPE_OF_THE_DRAGONHAWK, SPELL_SHAPE_OF_THE_LYNX,  NPC_SPIRIT_DRAGONHAWK }
};

struct boss_zuljin : public BossAI
{
    boss_zuljin(Creature* creature) : BossAI(creature, DATA_ZULJIN) { }

    void Reset() override
    {
        StopBotDirectors();
        _Reset();
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 33975);
        me->SetCombatMovement(true);
        me->m_Events.KillAllEvents(false);
        _nextPhase = 0;

        if (CreatureGroup* formation = me->GetFormation())
            formation->RespawnFormation(true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        DoZoneInCombat();

        Talk(SAY_INTRO);
        SpawnAdds();

        Talk(SAY_AGGRO, 37s);

        // Phase 1: Default (troll)
        ScheduleTimedEvent(12s, 29s, [&] {
            DoCastAOE(SPELL_WHIRLWIND);
        }, 12s, 29s);

        ScheduleTimedEvent(7s, 23s, [&] {
            if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                DoCast(target, SPELL_GRIEVOUS_THROW);
        }, 7s, 23s);

        me->m_Events.AddEventAtOffset([&]() {
            DoCastSelf(SPELL_BERSERK, true);
            Talk(SAY_BERSERK);
        }, 10min);

        // Phase 2: Bear Form.
        ScheduleHealthCheckEvent({ 80 }, [&] {
            EnterPhase(PHASE_BEAR);
            ScheduleTimedEvent(8s, [&] {
                DoCastAOE(SPELL_CREEPING_PARALYSIS);
            }, 26s, 30s);

            ScheduleTimedEvent(1s, [&] {
                if (!me->HasSpellCooldown(SPELL_OVERPOWER) && me->GetVictim() && me->GetComboPoints())
                    if (DoCastVictim(SPELL_OVERPOWER) == SPELL_CAST_OK)
                        me->AddSpellCooldown(SPELL_OVERPOWER, 0, 5000);
            }, 1s);
        });

        // Phase 3: Eagle Form.
        ScheduleHealthCheckEvent({ 60 }, [&] {
            EnterPhase(PHASE_EAGLE);
        });

        // Phase 4: Lynx Form.
        ScheduleHealthCheckEvent({ 40 }, [&] {
            EnterPhase(PHASE_LYNX);
            me->RemoveAurasDueToSpell(SPELL_ENERGY_STORM);
            me->RemoveOwnedAura(SPELL_ENERGY_STORM);
            summons.DespawnEntry(CREATURE_FEATHER_VORTEX);
            me->ResumeChasingVictim();

            ScheduleTimedEvent(5s, [&] {
                if (me->HasAura(SPELL_LYNX_RUSH_HASTE))
                    return;

                if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                    DoCast(target, SPELL_CLAW_RAGE_CHARGE);
            }, 15s, 20s);

            ScheduleTimedEvent(14s, [&] {
                if (me->HasAura(SPELL_CLAW_RAGE_AURA))
                    return;

                DoCastSelf(SPELL_LYNX_RUSH_HASTE);

                for (int8 count = 0; count <= 8; ++count)
                {
                    me->m_Events.AddEventAtOffset([&] {
                        if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                            DoCast(target, SPELL_LYNX_RUSH_DAMAGE, true);
                    }, Seconds(1 * count), Seconds(1 * count), GROUP_LYNX);
                }

            }, 15s, 20s);
        });

        // Phase 5: Dragonhawk Form.
        ScheduleHealthCheckEvent({ 20 }, [&] {
            me->m_Events.CancelEventGroup(GROUP_LYNX);
            EnterPhase(PHASE_DRAGONHAWK);

            ScheduleTimedEvent(12s, 26s, [&] {
                DoCastSelf(SPELL_FLAME_WHIRL);
            }, 12s, 26s);

            ScheduleTimedEvent(11s, 42s, [&] {
                if (Unit* target = SelectRandomEncounterTarget(100.0f, true))
                    DoCast(target, SPELL_SUMMON_PILLAR);
            }, 5s, 25s);

            ScheduleTimedEvent(16s, 26s, [&] {
                DoCastAOE(SPELL_FLAME_BREATH);
            }, 6s, 25s);
        });
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        StopBotDirectors();
        if (CreatureGroup* formation = me->GetFormation())
        {
            for (auto const& itr : formation->GetMembers())
            {
                if (itr.first && itr.first != me)
                    itr.first->DespawnOnEvade(2min);
            }
        }

        me->DespawnOnEvade(2min);
        _EnterEvadeMode();
    }

    void SpellHitTarget(Unit* target, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_CLAW_RAGE_CHARGE && target != me)
        {
            DoCastSelf(SPELL_CLAW_RAGE_AURA);
            _chargeTargetGUID = target->GetGUID();
        }
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_KILL);
    }

    ObjectGuid GetGUID(int32 index) const override
    {
        if (index == GUID_CHARGE_TARGET)
            return _chargeTargetGUID;

        return ObjectGuid::Empty;
    }

    void JustDied(Unit* /*killer*/) override
    {
        StopBotDirectors();
        instance->SetBossState(DATA_ZULJIN, DONE);
        Talk(SAY_DEATH);
        summons.DespawnEntry(CREATURE_COLUMN_OF_FIRE);
        summons.DespawnAll(3s);
    }

    void SpawnAdds()
    {
        for (auto const& spiritInfo : SpiritInfo)
        {
            if (Creature* creature = me->SummonCreature(spiritInfo.entry, spiritInfo.pos, TEMPSUMMON_DEAD_DESPAWN, 0))
            {
                creature->m_Events.AddEventAtOffset([creature] {
                    creature->CastSpell(creature, SPELL_SPIRIT_AURA);
                }, 1s);
            }
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_CENTER)
        {
            if (Creature* spirit = summons.GetCreatureWithEntry(Transform[_nextPhase].spiritEntry))
            {
                spirit->CastSpell(me, SPELL_SPIRIT_DRAIN, false);
                me->SetFacingToObject(spirit);
            }

            if (_nextPhase)
                if (Creature* spirit = summons.GetCreatureWithEntry(Transform[_nextPhase - 1].spiritEntry))
                {
                    spirit->CastStop();
                    spirit->SetUInt32Value(UNIT_FIELD_BYTES_1, UNIT_STAND_STATE_DEAD);
                }

            Talk(Transform[_nextPhase].text);

            me->m_Events.AddEventAtOffset([&] {
                DoCastSelf(Transform[_nextPhase].spell);
                DoResetThreatList();

                if (_nextPhase == PHASE_EAGLE)
                {
                    me->SetCombatMovement(false);
                    DoCastSelf(SPELL_ENERGY_STORM, true); // enemy aura
                    DoCastAOE(SPELL_SUMMON_CYCLONE, true);
                    me->SetFacingTo(me->GetHomePosition().GetOrientation());
                }
                else
                {
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->SetCombatMovement(true);
                    me->ResumeChasingVictim();
                    if (_nextPhase == PHASE_DRAGONHAWK)
                        StartDragonhawkBotDirector();
                }
            }, 2s);
        }
    }

    void EnterPhase(uint32 NextPhase)
    {
        StopBotDirectors();
        scheduler.CancelAll();
        me->SetReactState(REACT_PASSIVE);
        DoStopAttack();
        me->GetMotionMaster()->Clear();

        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 0);
        me->RemoveAurasDueToSpell(Transform[NextPhase].unaura);

        me->m_Events.AddEventAtOffset([&] {
            me->GetMotionMaster()->MovePoint(POINT_CENTER, CenterPosition);
        }, 1s);

        _nextPhase = NextPhase;
    }

    Unit* SelectRandomEncounterTarget(float range, bool includeVictim = true) const
    {
        return SelectRandomEncounterTargetForCreature(me, range, includeVictim);
    }

    void StopBotDirectors()
    {
        scheduler.CancelGroup(GROUP_BOT_DRAGONHAWK_DIRECTOR);
        scheduler.CancelGroup(GROUP_BOT_ZULJIN_PRIORITY);
    }

    void StartDragonhawkBotDirector()
    {
        scheduler.CancelGroup(GROUP_BOT_DRAGONHAWK_DIRECTOR);
        scheduler.Schedule(750ms, GROUP_BOT_DRAGONHAWK_DIRECTOR, [this](TaskContext context)
        {
            MoveBotsAwayFromHazard(CREATURE_COLUMN_OF_FIRE, 8.0f, 10.0f, true);
            MoveNonTanksOutOfFlameBreath();
            context.Repeat(750ms);
        });
    }

    void MoveBotsAwayFromHazard(uint32 entry, float dangerRadius, float moveDistance, bool interruptCasts)
    {
        std::list<Creature*> hazards;
        me->GetCreatureListWithEntryInGrid(hazards, entry, 90.0f);
        if (hazards.empty())
            return;

        for (Creature* bot : GatherEncounterBotsForCreature(me, 120.0f))
        {
            if (!bot || bot->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_LOST_CONTROL | UNIT_STATE_ISOLATED))
                continue;

            Creature* nearest = nullptr;
            float nearestDistance = std::numeric_limits<float>::max();
            float awayX = 0.0f;
            float awayY = 0.0f;

            for (Creature* hazard : hazards)
            {
                if (!hazard || !hazard->IsInWorld())
                    continue;

                float distance = bot->GetExactDist2d(hazard);
                if (distance > dangerRadius)
                    continue;

                float dx = bot->GetPositionX() - hazard->GetPositionX();
                float dy = bot->GetPositionY() - hazard->GetPositionY();
                float length = std::sqrt(dx * dx + dy * dy);
                if (length < 0.1f)
                {
                    dx = bot->GetPositionX() - CenterPosition.GetPositionX();
                    dy = bot->GetPositionY() - CenterPosition.GetPositionY();
                    length = std::sqrt(dx * dx + dy * dy);
                }

                if (length > 0.1f)
                {
                    awayX += dx / length;
                    awayY += dy / length;
                }

                if (distance < nearestDistance)
                {
                    nearest = hazard;
                    nearestDistance = distance;
                }
            }

            if (!nearest)
                continue;

            float length = std::sqrt(awayX * awayX + awayY * awayY);
            if (length < 0.1f)
            {
                awayX = bot->GetPositionX() - CenterPosition.GetPositionX();
                awayY = bot->GetPositionY() - CenterPosition.GetPositionY();
                length = std::sqrt(awayX * awayX + awayY * awayY);
            }

            if (length < 0.1f)
            {
                awayX = std::cos(me->GetOrientation());
                awayY = std::sin(me->GetOrientation());
                length = 1.0f;
            }

            float x = bot->GetPositionX() + awayX / length * moveDistance;
            float y = bot->GetPositionY() + awayY / length * moveDistance;
            float centerDist = std::sqrt((x - CenterPosition.GetPositionX()) * (x - CenterPosition.GetPositionX()) + (y - CenterPosition.GetPositionY()) * (y - CenterPosition.GetPositionY()));
            if (centerDist > 42.0f)
            {
                float scale = 42.0f / centerDist;
                x = CenterPosition.GetPositionX() + (x - CenterPosition.GetPositionX()) * scale;
                y = CenterPosition.GetPositionY() + (y - CenterPosition.GetPositionY()) * scale;
            }

            Position destination;
            destination.Relocate(x, y, bot->GetPositionZ(), bot->GetOrientation());

            if (interruptCasts && !IsBotCastingUninterruptible(bot))
                bot->InterruptNonMeleeSpells(false);

            SafeBotMoveTo(bot, destination);
        }
    }

    void MoveNonTanksOutOfFlameBreath()
    {
        if (!me->IsNonMeleeSpellCast(false))
            return;

        Spell* spell = me->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell || !spell->GetSpellInfo() || spell->GetSpellInfo()->Id != SPELL_FLAME_BREATH)
            return;

        for (Creature* bot : GatherEncounterBotsForCreature(me, 90.0f))
        {
            bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
            if (!ai || ai->IsTank() || ai->IsOffTank() || ai->HasRole(BOT_ROLE_TANK) || bot == me->GetVictim())
                continue;

            if (!me->HasInArc(float(M_PI) / 2.0f, bot) || bot->GetExactDist2d(me) > 25.0f)
                continue;

            float angle = me->GetAngle(bot) + float(M_PI) / 2.0f;
            Position destination;
            destination.Relocate(bot->GetPositionX() + std::cos(angle) * 8.0f, bot->GetPositionY() + std::sin(angle) * 8.0f, bot->GetPositionZ(), bot->GetOrientation());
            SafeBotMoveTo(bot, destination);
        }
    }

    private:
        ObjectGuid _chargeTargetGUID;
        uint8 _nextPhase;
};

struct npc_zuljin_vortex : public ScriptedAI
{
    npc_zuljin_vortex(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (WorldObject* summoner = GetSummoner())
            if (Creature* zuljin = summoner->ToCreature())
                me->SetLevel(zuljin->GetLevel());

        DoCastSelf(SPELL_CYCLONE_PASSIVE, true);
        DoCastSelf(SPELL_CYCLONE_VISUAL, true);
        me->SetSpeed(MOVE_RUN, 1.0f);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        DoZoneInCombat();
        // Start attacking random target
        ChangeToNewPlayer();
    }

    void ChangeToNewPlayer()
    {
        DoResetThreatList();
        if (WorldObject* summoner = GetSummoner())
            if (Creature* zuljin = summoner->ToCreature())
                if (Unit* target = SelectRandomEncounterTargetForCreature(zuljin, 80.0f, true))
                    me->AddThreat(target, 10000000.0f);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        UpdateVictim();

        //if the vortex reach the target, it change his target to another player
        if (me->IsWithinMeleeRange(me->GetVictim()))
            ChangeToNewPlayer();
    }
};

// 43149 - Claw Rage
class spell_claw_rage_aura : public AuraScript
{
    PrepareAuraScript(spell_claw_rage_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CLAW_RAGE_DAMAGE });
    }

    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        if (Creature* caster = GetCaster()->ToCreature())
        {
            if (Unit* target = ObjectAccessor::GetUnit(*caster, caster->AI()->GetGUID(GUID_CHARGE_TARGET)))
            {
                if (target->isTargetableForAttack(true, caster))
                    GetCaster()->CastSpell(target, SPELL_CLAW_RAGE_DAMAGE);
                else
                    GetCaster()->RemoveAurasDueToSpell(SPELL_CLAW_RAGE_AURA);
            }
            else
                GetCaster()->RemoveAurasDueToSpell(SPELL_CLAW_RAGE_AURA);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_claw_rage_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 42577 - Zap
class spell_zuljin_zap : public SpellScript
{
    PrepareSpellScript(spell_zuljin_zap);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_ZAP_DAMAGE });
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* victim = GetHitUnit())
            victim->CastSpell(GetCaster(), SPELL_ZAP_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_zuljin_zap::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_boss_zuljin()
{
    RegisterZulAmanCreatureAI(boss_zuljin);
    RegisterZulAmanCreatureAI(npc_zuljin_vortex);
    RegisterSpellScript(spell_claw_rage_aura);
    RegisterSpellScript(spell_zuljin_zap);
}
