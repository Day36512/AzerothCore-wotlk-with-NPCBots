/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * Custom Shirrak the Dead Watcher bridge overhaul for Auchenai Crypts.
 *
 * Behavior:
 * - Teleport the fight onto the bridge on pull.
 * - During the normal bridge phase, summon 3x NPC 828351 around a random encounter player every 10 seconds,
 *   with one summon directly on the player.
 * - During the normal bridge phase, summon 1x random flamewave runner on one of the 5 selectable bridge lanes
 *   (never the two "always" lanes) at intervals to keep the center phase active.
 * - On heroic, the normal summon behavior continues during the flamewave / special phase as well.
 * - Replace Possess / Mind Control with two player-centric run phases at 66% and 33%.
 * - During run phases, players must dodge randomized flame-wave lanes while running back to the boss.
 * - NPCBots are teleported with the group and held in Stasis (300334) so the player must do the mechanic.
 * - Stasis is refreshed periodically during the special phase and removed when the phase ends.
 * - When Shirrak returns to center, NPCBots are also teleported to center and each teleported unit
 *   casts teleport visual 51347 on self first.
 * - If a player dies during the run phase, that player's NPCBots die as well.
 * - If no living players remain during the run phase, all affected NPCBots die and the boss resets.
 * - Spawn bridge flamewall blockers on pull to prevent escape, and remove them on reset/evade/death.
 * - Summoned NPC 828351 casts spell 59159 on itself 6 seconds after being summoned on normal,
 *   and 4 seconds after being summoned on heroic.
 * - Summoned NPC 828351 despawns after 7 seconds on normal and 5 seconds on heroic.
 * - During the special bridge phase, Shirrak summons dedicated flamewave runners
 *   that patrol exact bridge lanes from the boss end of the bridge toward the raid.
 *
 * Notes:
 * - All flame lanes use explicit user-provided GPS nodes.
 * - The runner path is reversed at runtime when Shirrak is on the opposite bridge end so the
 *   flamewall always starts on the same side as the boss and travels end-to-end across the bridge.
 * - Lane selection is randomized with a small anti-streak rule so the same lane does not occur three times in a row.
 * - Each special-wave tick spawns 2 randomized runners on different lanes plus 2 additional fixed-path runners.
 * - Heroic uses a slightly faster special-wave spawn cadence.
 *
 * Placeholder requirements:
 * - SPELL_CHAIN_BEAM
 */

#include "CreatureScript.h"
#include "GameObject.h"
#include "Language.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Containers.h"
#include "Util.h"
#include "botmgr.h"
#include "bot_ai.h"
#include "auchenai_crypts.h"
#include "Log.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;

namespace
{
    enum ShirrakSpells : uint32
    {
        SPELL_CARNIVOROUS_BITE = 36383,  // stock Shirrak spell kept for tank pressure
        SPELL_CHAIN_BEAM = 502617,
        SPELL_SPECIAL_PHASE_EXPLODE = 502621,
        SPELL_TELEPORT_VISUAL = 51347,
        SPELL_BOT_STASIS = 300334
    };

    static constexpr uint32 NPC_NORMAL_PHASE_SUMMON = 828351;
    static constexpr uint32 NPC_SPECIAL_FLAMEWAVE_RUNNER = 828352; // reused for both special and normal random lane runner

    enum ShirrakGameObjects : uint32
    {
        GO_FLAMEWALL_BLOCKER = 203006
    };

    enum ShirrakEvents : uint32
    {
        EVENT_CARNIVOROUS_BITE = 1,
        EVENT_CHAIN_BEAM,
        EVENT_NORMAL_PHASE_SUMMON,
        EVENT_NORMAL_PHASE_RANDOM_RUNNER,
        EVENT_SPECIAL_WAVE,
        EVENT_SPECIAL_MONITOR,
        EVENT_BOT_STASIS_REFRESH
    };

    enum SpecialLane : uint8
    {
        LANE_LEFT = 0,
        LANE_LEFT_INNER = 1,
        LANE_MIDDLE = 2,
        LANE_RIGHT_INNER = 3,
        LANE_RIGHT = 4,
        LANE_ALWAYS_1 = 5,
        LANE_ALWAYS_2 = 6,
        LANE_MAX = 7
    };

    static constexpr uint32 SPECIAL_PHASE_DURATION_MS = 31000;
    static constexpr uint32 SPECIAL_WAVE_INTERVAL_MS_NORMAL = 2200;
    static constexpr uint32 SPECIAL_WAVE_INTERVAL_MS_HEROIC = 1800;
    static constexpr uint32 SPECIAL_WAVE_INITIAL_DELAY_MS_NORMAL = 1500;
    static constexpr uint32 SPECIAL_WAVE_INITIAL_DELAY_MS_HEROIC = 1200;
    static constexpr uint32 SPECIAL_MONITOR_MS = 200;

    static constexpr uint32 NORMAL_PHASE_SUMMON_LIFETIME_MS_NORMAL = 7000;
    static constexpr uint32 NORMAL_PHASE_SUMMON_LIFETIME_MS_HEROIC = 5000;
    static constexpr uint32 NORMAL_PHASE_SUMMON_INTERVAL_MS = 10000;

    static constexpr uint32 NORMAL_PHASE_RUNNER_INITIAL_DELAY_MS = 8000;
    static constexpr uint32 NORMAL_PHASE_RUNNER_INTERVAL_MIN_MS = 12000;
    static constexpr uint32 NORMAL_PHASE_RUNNER_INTERVAL_MAX_MS = 16000;

    static constexpr uint32 BOT_STASIS_REFRESH_MS = 5000;

    // Give the runner more than enough time to traverse the full bridge even if pathing slows briefly.
    static constexpr uint32 SPECIAL_WAVE_RUNNER_LIFETIME_MS = 22000;
    static constexpr uint32 SPECIAL_WAVE_RUNNER_START_DELAY_MS = 100;
    static constexpr float  SPECIAL_WAVE_RUNNER_SPEED_RATE = 2.4f;
    static constexpr float  SPECIAL_WAVE_NODE_INTERPOLATION_STEP = 4.5f;
    static constexpr uint8  SPECIAL_WAVE_RANDOM_RUNNERS_PER_TICK = 2;

    static constexpr float BRIDGE_CENTER_Z_FALLBACK = 15.53f;
    static constexpr float BRIDGE_FIGHT_RADIUS = 220.0f;
    static constexpr float PHASE_END_DISTANCE = 5.0f;

    static Position const BRIDGE_WEST_END = { 142.66f, -163.60f, 12.57f, 3.14159274f };
    static Position const BRIDGE_EAST_END = { 2.20f, -162.91f, 12.57f, 0.0f };
    static Position const BRIDGE_CENTER = { 72.01f, -163.24f, 15.53f, 3.14159274f };

    static Position const BLOCKER_POS_1 = { 172.52f, -163.39f, 26.43f, 0.0f };
    static Position const BLOCKER_POS_2 = { -28.84f, -162.75f, 26.14f, 0.0f };

