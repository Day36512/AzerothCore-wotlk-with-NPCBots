/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or at your
 * option any later version.
 */

#include "SpellScript.h"
#include "CreatureScript.h"
#include "GameObjectScript.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "magtheridons_lair.h"

#include <algorithm>
#include <map>
#include <vector>

enum Yells
{
    SAY_TAUNT = 0,
    SAY_FREE = 1,
    SAY_SLAY = 2,
    SAY_BANISH = 3,
    SAY_PHASE3 = 4,
    SAY_DEATH = 5,
};

enum Emotes
{
    SAY_EMOTE_BEGIN = 6,
    SAY_EMOTE_NEARLY = 7,
    SAY_EMOTE_FREE = 8,
    SAY_EMOTE_NOVA = 9
};

enum Spells
{
    SPELL_SHADOW_CAGE = 30205,
    SPELL_BLAST_NOVA = 30616,
    SPELL_CLEAVE = 30619,
    SPELL_BLAZE = 30541,
    SPELL_BLAZE_SUMMON = 30542,
    SPELL_BERSERK = 27680,
    SPELL_SHADOW_GRASP = 30410,
    SPELL_SHADOW_GRASP_VISUAL = 30166,
    SPELL_SHADOW_CAGE_STUN = 30168,
    SPELL_MIND_EXHAUSTION = 44032,
    SPELL_QUAKE = 30657,
    SPELL_QUAKE_KNOCKBACK = 30571,
    SPELL_COLLAPSE_DAMAGE = 36449,
    SPELL_CAMERA_SHAKE = 36455,
    SPELL_DEBRIS_TARGET = 30629,
    SPELL_DEBRIS_SPAWN = 30630,
    SPELL_DEBRIS_DAMAGE = 30631,
    SPELL_DEBRIS_VISUAL = 30632,

    SPELL_CHANNELER_SHADOW_BOLT_VOLLEY = 30510,
    SPELL_CHANNELER_DARK_MENDING = 30528,
    SPELL_CHANNELER_FEAR = 30530,
    SPELL_CHANNELER_BURNING_ABYSSAL = 30511,
    SPELL_CHANNELER_SHADOW_GRASP = 30207,
    SPELL_CHANNELER_SOUL_TRANSFER = 30531,
};

enum Groups
{
    GROUP_EARLY_RELEASE_CHECK = 0,
    GROUP_BLAST_NOVA = 1,
    GROUP_BOT_CUBE = 2,
    GROUP_MAIN_TANK_SUPPORT = 3
};

namespace
{
    static constexpr int32 ACTION_CHANNELER_SET_EMPOWERED = 3;
    static constexpr int32 ACTION_CHANNELER_CLEAR_EMPOWERED = 4;

    static constexpr float MAGTHERIDON_TANK_RESET_X = -21.42f;
    static constexpr float MAGTHERIDON_TANK_RESET_Y = 0.31f;
    static constexpr float MAGTHERIDON_TANK_RESET_Z = -0.40f;
    static constexpr float MAGTHERIDON_TANK_RESET_O = 0.0f;

    bool IsUsableEncounterUnitForThreat(Unit* unit, Creature const* source)
    {
        return unit &&
            source &&
            unit->IsInWorld() &&
            unit->IsAlive() &&
            unit->GetMap() == source->GetMap() &&
            !unit->HasUnitState(UNIT_STATE_ISOLATED) &&
            unit->IsWithinDist(source, 160.0f);
    }

    bool IsPlayerMarkedTank(Player* player)
    {
        if (!player)
            return false;

        Group* group = player->GetGroup();
        if (!group)
            return false;

        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::member_citerator itr = slots.begin(); itr != slots.end(); ++itr)
        {
            if (itr->guid == player->GetGUID())
                return itr->flags & (MEMBER_FLAG_MAINTANK | MEMBER_FLAG_MAINASSIST);
        }

        return false;
    }

    bool IsNpcBotTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && (ai->IsTank() || ai->IsOffTank());
    }

    bool IsNpcBotMainTank(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsNPCBot())
            return false;

        bot_ai* ai = creature->GetBotAI();
        return ai && ai->IsTank() && !ai->IsOffTank();
    }

    bool IsEncounterTank(Unit* unit)
    {
        if (!unit)
            return false;

        if (IsNpcBotTank(unit))
            return true;

        if (Player* player = unit->ToPlayer())
            return IsPlayerMarkedTank(player);

        return false;
    }

    void TryAddEncounterTank(Unit* unit, Creature* source, GuidSet& seen, std::vector<Unit*>& tanks)
    {
        if (!IsUsableEncounterUnitForThreat(unit, source))
            return;

        if (seen.count(unit->GetGUID()))
            return;

        if (!IsEncounterTank(unit))
            return;

        seen.insert(unit->GetGUID());
        tanks.push_back(unit);
    }

    std::vector<Unit*> GatherEncounterTanks(Creature* source)
    {
        std::vector<Unit*> tanks;
        GuidSet seen;

        if (!source || !source->GetMap())
            return tanks;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            TryAddEncounterTank(player, source, seen, tanks);

            if (Group* group = player->GetGroup())
            {
                for (Unit* member : BotMgr::GetAllGroupMembers(group))
                    TryAddEncounterTank(member, source, seen, tanks);
            }

            if (BotMgr* botMgr = player->GetBotMgr())
            {
                for (BotMap::value_type const& pair : *botMgr->GetBotMap())
                    TryAddEncounterTank(pair.second, source, seen, tanks);
            }
        }

        return tanks;
    }

    Unit* SelectEncounterTankForAdd(Creature* add)
    {
        std::vector<Unit*> tanks = GatherEncounterTanks(add);
        if (!tanks.empty())
            return tanks[add->GetGUID().GetCounter() % tanks.size()];

        Player* fallback = nullptr;
        float bestDistance = 160.0f;

        if (!add || !add->GetMap())
            return nullptr;

        Map::PlayerList const& players = add->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsUsableEncounterUnitForThreat(player, add))
                continue;

            float distance = add->GetDistance(player);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                fallback = player;
            }
        }

        return fallback;
    }

    void ForceAddThreatToEncounterTank(Creature* add, float threatValue, bool forceAttack)
    {
        if (!add || !add->IsAlive())
            return;

        Unit* tank = SelectEncounterTankForAdd(add);
        if (!tank)
            return;

        add->SetInCombatWithZone();
        add->SetInCombatWith(tank);
        tank->SetInCombatWith(add);
        add->GetThreatMgr().AddThreat(tank, threatValue);

        if (forceAttack && add->AI())
            add->AI()->AttackStart(tank);
    }

    bool IsNpcBotUnit(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot() && creature->GetBotAI();
    }
}

