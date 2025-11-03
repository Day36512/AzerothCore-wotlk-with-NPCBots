/*
 *  Hellfire Peninsula - Tower Activity Packs (with Aggro-on-Spawn)
 *  AzerothCore 3.3.5a
 *
 *  Spawns periodic packs at Broken Hill, Overlook, Stadium using OutdoorPvPHP
 *  capture-point positions. Optional aggro-on-spawn, forced faction, and
 *  aggressive react state.
 *
 *  Extended: Kill-reward hook that grants 24579 (Alliance) / 24581 (Horde)
 *  to the corpse's loot-recipient player when these spawned mobs die.
 *  Configurable on/off and amount via worldserver.conf (see keys below).
 */

#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Creature.h"
#include "Map.h"
#include "MapMgr.h"              // sMapMgr
#include "World.h"
#include "Position.h"
#include "ScriptedCreature.h"    // AttackStart
#include "OutdoorPvPHP.h"        // HPCapturePoints[], HP_TOWER_* enum

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <random>
#include <cmath>
#include <chrono>                // std::chrono::milliseconds/seconds
#include <unordered_set>         // reward de-dupe

namespace HPTAP // Hellfire Peninsula Tower Activity Packs
{
    // ---------- Config keys ----------
    static constexpr char const* CONF_ENABLE = "HPTAP.Enable";
    static constexpr char const* CONF_DEBUG = "HPTAP.Debug";
    static constexpr char const* CONF_INTERVAL_MS = "HPTAP.SpawnIntervalMs";
    static constexpr char const* CONF_DESPAWN_MS = "HPTAP.DespawnMs";
    static constexpr char const* CONF_PLAYER_RADIUS = "HPTAP.PlayerCheckRadius";
    static constexpr char const* CONF_MIN_PLAYERS_NEAR = "HPTAP.MinPlayersNear";
    static constexpr char const* CONF_PACK_MIN = "HPTAP.PackSizeMin";
    static constexpr char const* CONF_PACK_MAX = "HPTAP.PackSizeMax";
    static constexpr char const* CONF_SPAWN_RADIUS = "HPTAP.SpawnRadius";
    static constexpr char const* CONF_AGGRO_ON_SPAWN = "HPTAP.AggroOnSpawn";
    static constexpr char const* CONF_AGGRO_RADIUS = "HPTAP.AggroRadius";
    static constexpr char const* CONF_FORCE_FACTION = "HPTAP.ForceFactionId";   // 0 = no override
    static constexpr char const* CONF_REACT_AGGRESSIVE = "HPTAP.ReactAggressive";

    static constexpr char const* CONF_ENTRIES_BH = "HPTAP.Entries.BrokenHill";
    static constexpr char const* CONF_ENTRIES_OV = "HPTAP.Entries.Overlook";
    static constexpr char const* CONF_ENTRIES_ST = "HPTAP.Entries.Stadium";

    // --- kill-reward controls ---
    static constexpr char const* CONF_REWARD_ENABLE = "HPTAP.Reward.Enable";
    static constexpr char const* CONF_REWARD_COUNT = "HPTAP.Reward.Count";

    static constexpr uint32 MAP_OUTLAND = 530;

    // Token items requested:
    static constexpr uint32 ITEM_ALLIANCE = 24579; // Alliance token
    static constexpr uint32 ITEM_HORDE = 24581; // Horde token

    enum TowerSlot : uint8
    {
        TOWER_BROKEN_HILL = HP_TOWER_BROKEN_HILL,
        TOWER_OVERLOOK = HP_TOWER_OVERLOOK,
        TOWER_STADIUM = HP_TOWER_STADIUM,
        TOWER_COUNT = HP_TOWER_NUM
    };

    struct TowerCfg { std::vector<uint32> entries; };

    struct TowerRuntime
    {
        uint32 nextSpawnMs = 0;
        std::vector<ObjectGuid> summons;                 // live or corpse handles we’re tracking
        std::unordered_set<ObjectGuid::LowType> rewarded;// corpses already rewarded (de-dupe)
    };

    // ---------- State ----------
    static bool   s_enable = true;
    static bool   s_debug = false;
    static uint32 s_intervalMs = 240000;
    static uint32 s_despawnMs = 600000;
    static float  s_playerRadius = 120.0f;
    static uint32 s_minPlayersNear = 1;
    static uint32 s_packMin = 3;
    static uint32 s_packMax = 5;

    static float  s_spawnRadius = 18.0f;
    static bool   s_aggroOnSpawn = true;
    static float  s_aggroRadius = 40.0f;
    static uint32 s_forceFactionId = 0;       // 0 = no override
    static bool   s_reactAggressive = true;

    // --- kill-reward state ---
    static bool   s_rewardEnable = true;
    static uint32 s_rewardCount = 1;