    static constexpr float BRIDGE_LEFT_EDGE_Y = -170.20f;
    static constexpr float BRIDGE_RIGHT_EDGE_Y = -155.88f;

    // User-provided exact lane nodes.
    // Left side was provided west -> east.
    static std::array<Position, 10> const SPECIAL_WAVE_LEFT_LANE_POINTS =
    { {
        { 143.25f, -158.56f, 12.56f, 0.0f },
        { 123.19f, -158.78f, 13.24f, 0.0f },
        { 108.78f, -158.95f, 14.66f, 0.0f },
        {  90.88f, -158.15f, 15.28f, 0.0f },
        {  73.91f, -158.40f, 15.46f, 0.0f },
        {  56.78f, -157.70f, 15.28f, 0.0f },
        {  43.08f, -158.04f, 14.94f, 0.0f },
        {  30.75f, -157.91f, 14.27f, 0.0f },
        {  17.60f, -157.78f, 13.16f, 0.0f },
        {   4.09f, -157.64f, 12.60f, 0.0f }
    } };

    // Left-inner side was provided east -> west.
    static std::array<Position, 9> const SPECIAL_WAVE_LEFT_INNER_LANE_POINTS =
    { {
        {   1.51f, -159.91f, 12.57f, 0.0f },
        {  13.45f, -160.51f, 12.77f, 0.0f },
        {  39.76f, -160.93f, 14.82f, 0.0f },
        {  67.72f, -160.69f, 15.43f, 0.0f },
        {  81.54f, -160.38f, 15.37f, 0.0f },
        { 101.91f, -161.39f, 14.94f, 0.0f },
        { 108.40f, -160.93f, 14.70f, 0.0f },
        { 130.47f, -160.79f, 12.76f, 0.0f },
        { 138.77f, -161.06f, 12.60f, 0.0f }
    } };

    // Middle side was provided east -> west, with an extra west-most cap point.
    static std::array<Position, 11> const SPECIAL_WAVE_MIDDLE_LANE_POINTS =
    { {
        {   1.56f, -162.44f, 12.57f, 0.0f },
        {  15.25f, -162.69f, 12.95f, 0.0f },
        {  28.28f, -162.89f, 14.10f, 0.0f },
        {  43.01f, -163.07f, 14.95f, 0.0f },
        {  61.85f, -162.70f, 15.40f, 0.0f },
        {  81.31f, -162.78f, 15.42f, 0.0f },
        {  96.72f, -162.89f, 15.14f, 0.0f },
        { 109.92f, -163.04f, 14.56f, 0.0f },
        { 128.19f, -163.00f, 12.94f, 0.0f },
        { 136.69f, -162.83f, 12.62f, 0.0f },
        { 142.74f, -163.29f, 12.57f, 0.0f }
    } };

    // Right-inner side was provided west -> east.
    static std::array<Position, 12> const SPECIAL_WAVE_RIGHT_INNER_LANE_POINTS =
    { {
        { 140.60f, -165.58f, 12.59f, 0.0f },
        { 130.29f, -165.48f, 12.77f, 0.0f },
        { 114.97f, -165.07f, 14.03f, 0.0f },
        { 104.69f, -165.23f, 14.83f, 0.0f },
        {  89.85f, -165.46f, 15.29f, 0.0f },
        {  77.39f, -165.65f, 15.42f, 0.0f },
        {  63.40f, -165.55f, 15.37f, 0.0f },
        {  49.43f, -165.08f, 15.18f, 0.0f },
        {  40.43f, -165.22f, 14.85f, 0.0f },
        {  21.92f, -165.08f, 13.58f, 0.0f },
        {  11.32f, -164.94f, 12.68f, 0.0f },
        {   5.07f, -165.46f, 12.61f, 0.0f }
    } };

    // Right side was provided east -> west.
    static std::array<Position, 10> const SPECIAL_WAVE_RIGHT_LANE_POINTS =
    { {
        {   1.82f, -167.62f, 12.57f, 0.0f },
        {  25.71f, -166.95f, 13.88f, 0.0f },
        {  45.58f, -166.92f, 15.03f, 0.0f },
        {  63.52f, -166.89f, 15.35f, 0.0f },
        {  75.61f, -167.31f, 15.41f, 0.0f },
        {  90.46f, -167.28f, 15.29f, 0.0f },
        { 104.06f, -167.26f, 14.85f, 0.0f },
        { 118.69f, -167.23f, 13.64f, 0.0f },
        { 131.62f, -167.21f, 12.69f, 0.0f },
        { 141.67f, -167.20f, 12.58f, 0.0f }
    } };

    // Always path 1 was provided west -> east.
    static std::array<Position, 17> const SPECIAL_WAVE_ALWAYS_1_LANE_POINTS =
    { {
        { 140.74f, -150.32f, 13.85f, 0.0f },
        { 125.71f, -153.62f, 14.60f, 0.0f },
        { 112.06f, -154.76f, 15.60f, 0.0f },
        {  96.37f, -152.08f, 16.20f, 0.0f },
        {  91.83f, -150.93f, 16.24f, 0.0f },
        {  85.94f, -144.91f, 16.46f, 0.0f },
        {  81.77f, -146.73f, 16.81f, 0.0f },
        {  78.75f, -148.55f, 16.68f, 0.0f },
        {  66.58f, -148.43f, 16.92f, 0.0f },
        {  62.85f, -145.96f, 16.73f, 0.0f },
        {  60.37f, -144.62f, 16.47f, 0.0f },
        {  52.59f, -150.81f, 16.86f, 0.0f },
        {  39.42f, -153.19f, 16.31f, 0.0f },
        {  32.53f, -153.31f, 14.40f, 0.0f },
        {  25.18f, -154.06f, 14.70f, 0.0f },
        {  16.55f, -152.69f, 14.40f, 0.0f },
        {   5.03f, -150.28f, 14.01f, 0.0f }
    } };

    // Always path 2 was provided east -> west.
    static std::array<Position, 17> const SPECIAL_WAVE_ALWAYS_2_LANE_POINTS =
    { {
        {   4.95f, -175.35f, 13.71f, 0.0f },
        {  21.19f, -172.37f, 14.93f, 0.0f },
        {  34.79f, -171.60f, 15.88f, 0.0f },
        {  51.58f, -173.89f, 16.42f, 0.0f },
        {  60.51f, -181.59f, 16.67f, 0.0f },
        {  64.03f, -178.43f, 17.32f, 0.0f },
        {  67.11f, -176.84f, 17.01f, 0.0f },
        {  78.00f, -177.00f, 17.17f, 0.0f },
        {  80.73f, -178.22f, 17.37f, 0.0f },
        {  84.36f, -181.29f, 16.22f, 0.0f },
        {  89.48f, -176.31f, 16.24f, 0.0f },
        {  93.66f, -173.78f, 14.09f, 0.0f },
        { 102.06f, -172.55f, 14.66f, 0.0f },
        { 107.96f, -170.99f, 16.04f, 0.0f },
        { 119.92f, -171.73f, 15.38f, 0.0f },
        { 133.52f, -173.88f, 14.39f, 0.0f },
        { 143.72f, -175.85f, 12.56f, 0.0f }
    } };

