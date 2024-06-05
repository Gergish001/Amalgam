#pragma once
#include <mutex>
#include "../../Utils/Feature/Feature.h"
#include "../../SDK/Definitions/Main/CUserCmd.h"
#include "../../SDK/Definitions/Main/CBaseEntity.h"
#include "../../SDK/Helpers/Entities/Entities.h"
#include "../../SDK/Vars.h"
#include "../../Utils/Timer/Timer.h"
#include "../../SDK/SDK.h"

class CFollowbot
{
	bool ValidTarget(CBaseEntity* pTarget, CBaseEntity* pLocal);
	void OptimizePath(CBaseEntity* pLocal);
	CBaseEntity* FindTarget(CBaseEntity* pLocal);

	struct PathNode
	{
		Vec3 Location{};
		bool OnGround = false;
	};

	CBaseEntity* CurrentTarget = nullptr;
	std::deque<PathNode> PathNodes;
	std::mutex PathMutex;

public:
	void Run(CUserCmd* pCmd);
	void Reset();
	void Draw();
};

ADD_FEATURE(CFollowbot, Followbot)