    static TowerCfg      s_cfg[TOWER_COUNT];
    static TowerRuntime  s_rt[TOWER_COUNT];

    template<typename T> static inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static inline std::string EatWS(std::string s)
    {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }), s.end());
        return s;
    }

    static std::vector<uint32> ParseCSV(std::string s)
    {
        std::vector<uint32> out;
        s = EatWS(s);
        if (s.empty()) return out;
        size_t start = 0;
        while (start < s.size())
        {
            size_t comma = s.find(',', start);
            std::string tok = s.substr(start, (comma == std::string::npos ? s.size() : comma) - start);
            try { uint32 id = static_cast<uint32>(std::stoul(tok)); if (id) out.push_back(id); }
            catch (...) {}
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return out;
    }

    static void LoadConfig(bool reload)
    {
        auto& C = *sConfigMgr;

        s_enable = C.GetOption<bool>(CONF_ENABLE, true);
        s_debug = C.GetOption<bool>(CONF_DEBUG, false);
        s_intervalMs = Clamp<uint32>(C.GetOption<uint32>(CONF_INTERVAL_MS, 240000u), 30000u, 3600000u);
        s_despawnMs = Clamp<uint32>(C.GetOption<uint32>(CONF_DESPAWN_MS, 600000u), 30000u, 3600000u);
        s_playerRadius = std::max<float>(10.0f, C.GetOption<float>(CONF_PLAYER_RADIUS, 120.0f));
        s_minPlayersNear = Clamp<uint32>(C.GetOption<uint32>(CONF_MIN_PLAYERS_NEAR, 1u), 0u, 40u);
        s_packMin = Clamp<uint32>(C.GetOption<uint32>(CONF_PACK_MIN, 3u), 1u, 20u);
        s_packMax = Clamp<uint32>(C.GetOption<uint32>(CONF_PACK_MAX, 5u), s_packMin, 25u);

        s_spawnRadius = Clamp<float>(C.GetOption<float>(CONF_SPAWN_RADIUS, 18.0f), 3.0f, 60.0f);
        s_aggroOnSpawn = C.GetOption<bool>(CONF_AGGRO_ON_SPAWN, true);
        s_aggroRadius = Clamp<float>(C.GetOption<float>(CONF_AGGRO_RADIUS, 40.0f), 5.0f, 120.0f);
        s_forceFactionId = C.GetOption<uint32>(CONF_FORCE_FACTION, 0);
        s_reactAggressive = C.GetOption<bool>(CONF_REACT_AGGRESSIVE, true);

        s_cfg[TOWER_BROKEN_HILL].entries = ParseCSV(C.GetOption<std::string>(CONF_ENTRIES_BH, ""));
        s_cfg[TOWER_OVERLOOK].entries = ParseCSV(C.GetOption<std::string>(CONF_ENTRIES_OV, ""));
        s_cfg[TOWER_STADIUM].entries = ParseCSV(C.GetOption<std::string>(CONF_ENTRIES_ST, ""));

        // NEW: reward config
        s_rewardEnable = C.GetOption<bool>(CONF_REWARD_ENABLE, true);
        s_rewardCount = Clamp<uint32>(C.GetOption<uint32>(CONF_REWARD_COUNT, 1u), 1u, 100u);

        if (!reload)
            LOG_INFO("module", "[HPTAP] Config loaded. Enabled={} Interval={}ms Despawn={}ms NearRadius={:.1f} MinPlayers={} Pack=[{},{}] "
                "SpawnRadius={:.1f} Aggro={} AggroRadius={:.1f} ForceFaction={} ReactAggressive={} RewardEnable={} RewardCount={}",
                s_enable, s_intervalMs, s_despawnMs, s_playerRadius, s_minPlayersNear, s_packMin, s_packMax,
                s_spawnRadius, s_aggroOnSpawn, s_aggroRadius, s_forceFactionId, s_reactAggressive, s_rewardEnable, s_rewardCount);
        else
            LOG_INFO("module", "[HPTAP] Config reloaded.");
    }

    static bool AnyPlayersNear(Map* map, Position const& pos, float radius, uint32 minPlayers)
    {
        if (!map) return false;
        uint32 count = 0;
        Map::PlayerList const& players = map->GetPlayers();
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            Player* p = it->GetSource();
            if (!p || !p->IsInWorld() || p->IsGameMaster() || p->GetMapId() != MAP_OUTLAND) continue;
            if (p->IsAlive() && p->GetDistance(pos) <= radius)
            {
                if (++count >= minPlayers) return true;
            }
        }
        return minPlayers == 0;
    }

    static Player* FindNearestPlayer(Map* map, Position const& pos, float radius)
    {
        Player* best = nullptr;
        float bestD2 = radius * radius;

        Map::PlayerList const& players = map->GetPlayers();
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            Player* p = it->GetSource();
            if (!p || !p->IsInWorld() || p->IsGameMaster() || !p->IsAlive()) continue;
            if (p->GetMapId() != MAP_OUTLAND) continue;

            float d2 = p->GetExactDist2dSq(pos.GetPositionX(), pos.GetPositionY());
            if (d2 <= bestD2) { bestD2 = d2; best = p; }
        }
        return best;
    }

    // ---reward helper ---
    static void RewardPlayerForKill(Creature* dead)
    {
        if (!s_rewardEnable || !dead)
            return;

        Player* recipient = dead->GetLootRecipient();
        if (!recipient || !recipient->IsInWorld())
            return;

        uint32 itemId = (recipient->GetTeamId() == TEAM_ALLIANCE) ? ITEM_ALLIANCE : ITEM_HORDE;

        if (!recipient->AddItem(itemId, s_rewardCount))
        {
            if (s_debug)
                LOG_DEBUG("module", "[HPTAP] Reward AddItem failed for player {} (item {}, count {}). Inventory full?",
                    recipient->GetName(), itemId, s_rewardCount);
        }
        else if (s_debug)
        {
            LOG_DEBUG("module", "[HPTAP] Granted item {} x{} to {} (team {}).",
                itemId, s_rewardCount, recipient->GetName(), uint32(recipient->GetTeamId()));
        }
    }

    static void PrepareCreature(Creature* cr)
    {
        if (!cr) return;

        if (s_forceFactionId)
            cr->SetFaction(s_forceFactionId);

        cr->SetReactState(s_reactAggressive ? REACT_AGGRESSIVE : REACT_DEFENSIVE);
        cr->SetHomePosition(cr->GetPosition());
        cr->SetWalk(false);

        // Timed despawn (delay, forced respawn time)
        cr->DespawnOrUnsummon(std::chrono::milliseconds(s_despawnMs),
            std::chrono::seconds(0));
    }

    // Force the whole pack to engage a single shared target (robust)
    static void AggroPack(Map* map, std::vector<ObjectGuid> const& guids, Player* target)
    {
        if (!map || !target) return;

        for (ObjectGuid const& g : guids)
        {
            Creature* cr = map->GetCreature(g);
            if (!cr || !cr->IsInWorld() || !cr->IsAlive())
                continue;

            if (!cr->IsHostileTo(target) && s_forceFactionId == 0)
                cr->SetFaction(14); // hostile

            cr->SetReactState(REACT_AGGRESSIVE);

            cr->AddThreat(target, 1.0f);
            cr->SetInCombatWith(target);
            target->SetInCombatWith(cr);

            if (cr->AI())
                cr->AI()->AttackStart(target);

            cr->GetMotionMaster()->MoveChase(target);
        }
    }

    static uint32 PickOne(std::vector<uint32> const& v)
    {
        if (v.empty()) return 0;
        if (v.size() == 1) return v.front();
        return v[urand(0u, static_cast<uint32>(v.size() - 1))];
    }

    static Position TowerCenter(uint8 towerIndex)
    {
        Position p;
        p.m_positionX = HPCapturePoints[towerIndex].x;
        p.m_positionY = HPCapturePoints[towerIndex].y;
        p.m_positionZ = HPCapturePoints[towerIndex].z;
        p.m_orientation = HPCapturePoints[towerIndex].o;
        return p;
    }

    static Position RandAroundGrounded(Map* map, Position const& center, float radius)
    {
        static std::mt19937 rng(std::random_device{}());
        constexpr float TAU = 6.28318530717958647692f;

        std::uniform_real_distribution<float> ang(0.0f, TAU);
        std::uniform_real_distribution<float> rad(0.4f * radius, radius);

        Position out = center;
        float a = ang(rng);
        float r = rad(rng);
        out.m_positionX += std::cos(a) * r;
        out.m_positionY += std::sin(a) * r;

        // Snap Z to ground using a local probe around the tower's Z
        float zProbe = center.m_positionZ + 5.0f;
        float z = map ? map->GetHeight(out.GetPositionX(), out.GetPositionY(), zProbe) : center.m_positionZ;
        if (z > -500.0f) out.m_positionZ = z;

        return out;
    }

    static void SpawnPackForTower(Map* map, uint8 towerIndex, uint32 /*despawnMs*/, uint32 packMin, uint32 packMax)
    {
        if (!map) return;

        auto& cfg = s_cfg[towerIndex];
        if (cfg.entries.empty())
        {
            if (s_debug)
                LOG_DEBUG("module", "[HPTAP] Tower {} has empty entry list; skipping spawn.", uint32(towerIndex));
            return;
        }

        Position center = TowerCenter(towerIndex);
        uint32 count = urand(packMin, packMax);

        auto& rt = s_rt[towerIndex];
        rt.summons.clear();
        rt.rewarded.clear();
        rt.summons.reserve(count);

        for (uint32 i = 0; i < count; ++i)
        {
            uint32 entry = PickOne(cfg.entries);
            Position where = RandAroundGrounded(map, center, s_spawnRadius);

            if (Creature* cr = map->SummonCreature(entry, where, /*SummonProps*/nullptr,
                /*duration*/0, /*summoner*/nullptr, /*spellId*/0, /*effIndex*/0, /*active*/false))
            {
                PrepareCreature(cr);
                rt.summons.push_back(cr->GetGUID());
            }
            else if (s_debug)
                LOG_DEBUG("module", "[HPTAP] SummonCreature failed for entry {} at tower {}.", entry, uint32(towerIndex));
        }

        // Aggro pass (shared target)
        if (s_aggroOnSpawn && !rt.summons.empty())
        {
            Player* tgt = FindNearestPlayer(map, center, s_aggroRadius);
            if (!tgt)
                tgt = FindNearestPlayer(map, center, s_playerRadius);

            if (tgt)
                AggroPack(map, rt.summons, tgt);
        }
    }

    // handle deaths + prune; reward happens here exactly once per corpse
    static void PruneAndReward(Map* map, uint8 towerIndex)
    {
        auto& rt = s_rt[towerIndex];
        if (!map)
        {
            rt.summons.clear();
            rt.rewarded.clear();
            return;
        }

        // Erase-if dead or gone; reward dead exactly once.
        rt.summons.erase(std::remove_if(rt.summons.begin(), rt.summons.end(),
            [&](ObjectGuid const& g)
            {
                Creature* c = map->GetCreature(g);
                if (!c || !c->IsInWorld())
                    return true; // remove missing

                if (!c->IsAlive())
                {
                    // de-dupe by GUID low
                    ObjectGuid::LowType low = c->GetGUID().GetCounter();
                    if (rt.rewarded.insert(low).second)
                        RewardPlayerForKill(c); // single-shot

                    return true; // remove corpse from tracking
                }

                return false; // keep alive
            }), rt.summons.end());
    }

    static void TickTower(Map* map, uint8 towerIndex, uint32 diff)
    {
        auto& rt = s_rt[towerIndex];

        // prune & reward deaths first
        PruneAndReward(map, towerIndex);

        if (!rt.summons.empty())
            return;

        if (rt.nextSpawnMs > diff)
        {
            rt.nextSpawnMs -= diff;
            return;
        }

        Position pos = TowerCenter(towerIndex);
        if (!AnyPlayersNear(map, pos, s_playerRadius, s_minPlayersNear))
        {
            rt.nextSpawnMs = s_intervalMs / 2;
            return;
        }

        SpawnPackForTower(map, towerIndex, s_despawnMs, s_packMin, s_packMax);
        rt.nextSpawnMs = s_intervalMs;
    }

    static void InitRuntime()
    {
        for (uint8 i = 0; i < TOWER_COUNT; ++i)
        {
            s_rt[i].summons.clear();
            s_rt[i].rewarded.clear();
            s_rt[i].nextSpawnMs = urand(1000u, 10000u); // initial jitter
        }
    }

    static void Shutdown(Map* map)
    {
        if (!map) return;
        for (uint8 i = 0; i < TOWER_COUNT; ++i)
        {
            for (ObjectGuid const& g : s_rt[i].summons)
                if (Creature* c = map->GetCreature(g))
                    c->DespawnOrUnsummon();
            s_rt[i].summons.clear();
            s_rt[i].rewarded.clear();
        }
    }

} // namespace HPTAP

// ----------------- World Script -----------------
class HPTAP_WorldScript final : public WorldScript
{
public:
    HPTAP_WorldScript() : WorldScript("HPTAP_WorldScript"), _accum(0) {}

    void OnAfterConfigLoad(bool reload) override
    {
        HPTAP::LoadConfig(reload);
        HPTAP::InitRuntime();
    }

    void OnUpdate(uint32 diff) override
    {
        if (!HPTAP::s_enable)
            return;

        _accum += diff;
        if (_accum < 500)  // coarse tick; inner timers gate work
            return;

        uint32 slice = _accum;   
        _accum = 0;

        Map* outland = sMapMgr->FindMap(HPTAP::MAP_OUTLAND, 0);
        if (!outland)
            return;

        for (uint8 t = 0; t < HPTAP::TOWER_COUNT; ++t)
            HPTAP::TickTower(outland, t, slice);
    }

    void OnShutdown() override
    {
        Map* outland = sMapMgr->FindMap(HPTAP::MAP_OUTLAND, 0);
        HPTAP::Shutdown(outland);
    }

private:
    uint32 _accum;
};

void AddSC_mod_hp_activity_spawns()
{
    new HPTAP_WorldScript();
}
