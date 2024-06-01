#include "AimbotHitscan.h"
#include "../../Vars.h"
#include "../../Backtrack/Backtrack.h"
#include "../../Resolver/Resolver.h"
#include "../../Visuals/Visuals.h"

/* Returns all valid targets */
std::vector<Target_t> CAimbotHitscan::GetTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	std::vector<Target_t> validTargets;
	const auto sortMethod = static_cast<ESortMethod>(Vars::Aimbot::Hitscan::SortMethod.Value);

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Players
	if (Vars::Aimbot::Global::AimAt.Value & (ToAimAt::PLAYER))
	{
		const bool bIsMedigun = pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN;
		const bool hasPissRifle = G::CurItemDefIndex == Sniper_m_TheSydneySleeper;

		EGroupType groupType = EGroupType::PLAYERS_ENEMIES;
		if (bIsMedigun)
			groupType = EGroupType::PLAYERS_TEAMMATES;
		else if (hasPissRifle)
			groupType = EGroupType::PLAYERS_ALL;

		for (const auto& pTarget : g_EntityCache.GetGroup(groupType))
		{
			// Is the target valid and alive?
			if (!pTarget->IsAlive() || pTarget->IsAGhost() || pTarget == pLocal)
			{
				continue;
			}

			// Can we extinguish a teammate using the piss rifle?
			if (hasPissRifle && (pTarget->GetTeamNum() == pLocal->GetTeamNum()))
			{
				if (!Vars::Aimbot::Hitscan::ExtinguishTeam.Value || !pTarget->IsOnFire())
				{
					continue;
				}
			}

			// Can the medigun reach the target?
			if (bIsMedigun && (pLocal->GetWorldSpaceCenter().DistTo(pTarget->GetWorldSpaceCenter()) > 472.f))
			{
				continue;
			}

			// Should we ignore the target?
			if (F::AimbotGlobal.ShouldIgnore(pTarget, bIsMedigun))
			{
				continue;
			}

			Vec3 vPos = pTarget->GetHitboxPos(HITBOX_PELVIS);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::Hitscan::AimFOV.Value)
			{
				continue;
			}

			const auto& priority = F::AimbotGlobal.GetPriority(pTarget->GetIndex());
			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pTarget, ETargetType::PLAYER, vPos, vAngleTo, flFOVTo, flDistTo, -1, false, priority });
		}
	}

	// Buildings
	if (Vars::Aimbot::Global::AimAt.Value)
	{
		for (const auto& pBuilding : g_EntityCache.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			bool isSentry = pBuilding->GetClassID() == ETFClassID::CObjectSentrygun;
			bool isDispenser = pBuilding->GetClassID() == ETFClassID::CObjectDispenser;
			bool isTeleporter = pBuilding->GetClassID() == ETFClassID::CObjectTeleporter;

			if (!(Vars::Aimbot::Global::AimAt.Value & (ToAimAt::SENTRY)) && isSentry) { continue; }
			if (!(Vars::Aimbot::Global::AimAt.Value & (ToAimAt::DISPENSER)) && isDispenser) { continue; }
			if (!(Vars::Aimbot::Global::AimAt.Value & (ToAimAt::TELEPORTER)) && isTeleporter) { continue; }

			// Is the building valid and alive?
			if (!pBuilding || !pBuilding->IsAlive()) { continue; }

			Vec3 vPos = pBuilding->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::Hitscan::AimFOV.Value)
			{
				continue;
			}

			// The target is valid! Add it to the target vector.
			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pBuilding, isSentry ? ETargetType::SENTRY : (isDispenser ? ETargetType::DISPENSER : ETargetType::TELEPORTER), vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	// Stickies
	if (Vars::Aimbot::Global::AimAt.Value & (ToAimAt::STICKY))
	{
		for (const auto& pProjectile : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			// Is the projectile a sticky?
			if (!(pProjectile->GetPipebombType() == TYPE_STICKY))
			{
				continue;
			}

			const auto& pOwner = I::ClientEntityList->GetClientEntityFromHandle(reinterpret_cast<int>(pProjectile->GetThrower()));
			if (!pOwner) { continue; }

			if ((!pProjectile->GetTouched()) || (pOwner->GetTeamNum() == pLocal->GetTeamNum()))
			{
				continue;
			}

			Vec3 vPos = pProjectile->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::Hitscan::AimFOV.Value)
			{
				continue;
			}

			// The target is valid! Add it to the target vector.
			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pProjectile, ETargetType::STICKY, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	// NPCs
	if (Vars::Aimbot::Global::AimAt.Value & (ToAimAt::NPC))
	{
		for (const auto& pNPC : g_EntityCache.GetGroup(EGroupType::WORLD_NPC))
		{
			Vec3 vPos = pNPC->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::Hitscan::AimFOV.Value)
			{
				continue;
			}

			// The target is valid! Add it to the target vector.
			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pNPC, ETargetType::NPC, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	//Bombs
	if (Vars::Aimbot::Global::AimAt.Value & (ToAimAt::BOMB))
	{
		//TODO: Check if player is close enough to bomb to damage
		for (const auto& pBomb : g_EntityCache.GetGroup(EGroupType::WORLD_BOMBS))
		{
			Vec3 vPos = pBomb->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);

			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

			if (flFOVTo > Vars::Aimbot::Hitscan::AimFOV.Value)
			{
				continue;
			}

			// The target is valid! Add it to the target vector.
			validTargets.push_back({ pBomb, ETargetType::BOMBS, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	// Did we find at least one target?
	return validTargets;
}

std::vector<Target_t> CAimbotHitscan::SortTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	auto validTargets = GetTargets(pLocal, pWeapon);

	const auto& sortMethod = static_cast<ESortMethod>(1/*Vars::Aimbot::Melee::SortMethod.Value*/);
	F::AimbotGlobal.SortTargets(&validTargets, sortMethod);
	F::AimbotGlobal.SortPriority(&validTargets, sortMethod);

	std::vector<Target_t> sortedTargets = {};
	int i = 0; for (auto& target : validTargets)
	{
		i++; if (i > Vars::Aimbot::Global::MaxTargets.Value) break;

		sortedTargets.push_back(target);
	}

	return sortedTargets;
}



bool CAimbotHitscan::IsHitboxValid(int nHitbox)
{
	const int iHitboxes = Vars::Aimbot::Hitscan::Hitboxes.Value;
	switch (nHitbox)
	{
	case -1: return true;
	case HITBOX_HEAD: return iHitboxes & (1 << 0);
	case HITBOX_PELVIS: return iHitboxes & (1 << 1);
	case HITBOX_SPINE_0:
	case HITBOX_SPINE_1:
	case HITBOX_SPINE_2:
	case HITBOX_SPINE_3: return iHitboxes & (1 << 2);
	case HITBOX_UPPERARM_L:
	case HITBOX_LOWERARM_L:
	case HITBOX_HAND_L:
	case HITBOX_UPPERARM_R:
	case HITBOX_LOWERARM_R:
	case HITBOX_HAND_R: return iHitboxes & (1 << 3);
	case HITBOX_HIP_L:
	case HITBOX_KNEE_L:
	case HITBOX_FOOT_L:
	case HITBOX_HIP_R:
	case HITBOX_KNEE_R:
	case HITBOX_FOOT_R: return iHitboxes & (1 << 4);
	}

	return false;
}

int CAimbotHitscan::GetHitboxPriority(int nHitbox, CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CBaseEntity* pTarget)
{
	bool bHeadshot = false;
	{
		const int nClassNum = pLocal->GetClassNum();

		if (nClassNum == CLASS_SNIPER)
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic && pLocal->IsScoped() ||
				G::CurItemDefIndex == Sniper_m_TheClassic && pWeapon->GetChargeDamage() > 149.9f)
				bHeadshot = true;
		}
		if (nClassNum == CLASS_SPY)
		{
			if ((G::CurItemDefIndex == Spy_m_TheAmbassador || G::CurItemDefIndex == Spy_m_FestiveAmbassador) && pWeapon->AmbassadorCanHeadshot())
				bHeadshot = true;
		}

		if (Vars::Aimbot::Global::BAimLethal.Value && bHeadshot)
		{
			{
				float flBodyMult = 1.f;
				switch (G::CurItemDefIndex)
				{
				case Sniper_m_TheClassic: flBodyMult = 0.9f; break;
				case Sniper_m_TheHitmansHeatmaker: flBodyMult = 0.8f; break;
				case Sniper_m_TheMachina:
				case Sniper_m_ShootingStar: if (pWeapon->GetChargeDamage() > 149.9f) flBodyMult = 1.15f;
				}
				if (pWeapon->GetChargeDamage() * flBodyMult >= pTarget->GetHealth())
					bHeadshot = false;
			}

			if (G::CurItemDefIndex == Spy_m_TheAmbassador || G::CurItemDefIndex == Spy_m_FestiveAmbassador)
			{
				const float flDistTo = pTarget->GetAbsOrigin().DistTo(pLocal->GetAbsOrigin());
				const int nAmbassadorBodyshotDamage = Math::RemapValClamped(flDistTo, 90, 900, 51, 18);

				if (pTarget->GetHealth() < (nAmbassadorBodyshotDamage + 2)) // whatever
					bHeadshot = false;
			}
		}
	}

	switch (nHitbox)
	{
	case -1: return 2;
	case HITBOX_HEAD: return bHeadshot ? 0 : 2;
	case HITBOX_PELVIS: return 2;
	case HITBOX_SPINE_0:
	case HITBOX_SPINE_1:
	case HITBOX_SPINE_2:
	case HITBOX_SPINE_3: return bHeadshot ? 1 : 0;
	case HITBOX_UPPERARM_L:
	case HITBOX_LOWERARM_L:
	case HITBOX_HAND_L:
	case HITBOX_UPPERARM_R:
	case HITBOX_LOWERARM_R:
	case HITBOX_HAND_R: return 2;
	case HITBOX_HIP_L:
	case HITBOX_KNEE_L:
	case HITBOX_FOOT_L:
	case HITBOX_HIP_R:
	case HITBOX_KNEE_R:
	case HITBOX_FOOT_R: return 2;
	}

	return 2;
};

bool CAimbotHitscan::CanHit(Target_t& target, CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	if (Vars::Aimbot::Global::IgnoreOptions.Value & (1 << 6) && G::ChokeMap[target.m_pEntity->GetIndex()] > Vars::Aimbot::Global::TickTolerance.Value)
		return false;

	Vec3 vEyePos = pLocal->GetShootPos();

	const model_t* pModel = target.m_pEntity->GetModel();
	if (!pModel) return false;
	const studiohdr_t* pHDR = I::ModelInfoClient->GetStudioModel(pModel);
	if (!pHDR) return false;
	const mstudiohitboxset_t* pSet = pHDR->GetHitboxSet(target.m_pEntity->GetHitboxSet());
	if (!pSet) return false;

	std::deque<TickRecord> pRecords;
	{
		const auto& records = F::Backtrack.GetRecords(target.m_pEntity);
		if (!records || !Vars::Backtrack::Enabled.Value || target.m_TargetType != ETargetType::PLAYER)
		{
			matrix3x4 bones[128];
			if (!target.m_pEntity->SetupBones(bones, 128, BONE_USED_BY_ANYTHING, target.m_pEntity->GetSimulationTime()))
				return false;

			pRecords.push_front({
				target.m_pEntity->GetSimulationTime(),
				I::GlobalVars->curtime,
				I::GlobalVars->tickcount,
				false,
				*reinterpret_cast<BoneMatrixes*>(&bones),
				target.m_pEntity->m_vecOrigin()
			});
		}
		else
			pRecords = *records;
	}

	std::deque<TickRecord> validRecords = target.m_TargetType == ETargetType::PLAYER ? F::Backtrack.GetValidRecords(target.m_pEntity, pRecords, (BacktrackMode)Vars::Backtrack::Method.Value) : pRecords;
	for (auto& pTick : validRecords)
	{
		if (target.m_TargetType == ETargetType::PLAYER)
		{
			auto boneMatrix = (matrix3x4*)(&pTick.BoneMatrix.BoneMatrix);
			if (!boneMatrix) continue;

			std::vector<std::pair<const mstudiobbox_t*, int>> hitboxes;
			{
				std::vector<std::pair<const mstudiobbox_t*, int>> primary, secondary, tertiary; // dumb
				for (int nHitbox = 0/*-1*/; nHitbox < target.m_pEntity->GetNumOfHitboxes(); nHitbox++)
				{
					if (!IsHitboxValid(nHitbox)) continue;

					const mstudiobbox_t* pBox = pSet->hitbox(nHitbox);
					if (!pBox) continue;

					switch (GetHitboxPriority(nHitbox, pLocal, pWeapon, target.m_pEntity))
					{
					case 0: primary.push_back({ pBox, nHitbox }); break;
					case 1: secondary.push_back({ pBox, nHitbox }); break;
					case 2: tertiary.push_back({ pBox, nHitbox }); break;
					}
				}
				for (auto pair : primary) hitboxes.push_back(pair);
				for (auto pair : secondary) hitboxes.push_back(pair);
				for (auto pair : tertiary) hitboxes.push_back(pair);
			}

			for (auto pair : hitboxes)
			{
				const float flScale = Vars::Aimbot::Hitscan::PointScale.Value;
				const Vec3 vMins = pair.first->bbmin * flScale;
				const Vec3 vMaxs = pair.first->bbmax * flScale;

				std::vector<Vec3> vecPoints = { Vec3() };
				if (flScale > 0.f)
				{
					vecPoints = {
						Vec3(),
						Vec3(vMins.x, vMins.y, vMaxs.z),
						Vec3(vMaxs.x, vMins.y, vMaxs.z),
						Vec3(vMins.x, vMaxs.y, vMaxs.z),
						Vec3(vMaxs.x, vMaxs.y, vMaxs.z),
						Vec3(vMins.x, vMins.y, vMins.z),
						Vec3(vMaxs.x, vMins.y, vMins.z),
						Vec3(vMins.x, vMaxs.y, vMins.z),
						Vec3(vMaxs.x, vMaxs.y, vMins.z)
					};
				}

				for (const auto& point : vecPoints)
				{
					Vec3 vTransformed = {};
					Math::VectorTransform(point, boneMatrix[pair.first->bone], vTransformed);

					if (Utils::VisPosMask(pLocal, target.m_pEntity, vEyePos, vTransformed, MASK_SHOT)) // for the time being, no vischecks against other hitboxes
					{
						target.pTick = &pTick;
						if (Vars::Backtrack::Enabled.Value)
						{
							target.ShouldBacktrack = true;
						}
						target.m_vPos = vTransformed;
						target.m_vAngleTo = Math::CalcAngle(pLocal->GetShootPos(), vTransformed);
						target.m_nAimedHitbox = pair.second;
						if (!point.IsZero())
							target.m_bHasMultiPointed = true;
						return true;
					}
				}
			}
		}
		else
		{
			const float flScale = Vars::Aimbot::Hitscan::PointScale.Value;
			const Vec3 vMins = target.m_pEntity->GetCollideableMins() * flScale;
			const Vec3 vMaxs = target.m_pEntity->GetCollideableMaxs() * flScale;

			std::vector<Vec3> vecPoints = { Vec3() }; 
			if (flScale > 0.f)
			{
				vecPoints = {
					Vec3(),
					Vec3(vMins.x * 0.9f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
					Vec3(vMaxs.x * 0.9f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
					Vec3(((vMins.x + vMaxs.x) * 0.5f), vMins.y * 0.9f, ((vMins.z + vMaxs.z) * 0.5f)),
					Vec3(((vMins.x + vMaxs.x) * 0.5f), vMaxs.y * 0.9f, ((vMins.z + vMaxs.z) * 0.5f)),
					Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMins.z * 0.9f),
					Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMaxs.z * 0.9f)
				};
			}

			const matrix3x4& transform = target.m_pEntity->GetRgflCoordinateFrame();
			for (const auto& point : vecPoints)
			{
				Vec3 vTransformed = {};
				Math::VectorTransform(point, transform, vTransformed);

				if (Utils::VisPosMask(pLocal, target.m_pEntity, vEyePos, vTransformed, MASK_SHOT)) // for the time being, no traces against other hitboxes
				{
					target.m_vPos = vTransformed;
					target.m_vAngleTo = Math::CalcAngle(pLocal->GetShootPos(), vTransformed);
					if (!point.IsZero())
						target.m_bHasMultiPointed = true;
					return true;
				}
			}
		}
	}

	return false;
}



/* Returns whether AutoShoot should fire */
bool CAimbotHitscan::ShouldFire(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, const CUserCmd* pCmd, const Target_t& target)
{
	if (!Vars::Aimbot::Global::AutoShoot.Value) return false;

	switch (pLocal->GetClassNum())
	{
	case CLASS_SNIPER:
	{
		const bool bIsScoped = pLocal->IsScoped();

		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value)
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic
				&& G::CurItemDefIndex != Sniper_m_TheSydneySleeper
				&& !G::WeaponCanHeadShot && bIsScoped)
			{
				return false;
			}
		}

		if (Vars::Aimbot::Hitscan::WaitForCharge.Value && (bIsScoped || G::CurItemDefIndex == Sniper_m_TheClassic))
		{
			const int nHealth = target.m_pEntity->GetHealth();
			const bool bIsCritBoosted = pLocal->IsCritBoosted();

			if (target.m_nAimedHitbox == HITBOX_HEAD && G::CurItemDefIndex != Sniper_m_TheSydneySleeper && (G::CurItemDefIndex != Sniper_m_TheClassic || G::CurItemDefIndex == Sniper_m_TheClassic && pWeapon->GetChargeDamage() > 149.9f))
			{
				if (nHealth > 150)
				{
					const float flDamage = Math::RemapValClamped(pWeapon->GetChargeDamage(), 0.0f, 150.0f, 0.0f, 450.0f);
					const int nDamage = static_cast<int>(flDamage);

					if (nDamage < nHealth && nDamage != 450)
						return false;
				}

				else
				{
					if (!bIsCritBoosted && !G::WeaponCanHeadShot)
						return false;
				}
			}
			else
			{
				if (nHealth > (bIsCritBoosted ? 150 : 50))
				{
					float flCritMult = target.m_pEntity->IsInJarate() ? 1.36f : 1.0f;

					if (bIsCritBoosted)
						flCritMult = 3.0f;

					float flBodyMult = 1.f;
					switch (G::CurItemDefIndex)
					{
					case Sniper_m_TheClassic: flBodyMult = 0.9f; break;
					case Sniper_m_TheHitmansHeatmaker: flBodyMult = 0.8f; break;
					case Sniper_m_TheMachina:
					case Sniper_m_ShootingStar: if (pWeapon->GetChargeDamage() > 149.9f) flBodyMult = 1.15f;
					}

					const float flMax = 150.0f * flCritMult * flBodyMult;
					const int nDamage = static_cast<int>(pWeapon->GetChargeDamage() * flCritMult * flBodyMult);

					if (nDamage < target.m_pEntity->GetHealth() && nDamage != static_cast<int>(flMax))
						return false;
				}
			}
		}

		break;
	}
	case CLASS_SPY:
		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value && !pWeapon->AmbassadorCanHeadshot())
			return false;

		break;
	}

	if (Vars::Aimbot::Hitscan::AimMethod.Value == 1)
	{
		Vec3 vForward = {};
		Math::AngleVectors(pCmd->viewangles, &vForward);
		const Vec3 vTraceStart = pLocal->GetShootPos();
		const Vec3 vTraceEnd = (vTraceStart + (vForward * 8192.0f));

		CGameTrace trace = {};
		CTraceFilterHitscan filter = {};
		filter.pSkip = pLocal;

		Utils::Trace(vTraceStart, vTraceEnd, (MASK_SHOT /* | CONTENTS_GRATE | MASK_VISIBLE*/), &filter, &trace);

		if (trace.entity != target.m_pEntity)
			return false;

		if (target.m_nAimedHitbox == HITBOX_HEAD)
		{
			if (trace.hitbox != HITBOX_HEAD)
				return false;

			if (!target.m_bHasMultiPointed)
			{
				Vec3 vMins = {}, vMaxs = {}, vCenter = {};
				matrix3x4 matrix = {};

				if (!target.m_pEntity->GetHitboxMinsAndMaxsAndMatrix(HITBOX_HEAD, vMins, vMaxs, matrix, &vCenter))
					return false;

				const float flScale = Vars::Aimbot::Hitscan::PointScale.Value;
				vMins *= flScale;
				vMaxs *= flScale;

				if (!Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix))
					return false;
			}
		}
	}

	return true;
}