struct boss_magtheridon : public BossAI
{
    boss_magtheridon(Creature* creature) : BossAI(creature, DATA_MAGTHERIDON)
    {
    }

    void Reset() override
    {
        BossAI::Reset();

        _channelersKilled = 0;
        _currentPhase = 0;
        _castingQuake = false;
        _recentlySpoken = false;
        _magReleased = false;
        _blastNovaDamagePct = 100;

        ReleaseBotCubeAssignments(false);
        ClearChannelerEmpowerments();

        _interruptScheduler.CancelAll();
        scheduler.CancelGroup(GROUP_BLAST_NOVA);
        scheduler.CancelGroup(GROUP_BOT_CUBE);
        scheduler.CancelGroup(GROUP_MAIN_TANK_SUPPORT);

        scheduler.Schedule(90s, [this](TaskContext context)
            {
                if (!me->IsEngaged())
                    Talk(SAY_TAUNT);

                context.Repeat(90s);
            });

        DoCastSelf(SPELL_SHADOW_CAGE, true);
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        me->SetImmuneToPC(true);

        ScheduleHealthCheckEvent(30, [&]
            {
                _currentPhase = 1;
                Talk(SAY_PHASE3);
                me->GetMotionMaster()->Clear();
                scheduler.DelayAll(18s);

                scheduler.Schedule(8s, [this](TaskContext /*context*/)
                    {
                        DoCastSelf(SPELL_CAMERA_SHAKE, true);
                        instance->SetData(DATA_COLLAPSE, GO_STATE_ACTIVE);
                    }).Schedule(15s, [this](TaskContext /*context*/)
                        {
                            DoCastSelf(SPELL_COLLAPSE_DAMAGE, true);

                            me->resetAttackTimer();

                            if (me->GetVictim())
                                me->GetMotionMaster()->MoveChase(me->GetVictim());

                            ResumeEncounterNpcBots();

                            _currentPhase = 0;

                            scheduler.Schedule(20s, [this](TaskContext context)
                                {
                                    DoCastAOE(SPELL_DEBRIS_TARGET);
                                    context.Repeat(20s);
                                });
                        });
            });
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (!_recentlySpoken)
        {
            Talk(SAY_SLAY);
            _recentlySpoken = true;
        }

        scheduler.Schedule(5s, [this](TaskContext /*context*/)
            {
                _recentlySpoken = false;
            });
    }