    static float ClampFloat(float value, float minValue, float maxValue)
    {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    static float GetBridgeGroundZ(WorldObject const* source, float x, float y)
    {
        if (!source || !source->GetMap())
            return BRIDGE_CENTER_Z_FALLBACK;

        float z = source->GetMap()->GetHeight(source->GetPhaseMask(), x, y, BRIDGE_CENTER_Z_FALLBACK + 4.0f, true);
        if (z <= INVALID_HEIGHT)
            z = BRIDGE_CENTER_Z_FALLBACK;

        return z + 0.15f;
    }

    static bool IsWestSideX(float x)
    {
        return std::abs(x - BRIDGE_WEST_END.GetPositionX()) <= std::abs(x - BRIDGE_EAST_END.GetPositionX());
    }

    static SpecialLane GetSpecialLaneFromY(float y)
    {
        float const diffs[LANE_MAX] =
        {
            std::abs(y - SPECIAL_WAVE_LEFT_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_LEFT_INNER_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_MIDDLE_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_RIGHT_INNER_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_RIGHT_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_ALWAYS_1_LANE_POINTS.front().GetPositionY()),
            std::abs(y - SPECIAL_WAVE_ALWAYS_2_LANE_POINTS.front().GetPositionY())
        };

        uint8 bestIndex = 0;
        float bestDiff = diffs[0];

        for (uint8 i = 1; i < LANE_MAX; ++i)
        {
            if (diffs[i] < bestDiff)
            {
                bestDiff = diffs[i];
                bestIndex = i;
            }
        }

        return SpecialLane(bestIndex);
    }

    template <typename TContainer>
    static void AppendInterpolatedPathNodes(std::vector<Position>& out, TContainer const& anchors)
    {
        if (anchors.empty())
            return;

        if (out.empty())
            out.push_back(anchors.front());

        for (size_t i = 1; i < anchors.size(); ++i)
        {
            Position const& prev = anchors[i - 1];
            Position const& next = anchors[i];

            float const dx = next.GetPositionX() - prev.GetPositionX();
            float const dy = next.GetPositionY() - prev.GetPositionY();
            float const dz = next.GetPositionZ() - prev.GetPositionZ();
            float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            uint32 steps = 1;
            if (dist > SPECIAL_WAVE_NODE_INTERPOLATION_STEP)
                steps = uint32(std::ceil(dist / SPECIAL_WAVE_NODE_INTERPOLATION_STEP));

            for (uint32 step = 1; step <= steps; ++step)
            {
                float const t = float(step) / float(steps);
                out.push_back({
                    prev.GetPositionX() + dx * t,
                    prev.GetPositionY() + dy * t,
                    prev.GetPositionZ() + dz * t,
                    0.0f
                    });
            }
        }
    }

    static void ApplyPathOrientations(std::vector<Position>& path)
    {
        if (path.empty())
            return;

        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            float const dx = path[i + 1].GetPositionX() - path[i].GetPositionX();
            float const dy = path[i + 1].GetPositionY() - path[i].GetPositionY();
            path[i].m_orientation = std::atan2(dy, dx);
        }

        if (path.size() > 1)
            path.back().m_orientation = path[path.size() - 2].GetOrientation();
    }

    static std::vector<Position> BuildExactLanePath(bool startsOnWest, SpecialLane lane)
    {
        std::vector<Position> path;

        switch (lane)
        {
        case LANE_LEFT:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_LEFT_LANE_POINTS);
            break;
        case LANE_LEFT_INNER:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_LEFT_INNER_LANE_POINTS);
            break;
        case LANE_MIDDLE:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_MIDDLE_LANE_POINTS);
            break;
        case LANE_RIGHT_INNER:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_RIGHT_INNER_LANE_POINTS);
            break;
        case LANE_RIGHT:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_RIGHT_LANE_POINTS);
            break;
        case LANE_ALWAYS_1:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_ALWAYS_1_LANE_POINTS);
            break;
        case LANE_ALWAYS_2:
        default:
            AppendInterpolatedPathNodes(path, SPECIAL_WAVE_ALWAYS_2_LANE_POINTS);
            break;
        }

        bool suppliedPathStartsOnWest = false;
        switch (lane)
        {
        case LANE_LEFT:
        case LANE_RIGHT_INNER:
        case LANE_ALWAYS_1:
            suppliedPathStartsOnWest = true;
            break;
        case LANE_LEFT_INNER:
        case LANE_MIDDLE:
        case LANE_RIGHT:
        case LANE_ALWAYS_2:
        default:
            suppliedPathStartsOnWest = false;
            break;
        }

        if (startsOnWest != suppliedPathStartsOnWest)
            std::reverse(path.begin(), path.end());

        ApplyPathOrientations(path);
        return path;
    }

    static Position GetLaneStartPosition(bool startsOnWest, SpecialLane lane)
    {
        switch (lane)
        {
        case LANE_LEFT:
            return startsOnWest ? SPECIAL_WAVE_LEFT_LANE_POINTS.front() : SPECIAL_WAVE_LEFT_LANE_POINTS.back();
        case LANE_LEFT_INNER:
            return startsOnWest ? SPECIAL_WAVE_LEFT_INNER_LANE_POINTS.back() : SPECIAL_WAVE_LEFT_INNER_LANE_POINTS.front();
        case LANE_MIDDLE:
            return startsOnWest ? SPECIAL_WAVE_MIDDLE_LANE_POINTS.back() : SPECIAL_WAVE_MIDDLE_LANE_POINTS.front();
        case LANE_RIGHT_INNER:
            return startsOnWest ? SPECIAL_WAVE_RIGHT_INNER_LANE_POINTS.front() : SPECIAL_WAVE_RIGHT_INNER_LANE_POINTS.back();
        case LANE_RIGHT:
            return startsOnWest ? SPECIAL_WAVE_RIGHT_LANE_POINTS.back() : SPECIAL_WAVE_RIGHT_LANE_POINTS.front();
        case LANE_ALWAYS_1:
            return startsOnWest ? SPECIAL_WAVE_ALWAYS_1_LANE_POINTS.front() : SPECIAL_WAVE_ALWAYS_1_LANE_POINTS.back();
        case LANE_ALWAYS_2:
        default:
            return startsOnWest ? SPECIAL_WAVE_ALWAYS_2_LANE_POINTS.back() : SPECIAL_WAVE_ALWAYS_2_LANE_POINTS.front();
        }
    }

    static char const* GetLaneName(SpecialLane lane)
    {
        switch (lane)
        {
        case LANE_LEFT:        return "left";
        case LANE_LEFT_INNER:  return "left-inner";
        case LANE_MIDDLE:      return "middle";
        case LANE_RIGHT_INNER: return "right-inner";
        case LANE_RIGHT:       return "right";
        case LANE_ALWAYS_1:    return "always-1";
        case LANE_ALWAYS_2:    return "always-2";
        default:               return "unknown";
        }
    }

    static bool LaneExcluded(SpecialLane lane, std::vector<SpecialLane> const& excluded)
    {
        return std::find(excluded.begin(), excluded.end(), lane) != excluded.end();
    }

    static bool IsRandomSelectableLane(SpecialLane lane)
    {
        return lane == LANE_LEFT ||
            lane == LANE_LEFT_INNER ||
            lane == LANE_MIDDLE ||
            lane == LANE_RIGHT_INNER ||
            lane == LANE_RIGHT;
    }
}

