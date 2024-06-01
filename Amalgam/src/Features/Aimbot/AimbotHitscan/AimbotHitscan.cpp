#include "AimbotHitscan.h"

#include "../../Vars.h"
#include "../../Backtrack/Backtrack.h"
#include "../../Resolver/Resolver.h"
#include "../../Visuals/Visuals.h"

std::vector<Target_t> CAimbotHitscan::GetTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	std::vector<Target_t> validTargets;
	const auto sortMethod = static_cast<ESortMethod>(Vars::Aimbot::General::TargetSelection.Value);

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	const bool bIsMedigun = pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN;

	if (Vars::Aimbot::General::Target.Value & ToAimAt::PLAYER)
	{
		const bool bPissRifle = G::CurItemDefIndex == Sniper_m_TheSydneySleeper;

		EGroupType groupType = EGroupType::PLAYERS_ENEMIES;
		if (bIsMedigun)
			groupType = EGroupType::PLAYERS_TEAMMATES;
		else if (bPissRifle)
			groupType = EGroupType::PLAYERS_ALL;

		for (const auto& pTarget : g_EntityCache.GetGroup(groupType))
		{
			if (!pTarget->IsAlive() || pTarget->IsAGhost() || pTarget == pLocal)
				continue;

			// Can we extinguish a teammate using the piss rifle?
			if (bPissRifle && (pTarget->m_iTeamNum() == pLocal->m_iTeamNum()))
			{
				if (!(Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 6)) || !pTarget->IsOnFire())
					continue;
			}

			if (F::AimbotGlobal.ShouldIgnore(pTarget, bIsMedigun))
				continue;

			Vec3 vPos = pTarget->GetHitboxPos(HITBOX_PELVIS);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			const float flDistTo = vLocalPos.DistTo(vPos);
			const int priority = F::AimbotGlobal.GetPriority(pTarget->GetIndex());
			validTargets.push_back({ pTarget, ETargetType::PLAYER, vPos, vAngleTo, flFOVTo, flDistTo, priority });
		}
	}

	if (bIsMedigun) // do not attempt to heal buildings or other
		return validTargets;

	if (Vars::Aimbot::General::Target.Value)
	{
		for (const auto& pBuilding : g_EntityCache.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			if (!pBuilding->IsAlive())
				continue;

			bool isSentry = pBuilding->IsSentrygun(), isDispenser = pBuilding->IsDispenser(), isTeleporter = pBuilding->IsTeleporter();

			if (!(Vars::Aimbot::General::Target.Value & ToAimAt::SENTRY) && isSentry)
				continue;
			if (!(Vars::Aimbot::General::Target.Value & ToAimAt::DISPENSER) && isDispenser)
				continue;
			if (!(Vars::Aimbot::General::Target.Value & ToAimAt::TELEPORTER) && isTeleporter)
				continue;

			Vec3 vPos = pBuilding->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pBuilding, isSentry ? ETargetType::SENTRY : (isDispenser ? ETargetType::DISPENSER : ETargetType::TELEPORTER), vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (Vars::Aimbot::General::Target.Value & ToAimAt::STICKY)
	{
		for (const auto& pProjectile : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (pProjectile->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || pProjectile->m_iType() != TF_GL_MODE_REMOTE_DETONATE || !pProjectile->m_bTouched())
				continue;

			const auto& pOwner = I::ClientEntityList->GetClientEntityFromHandle(pProjectile->m_hThrower());
			if (!pOwner || pOwner->m_iTeamNum() == pLocal->m_iTeamNum())
				continue;

			Vec3 vPos = pProjectile->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pProjectile, ETargetType::STICKY, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (Vars::Aimbot::General::Target.Value & ToAimAt::NPC)
	{
		for (const auto& pNPC : g_EntityCache.GetGroup(EGroupType::WORLD_NPC))
		{
			Vec3 vPos = pNPC->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({ pNPC, ETargetType::NPC, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (Vars::Aimbot::General::Target.Value & ToAimAt::BOMB)
	{
		for (const auto& pBomb : g_EntityCache.GetGroup(EGroupType::WORLD_BOMBS))
		{
			Vec3 vPos = pBomb->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);

			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			if (!F::AimbotGlobal.ValidBomb(pBomb))
				continue;

			validTargets.push_back({ pBomb, ETargetType::BOMBS, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	return validTargets;
}

std::vector<Target_t> CAimbotHitscan::SortTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	auto validTargets = GetTargets(pLocal, pWeapon);

	const auto& sortMethod = static_cast<ESortMethod>(Vars::Aimbot::General::TargetSelection.Value);
	F::AimbotGlobal.SortTargets(&validTargets, sortMethod);

	std::vector<Target_t> sortedTargets = {};
	int i = 0; for (auto& target : validTargets)
	{
		i++; if (i > Vars::Aimbot::General::MaxTargets.Value) break;

		sortedTargets.push_back(target);
	}

	F::AimbotGlobal.SortPriority(&sortedTargets);

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
		const int nClassNum = pLocal->m_iClass();

		if (nClassNum == CLASS_SNIPER)
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic && pLocal->IsScoped() ||
				G::CurItemDefIndex == Sniper_m_TheClassic && pWeapon->m_flChargedDamage() > 149.9f)
				bHeadshot = true;
		}
		if (nClassNum == CLASS_SPY)
		{
			if ((G::CurItemDefIndex == Spy_m_TheAmbassador || G::CurItemDefIndex == Spy_m_FestiveAmbassador) && pWeapon->AmbassadorCanHeadshot())
				bHeadshot = true;
		}

		if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 5) && bHeadshot)
		{
			{
				float flBodyMult = 1.f;
				switch (G::CurItemDefIndex)
				{
				case Sniper_m_TheClassic: flBodyMult = 0.9f; break;
				case Sniper_m_TheHitmansHeatmaker: flBodyMult = 0.8f; break;
				case Sniper_m_TheMachina:
				case Sniper_m_ShootingStar: if (pWeapon->m_flChargedDamage() > 149.9f) flBodyMult = 1.15f;
				}
				if (pWeapon->m_flChargedDamage() * flBodyMult >= pTarget->m_iHealth())
					bHeadshot = false;
			}

			if (G::CurItemDefIndex == Spy_m_TheAmbassador || G::CurItemDefIndex == Spy_m_FestiveAmbassador)
			{
				const float flDistTo = pTarget->m_vecOrigin().DistTo(pLocal->m_vecOrigin());
				const int nAmbassadorBodyshotDamage = Math::RemapValClamped(flDistTo, 90, 900, 51, 18);

				if (pTarget->m_iHealth() < (nAmbassadorBodyshotDamage + 2)) // whatever
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

float CAimbotHitscan::GetMaxRange(CBaseCombatWeapon* pWeapon)
{
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_MEDIGUN: return 450.f;
	case TF_WEAPON_MECHANICAL_ARM: return 256.f;
	}

	auto tfWeaponInfo = pWeapon->GetTFWeaponInfo();
	return tfWeaponInfo ? tfWeaponInfo->GetWeaponData(0).m_flRange : 8192.f;
}

int CAimbotHitscan::CanHit(Target_t& target, CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	if (Vars::Aimbot::General::Ignore.Value & UNSIMULATED && G::ChokeMap[target.m_pEntity->GetIndex()] > Vars::Aimbot::General::TickTolerance.Value)
		return false;

	Vec3 vEyePos = pLocal->GetShootPos();
	const float flMaxRange = powf(GetMaxRange(pWeapon), 2.f);

	const model_t* pModel = target.m_pEntity->GetModel();
	if (!pModel) return false;
	const studiohdr_t* pHDR = I::ModelInfoClient->GetStudioModel(pModel);
	if (!pHDR) return false;
	const mstudiohitboxset_t* pSet = pHDR->GetHitboxSet(target.m_pEntity->m_nHitboxSet());
	if (!pSet) return false;

	std::deque<TickRecord> vRecords;
	{
		const auto& pRecords = F::Backtrack.GetRecords(target.m_pEntity);
		if (pRecords && target.m_TargetType == ETargetType::PLAYER)
		{
			vRecords = F::Backtrack.GetValidRecords(pRecords, pLocal);
			if (!Vars::Backtrack::Enabled.Value && !vRecords.empty())
				vRecords = { vRecords.front() };
		}
		else
		{
			matrix3x4 bones[128];
			if (!target.m_pEntity->SetupBones(bones, 128, BONE_USED_BY_ANYTHING, target.m_pEntity->m_flSimulationTime()))
				return false;

			vRecords.push_front({
				target.m_pEntity->m_flSimulationTime(),
				I::GlobalVars->curtime,
				I::GlobalVars->tickcount,
				false,
				*reinterpret_cast<BoneMatrixes*>(&bones),
				target.m_pEntity->m_vecOrigin()
			});
		}
	}

	auto RayToOBB = [](const Vec3& origin, const Vec3& direction, const Vec3& position, const Vec3& min, const Vec3& max, const matrix3x4 orientation) -> bool
		{
			if (Vars::Aimbot::General::AimType.Value != 2)
				return true;

			return Math::RayToOBB(origin, direction, position, min, max, orientation);
		};

	int iReturn = false;
	for (auto& pTick : vRecords)
	{
		if (target.m_TargetType == ETargetType::PLAYER || target.m_TargetType == ETargetType::SENTRY)
		{
			auto boneMatrix = (matrix3x4*)(&pTick.BoneMatrix.BoneMatrix);
			if (!boneMatrix)
				continue;

			std::vector<std::pair<const mstudiobbox_t*, int>> hitboxes;
			{
				if (target.m_TargetType != ETargetType::SENTRY)
				{
					std::vector<std::pair<const mstudiobbox_t*, int>> primary, secondary, tertiary; // dumb
					for (int nHitbox = 0; nHitbox < target.m_pEntity->GetNumOfHitboxes(); nHitbox++)
					{
						if (!IsHitboxValid(nHitbox))
							continue;

						const mstudiobbox_t* pBox = pSet->hitbox(nHitbox);
						if (!pBox) continue;

						switch (GetHitboxPriority(nHitbox, pLocal, pWeapon, target.m_pEntity))
						{
						case 0: primary.push_back({ pBox, nHitbox }); break;
						case 1: secondary.push_back({ pBox, nHitbox }); break;
						case 2: tertiary.push_back({ pBox, nHitbox }); break;
						}
					}
					for (auto& pair : primary) hitboxes.push_back(pair);
					for (auto& pair : secondary) hitboxes.push_back(pair);
					for (auto& pair : tertiary) hitboxes.push_back(pair);
				}
				else
				{
					for (int nHitbox = 0; nHitbox < target.m_pEntity->GetNumOfHitboxes(); nHitbox++)
					{
						const mstudiobbox_t* pBox = pSet->hitbox(nHitbox);
						if (!pBox) continue;

						hitboxes.push_back({ pBox, nHitbox });
					}
					std::sort(hitboxes.begin(), hitboxes.end(), [&](const auto& a, const auto& b) -> bool
						{
							Vec3 aCenter = {}, bCenter = {}, vCenter = target.m_pEntity->GetWorldSpaceCenter();
							Math::VectorTransform({}, boneMatrix[a.first->bone], aCenter);
							Math::VectorTransform({}, boneMatrix[b.first->bone], bCenter);

							return vCenter.DistTo(aCenter) < vCenter.DistTo(bCenter);
						});
				}
			}

			for (auto& pair : hitboxes)
			{
				const float flScale = Vars::Aimbot::Hitscan::PointScale.Value / 100;
				const Vec3 vMins = pair.first->bbmin, vMinsS = vMins * flScale;
				const Vec3 vMaxs = pair.first->bbmax, vMaxsS = vMaxs * flScale;

				std::vector<Vec3> vecPoints = { Vec3() };
				if (flScale > 0.f)
				{
					vecPoints = {
						Vec3(),
						Vec3(vMinsS.x, vMinsS.y, vMaxsS.z),
						Vec3(vMaxsS.x, vMinsS.y, vMaxsS.z),
						Vec3(vMinsS.x, vMaxsS.y, vMaxsS.z),
						Vec3(vMaxsS.x, vMaxsS.y, vMaxsS.z),
						Vec3(vMinsS.x, vMinsS.y, vMinsS.z),
						Vec3(vMaxsS.x, vMinsS.y, vMinsS.z),
						Vec3(vMinsS.x, vMaxsS.y, vMinsS.z),
						Vec3(vMaxsS.x, vMaxsS.y, vMinsS.z)
					};
				}

				for (const auto& point : vecPoints)
				{
					Vec3 vCenter = {}, vTransformed = {};
					Math::VectorTransform({}, boneMatrix[pair.first->bone], vCenter);
					Math::VectorTransform(point, boneMatrix[pair.first->bone], vTransformed);

					if (vEyePos.DistToSqr(vTransformed) > flMaxRange)
						continue;

					auto vAngles = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(pLocal->GetShootPos(), vTransformed));
					Vec3 vForward = {};
					Math::AngleVectors(vAngles, &vForward);

					if (Utils::VisPos(pLocal, target.m_pEntity, vEyePos, vTransformed))
					{
						target.m_vAngleTo = vAngles;
						if (RayToOBB(vEyePos, vForward, vCenter, vMins, vMaxs, boneMatrix[pair.first->bone])) // for the time being, no vischecks against other hitboxes
						{
							bool bWillHit = true;
							if (target.m_TargetType == ETargetType::SENTRY) // point of hit for sentries needs to be within bbox
							{
								const matrix3x4& transform = target.m_pEntity->GetRgflCoordinateFrame();
								const Vec3 vMin = target.m_pEntity->m_vecMins(), vMax = target.m_pEntity->m_vecMaxs();
								bWillHit = RayToOBB(vEyePos, vForward, target.m_pEntity->m_vecOrigin(), vMin, vMax, transform);
							}
							if (bWillHit)
							{
								target.pTick = &pTick;
								target.m_vPos = vTransformed;
								if (target.m_TargetType == ETargetType::PLAYER)
								{
									//if (Vars::Backtrack::Enabled.Value)
										target.ShouldBacktrack = target.m_TargetType == ETargetType::PLAYER;
									target.m_nAimedHitbox = pair.second;
								}
								return true;
							}
						}
						iReturn = 2;
					}
				}
			}
		}
		else
		{
			const float flScale = Vars::Aimbot::Hitscan::PointScale.Value / 100;
			const Vec3 vMins = target.m_pEntity->m_vecMins(), vMinsS = vMins * flScale;
			const Vec3 vMaxs = target.m_pEntity->m_vecMaxs(), vMaxsS = vMaxs * flScale;

			std::vector<Vec3> vecPoints = { Vec3() };
			if (flScale > 0.f)
			{
				vecPoints = {
					Vec3(),
					Vec3(vMinsS.x, vMinsS.y, vMaxsS.z),
					Vec3(vMaxsS.x, vMinsS.y, vMaxsS.z),
					Vec3(vMinsS.x, vMaxsS.y, vMaxsS.z),
					Vec3(vMaxsS.x, vMaxsS.y, vMaxsS.z),
					Vec3(vMinsS.x, vMinsS.y, vMinsS.z),
					Vec3(vMaxsS.x, vMinsS.y, vMinsS.z),
					Vec3(vMinsS.x, vMaxsS.y, vMinsS.z),
					Vec3(vMaxsS.x, vMaxsS.y, vMinsS.z)
				};
			}

			const matrix3x4& transform = target.m_pEntity->GetRgflCoordinateFrame();
			for (const auto& point : vecPoints)
			{
				Vec3 vTransformed = target.m_pEntity->GetWorldSpaceCenter() + point;

				if (vEyePos.DistToSqr(vTransformed) > flMaxRange)
					continue;

				auto vAngles = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(pLocal->GetShootPos(), vTransformed));
				Vec3 vForward = {};
				Math::AngleVectors(vAngles, &vForward);

				if (Utils::VisPos(pLocal, target.m_pEntity, vEyePos, vTransformed))
				{
					target.m_vAngleTo = vAngles;
					if (RayToOBB(vEyePos, vForward, target.m_pEntity->m_vecOrigin(), vMins, vMaxs, transform)) // for the time being, no vischecks against other hitboxes
					{
						target.pTick = &pTick;
						target.m_vPos = vTransformed;
						return true;
					}
					iReturn = 2;
				}
			}
		}
	}

	return iReturn;
}



/* Returns whether AutoShoot should fire */
bool CAimbotHitscan::ShouldFire(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, const CUserCmd* pCmd, const Target_t& target)
{
	if (!Vars::Aimbot::General::AutoShoot.Value) return false;

	switch (pLocal->m_iClass())
	{
	case CLASS_SNIPER:
	{
		const bool bIsScoped = pLocal->IsScoped();

		if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 1))
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic
				&& G::CurItemDefIndex != Sniper_m_TheSydneySleeper
				&& !G::CanHeadShot && bIsScoped)
			{
				return false;
			}
		}

		if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 2) && (bIsScoped || G::CurItemDefIndex == Sniper_m_TheClassic))
		{
			const int nHealth = target.m_pEntity->m_iHealth();
			const bool bIsCritBoosted = pLocal->IsCritBoosted();

			if (target.m_nAimedHitbox == HITBOX_HEAD && G::CurItemDefIndex != Sniper_m_TheSydneySleeper && (G::CurItemDefIndex != Sniper_m_TheClassic || G::CurItemDefIndex == Sniper_m_TheClassic && pWeapon->m_flChargedDamage() > 149.9f))
			{
				if (nHealth > 150)
				{
					const float flDamage = Math::RemapValClamped(pWeapon->m_flChargedDamage(), 0.0f, 150.0f, 0.0f, 450.0f);
					const int nDamage = static_cast<int>(flDamage);

					if (nDamage < nHealth && nDamage != 450)
						return false;
				}

				else
				{
					if (!bIsCritBoosted && !G::CanHeadShot)
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
					case Sniper_m_ShootingStar: if (pWeapon->m_flChargedDamage() > 149.9f) flBodyMult = 1.15f;
					}

					const float flMax = 150.0f * flCritMult * flBodyMult;
					const int nDamage = static_cast<int>(pWeapon->m_flChargedDamage() * flCritMult * flBodyMult);

					if (nDamage < target.m_pEntity->m_iHealth() && nDamage != static_cast<int>(flMax))
						return false;
				}
			}
		}

		break;
	}
	case CLASS_SPY:
		if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 1) && !pWeapon->AmbassadorCanHeadshot())
			return false;

		break;
	}

	return true;
}