    void JustDied(Unit* killer) override
    {
        ReleaseBotCubeAssignments(false);
        ClearChannelerEmpowerments();

        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void ScheduleCombatEvents()
    {
        DoResetThreatList();

        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        me->SetImmuneToPC(false);
        me->SetReactState(REACT_AGGRESSIVE);
        instance->SetData(DATA_ACTIVATE_CUBES, 1);
        me->RemoveAurasDueToSpell(SPELL_SHADOW_CAGE);

        scheduler.Schedule(2s, GROUP_MAIN_TANK_SUPPORT, [this](TaskContext context)
            {
                ReinforceMainTankBotThreat(false);
                context.Repeat(2s);
            });

        scheduler.Schedule(18s, GROUP_MAIN_TANK_SUPPORT, [this](TaskContext context)
            {
                MoveMainTankBotToMiddleReset();
                context.Repeat(35s);
            });

        scheduler.Schedule(9s, [this](TaskContext context)
            {
                if (!_castingQuake && me->GetVictim())
                    DoCastVictim(SPELL_CLEAVE);

                context.Repeat(12s);
            }).Schedule(20s, [this](TaskContext context)
                {
                    me->CastCustomSpell(SPELL_BLAZE, SPELLVALUE_MAX_TARGETS, 1);
                    context.Repeat(11s, 39s);
                }).Schedule(28300ms, [this](TaskContext context)
                    {
                        if (!me->GetVictim())
                            return;

                        DoCastSelf(SPELL_QUAKE);

                        _castingQuake = true;
                        me->GetMotionMaster()->Clear();
                        me->SetReactState(REACT_PASSIVE);
                        me->SetOrientation(me->GetAngle(me->GetVictim()));
                        me->SetTarget(ObjectGuid::Empty);

                        scheduler.DelayAll(6999ms);

                        scheduler.Schedule(7s, [this](TaskContext /*context*/)
                            {
                                _castingQuake = false;
                                me->SetReactState(REACT_AGGRESSIVE);

                                if (me->GetVictim())
                                    me->GetMotionMaster()->MoveChase(me->GetVictim());

                                ResumeEncounterNpcBots();
                            });

                        context.Repeat(56300ms, 64300ms);
                    }).Schedule(47650ms, GROUP_BLAST_NOVA, [this](TaskContext context)
                        {
                            PrepareBlastNova();

                            scheduler.Schedule(8s, GROUP_BLAST_NOVA, [this](TaskContext /*context*/)
                                {
                                    ResolveBlastNova();
                                    scheduler.DelayAll(10s);
                                });

                            context.Repeat(54350ms, 55400ms);
                        }).Schedule(22min, [this](TaskContext /*context*/)
                            {
                                DoCastSelf(SPELL_BERSERK, true);
                            });
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_RELEASE_MAGTHERIDON)
        {
            if (_magReleased)
                return;

            ++_channelersKilled;
            RefreshChannelerEmpowerments();

            if (_channelersKilled < 5 && !GetLivingChannelers().empty())
                return;

            Talk(SAY_EMOTE_FREE);
            Talk(SAY_FREE);
            scheduler.CancelGroup(GROUP_EARLY_RELEASE_CHECK);
            _magReleased = true;

            scheduler.Schedule(3s, [this](TaskContext)
            {
                ScheduleCombatEvents();
            });

            return;
        }

        if (action == ACTION_BANISH_SELF)
        {
            Talk(SAY_BANISH);
            me->InterruptNonMeleeSpells(true);
            me->CastSpell(me, SPELL_SHADOW_CAGE_STUN, true);

            scheduler.Schedule(6s, GROUP_BLAST_NOVA, [this](TaskContext /*context*/)
                {
                    me->RemoveAurasDueToSpell(SPELL_SHADOW_CAGE_STUN);
                });

            return;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_EMOTE_BEGIN);

        instance->DoForAllMinions(DATA_MAGTHERIDON, [&](Creature* creature)
            {
                if (creature && creature->IsAlive())
                    creature->SetInCombatWithZone();
            });

        RefreshChannelerEmpowerments();

        scheduler.Schedule(60s, GROUP_EARLY_RELEASE_CHECK, [this](TaskContext /*context*/)
            {
                Talk(SAY_EMOTE_NEARLY);
            }).Schedule(120s, GROUP_EARLY_RELEASE_CHECK, [this](TaskContext /*context*/)
                {
                    Talk(SAY_EMOTE_FREE);
                    Talk(SAY_FREE);
                    _magReleased = true;
                }).Schedule(123s, GROUP_EARLY_RELEASE_CHECK, [this](TaskContext /*context*/)
                    {
                        ScheduleCombatEvents();
                    });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        _interruptScheduler.Update(diff);

        if (_currentPhase != 1 && !_castingQuake)
            DoMeleeAttackIfReady();
    }

    uint8 GetBlastNovaDamagePct() const
    {
        return _blastNovaDamagePct;
    }

private:
    struct BotCubeAssignment
    {
        ObjectGuid cubeGuid;
        bool started = false;
        bool noCastSet = false;
        bool inactionSet = false;
    };

    std::vector<Creature*> GetLivingChannelers() const
    {
        std::vector<Creature*> channelers;

        instance->DoForAllMinions(DATA_MAGTHERIDON, [&](Creature* creature)
            {
                if (creature && creature->GetEntry() == NPC_HELLFIRE_CHANNELER && creature->IsAlive())
                    channelers.push_back(creature);
            });

        return channelers;
    }

    void ClearChannelerEmpowerments()
    {
        instance->DoForAllMinions(DATA_MAGTHERIDON, [&](Creature* creature)
            {
                if (creature && creature->AI())
                    creature->AI()->DoAction(ACTION_CHANNELER_CLEAR_EMPOWERED);
            });

        _empoweredChannelers.clear();
    }

    void RefreshChannelerEmpowerments()
    {
        std::vector<Creature*> channelers = GetLivingChannelers();

        GuidSet livingGuids;
        for (Creature* channeler : channelers)
            livingGuids.insert(channeler->GetGUID());

        for (GuidSet::iterator itr = _empoweredChannelers.begin(); itr != _empoweredChannelers.end();)
        {
            if (!livingGuids.count(*itr))
            {
                if (Creature* channeler = ObjectAccessor::GetCreature(*me, *itr))
                    if (channeler->AI())
                        channeler->AI()->DoAction(ACTION_CHANNELER_CLEAR_EMPOWERED);

                itr = _empoweredChannelers.erase(itr);
            }
            else
                ++itr;
        }

        while (_empoweredChannelers.size() > 2)
        {
            ObjectGuid guid = *_empoweredChannelers.begin();

            if (Creature* channeler = ObjectAccessor::GetCreature(*me, guid))
                if (channeler->AI())
                    channeler->AI()->DoAction(ACTION_CHANNELER_CLEAR_EMPOWERED);

            _empoweredChannelers.erase(guid);
        }

        std::vector<Creature*> candidates;
        for (Creature* channeler : channelers)
        {
            if (!_empoweredChannelers.count(channeler->GetGUID()))
                candidates.push_back(channeler);
        }

        while (_empoweredChannelers.size() < 2 && !candidates.empty())
        {
            uint32 index = urand(0, candidates.size() - 1);
            Creature* channeler = candidates[index];

            if (channeler->AI())
                channeler->AI()->DoAction(ACTION_CHANNELER_SET_EMPOWERED);

            _empoweredChannelers.insert(channeler->GetGUID());
            candidates.erase(candidates.begin() + index);
        }
    }

    bool IsEncounterParticipant(Unit* unit) const
    {
        if (!unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMap() != me->GetMap())
            return false;

        if (unit->HasUnitState(UNIT_STATE_ISOLATED) || !unit->IsWithinDist(me, 140.0f))
            return false;

        if (unit->IsPlayer())
            return true;

        if (Creature* creature = unit->ToCreature())
            return creature->IsNPCBot() && !creature->IsTempBot() && !creature->IsFreeBot() && creature->GetBotAI();

        return false;
    }