class npc_shirrak_normal_phase_summon : public CreatureScript
{
public:
    npc_shirrak_normal_phase_summon() : CreatureScript("npc_shirrak_normal_phase_summon") {}

    struct npc_shirrak_normal_phase_summonAI : public ScriptedAI
    {
        explicit npc_shirrak_normal_phase_summonAI(Creature* creature)
            : ScriptedAI(creature), _useCustomDamage(false), _customDamage(1200) {
        }

        EventMap events;
        bool _useCustomDamage;
        int32 _customDamage;

        enum : uint32
        {
            EVENT_SELF_CAST = 1,
            SPELL_THUNDERSTORM = 502618
        };

        static constexpr int32 BASE_CUSTOM_THUNDERSTORM_DAMAGE = 1200;
        static constexpr uint8 LEVEL_DAMAGE_START = 65;
        static constexpr int32 DAMAGE_PER_LEVEL_OVER_65 = 50;

        void Reset() override
        {
            events.Reset();
            _useCustomDamage = false;
            _customDamage = BASE_CUSTOM_THUNDERSTORM_DAMAGE;

            uint32 const selfCastDelayMs = me->GetMap() && me->GetMap()->IsHeroic()
                ? 4000u
                : 6000u;

            events.ScheduleEvent(EVENT_SELF_CAST, Milliseconds(selfCastDelayMs));
        }