bool CAimbotHitscan::IsAttacking(const CUserCmd* pCmd, CBaseCombatWeapon* pWeapon)
{
	return ((pCmd->buttons & IN_ATTACK) && G::CanPrimaryAttack);
}

// assume angle calculated outside with other overload
void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle)
{
	if (Vars::Aimbot::General::AimType.Value != 3)
	{
		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(pCmd->viewangles); // remove these if uncommenting l126 of createmove
	}
	else if (G::IsAttacking)
	{
		Utils::FixMovement(pCmd, vAngle);
		pCmd->viewangles = vAngle;
		G::SilentAngles = true;
	}
}

Vec3 CAimbotHitscan::Aim(Vec3 vCurAngle, Vec3 vToAngle, int iMethod)
{
	Vec3 vReturn = {};

	vToAngle -= G::PunchAngles;
	Math::ClampAngles(vToAngle);

	switch (iMethod)
	{
	case 1: // Plain
	case 3: // Silent
		vReturn = vToAngle;
		break;
	case 2: //Smooth
	{
		auto shortDist = [](const float flAngleA, const float flAngleB)
			{
				const float flDelta = fmodf((flAngleA - flAngleB), 360.f);
				return fmodf(2 * flDelta, 360.f) - flDelta;
			};
		const float t = 1.f - Vars::Aimbot::General::Smoothing.Value / 100.f;
		vReturn.x = vCurAngle.x - shortDist(vCurAngle.x, vToAngle.x) * t;
		vReturn.y = vCurAngle.y - shortDist(vCurAngle.y, vToAngle.y) * t;
		break;
	}
	}

	return vReturn;
}