    std::vector<Unit*> GatherEncounterUnits() const
    {
        std::vector<Unit*> units;
        GuidSet seen;

        auto addUnit = [&](Unit* unit)
            {
                if (!IsEncounterParticipant(unit) || seen.count(unit->GetGUID()))
                    return;

                seen.insert(unit->GetGUID());
                units.push_back(unit);
            };

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

    Creature* SelectMainTankBotForBoss() const
    {
        Creature* bestTank = nullptr;
        float bestDistance = 200.0f;

        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = unit ? unit->ToCreature() : nullptr;
            if (!bot || !IsNpcBotMainTank(bot))
                continue;

            float distance = bot->GetDistance(me);
            if (!bestTank || distance < bestDistance)
            {
                bestTank = bot;
                bestDistance = distance;
            }
        }

        return bestTank;
    }

    bool CanUseMainTankBotSupport() const
    {
        if (!me->IsEngaged() || !me->GetVictim())
            return false;

        if (_currentPhase == 1 || _castingQuake)
            return false;

        if (me->HasAura(SPELL_SHADOW_CAGE_STUN))
            return false;

        if (!_botCubeAssignments.empty())
            return false;

        return true;
    }

    void ReinforceMainTankBotThreat(bool forceAttack)
    {
        if (!CanUseMainTankBotSupport())
            return;

        Creature* tankBot = SelectMainTankBotForBoss();
        if (!tankBot || !tankBot->IsAlive())
            return;

        bot_ai* ai = tankBot->GetBotAI();
        if (!ai)
            return;

        tankBot->SetInCombatWith(me);
        me->SetInCombatWith(tankBot);
        me->GetThreatMgr().AddThreat(tankBot, 175000.0f);

        ai->RemoveBotCommandState(BOT_COMMAND_COMBATRESET | BOT_COMMAND_MASK_NOCAST_ANY);
        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        ai->AttackStart(me);

        if ((forceAttack || me->GetVictim() != tankBot) && me->AI())
            me->AI()->AttackStart(tankBot);
    }

    void MoveMainTankBotToMiddleReset()
    {
        if (!CanUseMainTankBotSupport())
            return;

        Creature* tankBot = SelectMainTankBotForBoss();
        if (!tankBot || !tankBot->IsAlive())
            return;

        bot_ai* ai = tankBot->GetBotAI();
        if (!ai)
            return;

        if (ai->HasBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
            return;

        ReinforceMainTankBotThreat(true);

        Position center;
        center.Relocate(MAGTHERIDON_TANK_RESET_X, MAGTHERIDON_TANK_RESET_Y, MAGTHERIDON_TANK_RESET_Z, MAGTHERIDON_TANK_RESET_O);

        tankBot->InterruptNonMeleeSpells(false);
        tankBot->AttackStop();
        tankBot->BotStopMovement();

        ai->RemoveBotCommandState(BOT_COMMAND_COMBATRESET | BOT_COMMAND_MASK_NOCAST_ANY);
        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
        ai->MoveToSendPosition(center);

        ObjectGuid tankGuid = tankBot->GetGUID();

        scheduler.Schedule(2500ms, GROUP_MAIN_TANK_SUPPORT, [this, tankGuid](TaskContext /*context*/)
            {
                Creature* tankBot = ObjectAccessor::GetCreature(*me, tankGuid);
                if (!tankBot || !tankBot->IsAlive())
                    return;

                bot_ai* ai = tankBot->GetBotAI();
                if (!ai)
                    return;

                ReinforceMainTankBotThreat(true);

                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                ai->AttackStart(me);

                if (me->GetVictim())
                    me->GetMotionMaster()->MoveChase(me->GetVictim());
            });
    }

    uint8 GetRequiredCubeChannels() const
    {
        uint8 livingEncounterUnits = uint8(GatherEncounterUnits().size());
        return std::min<uint8>(5, std::max<uint8>(2, livingEncounterUnits / 3));
    }

    uint8 CountActiveCubeChannels() const
    {
        uint8 activeChannels = 0;

        for (Unit* unit : GatherEncounterUnits())
        {
            if (unit->HasAura(SPELL_SHADOW_GRASP))
                ++activeChannels;
        }

        activeChannels = std::max<uint8>(activeChannels, std::min<uint8>(5, me->GetAuraCount(SPELL_SHADOW_GRASP_VISUAL)));

        uint8 scriptedBotChannels = 0;
        for (std::map<ObjectGuid, BotCubeAssignment>::value_type const& pair : _botCubeAssignments)
        {
            if (!pair.second.started)
                continue;

            Creature* bot = ObjectAccessor::GetCreature(*me, pair.first);
            if (!bot || !bot->IsAlive() || bot->HasAura(SPELL_MIND_EXHAUSTION))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai)
                continue;

            if (ai->HasBotCommandState(BOT_COMMAND_INACTION))
                ++scriptedBotChannels;
        }

        activeChannels = std::max<uint8>(activeChannels, std::min<uint8>(5, scriptedBotChannels));
        return std::min<uint8>(5, activeChannels);
    }

