/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * at your option any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScriptLoader.h"
#include "shadow_labyrinth.h"
#include "SpellScript.h"

#include "Config.h"
#include "Creature.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Position.h"
#include "SharedDefines.h"

#include "bot_ai.h"
#include "botmgr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace DBMFTABotCallouts
{
    uint32 GetCooldownMs();
    Creature* AsNPCBotCreature(Unit* unit);
    void AnnounceMoveAwayFromMeForModule(Creature* bot, uint32 spellId, char const* moduleFolder, char const* moduleId, std::string const& mechanicName, uint32 cooldownMs = 5000);
}

enum Spells
{
    SPELL_SUPPRESSION = 33332,
    SPELL_SHOCKWAVE = 33686,
    SPELL_SHOCKWAVE_SERVERSIDE = 33673,
    SPELL_RESONANCE = 33657,
    SPELL_MAGNETIC_PULL = 33689,
    SPELL_SONIC_SHOCK = 38797,
    SPELL_THUNDERING_STORM = 39365,
    SPELL_MURMUR_WRATH_AOE = 33329,
    SPELL_MURMUR_WRATH = 33331,

    SPELL_SONIC_BOOM_CAST = 33923,
    SPELL_SONIC_BOOM_EFFECT = 38795,
    SPELL_MURMURS_TOUCH = 33711,

    SPELL_SWIFTNESS_POTION = 2379
};

enum Misc
{
    EMOTE_SONIC_BOOM = 0,

    GROUP_RESONANCE = 1,
    GROUP_OOC_CAST = 2,

    GUID_MURMUR_NPCS = 1
};

enum Npc
{
    NPC_CABAL_SPELLBINDER = 18639
};

namespace MurmurCustom
{
    constexpr uint32 DEFAULT_SONIC_BOOM_DAMAGE_NORMAL = 15000;
    constexpr uint32 DEFAULT_SONIC_BOOM_DAMAGE_HEROIC = 30000;

    constexpr float BOT_MAX_FORCED_MOVE_DISTANCE = 120.0f;

    // These are measured from BeginSonicBoomCast(), which is the
    // "Murmur draws energy from the air..." emote / Sonic Boom cast start.
    constexpr uint32 SONIC_BOOM_CAST_TIME_MS = 6000;
    constexpr uint32 BOT_SAFE_SPOT_TELEPORT_DELAY_AFTER_CAST_START_MS = 5000;
    constexpr uint32 SONIC_BOOM_FOLLOW_FALLBACK_DELAY_AFTER_CAST_START_MS = 7000;

    static std::array<Position, 8> const SafeSpots =
    {
        Position(-156.26f, -467.69f, 17.08f, 0.0f),
        Position(-176.23f, -477.03f, 18.20f, 0.0f),
        Position(-184.49f, -491.53f, 18.20f, 0.0f),
        Position(-176.92f, -514.48f, 18.20f, 0.0f),
        Position(-156.23f, -523.22f, 18.20f, 0.0f),
        Position(-131.42f, -503.26f, 18.20f, 0.0f),
        Position(-134.65f, -478.22f, 18.20f, 0.0f),
        Position(-145.94f, -470.17f, 18.20f, 0.0f)
    };