bool CAimbotHitscan::IsAttacking(const CUserCmd* pCmd, CBaseCombatWeapon* pWeapon)
{
	return ((pCmd->buttons & IN_ATTACK) && G::WeaponCanAttack);
}

// assume angle calculated outside with other overload
void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle)
{
	if (Vars::Aimbot::Hitscan::AimMethod.Value != 2)
	{
		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(pCmd->viewangles);
	}
	else if (G::IsAttacking)
	{
		pCmd->viewangles = vAngle;
		Utils::FixMovement(pCmd, pCmd->viewangles);
	}
}

Vec3 CAimbotHitscan::Aim(Vec3 vCurAngle, Vec3 vToAngle)
{
	Vec3 vReturn = {};

	vToAngle -= G::PunchAngles;
	Math::ClampAngles(vToAngle);

	switch (Vars::Aimbot::Melee::AimMethod.Value)
	{
	case 0: // Plain
		vReturn = vToAngle;
		break;

	case 1: //Smooth
		if (Vars::Aimbot::Melee::SmoothingAmount.Value == 0)
		{ // plain aim at 0 smoothing factor
			vReturn = vToAngle;
			break;
		}
		//a + (b - a) * t [lerp]
		vReturn = vCurAngle + (vToAngle - vCurAngle) * (1.f - (float)Vars::Aimbot::Melee::SmoothingAmount.Value / 100.f);
		break;

	case 2: // Silent
		vReturn = vToAngle;
		break;

	default: break;
	}

	return vReturn;
}