    std::vector<GameObject*> GetManticronCubes() const
    {
        std::list<GameObject*> cubeList;
        me->GetGameObjectListWithEntryInGrid(cubeList, GO_MANTICRON_CUBE, 120.0f);

        std::vector<GameObject*> cubes;
        cubes.reserve(cubeList.size());

        for (GameObject* cube : cubeList)
        {
            if (cube && !cube->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
                cubes.push_back(cube);
        }

        return cubes;
    }

    GameObject* FindNearestAvailableCube(WorldObject* source, std::vector<GameObject*> const& cubes, GuidSet const& reservedCubes, float maxDistance) const
    {
        GameObject* nearestCube = nullptr;
        float nearestDistance = maxDistance;

        for (GameObject* cube : cubes)
        {
            if (!cube || reservedCubes.count(cube->GetGUID()))
                continue;

            float distance = source->GetDistance(cube);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestCube = cube;
            }
        }

        return nearestCube;
    }

    bool IsBotCastingUninterruptible(Creature* bot) const
    {
        if (Spell* spell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            if (SpellInfo const* spellInfo = spell->GetSpellInfo())
                if (!(spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT))
                    return true;
        }

        if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (SpellInfo const* spellInfo = spell->GetSpellInfo())
                if (!(spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))
                    return true;
        }