    inline float ClampFloat(float value, float minValue, float maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    inline uint32 ClampUInt32(uint32 value, uint32 minValue, uint32 maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    inline uint32 GetMagneticPullLeadTimeMs()
    {
        uint32 value = sConfigMgr->GetOption<uint32>("ShadowLabyrinth.Murmur.SonicBoom.MagneticPullLeadTimeMs", 2000);
        return ClampUInt32(value, 0, 10000);
    }

    inline bool BotSwiftnessEnabled()
    {
        return sConfigMgr->GetOption<bool>("ShadowLabyrinth.Murmur.SonicBoom.BotSwiftness.Enable", true);
    }

    inline bool SonicBoomNonAttackableEnabled()
    {
        return sConfigMgr->GetOption<bool>("ShadowLabyrinth.Murmur.SonicBoom.NonAttackableDuringCast", true);
    }

    inline float GetSonicBoomPointBlankRadius()
    {
        float value = sConfigMgr->GetOption<float>("ShadowLabyrinth.Murmur.SonicBoom.PointBlankRadius", 5.0f);
        return ClampFloat(value, 0.0f, 40.0f);
    }

    inline float GetSonicBoomMaxFalloffDistance()
    {
        float pointBlankRadius = GetSonicBoomPointBlankRadius();
        float value = sConfigMgr->GetOption<float>("ShadowLabyrinth.Murmur.SonicBoom.MaxFalloffDistance", 35.0f);
        return ClampFloat(value, pointBlankRadius + 1.0f, 80.0f);
    }

    inline float GetSonicBoomMinDamagePctAtMaxRange()
    {
        float value = sConfigMgr->GetOption<float>("ShadowLabyrinth.Murmur.SonicBoom.MinDamagePctAtMaxRange", 25.0f);
        return ClampFloat(value, 0.0f, 100.0f);
    }

    inline float GetSonicBoomNpcBotFinalDamageReductionPct()
    {
        float value = sConfigMgr->GetOption<float>("ShadowLabyrinth.Murmur.SonicBoom.NpcBotFinalDamageReductionPct", 0.0f);
        return ClampFloat(value, 0.0f, 100.0f);
    }

    inline uint32 GetSonicBoomBaseDamage(Map* map)
    {
        bool heroic = map && map->IsHeroic();

        uint32 value = heroic
            ? sConfigMgr->GetOption<uint32>("ShadowLabyrinth.Murmur.SonicBoom.BaseDamage.Heroic", DEFAULT_SONIC_BOOM_DAMAGE_HEROIC)
            : sConfigMgr->GetOption<uint32>("ShadowLabyrinth.Murmur.SonicBoom.BaseDamage.Normal", DEFAULT_SONIC_BOOM_DAMAGE_NORMAL);

        return ClampUInt32(value, 1, uint32(std::numeric_limits<int32>::max()));
    }

    inline bool IsNpcBot(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->IsNPCBot();
    }

    inline int32 CalculateSonicBoomDamage(Unit* caster, Unit* target)
    {
        if (!caster || !target)
            return 0;

        uint32 baseDamage = GetSonicBoomBaseDamage(caster->GetMap());

        float pointBlankRadius = GetSonicBoomPointBlankRadius();
        float maxFalloffDistance = GetSonicBoomMaxFalloffDistance();
        float minScale = GetSonicBoomMinDamagePctAtMaxRange() / 100.0f;

        float dx = target->GetPositionX() - caster->GetPositionX();
        float dy = target->GetPositionY() - caster->GetPositionY();
        float distance = std::sqrt((dx * dx) + (dy * dy));

        float scale = 1.0f;
        if (distance > pointBlankRadius)
        {
            float t = (distance - pointBlankRadius) / (maxFalloffDistance - pointBlankRadius);
            t = ClampFloat(t, 0.0f, 1.0f);
            scale = 1.0f - ((1.0f - minScale) * t);
        }

        double damage = double(baseDamage) * double(scale);

        if (IsNpcBot(target))
        {
            float botReductionPct = GetSonicBoomNpcBotFinalDamageReductionPct();
            damage *= double((100.0f - botReductionPct) / 100.0f);
        }

        if (damage <= 0.0)
            return 0;

        if (damage >= double(std::numeric_limits<int32>::max()))
            return std::numeric_limits<int32>::max();

        return int32(damage + 0.5);
    }

    inline bool IsValidDungeonUnit(Unit* unit, Map* map)
    {
        return unit && unit->IsInWorld() && unit->IsAlive() && unit->GetMap() == map;
    }

    inline bool ContainsGuid(std::vector<Unit*> const& units, ObjectGuid const& guid)
    {
        return std::any_of(units.begin(), units.end(), [guid](Unit const* unit)
            {
                return unit && unit->GetGUID() == guid;
            });
    }

    inline bool ContainsCreatureGuid(std::vector<Creature*> const& creatures, ObjectGuid const& guid)
    {
        return std::any_of(creatures.begin(), creatures.end(), [guid](Creature const* creature)
            {
                return creature && creature->GetGUID() == guid;
            });
    }

    void AddUnitIfValid(std::vector<Unit*>& units, Unit* unit, Map* map)
    {
        if (!IsValidDungeonUnit(unit, map))
            return;

        if (ContainsGuid(units, unit->GetGUID()))
            return;

        units.push_back(unit);
    }

    void AddBotIfValid(std::vector<Creature*>& bots, Creature* bot, Map* map)
    {
        if (!bot || !bot->IsNPCBot())
            return;

        if (!IsValidDungeonUnit(bot, map))
            return;

        if (ContainsCreatureGuid(bots, bot->GetGUID()))
            return;

        bots.push_back(bot);
    }

    void AddPlayerBots(Player* player, Map* map, std::vector<Unit*>* units, std::vector<Creature*>* bots)
    {
        if (!player || !player->HaveBot())
            return;

        BotMap const* botMap = player->GetBotMgr()->GetBotMap();
        if (!botMap)
            return;

        for (BotMap::const_iterator itr = botMap->begin(); itr != botMap->end(); ++itr)
        {
            Creature* bot = itr->second;
            if (!bot)
                continue;

            if (units)
                AddUnitIfValid(*units, bot, map);

            if (bots)
                AddBotIfValid(*bots, bot, map);
        }
    }

    std::vector<Unit*> GetDungeonParticipants(Map* map)
    {
        std::vector<Unit*> units;

        if (!map)
            return units;

        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            AddUnitIfValid(units, player, map);
            AddPlayerBots(player, map, &units, nullptr);
        }

        return units;
    }

    std::vector<Creature*> GetDungeonNpcBots(Map* map)
    {
        std::vector<Creature*> bots;

        if (!map)
            return bots;

        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            AddPlayerBots(player, map, nullptr, &bots);
        }

        return bots;
    }

