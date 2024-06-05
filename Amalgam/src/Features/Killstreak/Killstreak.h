#pragma once
#include "../../Utils/Feature/Feature.h"
#include <unordered_map>
#include "../../Utils/Hash/FNV1A.h"
#include "../../SDK/Definitions/Interfaces/IGameEvents.h"
#include "../../SDK/SDK.h"
class CKillstreaker
{
private:
	std::unordered_map<int, int> KillstreakMap;
	int Killstreak;
public:
	void ResetKillstreak();
	int	 GetCurrentStreak();
	int  GetCurrentWeaponStreak();
	void ApplyKillstreak();
private:
	void PlayerDeath(IGameEvent* pEvent);
	void PlayerSpawn(IGameEvent* pEvent);
public:
	void FireEvents(IGameEvent* pEvent, const FNV1A_t uNameHash);
};

ADD_FEATURE(CKillstreaker, Killstreaker)