        return false;
    }

    bool IsBotEligibleForCube(Creature* bot) const
    {
        if (!IsEncounterParticipant(bot) || !bot->IsNPCBot())
            return false;

        if (bot->HasAnyAuras(SPELL_MIND_EXHAUSTION, SPELL_SHADOW_GRASP))
            return false;

        if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_ROOT) || IsBotCastingUninterruptible(bot))
            return false;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return false;

        // Do not ever pull the main boss tank off Magtheridon for cube duty.
        // Off-tanks may still be assigned if they are otherwise eligible.
        if (ai->IsTank() && !ai->IsOffTank())
            return false;

        if (ai->HasBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY))
            return false;

        return true;
    }

    bool IsAssignedBotStillUsable(Creature* bot) const
    {
        if (!IsEncounterParticipant(bot) || !bot->IsNPCBot())
            return false;

        if (bot->HasAura(SPELL_MIND_EXHAUSTION) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_ROOT))
            return false;

        return !IsBotCastingUninterruptible(bot);
    }

    void PrepareBlastNova()
    {
        _blastNovaDamagePct = 100;

        Talk(SAY_EMOTE_NOVA);

        ReleaseBotCubeAssignments(false);

        uint8 requiredChannels = GetRequiredCubeChannels();
        uint8 activeChannels = CountActiveCubeChannels();

        if (activeChannels < requiredChannels)
            AssignBotsToCubes(requiredChannels, activeChannels);
    }

    void AssignBotsToCubes(uint8 requiredChannels, uint8 activeChannels)
    {
        std::vector<GameObject*> cubes = GetManticronCubes();
        if (cubes.empty())
            return;

        GuidSet reservedCubes;
        std::vector<Unit*> encounterUnits = GatherEncounterUnits();

        for (Unit* unit : encounterUnits)
        {
            if (!unit->IsPlayer())
                continue;

            if (unit->HasAura(SPELL_SHADOW_GRASP))
            {
                if (GameObject* cube = FindNearestAvailableCube(unit, cubes, reservedCubes, 35.0f))
                    reservedCubes.insert(cube->GetGUID());
            }
            else if (!unit->HasAura(SPELL_MIND_EXHAUSTION))
            {
                if (GameObject* cube = FindNearestAvailableCube(unit, cubes, reservedCubes, 10.0f))
                    reservedCubes.insert(cube->GetGUID());
            }
        }

        std::vector<Creature*> candidateBots;
        for (Unit* unit : encounterUnits)
        {
            if (Creature* bot = unit->ToCreature())
                if (IsBotEligibleForCube(bot))
                    candidateBots.push_back(bot);
        }

        uint8 missingChannels = requiredChannels > activeChannels ? requiredChannels - activeChannels : 0;

        for (uint8 i = 0; i < missingChannels && !candidateBots.empty(); ++i)
        {
            Creature* chosenBot = nullptr;
            GameObject* chosenCube = nullptr;
            float bestDistance = 100.0f;

            for (Creature* bot : candidateBots)
            {
                if (_botCubeAssignments.count(bot->GetGUID()))
                    continue;

                if (GameObject* cube = FindNearestAvailableCube(bot, cubes, reservedCubes, 100.0f))
                {
                    float distance = bot->GetDistance(cube);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        chosenBot = bot;
                        chosenCube = cube;
                    }
                }
            }

            if (!chosenBot || !chosenCube)
                break;

            reservedCubes.insert(chosenCube->GetGUID());
            StartBotCubeAssignment(chosenBot, chosenCube);

            candidateBots.erase(std::remove(candidateBots.begin(), candidateBots.end(), chosenBot), candidateBots.end());
        }
    }

    void StartBotCubeAssignment(Creature* bot, GameObject* cube)
    {
        bot_ai* ai = bot ? bot->GetBotAI() : nullptr;
        if (!bot || !cube || !ai)
            return;

        Position cubePosition;
        cubePosition.Relocate(cube->GetPositionX(), cube->GetPositionY(), cube->GetPositionZ(), bot->GetOrientation());

        bot->AttackStop();
        bot->InterruptNonMeleeSpells(false);

        ai->SetBotCommandState(BOT_COMMAND_INACTION);
        ai->SetBotCommandState(BOT_COMMAND_NO_CAST);
        ai->MoveToSendPosition(cubePosition);

        BotCubeAssignment assignment;
        assignment.cubeGuid = cube->GetGUID();
        assignment.noCastSet = true;
        assignment.inactionSet = true;
        _botCubeAssignments[bot->GetGUID()] = assignment;

        ObjectGuid botGuid = bot->GetGUID();

        // Move on the warning, channel at the very end.
        // Blast Nova resolves 8 seconds after PrepareBlastNova().
        scheduler.Schedule(6200ms, GROUP_BOT_CUBE, [this, botGuid](TaskContext /*context*/)
            {
                TryStartBotCubeChannel(botGuid);
            }).Schedule(6800ms, GROUP_BOT_CUBE, [this, botGuid](TaskContext /*context*/)
                {
                    TryStartBotCubeChannel(botGuid);
                }).Schedule(7300ms, GROUP_BOT_CUBE, [this, botGuid](TaskContext /*context*/)
                    {
                        TryStartBotCubeChannel(botGuid);
                    }).Schedule(7750ms, GROUP_BOT_CUBE, [this, botGuid](TaskContext /*context*/)
                        {
                            TryStartBotCubeChannel(botGuid);
                        });
    }

    void TryStartBotCubeChannel(ObjectGuid botGuid)
    {
        std::map<ObjectGuid, BotCubeAssignment>::iterator itr = _botCubeAssignments.find(botGuid);
        if (itr == _botCubeAssignments.end() || itr->second.started)
            return;

        Creature* bot = ObjectAccessor::GetCreature(*me, botGuid);
        GameObject* cube = ObjectAccessor::GetGameObject(*me, itr->second.cubeGuid);

        if (!bot || !cube || !IsAssignedBotStillUsable(bot))
            return;

        if (bot->GetDistance(cube) > 17.0f)
        {
            if (bot_ai* ai = bot->GetBotAI())
            {
                Position cubePosition;
                cubePosition.Relocate(cube->GetPositionX(), cube->GetPositionY(), cube->GetPositionZ(), bot->GetOrientation());
                ai->MoveToSendPosition(cubePosition);
            }

            return;
        }

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        bot->BotStopMovement();
        bot->AttackStop();
        bot->InterruptNonMeleeSpells(false);
        bot->SetFacingTo(bot->GetAngle(cube));

        ai->SetBotCommandState(BOT_COMMAND_INACTION);
        ai->SetBotCommandState(BOT_COMMAND_NO_CAST);
        itr->second.inactionSet = true;
        itr->second.noCastSet = true;

        if (Creature* trigger = cube->FindNearestCreature(NPC_HELLFIRE_RAID_TRIGGER, 15.0f))
            trigger->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHADOW_GRASP_VISUAL, true);

        bot->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHADOW_GRASP, true);
        itr->second.started = true;
    }

    void ResolveBlastNova()
    {
        uint8 requiredChannels = GetRequiredCubeChannels();
        uint8 activeChannels = CountActiveCubeChannels();

        if (activeChannels >= requiredChannels)
        {
            _blastNovaDamagePct = 0;
            DoAction(ACTION_BANISH_SELF);

            scheduler.Schedule(500ms, GROUP_BOT_CUBE, [this](TaskContext /*context*/)
                {
                    ReleaseBotCubeAssignments(true);
                });

            return;
        }

        if (activeChannels == 0)
            _blastNovaDamagePct = 100;
        else
        {
            uint8 reductionPct = uint8(activeChannels * 75 / requiredChannels);
            _blastNovaDamagePct = std::max<uint8>(25, 100 - reductionPct);
        }

        DoCastSelf(SPELL_BLAST_NOVA);

        scheduler.Schedule(1200ms, GROUP_BOT_CUBE, [this](TaskContext /*context*/)
            {
                ReleaseBotCubeAssignments(true);
            });
    }

    void RestoreBotAfterCube(Creature* bot, bot_ai* ai)
    {
        if (!bot || !ai)
            return;

        ai->RemoveBotCommandState(BOT_COMMAND_INACTION | BOT_COMMAND_MASK_NOCAST_ANY | BOT_COMMAND_ATTACK | BOT_COMMAND_COMBATRESET);
        bot->InterruptNonMeleeSpells(true);
        bot->AttackStop();
        bot->BotStopMovement();

        if (!bot->IsAlive())
            return;

        if (me->IsEngaged() && me->GetVictim())
        {
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
            return;
        }

        ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
    }

    void ReleaseBotCubeAssignments(bool applyExhaustion)
    {
        scheduler.CancelGroup(GROUP_BOT_CUBE);

        for (std::map<ObjectGuid, BotCubeAssignment>::value_type& pair : _botCubeAssignments)
        {
            Creature* bot = ObjectAccessor::GetCreature(*me, pair.first);
            if (!bot)
                continue;

            if (pair.second.started)
            {
                bot->RemoveAurasDueToSpell(SPELL_SHADOW_GRASP);

                if (applyExhaustion && bot->IsAlive())
                    bot->CastSpell(bot, SPELL_MIND_EXHAUSTION, true);
            }

            if (bot_ai* ai = bot->GetBotAI())
                RestoreBotAfterCube(bot, ai);
        }

        _botCubeAssignments.clear();
    }

    void ResumeEncounterNpcBots()
    {
        if (!me->IsEngaged() || !me->GetVictim())
            return;

        for (Unit* unit : GatherEncounterUnits())
        {
            Creature* bot = unit->ToCreature();
            if (!bot || !bot->IsNPCBot() || _botCubeAssignments.count(bot->GetGUID()))
                continue;

            bot_ai* ai = bot->GetBotAI();
            if (!ai || ai->HasBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FULLSTOP | BOT_COMMAND_INACTION))
                continue;

            ai->RemoveBotCommandState(BOT_COMMAND_MASK_NOCAST_ANY | BOT_COMMAND_COMBATRESET);
            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
            ai->AttackStart(me);
        }
    }

    bool _castingQuake;
    bool _recentlySpoken;
    bool _magReleased;
    uint8 _currentPhase;
    uint8 _channelersKilled;
    uint8 _blastNovaDamagePct;
    GuidSet _empoweredChannelers;
    std::map<ObjectGuid, BotCubeAssignment> _botCubeAssignments;
    TaskScheduler _interruptScheduler;
};