        void IsSummonedBy(WorldObject* summoner) override
        {
            _useCustomDamage = false;
            _customDamage = BASE_CUSTOM_THUNDERSTORM_DAMAGE;

            if (!summoner)
                return;

            if (Player* player = summoner->ToPlayer())
            {
                me->SetFaction(player->GetFaction());
                _useCustomDamage = true;

                uint8 level = player->GetLevel();
                if (level > LEVEL_DAMAGE_START)
                    _customDamage += int32(level - LEVEL_DAMAGE_START) * DAMAGE_PER_LEVEL_OVER_65;

                return;
            }

            if (Creature* creature = summoner->ToCreature())
            {
                if (creature->IsNPCBot())
                {
                    me->SetFaction(creature->GetFaction());
                    _useCustomDamage = true;

                    uint8 level = creature->GetLevel();
                    if (level > LEVEL_DAMAGE_START)
                        _customDamage += int32(level - LEVEL_DAMAGE_START) * DAMAGE_PER_LEVEL_OVER_65;
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                if (eventId == EVENT_SELF_CAST)
                {
                    if (!me->IsAlive() || me->HasUnitState(UNIT_STATE_CASTING))
                        continue;

                    if (_useCustomDamage)
                    {
                        int32 bp0 = _customDamage;
                        me->CastCustomSpell(me, SPELL_THUNDERSTORM, &bp0, nullptr, nullptr, true);
                    }
                    else
                    {
                        me->CastSpell(me, SPELL_THUNDERSTORM, true);
                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_shirrak_normal_phase_summonAI(creature);
    }
};

class npc_shirrak_special_flamewave_runner : public CreatureScript
{
public:
    npc_shirrak_special_flamewave_runner() : CreatureScript("npc_shirrak_special_flamewave_runner") {}

    struct npc_shirrak_special_flamewave_runnerAI : public ScriptedAI
    {
        explicit npc_shirrak_special_flamewave_runnerAI(Creature* creature) : ScriptedAI(creature) {}

        EventMap events;
        std::vector<Position> _path;
        uint16 _currentPathIndex = 0;

        enum : uint32
        {
            EVENT_BEGIN_MOVEMENT = 1,
            EVENT_MOVE_STEP
        };

        enum : uint32
        {
            POINT_PATH_BASE = 100
        };

        void InitializeRunner()
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            me->SetSpeedRate(MOVE_RUN, SPECIAL_WAVE_RUNNER_SPEED_RATE);
            me->AttackStop();
            me->SetWalk(false);
        }

        uint32 GetTravelTimeMs(Position const& from, Position const& to) const
        {
            float const dx = to.GetPositionX() - from.GetPositionX();
            float const dy = to.GetPositionY() - from.GetPositionY();
            float const dz = to.GetPositionZ() - from.GetPositionZ();
            float const distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            float const runSpeed = 7.0f * SPECIAL_WAVE_RUNNER_SPEED_RATE;
            if (runSpeed <= 0.01f)
                return 800;

            uint32 travelMs = uint32((distance / runSpeed) * 1000.0f);
            if (travelMs < 120)
                travelMs = 120;

            return travelMs + 35;
        }

        void Reset() override
        {
            events.Reset();
            _path.clear();
            _currentPathIndex = 0;
            InitializeRunner();
        }

        void IsSummonedBy(WorldObject* /*summoner*/) override
        {
            events.Reset();
            _path = BuildExactLanePath(IsWestSideX(me->GetPositionX()), GetSpecialLaneFromY(me->GetPositionY()));
            _currentPathIndex = 0;

            InitializeRunner();

            if (_path.empty())
            {
                LOG_ERROR("scripts", "Shirrak flame runner: empty path, despawning");
                me->DespawnOrUnsummon();
                return;
            }

            Position const& start = _path.front();
            me->NearTeleportTo(start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), start.GetOrientation());

            events.ScheduleEvent(EVENT_BEGIN_MOVEMENT, Milliseconds(SPECIAL_WAVE_RUNNER_START_DELAY_MS));
        }

        void MoveToNode(uint16 nextIndex)
        {
            if (nextIndex >= _path.size())
            {
                me->DespawnOrUnsummon(100ms);
                return;
            }

            Position const currentPos = me->GetPosition();
            Position const& nextPos = _path[nextIndex];

            me->SetFacingTo(nextPos.GetOrientation());
            me->GetMotionMaster()->MovePoint(POINT_PATH_BASE + nextIndex, nextPos);
            _currentPathIndex = nextIndex;

            events.ScheduleEvent(EVENT_MOVE_STEP, Milliseconds(GetTravelTimeMs(currentPos, nextPos)));
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_BEGIN_MOVEMENT:
                    MoveToNode(1);
                    break;
                case EVENT_MOVE_STEP:
                    MoveToNode(_currentPathIndex + 1);
                    break;
                default:
                    break;
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_shirrak_special_flamewave_runnerAI(creature);
    }
};

struct boss_shirrak_the_dead_watcher : public BossAI
{
    boss_shirrak_the_dead_watcher(Creature* creature) : BossAI(creature, DATA_SHIRRAK_THE_DEAD_WATCHER) {}

    EventMap events;

    bool _phase66Done = false;
    bool _phase33Done = false;
    bool _inSpecialPhase = false;
    bool _specialBossOnWest = true;
    uint32 _specialElapsedMs = 0;

    int8 _lastSpecialLane = -1;
    uint8 _sameLaneStreak = 0;

    std::vector<ObjectGuid> _blockerGuids;
    std::vector<ObjectGuid> _runnerGuids;

    bool IsEncounterPlayer(Player const* player) const
    {
        if (!player)
            return false;

        if (player->GetMap() != me->GetMap())
            return false;

        if (player->IsGameMaster())
            return false;

        return player->GetDistance2d(BRIDGE_CENTER.GetPositionX(), BRIDGE_CENTER.GetPositionY()) <= BRIDGE_FIGHT_RADIUS;
    }

    bool ShouldSummonDuringSpecialPhase() const
    {
        return IsHeroic();
    }

    uint32 GetSpecialWaveIntervalMs() const
    {
        return IsHeroic() ? SPECIAL_WAVE_INTERVAL_MS_HEROIC : SPECIAL_WAVE_INTERVAL_MS_NORMAL;
    }

    uint32 GetSpecialWaveInitialDelayMs() const
    {
        return IsHeroic() ? SPECIAL_WAVE_INITIAL_DELAY_MS_HEROIC : SPECIAL_WAVE_INITIAL_DELAY_MS_NORMAL;
    }

    uint32 GetNormalPhaseSummonLifetimeMs() const
    {
        return IsHeroic() ? NORMAL_PHASE_SUMMON_LIFETIME_MS_HEROIC : NORMAL_PHASE_SUMMON_LIFETIME_MS_NORMAL;
    }

    uint32 GetNextNormalPhaseRunnerIntervalMs() const
    {
        return urand(NORMAL_PHASE_RUNNER_INTERVAL_MIN_MS, NORMAL_PHASE_RUNNER_INTERVAL_MAX_MS);
    }

    void YellSpecialPhaseStart()
    {
        switch (urand(0, 2))
        {
        case 0:
            me->Yell("Run, little souls! The bridge itself hungers for you!", LANG_UNIVERSAL);
            break;
        case 1:
            me->Yell("Flee if you wish! The dead flame runs faster!", LANG_UNIVERSAL);
            break;
        default:
            me->Yell("Cross the fire and be unmade!", LANG_UNIVERSAL);
            break;
        }
    }

    void YellRandomFlameboy()
    {
        switch (urand(0, 2))
        {
        case 0:
            me->Yell("The bridge burns beneath your feet!", LANG_UNIVERSAL);
            break;
        case 1:
            me->Yell("Dance, mortals... dance!", LANG_UNIVERSAL);
            break;
        default:
            me->Yell("Another flame for your feet!", LANG_UNIVERSAL);
            break;
        }
    }

    float GetBridgeGroundZ(float x, float y) const
    {
        return ::GetBridgeGroundZ(me, x, y);
    }

    SpecialLane SelectRandomSpecialLane(std::vector<SpecialLane> const& excluded = {})
    {
        std::vector<SpecialLane> candidates;
        candidates.reserve(LANE_MAX);

        for (uint8 laneIndex = 0; laneIndex < LANE_MAX; ++laneIndex)
        {
            SpecialLane lane = SpecialLane(laneIndex);
            if (!IsRandomSelectableLane(lane))
                continue;

            if (!LaneExcluded(lane, excluded))
                candidates.push_back(lane);
        }

        if (candidates.empty())
            return LANE_MIDDLE;

        if (_lastSpecialLane >= 0 && _sameLaneStreak >= 2 && candidates.size() > 1)
        {
            candidates.erase(
                std::remove(candidates.begin(), candidates.end(), SpecialLane(_lastSpecialLane)),
                candidates.end());

            if (candidates.empty())
            {
                for (uint8 laneIndex = 0; laneIndex < LANE_MAX; ++laneIndex)
                {
                    SpecialLane lane = SpecialLane(laneIndex);
                    if (!IsRandomSelectableLane(lane))
                        continue;

                    if (!LaneExcluded(lane, excluded))
                        candidates.push_back(lane);
                }
            }
        }

        SpecialLane lane = Acore::Containers::SelectRandomContainerElement(candidates);

        if (int8(lane) == _lastSpecialLane)
            ++_sameLaneStreak;
        else
        {
            _lastSpecialLane = int8(lane);
            _sameLaneStreak = 1;
        }

        return lane;
    }

    void GetSpreadTeleportPosition(Position const& base, uint32 index, float& x, float& y, float& z, float& o) const
    {
        float lateral = 0.0f;

        if (index > 0)
        {
            uint32 step = (index + 1) / 2;
            lateral = 0.9f * float(step) * ((index % 2 == 0) ? -1.0f : 1.0f);
        }

        x = base.GetPositionX();
        y = ClampFloat(base.GetPositionY() + lateral, BRIDGE_LEFT_EDGE_Y + 1.0f, BRIDGE_RIGHT_EDGE_Y - 1.0f);
        z = GetBridgeGroundZ(x, y);
        o = base.GetOrientation();
    }

    std::vector<Player*> GetEncounterPlayers(bool aliveOnly) const
    {
        std::vector<Player*> players;
        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();

        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player))
                continue;

            if (aliveOnly && !player->IsAlive())
                continue;

            players.push_back(player);
        }

        return players;
    }

    bool IsBotOwnedByPlayer(Creature* bot, Player const* owner) const
    {
        if (!bot || !owner || !bot->IsNPCBot())
            return false;

        if (bot_ai* ai = bot->GetBotAI())
            return ai->GetBotOwnerGuid() == owner->GetGUID().GetCounter();

        return false;
    }

    std::vector<Creature*> GetEncounterBots(bool aliveOnly) const
    {
        std::vector<Creature*> bots;
        std::unordered_set<ObjectGuid::LowType> seen;

        Map::PlayerList const& playerList = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!IsEncounterPlayer(player))
                continue;

            for (Unit* member : BotMgr::GetAllGroupMembers(player))
            {
                if (!member || !member->IsInWorld())
                    continue;

                if (member->FindMap() != me->GetMap())
                    continue;

                if (aliveOnly && !member->IsAlive())
                    continue;

                Creature* bot = member->ToCreature();
                if (!bot || !bot->IsNPCBot())
                    continue;

                if (!seen.insert(bot->GetGUID().GetCounter()).second)
                    continue;

                bots.push_back(bot);
            }
        }