    Unit* SelectRandomDungeonParticipant(Map* map)
    {
        std::vector<Unit*> targets = GetDungeonParticipants(map);
        if (targets.empty())
            return nullptr;

        return targets[urand(0, uint32(targets.size() - 1))];
    }

    void SetDungeonNpcBotsToFollow(Map* map)
    {
        std::vector<Creature*> bots = GetDungeonNpcBots(map);

        for (Creature* bot : bots)
        {
            if (!bot || !bot->IsAlive())
                continue;

            if (bot_ai* ai = bot->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_FOLLOW, true);
        }
    }

    void SetMurmurSonicBoomNonAttackable(Creature* murmur, bool apply)
    {
        if (!murmur)
            return;

        if (apply)
        {
            if (SonicBoomNonAttackableEnabled())
                murmur->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);

            return;
        }

        murmur->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }

    uint8 FindNearestAvailableSafeSpot(Creature* bot, std::array<bool, SafeSpots.size()>& used)
    {
        uint8 bestIndex = 0;
        float bestDist = std::numeric_limits<float>::max();

        bool allUsed = true;
        for (bool spotUsed : used)
        {
            if (!spotUsed)
            {
                allUsed = false;
                break;
            }
        }

        if (allUsed)
            used.fill(false);

        for (uint8 i = 0; i < SafeSpots.size(); ++i)
        {
            if (used[i])
                continue;

            float dist = bot->GetExactDist(
                SafeSpots[i].GetPositionX(),
                SafeSpots[i].GetPositionY(),
                SafeSpots[i].GetPositionZ());

            if (dist < bestDist)
            {
                bestDist = dist;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

    Position BuildSafeSpotDestination(Creature* bot, Position const& safeSpot)
    {
        float x = safeSpot.GetPositionX();
        float y = safeSpot.GetPositionY();
        float z = safeSpot.GetPositionZ();

        bot->UpdateAllowedPositionZ(x, y, z);

        float orientation = bot->GetOrientation();
        Position dest;
        dest.Relocate(x, y, z, orientation);
        return dest;
    }
}

struct boss_murmur : public BossAI
{
    boss_murmur(Creature* creature) : BossAI(creature, DATA_MURMUR)
    {
        me->SetCombatMovement(false);
    }

    void Reset() override
    {
        MurmurCustom::SetMurmurSonicBoomNonAttackable(me, false);
        MurmurCustom::SetDungeonNpcBotsToFollow(me->GetMap());

        _Reset();
        me->SetHealth(me->CountPctFromMaxHealth(40));
        me->ResetPlayerDamageReq();
        CastSuppressionOOC();
    }

    void CastSuppressionOOC()
    {
        me->m_Events.CancelEventGroup(GROUP_OOC_CAST);
        me->m_Events.AddEventAtOffset([this]
            {
                if (me->FindNearestCreature(NPC_CABAL_SPELLBINDER, 35.0f))
                {
                    me->CastCustomSpell(SPELL_SUPPRESSION, SPELLVALUE_MAX_TARGETS, 5, (Unit*)nullptr, false);
                    CastSuppressionOOC();
                }
            }, 3600ms, 10900ms, GROUP_OOC_CAST);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        MurmurCustom::SetMurmurSonicBoomNonAttackable(me, false);
        MurmurCustom::SetDungeonNpcBotsToFollow(me->GetMap());

        if (me->GetThreatMgr().IsThreatListEmpty())
            BossAI::EnterEvadeMode(why);
    }

    bool ShouldCastResonance()
    {
        if (Unit* victim = me->GetVictim())
        {
            if (!me->IsWithinMeleeRange(victim))
                return true;

            if (Unit* victimTarget = victim->GetVictim())
                return victimTarget != me;
        }

        return true;
    }

    void SetGUID(ObjectGuid const& guid, int32 index) override
    {
        if (index == GUID_MURMUR_NPCS)
        {
            if (Creature* creature = ObjectAccessor::GetCreature(*me, guid))
                DoCast(creature, SPELL_MURMUR_WRATH, true);
        }
    }

    void JustDied(Unit* killer) override
    {
        MurmurCustom::SetMurmurSonicBoomNonAttackable(me, false);
        MurmurCustom::SetDungeonNpcBotsToFollow(me->GetMap());
        BossAI::JustDied(killer);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (who->IsPlayer() || who->IsPet() || who->IsGuardian())
            _JustEngagedWith();

        scheduler.Schedule(28s, [this](TaskContext context)
            {
                DoSonicBoomSequence();
                context.Repeat(34s, 40s);
            }).Schedule(14600ms, 25500ms, [this](TaskContext context)
                {
                    CastMurmursTouchOnRandomDungeonParticipant();
                    context.Repeat(14600ms, 25500ms);
                }).Schedule(3s, [this](TaskContext context)
                    {
                        if (ShouldCastResonance())
                        {
                            if (!scheduler.IsGroupScheduled(GROUP_RESONANCE))
                            {
                                scheduler.Schedule(5s, 5s, GROUP_RESONANCE, [this](TaskContext context)
                                    {
                                        if (ShouldCastResonance())
                                        {
                                            DoCastAOE(SPELL_RESONANCE);
                                            context.Repeat(6s, 18s);
                                        }
                                    });
                            }
                        }

                        context.Repeat();
                    });

                if (IsHeroic())
                {
                    scheduler.Schedule(5s, [this](TaskContext context)
                        {
                            DoCastAOE(SPELL_THUNDERING_STORM);
                            context.Repeat(6050ms, 10s);
                        }).Schedule(3650ms, 9150ms, [this](TaskContext context)
                            {
                                DoCastVictim(SPELL_SONIC_SHOCK);
                                context.Repeat(3650ms, 9150ms);
                            });
                }

                me->m_Events.CancelEventGroup(GROUP_OOC_CAST);
    }

private:
    void CastMurmursTouchOnRandomDungeonParticipant()
    {
        Unit* target = MurmurCustom::SelectRandomDungeonParticipant(me->GetMap());
        if (!target)
            return;

        me->CastSpell(target, SPELL_MURMURS_TOUCH, false);
        if (Creature* bot = DBMFTABotCallouts::AsNPCBotCreature(target))
            DBMFTABotCallouts::AnnounceMoveAwayFromMeForModule(bot, SPELL_MURMURS_TOUCH, "DBM-Party-BC", "547", "Murmur's Touch", DBMFTABotCallouts::GetCooldownMs());
    }

    void CastMagneticPullOnDungeonParticipants()
    {
        std::vector<Unit*> targets = MurmurCustom::GetDungeonParticipants(me->GetMap());

        for (Unit* target : targets)
        {
            if (!target || target == me)
                continue;

            me->CastSpell(target, SPELL_MAGNETIC_PULL, true);
        }
    }

    void MoveNpcBotsToSafeSpots()
    {
        std::vector<Creature*> bots = MurmurCustom::GetDungeonNpcBots(me->GetMap());
        std::array<bool, MurmurCustom::SafeSpots.size()> used = {};

        for (Creature* bot : bots)
        {
            if (!bot || !bot->IsAlive())
                continue;

            uint8 safeSpotIndex = MurmurCustom::FindNearestAvailableSafeSpot(bot, used);
            used[safeSpotIndex] = true;

            if (MurmurCustom::BotSwiftnessEnabled() && !bot->HasAura(SPELL_SWIFTNESS_POTION))
                bot->CastSpell(bot, SPELL_SWIFTNESS_POTION, true);

            bot->AttackStop();
            bot->InterruptNonMeleeSpells(true);

            Position dest = MurmurCustom::BuildSafeSpotDestination(bot, MurmurCustom::SafeSpots[safeSpotIndex]);

            if (bot->GetExactDist(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ()) > MurmurCustom::BOT_MAX_FORCED_MOVE_DISTANCE)
                continue;

            if (bot_ai* ai = bot->GetBotAI())
                ai->MoveToSendPosition(dest);
        }
    }

    void TeleportNpcBotsToSafeSpotsAndStay()
    {
        std::vector<Creature*> bots = MurmurCustom::GetDungeonNpcBots(me->GetMap());
        std::array<bool, MurmurCustom::SafeSpots.size()> used = {};

        for (Creature* bot : bots)
        {
            if (!bot || !bot->IsAlive())
                continue;

            uint8 safeSpotIndex = MurmurCustom::FindNearestAvailableSafeSpot(bot, used);
            used[safeSpotIndex] = true;

            bot->AttackStop();
            bot->InterruptNonMeleeSpells(true);

            Position dest = MurmurCustom::BuildSafeSpotDestination(bot, MurmurCustom::SafeSpots[safeSpotIndex]);

            if (bot->GetExactDist(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ()) > MurmurCustom::BOT_MAX_FORCED_MOVE_DISTANCE)
                continue;

            bot->NearTeleportTo(
                dest.GetPositionX(),
                dest.GetPositionY(),
                dest.GetPositionZ(),
                dest.GetOrientation());

            if (bot_ai* ai = bot->GetBotAI())
                ai->SetBotCommandState(BOT_COMMAND_STAY);
        }
    }

    void DoSonicBoomSequence()
    {
        if (!me->IsAlive() || !me->IsInCombat())
            return;

        CastMagneticPullOnDungeonParticipants();

        scheduler.Schedule(std::chrono::milliseconds(MurmurCustom::GetMagneticPullLeadTimeMs()), [this](TaskContext)
            {
                BeginSonicBoomCast();
            });
    }

    void BeginSonicBoomCast()
    {
        if (!me->IsAlive() || !me->IsInCombat())
            return;

        MurmurCustom::SetMurmurSonicBoomNonAttackable(me, true);

        Talk(EMOTE_SONIC_BOOM);
        DoCastAOE(SPELL_SONIC_BOOM_CAST);

        // Sonic Boom is a 6 second cast. The emote/cast start is the moment bots must run.
        MoveNpcBotsToSafeSpots();

        // Failsafe short teleport 5 seconds into the 6 second Sonic Boom cast.
        // Bots remain in STAY so they do not run back before the damage resolves.
        scheduler.Schedule(std::chrono::milliseconds(MurmurCustom::BOT_SAFE_SPOT_TELEPORT_DELAY_AFTER_CAST_START_MS), [this](TaskContext)
            {
                if (!me->IsAlive() || !me->IsInCombat())
                    return;

                TeleportNpcBotsToSafeSpotsAndStay();
            });

        // Sonic Boom damage resolves when the 6 second cast ends.
        scheduler.Schedule(std::chrono::milliseconds(MurmurCustom::SONIC_BOOM_CAST_TIME_MS), [this](TaskContext)
            {
                if (!me->IsAlive() || !me->IsInCombat())
                    return;

                DoCastAOE(SPELL_SONIC_BOOM_EFFECT, true);
            });

        // Safety fallback after the cast window. The spell script should already do this when damage resolves.
        scheduler.Schedule(std::chrono::milliseconds(MurmurCustom::SONIC_BOOM_FOLLOW_FALLBACK_DELAY_AFTER_CAST_START_MS), [this](TaskContext)
            {
                MurmurCustom::SetMurmurSonicBoomNonAttackable(me, false);
                MurmurCustom::SetDungeonNpcBotsToFollow(me->GetMap());
            });
    }
};

class spell_murmur_thundering_storm : public SpellScript
{
    PrepareSpellScript(spell_murmur_thundering_storm);

    void SelectTarget(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Acore::AllWorldObjectsInExactRange(GetCaster(), 100.0f, true));
        targets.remove_if(Acore::AllWorldObjectsInExactRange(GetCaster(), 25.0f, false));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_murmur_thundering_storm::SelectTarget, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_shockwave_knockback : public SpellScript
{
    PrepareSpellScript(spell_shockwave_knockback);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHOCKWAVE_SERVERSIDE });
    }

    void HandleOnHit()
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_SHOCKWAVE_SERVERSIDE, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_shockwave_knockback::HandleOnHit);
    }
};

class spell_murmur_sonic_boom_effect : public SpellScript
{
    PrepareSpellScript(spell_murmur_sonic_boom_effect);

public:
    spell_murmur_sonic_boom_effect() : SpellScript(), _sonicBoomResolved(false) {}

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        if (!caster || !target)
            return;

        SetHitDamage(MurmurCustom::CalculateSonicBoomDamage(caster, target));

        if (!_sonicBoomResolved)
        {
            _sonicBoomResolved = true;

            if (Creature* murmur = caster->ToCreature())
                MurmurCustom::SetMurmurSonicBoomNonAttackable(murmur, false);

            MurmurCustom::SetDungeonNpcBotsToFollow(caster->GetMap());
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_murmur_sonic_boom_effect::HandleOnHit);
    }

private:
    bool _sonicBoomResolved;
};

void AddSC_boss_murmur()
{
    RegisterShadowLabyrinthCreatureAI(boss_murmur);
    RegisterSpellScript(spell_murmur_thundering_storm);
    RegisterSpellScript(spell_shockwave_knockback);
    RegisterSpellScript(spell_murmur_sonic_boom_effect);
}