struct npc_hellfire_channeler_magtheridon : public ScriptedAI
{
    npc_hellfire_channeler_magtheridon(Creature* creature)
        : ScriptedAI(creature),
        instance(creature->GetInstanceScript()),
        _scheduler(),
        summons(me),
        _empowered(false)
    {
    }

    void Reset() override
    {
        _scheduler.CancelAll();
        summons.DespawnAll();

        _empowered = false;

        me->SetReactState(REACT_DEFENSIVE);
        ScheduleOutOfCombatChannel();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (instance)
            instance->SetData(DATA_CHANNELER_COMBAT, 1);

        me->InterruptNonMeleeSpells(true);
        _scheduler.CancelAll();

        ForceAddThreatToEncounterTank(me, 350000.0f, true);

        ScheduleCombatEvents();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _scheduler.CancelAll();
        summons.DespawnAll();

        me->CastSpell(me, SPELL_CHANNELER_SOUL_TRANSFER, true);

        if (!instance)
            return;

        if (Creature* magtheridon = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_MAGTHERIDON)))
            if (magtheridon->AI())
                magtheridon->AI()->DoAction(ACTION_RELEASE_MAGTHERIDON);
    }

    void JustSummoned(Creature* summon) override
    {
        if (!summon)
            return;

        summons.Summon(summon);

        summon->SetInCombatWithZone();
        ForceAddThreatToEncounterTank(summon, 750000.0f, true);

        ObjectGuid summonGuid = summon->GetGUID();

        _scheduler.Schedule(1500ms, [this, summonGuid](TaskContext /*context*/)
            {
                Creature* summon = ObjectAccessor::GetCreature(*me, summonGuid);
                if (summon && summon->IsAlive())
                    ForceAddThreatToEncounterTank(summon, 500000.0f, true);
            });
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        summons.Despawn(summon);
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_CHANNELER_SET_EMPOWERED)
            _empowered = true;
        else if (action == ACTION_CHANNELER_CLEAR_EMPOWERED)
            _empowered = false;
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    void ScheduleOutOfCombatChannel()
    {
        _scheduler.Schedule(3600ms, [this](TaskContext context)
            {
                if (!me->IsEngaged() && !me->HasUnitState(UNIT_STATE_CASTING))
                    DoCastSelf(SPELL_CHANNELER_SHADOW_GRASP);

                context.Repeat(3600ms);
            });
    }

    void ScheduleCombatEvents()
    {
        _scheduler.Schedule(2s, [this](TaskContext context)
            {
                bool const badVictim = me->GetVictim() && !IsEncounterTank(me->GetVictim());
                ForceAddThreatToEncounterTank(me, _empowered ? 90000.0f : 50000.0f, badVictim);
                context.Repeat(3s);
            }).Schedule(20900ms, 28200ms, [this](TaskContext context)
                {
                    DoCastSelf(SPELL_CHANNELER_SHADOW_BOLT_VOLLEY);
                    context.Repeat(_empowered ? Milliseconds(8000) : Milliseconds(22000), _empowered ? Milliseconds(12000) : Milliseconds(32000));
                }).Schedule(14500ms, 15000ms, [this](TaskContext context)
                    {
                        if (Creature* target = SelectDarkMendingTarget())
                            DoCast(target, SPELL_CHANNELER_DARK_MENDING);

                        context.Repeat(_empowered ? Milliseconds(9000) : Milliseconds(22000), _empowered ? Milliseconds(12000) : Milliseconds(30000));
                    }).Schedule(6s, 12s, [this](TaskContext context)
                        {
                            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, false))
                                DoCast(target, SPELL_CHANNELER_FEAR);

                            context.Repeat(_empowered ? Milliseconds(12000) : Milliseconds(17000), _empowered ? Milliseconds(20000) : Milliseconds(28000));
                        }).Schedule(19650ms, 43350ms, [this](TaskContext context)
                            {
                                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 80.0f, true))
                                    DoCast(target, SPELL_CHANNELER_BURNING_ABYSSAL);

                                context.Repeat(_empowered ? Milliseconds(40000) : Milliseconds(80000), _empowered ? Milliseconds(55000) : Milliseconds(100000));
                            });
    }

    Creature* SelectDarkMendingTarget()
    {
        Creature* lowestHealthChanneler = nullptr;
        float lowestHealthPct = 50.0f;

        auto trySelect = [&](Creature* channeler)
            {
                if (!channeler || channeler->GetEntry() != NPC_HELLFIRE_CHANNELER || !channeler->IsAlive())
                    return;

                if (!channeler->IsWithinDist(me, 30.0f))
                    return;

                float healthPct = channeler->GetHealthPct();
                if (healthPct < lowestHealthPct)
                {
                    lowestHealthPct = healthPct;
                    lowestHealthChanneler = channeler;
                }
            };

        trySelect(me);

        std::list<Creature*> channelers;
        me->GetCreatureListWithEntryInGrid(channelers, NPC_HELLFIRE_CHANNELER, 30.0f);

        for (Creature* channeler : channelers)
            trySelect(channeler);

        return lowestHealthChanneler;
    }

    InstanceScript* instance;
    TaskScheduler _scheduler;
    SummonList summons;
    bool _empowered;
};