        return bots;
    }

    std::vector<Creature*> GetOwnedEncounterBots(Player const* owner, bool aliveOnly) const
    {
        std::vector<Creature*> bots;
        std::unordered_set<ObjectGuid::LowType> seen;

        if (!owner || !IsEncounterPlayer(owner))
            return bots;

        for (Unit* member : BotMgr::GetAllGroupMembers(const_cast<Player*>(owner)))
        {
            if (!member || !member->IsInWorld())
                continue;

            if (member->FindMap() != me->GetMap())
                continue;

            if (aliveOnly && !member->IsAlive())
                continue;

            Creature* bot = member->ToCreature();
            if (!bot || !bot->IsNPCBot())
                continue;

            if (!IsBotOwnedByPlayer(bot, owner))
                continue;

            if (!seen.insert(bot->GetGUID().GetCounter()).second)
                continue;

            bots.push_back(bot);
        }

        return bots;
    }

    void TeleportUnitWithVisual(Unit* unit, float x, float y, float z, float o)
    {
        if (!unit)
            return;

        unit->CastSpell(unit, SPELL_TELEPORT_VISUAL, true);
        unit->NearTeleportTo(x, y, z, o);
    }

    void TeleportUnitWithVisual(Unit* unit, Position const& pos)
    {
        TeleportUnitWithVisual(unit, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation());
    }

    void ApplyBotStasis(Creature* bot)
    {
        if (!bot || !bot->IsAlive())
            return;

        bot->InterruptNonMeleeSpells(true);
        bot->AttackStop();
        bot->StopMoving();
        bot->RemoveAurasDueToSpell(SPELL_BOT_STASIS);
        bot->AddAura(SPELL_BOT_STASIS, bot);
    }

    void RemoveBotStasis(Creature* bot)
    {
        if (!bot)
            return;

        bot->RemoveAurasDueToSpell(SPELL_BOT_STASIS);
    }

    void RefreshAllEncounterBotStasis()
    {
        for (Creature* bot : GetEncounterBots(true))
            ApplyBotStasis(bot);
    }

    void ClearAllEncounterBotPhaseState()
    {
        for (Creature* bot : GetEncounterBots(false))
            RemoveBotStasis(bot);
    }

    void KillOwnedBots(Player const* owner)
    {
        for (Creature* bot : GetOwnedEncounterBots(owner, true))
            bot->KillSelf();
    }

    void KillAllEncounterBots()
    {
        for (Creature* bot : GetEncounterBots(true))
            bot->KillSelf();
    }

    void TeleportPlayerOwnedBots(Player* owner, float ownerX, float ownerY, float ownerZ, float ownerO, bool applyStasis)
    {
        uint32 botIndex = 0;
        for (Creature* bot : GetOwnedEncounterBots(owner, true))
        {
            float bx = ownerX;
            float by = ClampFloat(ownerY + 0.6f + 0.5f * float(botIndex), BRIDGE_LEFT_EDGE_Y + 1.0f, BRIDGE_RIGHT_EDGE_Y - 1.0f);
            float bz = GetBridgeGroundZ(bx, by);

            TeleportUnitWithVisual(bot, bx, by, bz, ownerO);

            if (applyStasis)
                ApplyBotStasis(bot);

            ++botIndex;
        }
    }

    void TeleportRaidAndBots(Position const& base, bool applyStasis)
    {
        std::vector<Player*> players = GetEncounterPlayers(true);

        for (uint32 i = 0; i < players.size(); ++i)
        {
            Player* player = players[i];

            float x, y, z, o;
            GetSpreadTeleportPosition(base, i, x, y, z, o);

            TeleportUnitWithVisual(player, x, y, z, o);
            TeleportPlayerOwnedBots(player, x, y, z, o, applyStasis);
        }
    }

    void TeleportEncounterBotsToPosition(Position const& base, bool removeStasisFirst, bool applyStasisAfter)
    {
        uint32 index = 0;
        for (Creature* bot : GetEncounterBots(false))
        {
            if (removeStasisFirst)
                RemoveBotStasis(bot);

            if (!bot->IsAlive())
                continue;

            float x, y, z, o;
            GetSpreadTeleportPosition(base, ++index, x, y, z, o);

            TeleportUnitWithVisual(bot, x, y, z, o);

            if (applyStasisAfter)
                ApplyBotStasis(bot);
        }
    }

    Player* SelectEncounterPlayerForSummon()
    {
        std::vector<Player*> candidates = GetEncounterPlayers(true);

        if (candidates.empty())
            return nullptr;

        return Acore::Containers::SelectRandomContainerElement(candidates);
    }

    TempSummon* SummonNormalPhaseNpcAt(Player* target, float x, float y, float z, float o)
    {
        target->UpdateAllowedPositionZ(x, y, z);

        TempSummon* summon = me->SummonCreature(
            NPC_NORMAL_PHASE_SUMMON,
            x, y, z, o,
            TEMPSUMMON_TIMED_DESPAWN,
            GetNormalPhaseSummonLifetimeMs());

        if (!summon)
        {
            LOG_ERROR("scripts",
                "Shirrak: SummonCreature failed for entry {} on player {} at ({}, {}, {}, {})",
                uint32(NPC_NORMAL_PHASE_SUMMON), target->GetName(), x, y, z, o);
            return nullptr;
        }

        summon->SetHomePosition(x, y, z, o);
        summon->SetInCombatWithZone();
        summon->SetInCombatWith(target);
        return summon;
    }

    void SummonNormalPhaseNpcOnPlayer()
    {
        Player* target = SelectEncounterPlayerForSummon();
        if (!target)
        {
            LOG_ERROR("scripts", "Shirrak: no valid player found for summon {}", uint32(NPC_NORMAL_PHASE_SUMMON));
            return;
        }

        float baseX = target->GetPositionX();
        float baseY = target->GetPositionY();
        float baseZ = target->GetPositionZ();
        float baseO = target->GetOrientation();

        float const sideOffset = 3.0f;
        float const sinO = std::sin(baseO);
        float const cosO = std::cos(baseO);

        float leftX = baseX - (sinO * sideOffset);
        float leftY = baseY + (cosO * sideOffset);
        float leftZ = baseZ;

        float rightX = baseX + (sinO * sideOffset);
        float rightY = baseY - (cosO * sideOffset);
        float rightZ = baseZ;

        uint32 successCount = 0;
        successCount += SummonNormalPhaseNpcAt(target, baseX, baseY, baseZ, baseO) ? 1u : 0u;
        successCount += SummonNormalPhaseNpcAt(target, leftX, leftY, leftZ, baseO) ? 1u : 0u;
        successCount += SummonNormalPhaseNpcAt(target, rightX, rightY, rightZ, baseO) ? 1u : 0u;

        LOG_INFO("scripts",
            "Shirrak: summoned {}x entry {} around player {}",
            successCount, uint32(NPC_NORMAL_PHASE_SUMMON), target->GetName());
    }

    void CleanupFlamewaveRunners()
    {
        if (_runnerGuids.empty())
            return;

        for (ObjectGuid const& guid : _runnerGuids)
        {
            if (Creature* runner = ObjectAccessor::GetCreature(*me, guid))
                runner->DespawnOrUnsummon();
        }

        _runnerGuids.clear();
    }

    bool SummonFlamewaveRunnerOnLane(SpecialLane lane, bool startsOnWest, char const* sourceTag)
    {
        Position const spawnPos = GetLaneStartPosition(startsOnWest, lane);

        TempSummon* summon = me->SummonCreature(
            NPC_SPECIAL_FLAMEWAVE_RUNNER,
            spawnPos.GetPositionX(), spawnPos.GetPositionY(), spawnPos.GetPositionZ(), spawnPos.GetOrientation(),
            TEMPSUMMON_TIMED_DESPAWN,
            SPECIAL_WAVE_RUNNER_LIFETIME_MS);

        if (!summon)
        {
            LOG_ERROR("scripts",
                "Shirrak: SummonCreature failed for flamewave runner entry {} at ({}, {}, {}, {}) from {}",
                uint32(NPC_SPECIAL_FLAMEWAVE_RUNNER),
                spawnPos.GetPositionX(), spawnPos.GetPositionY(), spawnPos.GetPositionZ(), spawnPos.GetOrientation(),
                sourceTag ? sourceTag : "unknown");
            return false;
        }

        summon->SetHomePosition(
            spawnPos.GetPositionX(),
            spawnPos.GetPositionY(),
            spawnPos.GetPositionZ(),
            spawnPos.GetOrientation());

        _runnerGuids.push_back(summon->GetGUID());

        LOG_DEBUG("scripts",
            "Shirrak: summoned {} flamewave runner {} on {} lane from {} side at ({}, {}, {})",
            sourceTag ? sourceTag : "unknown",
            uint32(NPC_SPECIAL_FLAMEWAVE_RUNNER),
            GetLaneName(lane),
            startsOnWest ? "west" : "east",
            spawnPos.GetPositionX(), spawnPos.GetPositionY(), spawnPos.GetPositionZ());

        return true;
    }

    void SummonNormalPhaseRandomRunner()
    {
        SpecialLane const lane = SelectRandomSpecialLane();
        bool const startsOnWest = urand(0, 1) == 0;

        if (SummonFlamewaveRunnerOnLane(lane, startsOnWest, "normal-phase"))
            YellRandomFlameboy();
    }

    void SummonSpecialWaveRunners()
    {
        SummonFlamewaveRunnerOnLane(LANE_ALWAYS_1, _specialBossOnWest, "special-phase");
        SummonFlamewaveRunnerOnLane(LANE_ALWAYS_2, _specialBossOnWest, "special-phase");

        std::vector<SpecialLane> excluded;
        excluded.reserve(SPECIAL_WAVE_RANDOM_RUNNERS_PER_TICK);

        for (uint8 i = 0; i < SPECIAL_WAVE_RANDOM_RUNNERS_PER_TICK; ++i)
        {
            SpecialLane const lane = SelectRandomSpecialLane(excluded);
            SummonFlamewaveRunnerOnLane(lane, _specialBossOnWest, "special-phase");
            excluded.push_back(lane);
        }
    }

    void DespawnFlamewallBlockers()
    {
        if (_blockerGuids.empty())
            return;

        for (ObjectGuid const& guid : _blockerGuids)
        {
            if (GameObject* go = me->GetMap()->GetGameObject(guid))
                go->Delete();
        }

        _blockerGuids.clear();
    }

    void SpawnFlamewallBlockers()
    {
        DespawnFlamewallBlockers();

        if (GameObject* go = me->SummonGameObject(
            GO_FLAMEWALL_BLOCKER,
            BLOCKER_POS_1.GetPositionX(), BLOCKER_POS_1.GetPositionY(), BLOCKER_POS_1.GetPositionZ(), BLOCKER_POS_1.GetOrientation(),
            0.0f, 0.0f, 0.0f, 0.0f,
            0))
        {
            _blockerGuids.push_back(go->GetGUID());
        }

        if (GameObject* go = me->SummonGameObject(
            GO_FLAMEWALL_BLOCKER,
            BLOCKER_POS_2.GetPositionX(), BLOCKER_POS_2.GetPositionY(), BLOCKER_POS_2.GetPositionZ(), BLOCKER_POS_2.GetOrientation(),
            0.0f, 0.0f, 0.0f, 0.0f,
            0))
        {
            _blockerGuids.push_back(go->GetGUID());
        }
    }

    void ScheduleNormalPhaseEvents()
    {
        events.Reset();
        events.ScheduleEvent(EVENT_CARNIVOROUS_BITE, 6s);
        events.ScheduleEvent(EVENT_CHAIN_BEAM, 9s);
        events.ScheduleEvent(EVENT_NORMAL_PHASE_SUMMON, Milliseconds(NORMAL_PHASE_SUMMON_INTERVAL_MS));
        events.ScheduleEvent(EVENT_NORMAL_PHASE_RANDOM_RUNNER, Milliseconds(NORMAL_PHASE_RUNNER_INITIAL_DELAY_MS));
    }

    void BeginNormalPhase(bool teleportRaidToCenter)
    {
        _inSpecialPhase = false;
        _specialElapsedMs = 0;
        _lastSpecialLane = -1;
        _sameLaneStreak = 0;

        CleanupFlamewaveRunners();

        me->InterruptNonMeleeSpells(true);
        me->AttackStop();
        me->SetReactState(REACT_AGGRESSIVE);

        if (teleportRaidToCenter)
            TeleportRaidAndBots(BRIDGE_CENTER, false);
        else
            TeleportEncounterBotsToPosition(BRIDGE_CENTER, true, false);

        Position bossPos = BRIDGE_CENTER;
        bossPos.m_positionZ = GetBridgeGroundZ(BRIDGE_CENTER.GetPositionX(), BRIDGE_CENTER.GetPositionY());
        TeleportUnitWithVisual(me, bossPos);

        me->SetHomePosition(
            bossPos.GetPositionX(),
            bossPos.GetPositionY(),
            bossPos.GetPositionZ(),
            bossPos.GetOrientation());

        me->SetControlled(true, UNIT_STATE_ROOT);

        ScheduleNormalPhaseEvents();
    }

    void StartSpecialPhase()
    {
        _inSpecialPhase = true;
        _specialElapsedMs = 0;
        _specialBossOnWest = !_specialBossOnWest;
        _lastSpecialLane = -1;
        _sameLaneStreak = 0;

        CleanupFlamewaveRunners();

        events.Reset();

        me->InterruptNonMeleeSpells(true);
        me->AttackStop();
        me->StopMoving();
        me->SetReactState(REACT_PASSIVE);
        me->SetControlled(true, UNIT_STATE_ROOT);

        Position bossPos = _specialBossOnWest ? BRIDGE_WEST_END : BRIDGE_EAST_END;
        bossPos.m_positionZ = GetBridgeGroundZ(bossPos.GetPositionX(), bossPos.GetPositionY());
        TeleportUnitWithVisual(me, bossPos);

        Position playerPos = _specialBossOnWest ? BRIDGE_EAST_END : BRIDGE_WEST_END;
        playerPos.m_positionZ = GetBridgeGroundZ(playerPos.GetPositionX(), playerPos.GetPositionY());
        TeleportRaidAndBots(playerPos, true);

        RefreshAllEncounterBotStasis();
        me->CastSpell(me, SPELL_SPECIAL_PHASE_EXPLODE, false);
        YellSpecialPhaseStart();

        events.ScheduleEvent(EVENT_SPECIAL_WAVE, Milliseconds(GetSpecialWaveInitialDelayMs()));
        events.ScheduleEvent(EVENT_SPECIAL_MONITOR, Milliseconds(SPECIAL_MONITOR_MS));
        events.ScheduleEvent(EVENT_BOT_STASIS_REFRESH, Milliseconds(BOT_STASIS_REFRESH_MS));

        if (ShouldSummonDuringSpecialPhase())
            events.ScheduleEvent(EVENT_NORMAL_PHASE_SUMMON, Milliseconds(NORMAL_PHASE_SUMMON_INTERVAL_MS));
    }

    void EndSpecialPhase()
    {
        BeginNormalPhase(false);
    }

    void CheckSpecialPhaseFailureOrCompletion()
    {
        std::vector<Player*> allPlayers = GetEncounterPlayers(false);
        uint32 alivePlayers = 0;

        for (Player* player : allPlayers)
        {
            if (!player->IsAlive())
            {
                KillOwnedBots(player);
                continue;
            }

            ++alivePlayers;

            if (player->GetDistance2d(me) <= PHASE_END_DISTANCE)
            {
                EndSpecialPhase();
                return;
            }
        }

        if (alivePlayers == 0)
        {
            KillAllEncounterBots();
            EnterEvadeMode(EVADE_REASON_OTHER);
            return;
        }

        if (_specialElapsedMs >= SPECIAL_PHASE_DURATION_MS)
            EndSpecialPhase();
    }

    void Reset() override
    {
        _Reset();
        events.Reset();

        _phase66Done = false;
        _phase33Done = false;
        _inSpecialPhase = false;
        _specialBossOnWest = true;
        _specialElapsedMs = 0;
        _lastSpecialLane = -1;
        _sameLaneStreak = 0;

        ClearAllEncounterBotPhaseState();
        CleanupFlamewaveRunners();
        DespawnFlamewallBlockers();

        me->SetReactState(REACT_AGGRESSIVE);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetControlled(false, UNIT_STATE_STUNNED);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ClearAllEncounterBotPhaseState();
        CleanupFlamewaveRunners();
        DespawnFlamewallBlockers();

        _inSpecialPhase = false;
        _specialElapsedMs = 0;
        _lastSpecialLane = -1;
        _sameLaneStreak = 0;

        me->SetReactState(REACT_AGGRESSIVE);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetControlled(false, UNIT_STATE_STUNNED);

        BossAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        SpawnFlamewallBlockers();
        BeginNormalPhase(true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        ClearAllEncounterBotPhaseState();
        CleanupFlamewaveRunners();
        DespawnFlamewallBlockers();

        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetControlled(false, UNIT_STATE_STUNNED);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellSchoolMask /*schoolMask*/) override
    {
        if (_inSpecialPhase)
        {
            damage = 0;
            return;
        }

        if (!_phase66Done && me->HealthBelowPctDamaged(66, damage))
        {
            _phase66Done = true;
            StartSpecialPhase();
            damage = 0;
            return;
        }

        if (!_phase33Done && me->HealthBelowPctDamaged(33, damage))
        {
            _phase33Done = true;
            StartSpecialPhase();
            damage = 0;
            return;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (_inSpecialPhase)
        {
            _specialElapsedMs += diff;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_SPECIAL_WAVE:
                    SummonSpecialWaveRunners();
                    if (_inSpecialPhase)
                        events.ScheduleEvent(EVENT_SPECIAL_WAVE, Milliseconds(GetSpecialWaveIntervalMs()));
                    break;
                case EVENT_SPECIAL_MONITOR:
                    CheckSpecialPhaseFailureOrCompletion();
                    if (_inSpecialPhase)
                        events.ScheduleEvent(EVENT_SPECIAL_MONITOR, Milliseconds(SPECIAL_MONITOR_MS));
                    break;
                case EVENT_BOT_STASIS_REFRESH:
                    RefreshAllEncounterBotStasis();
                    if (_inSpecialPhase)
                        events.ScheduleEvent(EVENT_BOT_STASIS_REFRESH, Milliseconds(BOT_STASIS_REFRESH_MS));
                    break;
                case EVENT_NORMAL_PHASE_SUMMON:
                    if (ShouldSummonDuringSpecialPhase())
                    {
                        SummonNormalPhaseNpcOnPlayer();
                        events.ScheduleEvent(EVENT_NORMAL_PHASE_SUMMON, Milliseconds(NORMAL_PHASE_SUMMON_INTERVAL_MS));
                    }
                    break;
                default:
                    break;
                }
            }

            return;
        }

        bool hasVictim = UpdateVictim();
        bool canCastCombatSpell = hasVictim && !me->HasUnitState(UNIT_STATE_CASTING);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_NORMAL_PHASE_SUMMON:
                SummonNormalPhaseNpcOnPlayer();
                events.ScheduleEvent(EVENT_NORMAL_PHASE_SUMMON, Milliseconds(NORMAL_PHASE_SUMMON_INTERVAL_MS));
                break;

            case EVENT_NORMAL_PHASE_RANDOM_RUNNER:
                SummonNormalPhaseRandomRunner();
                events.ScheduleEvent(EVENT_NORMAL_PHASE_RANDOM_RUNNER, Milliseconds(GetNextNormalPhaseRunnerIntervalMs()));
                break;

            case EVENT_CARNIVOROUS_BITE:
                if (canCastCombatSpell)
                {
                    if (Unit* victim = me->GetVictim())
                        me->CastSpell(victim, SPELL_CARNIVOROUS_BITE, false);
                }
                events.ScheduleEvent(EVENT_CARNIVOROUS_BITE, 9s);
                break;

            case EVENT_CHAIN_BEAM:
                if (canCastCombatSpell)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                        me->CastSpell(target, SPELL_CHAIN_BEAM, false);
                }
                events.ScheduleEvent(EVENT_CHAIN_BEAM, 12s);
                break;

            default:
                break;
            }
        }

        if (canCastCombatSpell)
            DoMeleeAttackIfReady();
    }
};

void AddSC_boss_shirrak_the_dead_watcher()
{
    new npc_shirrak_normal_phase_summon();
    new npc_shirrak_special_flamewave_runner();
    RegisterAuchenaiCryptsCreatureAI(boss_shirrak_the_dead_watcher);
}
