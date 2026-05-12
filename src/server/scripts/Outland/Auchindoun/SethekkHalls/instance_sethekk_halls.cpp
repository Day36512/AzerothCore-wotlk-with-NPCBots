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

#include "Config.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Player.h"
#include "sethekk_halls.h"

namespace
{
    constexpr uint32 AnzuSummonEventId = 14797;
    constexpr char const* CONFIG_ANZU_ALWAYS_VISIBLE = "SethekkHalls.Anzu.AlwaysVisible";

    Position const AnzuSummonPosition = { -88.02f, 288.18f, 75.2f, 6.0f };
}

DoorData const doorData[] =
{
    { GO_IKISS_DOOR, DATA_IKISS, DOOR_TYPE_PASSAGE },
    { 0,                      0, DOOR_TYPE_ROOM    }  // END
};

ObjectData const gameObjectData[] =
{
    { GO_THE_TALON_KINGS_COFFER, DATA_GO_TALON_KING_COFFER },
    { 0,                         0                         }
};

ObjectData const creatureData[] =
{
    { NPC_VOICE_OF_THE_RAVEN_GOD, DATA_VOICE_OF_THE_RAVEN_GOD },
    { NPC_ANZU,                   DATA_ANZU_CREATURE          },
    { 0,                          0                           }
};

class instance_sethekk_halls : public InstanceMapScript
{
public:
    instance_sethekk_halls() : InstanceMapScript("instance_sethekk_halls", MAP_AUCHINDOUN_SETHEKK_HALLS) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_sethekk_halls_InstanceMapScript(map);
    }

    struct instance_sethekk_halls_InstanceMapScript : public InstanceScript
    {
        instance_sethekk_halls_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetHeaders(DataHeaders);
            SetBossNumber(EncounterCount);
            LoadDoorData(doorData);
            LoadObjectData(creatureData, gameObjectData);
        }

        void OnPlayerEnter(Player* /*player*/) override
        {
            if (sConfigMgr->GetOption<bool>(CONFIG_ANZU_ALWAYS_VISIBLE, false))
            {
                TrySummonAnzu();
            }
        }

        void ProcessEvent(WorldObject* /*obj*/, uint32 eventId) override
        {
            if (eventId == AnzuSummonEventId)
            {
                TrySummonAnzu();
            }
        }

    private:
        void TrySummonAnzu()
        {
            if (GetBossState(DATA_ANZU) == DONE)
            {
                return;
            }

            if (GetCreature(DATA_VOICE_OF_THE_RAVEN_GOD) || GetCreature(DATA_ANZU_CREATURE))
            {
                return;
            }

            instance->SummonCreature(NPC_VOICE_OF_THE_RAVEN_GOD, AnzuSummonPosition);
        }
    };
};

void AddSC_instance_sethekk_halls()
{
    new instance_sethekk_halls();
}