struct npc_target_trigger : public ScriptedAI
{
    npc_target_trigger(Creature* creature) : ScriptedAI(creature), _cast(false)
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void Reset() override
    {
        if (_cast)
            return;

        DoCastSelf(SPELL_DEBRIS_VISUAL);
        _cast = true;

        _scheduler.Schedule(5s, [this](TaskContext /*context*/)
            {
                DoCastSelf(SPELL_DEBRIS_DAMAGE);
                me->DespawnOrUnsummon(6s);
            });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

protected:
    TaskScheduler _scheduler;
    bool _cast;
};

class spell_magtheridon_blaze : public SpellScript
{
    PrepareSpellScript(spell_magtheridon_blaze);

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_BLAZE_SUMMON, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_magtheridon_blaze::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_magtheridon_shadow_grasp : public AuraScript
{
    PrepareAuraScript(spell_magtheridon_shadow_grasp);

    void HandleDummyApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHADOW_GRASP_VISUAL, false);
    }

    void HandleDummyRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->InterruptNonMeleeSpells(true);
    }

    void HandlePeriodicRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetUnitOwner()->CastSpell(GetUnitOwner(), SPELL_MIND_EXHAUSTION, true);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_magtheridon_shadow_grasp::HandleDummyApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_magtheridon_shadow_grasp::HandleDummyRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_magtheridon_shadow_grasp::HandlePeriodicRemove, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_magtheridon_shadow_grasp_visual : public AuraScript
{
    PrepareAuraScript(spell_magtheridon_shadow_grasp_visual);

    void HandleDummyApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTarget()->GetAuraCount(SPELL_SHADOW_GRASP_VISUAL) == 5)
        {
            if (GetTarget()->GetAI())
                GetTarget()->GetAI()->DoAction(ACTION_BANISH_SELF);
        }
    }

    void HandleDummyRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_SHADOW_CAGE_STUN);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_magtheridon_shadow_grasp_visual::HandleDummyApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_magtheridon_shadow_grasp_visual::HandleDummyRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_magtheridon_blast_nova : public SpellScript
{
    PrepareSpellScript(spell_magtheridon_blast_nova);

    void ScaleDamage()
    {
        Unit* casterUnit = GetCaster();
        if (!casterUnit)
            return;

        Creature* caster = casterUnit->ToCreature();
        if (!caster)
            return;

        if (boss_magtheridon* magtheridonAI = CAST_AI(boss_magtheridon, caster->AI()))
            SetHitDamage(CalculatePct(GetHitDamage(), magtheridonAI->GetBlastNovaDamagePct()));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_magtheridon_blast_nova::ScaleDamage);
    }
};

class spell_magtheridon_quake : public SpellScript
{
    PrepareSpellScript(spell_magtheridon_quake);

    void HandleHit()
    {
        if (urand(0, 3) == 0)
            GetCaster()->CastSpell(GetCaster(), SPELL_QUAKE_KNOCKBACK, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_magtheridon_quake::HandleHit);
    }
};

class spell_magtheridon_debris_target_selector : public SpellScript
{
    PrepareSpellScript(spell_magtheridon_debris_target_selector);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject* target) -> bool
            {
                return !target || target->GetEntry() != NPC_TARGET_TRIGGER;
            });

        Acore::Containers::RandomResize(targets, 1);
    }

    void HandleHit()
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_DEBRIS_SPAWN);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_magtheridon_debris_target_selector::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnHit += SpellHitFn(spell_magtheridon_debris_target_selector::HandleHit);
    }
};

class go_manticron_cube : public GameObjectScript
{
public:
    go_manticron_cube() : GameObjectScript("go_manticron_cube")
    {
    }

    bool OnGossipHello(Player* player, GameObject* /*go*/) override
    {
        if (player->HasAnyAuras(SPELL_MIND_EXHAUSTION, SPELL_SHADOW_GRASP))
            return true;

        if (Creature* trigger = player->FindNearestCreature(NPC_HELLFIRE_RAID_TRIGGER, 10.0f))
            trigger->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHADOW_GRASP_VISUAL, true);

        player->CastSpell(static_cast<Unit*>(nullptr), SPELL_SHADOW_GRASP, true);
        return true;
    }
};

// 30619 - Magtheridon Cleave
// 30619 - Magtheridon Cleave
class spell_magtheridon_cleave : public SpellScript
{
    PrepareSpellScript(spell_magtheridon_cleave);

    bool ShouldTakeCleave(Unit* unit) const
    {
        if (!unit)
            return false;

        // Real players are always responsible for their own positioning.
        if (unit->GetTypeId() == TYPEID_PLAYER)
            return true;

        // NPCBot tanks and off-tanks may take Cleave.
        // Non-tank bots, pets, summons, totems, guardians, and incidental creatures do not.
        if (Creature* creature = unit->ToCreature())
        {
            if (!creature->IsNPCBot())
                return false;

            bot_ai* ai = creature->GetBotAI();
            return ai && (ai->IsTank() || ai->IsOffTank());
        }

        return false;
    }

    void HandleHit()
    {
        Unit* hitUnit = GetHitUnit();
        if (!ShouldTakeCleave(hitUnit))
            SetHitDamage(0);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_magtheridon_cleave::HandleHit);
    }
};

void AddSC_boss_magtheridon()
{
    RegisterMagtheridonsLairCreatureAI(boss_magtheridon);
    RegisterMagtheridonsLairCreatureAI(npc_hellfire_channeler_magtheridon);
    RegisterMagtheridonsLairCreatureAI(npc_target_trigger);

    RegisterSpellScript(spell_magtheridon_blaze);
    RegisterSpellScript(spell_magtheridon_shadow_grasp);
    RegisterSpellScript(spell_magtheridon_shadow_grasp_visual);
    RegisterSpellScript(spell_magtheridon_blast_nova);
    RegisterSpellScript(spell_magtheridon_quake);
    RegisterSpellScript(spell_magtheridon_debris_target_selector);
    RegisterSpellScript(spell_magtheridon_cleave);

    new go_manticron_cube();
}