void CAimbotHitscan::Run(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	const bool bAutomatic = pWeapon->IsStreamingWeapon(), bKeepFiring = bAutomatic && bLastTickAttack;
	if (!Vars::Aimbot::Global::Active.Value || !Vars::Aimbot::Hitscan::Active.Value || !G::WeaponCanAttack && Vars::Aimbot::Hitscan::AimMethod.Value == 2 && !G::ShouldShift && !bKeepFiring)
	{
		bLastTickHeld = false;
		Exit(pWeapon, pCmd); return;
	}

	const bool bShouldAim = (Vars::Aimbot::Global::AimKey.Value == VK_LBUTTON ? (pCmd->buttons & IN_ATTACK) : F::AimbotGlobal.IsKeyDown());
	if (!bShouldAim)
	{
		Exit(pWeapon, pCmd); return;
	}

	const int nWeaponID = pWeapon->GetWeaponID();

	if (nWeaponID == TF_WEAPON_MINIGUN)
		pCmd->buttons |= IN_ATTACK2;
	else if (G::CurItemDefIndex == Sniper_m_TheClassic)
	{
		const bool bHeld = bLastTickHeld;

		pCmd->buttons |= IN_ATTACK;
		bLastTickHeld = true;
		if (!bHeld)
		{
			Exit(pWeapon, pCmd); return;
		}
	}

	if (nWeaponID != TF_WEAPON_COMPOUND_BOW
		&& pLocal->GetClassNum() == CLASS_SNIPER
		&& pWeapon->GetSlot() == SLOT_PRIMARY)
	{
		const bool bScoped = pLocal->IsScoped();

		if (Vars::Aimbot::Hitscan::AutoScope.Value && !bScoped)
		{
			pCmd->buttons |= IN_ATTACK2;
			Exit(pWeapon, pCmd); return;
		}

		if ((Vars::Aimbot::Hitscan::ScopedOnly.Value || G::CurItemDefIndex == Sniper_m_TheMachina || G::CurItemDefIndex == Sniper_m_ShootingStar) && !bScoped)
		{
			Exit(pWeapon, pCmd); return;
		}
	}

	auto targets = SortTargets(pLocal, pWeapon);
	if (targets.empty())
	{
		Exit(pWeapon, pCmd); return;
	}

	for (auto& target : targets)
	{
		if (!CanHit(target, pLocal, pWeapon)) continue;

		G::CurrentTargetIdx = target.m_pEntity->GetIndex();
		G::HitscanRunning = true;
		G::HitscanSilentActive = Vars::Aimbot::Hitscan::AimMethod.Value == 2;

		if (G::HitscanSilentActive)
		{
			G::AimPos = target.m_vPos;
		}

		bool bShouldFire = ShouldFire(pLocal, pWeapon, pCmd, target);

		if (bShouldFire)
		{
			if (G::CurItemDefIndex == Engi_s_TheWrangler || G::CurItemDefIndex ==
				Engi_s_FestiveWrangler)
			{
				pCmd->buttons |= IN_ATTACK2;
			}

			if (G::CurItemDefIndex != Sniper_m_TheClassic)
				pCmd->buttons |= IN_ATTACK;
			else
			{
				pCmd->buttons &= ~IN_ATTACK;
				bLastTickHeld = false;
			}

			/*
			// Get circular gaussian spread. Under some cases we fire a bullet right down the crosshair:
			//	- The first bullet of a spread weapon (except for rapid fire spread weapons like the minigun)
			//	- The first bullet of a non-spread weapon if it's been >1.25 second since firing
			bool bFirePerfect = false;
			if ( iBullet == 0 && pWpn )
			{
				float flTimeSinceLastShot = (gpGlobals->curtime - pWpn->m_flLastFireTime );
				if ( nBulletsPerShot > 1 && flTimeSinceLastShot > 0.25 )
				{
					bFirePerfect = true;
				}
				else if ( nBulletsPerShot == 1 && flTimeSinceLastShot > 1.25 )
				{
					bFirePerfect = true;
				}
			}
			*/

			if (Vars::Aimbot::Hitscan::TapFire.Value && nWeaponID == TF_WEAPON_MINIGUN && !pLocal->IsPrecisionRune())
			{
				const bool bDo = Vars::Aimbot::Hitscan::TapFire.Value == 1
					? pLocal->GetShootPos().DistTo(target.m_vPos) > Vars::Aimbot::Hitscan::TapFireDist.Value
					: true;

				if (bDo && pWeapon->GetWeaponSpread() != 0.f)
				{
					const float flTimeSinceLastShot = (pLocal->GetTickBase() * TICK_INTERVAL) - pWeapon->GetLastFireTime();

					if (pWeapon->GetWeaponData().m_nBulletsPerShot > 1)
					{
						if (flTimeSinceLastShot <= 0.25f)
						{
							pCmd->buttons &= ~IN_ATTACK;
						}
					}
					else if (flTimeSinceLastShot <= 1.25f)
					{
						pCmd->buttons &= ~IN_ATTACK;
					}
				}
			}
		}
		
		G::IsAttacking = G::CurItemDefIndex == Sniper_m_TheClassic ? bShouldFire : IsAttacking(pCmd, pWeapon);

		if (G::IsAttacking)
		{
			F::Resolver.Aimbot(target.m_pEntity, target.m_nAimedHitbox == 0);

			if (target.pTick)
			{
				if (target.ShouldBacktrack)
					pCmd->tick_count = TIME_TO_TICKS((*target.pTick).flSimTime) + TIME_TO_TICKS(F::Backtrack.flFakeInterp);

				if (Vars::Visuals::BulletTracer.Value)
				{
					F::Visuals.ClearBulletLines();
					G::BulletsStorage.push_back({ {pLocal->GetShootPos(), target.m_vPos}, I::GlobalVars->curtime + 5.f, Colors::BulletTracer });
				}
				if (Vars::Aimbot::Global::ShowHitboxes.Value)
				{
					F::Visuals.DrawHitbox((matrix3x4*)(&(*target.pTick).BoneMatrix.BoneMatrix), target.m_pEntity);
				}
			}
		}

		// todo: move into canhit & possibly do Math::RayToOBB instead of vischeck ?
		auto vAngle = Aim(pCmd->viewangles, target.m_vAngleTo);
		Aim(pCmd, vAngle);

		break;
	}

	Exit(pWeapon, pCmd);
}

void CAimbotHitscan::Exit(CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	bLastTickAttack = pCmd->buttons & IN_ATTACK;
}