void CAimbotHitscan::Run(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	const bool bAutomatic = pWeapon->IsStreamingWeapon(), bKeepFiring = bAutomatic && G::LastUserCmd->buttons & IN_ATTACK;
	if (bKeepFiring && !G::CanPrimaryAttack && Vars::Aimbot::General::AimType.Value)
		pCmd->buttons |= IN_ATTACK;

	const int nWeaponID = pWeapon->GetWeaponID();
	if (!Vars::Aimbot::General::AimType.Value || !G::CanPrimaryAttack && Vars::Aimbot::General::AimType.Value == 3/* && !G::DoubleTap*/ && (nWeaponID == TF_WEAPON_MINIGUN ? pWeapon->GetMinigunState() == AC_STATE_FIRING || pWeapon->GetMinigunState() == AC_STATE_SPINNING : true))
	{
		bLastTickHeld = false;
		return;
	}

	switch (nWeaponID)
	{
	case TF_WEAPON_MINIGUN:
	{
		pCmd->buttons |= IN_ATTACK2;
		if (pWeapon->GetMinigunState() != AC_STATE_FIRING && pWeapon->GetMinigunState() != AC_STATE_SPINNING)
			return;
		break;
	}
	case TF_WEAPON_SNIPERRIFLE:
	case TF_WEAPON_SNIPERRIFLE_DECAP:
	{
		const bool bScoped = pLocal->IsScoped();

		if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 4) && !bScoped)
		{
			pCmd->buttons |= IN_ATTACK2;
			return;
		}

		if ((Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 3) || G::CurItemDefIndex == Sniper_m_TheMachina || G::CurItemDefIndex == Sniper_m_ShootingStar) && !bScoped)
			return;

		break;
	}
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
	{
		const bool bHeld = bLastTickHeld;
		bLastTickHeld = true;
		pCmd->buttons |= IN_ATTACK;
		if (!bHeld)
			return;
	}
	}

	auto targets = SortTargets(pLocal, pWeapon);
	if (targets.empty())
		return;

	for (auto& target : targets)
	{
		const auto iResult = CanHit(target, pLocal, pWeapon);
		if (!iResult) continue;
		if (iResult == 2)
		{
			Aim(pCmd, target.m_vAngleTo);
			break;
		}

		G::CurrentTarget = { target.m_pEntity->GetIndex(), I::GlobalVars->tickcount };
		if (Vars::Aimbot::General::AimType.Value == 3)
			G::AimPos = target.m_vPos;

		bool bShouldFire = ShouldFire(pLocal, pWeapon, pCmd, target);

		if (bShouldFire)
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic)
				pCmd->buttons |= IN_ATTACK;
			else
			{
				pCmd->buttons &= ~IN_ATTACK;
				bLastTickHeld = false;
			}

			if (G::CurItemDefIndex == Engi_s_TheWrangler || G::CurItemDefIndex == Engi_s_FestiveWrangler)
				pCmd->buttons |= IN_ATTACK2;

			/*
			// Get circular gaussian spread. Under some cases we fire a bullet right down the crosshair:
			//	- The first bullet of a spread weapon (except for rapid fire spread weapons like the minigun)
			//	- The first bullet of a non-spread weapon if it's been >1.25 second since firing
			bool bFirePerfect = false;
			if ( iBullet == 0 && pWpn )
			{
				float flTimeSinceLastShot = (gpGlobals->curtime - pWpn->m_flLastFireTime );
				if ( nBulletsPerShot > 1 && flTimeSinceLastShot > 0.25 )
					bFirePerfect = true;
				else if ( nBulletsPerShot == 1 && flTimeSinceLastShot > 1.25 )
					bFirePerfect = true;
			}
			*/

			if (Vars::Aimbot::Hitscan::Modifiers.Value & (1 << 0) && nWeaponID == TF_WEAPON_MINIGUN && !pLocal->IsPrecisionRune())
			{
				if (pLocal->GetShootPos().DistTo(target.m_vPos) > Vars::Aimbot::Hitscan::TapFireDist.Value && pWeapon->GetWeaponSpread() != 0.f)
				{
					const float flTimeSinceLastShot = (pLocal->m_nTickBase() * TICK_INTERVAL) - pWeapon->m_flLastFireTime();

					if (pWeapon->GetWeaponData().m_nBulletsPerShot > 1)
					{
						if (flTimeSinceLastShot <= 0.25f)
							pCmd->buttons &= ~IN_ATTACK;
					}
					else if (flTimeSinceLastShot <= 1.25f)
						pCmd->buttons &= ~IN_ATTACK;
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

				if (!pWeapon->IsInReload())
				{
					if (Vars::Visuals::Bullet::BulletTracer.Value)
					{
						G::BulletsStorage.clear();
						G::BulletsStorage.push_back({ {pLocal->GetShootPos(), target.m_vPos}, I::GlobalVars->curtime + 5.f, Vars::Colors::BulletTracer.Value, true });
					}
					if (Vars::Visuals::Hitbox::ShowHitboxes.Value)
					{
						G::BoxesStorage.clear();
						auto vBoxes = F::Visuals.GetHitboxes((matrix3x4*)(&(*target.pTick).BoneMatrix.BoneMatrix), target.m_pEntity);
						G::BoxesStorage.insert(G::BoxesStorage.end(), vBoxes.begin(), vBoxes.end());
					}
				}
			}
		}

		Aim(pCmd, target.m_vAngleTo);
		break;
	}
}
