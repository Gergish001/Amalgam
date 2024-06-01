#include "CritHack.h"
#include "../TickHandler/TickHandler.h"
#include "../Aimbot/AutoRocketJump/AutoRocketJump.h"

#define WEAPON_RANDOM_RANGE				10000
#define TF_DAMAGE_CRIT_MULTIPLIER		3.0f
#define TF_DAMAGE_CRIT_CHANCE			0.02f
#define TF_DAMAGE_CRIT_CHANCE_RAPID		0.02f
#define TF_DAMAGE_CRIT_CHANCE_MELEE		0.15f
#define TF_DAMAGE_CRIT_DURATION_RAPID	2.0f

void CCritHack::Fill(CBaseEntity* pLocal, const CUserCmd* pCmd, int n)
{
	static int iStart = pCmd->command_number;

	for (auto& [iSlot, tStorage] : Storage)
	{
		for (auto it = tStorage.CritCommands.begin(); it != tStorage.CritCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.CritCommands.erase(it);
			else
				++it;
		}
		for (auto it = tStorage.SkipCommands.begin(); it != tStorage.SkipCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.SkipCommands.erase(it);
			else
				++it;
		}

		for (int i = 0; i < n; i++)
		{
			if (tStorage.CritCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = iStart + i;
			if (IsCritCommand(iSlot, tStorage.EntIndex, iCmdNum))
				tStorage.CritCommands.push_back(iCmdNum);
		}
		for (int i = 0; i < n; i++)
		{
			if (tStorage.SkipCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = iStart + i;
			if (IsCritCommand(iSlot, tStorage.EntIndex, iCmdNum, false))
				tStorage.SkipCommands.push_back(iCmdNum);
		}
	}

	iStart += n;
}

int CCritHack::FirstGoodCommand(std::deque<int>& vCommands, const CUserCmd* pCmd)
{
	for (const auto& cmd : vCommands)
		return cmd;

	return 0;
}



bool CCritHack::IsCritCommand(int iSlot, int iIndex, const i32 command_number, const bool bCrit, const bool bSafe)
{
	const auto uSeed = MD5_PseudoRandom(command_number) & 0x7FFFFFFF;
	Utils::RandomSeed(DecryptOrEncryptSeed(iSlot, iIndex, uSeed));
	const int iRandom = Utils::RandomInt(0, WEAPON_RANDOM_RANGE - 1);

	if (bSafe)
		return bCrit ? iRandom < 100 : !(iRandom < 6000);
	else
	{
		const int iRange = (CritChance - 0.1f) * WEAPON_RANDOM_RANGE;
		return bCrit ? iRandom < iRange : !(iRandom < iRange);
	}
}

u32 CCritHack::DecryptOrEncryptSeed(int iSlot, int iIndex, u32 uSeed)
{
	int iLeft = iSlot == SLOT_MELEE ? 8 : 0;
	unsigned int iMask = iIndex << (iLeft + 8) | I::EngineClient->GetLocalPlayer() << iLeft;
	return iMask ^ uSeed;
}



void CCritHack::GetTotalCrits(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	static auto bucketCap = g_ConVars.FindVar("tf_weapon_criticals_bucket_cap");
	const float flBucketCap = bucketCap ? bucketCap->GetFloat() : 1000.f;

	auto tfWeaponInfo = pWeapon->GetTFWeaponInfo();
	if (!tfWeaponInfo)
		return;

	const int iSlot = pWeapon->GetSlot();
	auto& tWeaponData = tfWeaponInfo->GetWeaponData(0);

	float flDamage = tWeaponData.m_nDamage;
	flDamage = Utils::AttribHookValue(flDamage, "mult_dmg", pWeapon);
	int nProjectilesPerShot = tWeaponData.m_nBulletsPerShot;
	if (nProjectilesPerShot >= 1)
		nProjectilesPerShot = Utils::AttribHookValue(nProjectilesPerShot, "mult_bullets_per_shot", pWeapon);
	else
		nProjectilesPerShot = 1;
	Storage[iSlot].Damage = flDamage *= nProjectilesPerShot;

	if (pWeapon->IsRapidFire())
	{
		flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / tWeaponData.m_flTimeFireDelay;

		if (flDamage * TF_DAMAGE_CRIT_MULTIPLIER > flBucketCap)
			flDamage = flBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
	}

	float flMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped((float)(pWeapon->CritSeedRequests() + 1) / (float)(pWeapon->CritChecks() + 1), 0.1f, 1.f, 1.f, 3.f);
	Storage[iSlot].Cost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER * flMult;

	if (flBucketCap)
		Storage[iSlot].PotentialCrits = static_cast<unsigned int>((flBucketCap - Storage[iSlot].Damage) / (3 * flDamage / (iSlot == SLOT_MELEE ? 2 : 1) - Storage[iSlot].Damage));

	int iCrits = 0;
	{
		int shots = pWeapon->CritChecks(), crits = pWeapon->CritSeedRequests();
		float bucket = pWeapon->CritTokenBucket(), flCost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER;
		const int iAttempts = std::min(Storage[iSlot].PotentialCrits + 1, 100);
		for (int i = 0; i < iAttempts; i++)
		{
			shots++; crits++;

			flMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped((float)crits / (float)shots, 0.1f, 1.f, 1.f, 3.f);
			bucket = std::min(bucket + Storage[iSlot].Damage, flBucketCap) - flCost * flMult;
			if (bucket < 0.f)
				break;

			iCrits++;
		}
	}

	if (iCrits <= 0)
	{
		iCrits = 0;

		int shots = pWeapon->CritChecks() + 1, crits = pWeapon->CritSeedRequests() + 1;
		float bucket = std::min(pWeapon->CritTokenBucket() + Storage[iSlot].Damage, flBucketCap), flCost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER;
		for (int i = 0; i < 100; i++)
		{
			iCrits--;
			if (!pWeapon->IsRapidFire() || !(i % int(tWeaponData.m_flTimeFireDelay / TICK_INTERVAL)))
				shots++;

			flMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped((float)crits / (float)shots, 0.1f, 1.f, 1.f, 3.f);
			bucket = std::min(bucket + Storage[iSlot].Damage, flBucketCap);
			if (bucket >= flCost * flMult)
				break;
		}
	}

	Storage[iSlot].AvailableCrits = iCrits;
}

void CCritHack::CanFireCritical(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	CritBanned = false;

	if (pWeapon->GetSlot() == SLOT_MELEE)
		CritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * pLocal->GetCritMult();
	else if (pWeapon->IsRapidFire())
	{
		CritChance = TF_DAMAGE_CRIT_CHANCE_RAPID * pLocal->GetCritMult();
		float flNonCritDuration = (TF_DAMAGE_CRIT_DURATION_RAPID / CritChance) - TF_DAMAGE_CRIT_DURATION_RAPID;
		CritChance = 1.f / flNonCritDuration;
	}
	else
		CritChance = TF_DAMAGE_CRIT_CHANCE * pLocal->GetCritMult();
	CritChance = Utils::AttribHookValue(CritChance, "mult_crit_chance", pWeapon) + 0.1f;

	if (CritDamage == 0.f || pWeapon->GetSlot() == SLOT_MELEE)
		return;

	const auto divCritDamage = CritDamage / TF_DAMAGE_CRIT_MULTIPLIER;
	const auto ratio = divCritDamage / (AllDamage - 2 * divCritDamage);

	CritBanned = ratio >= CritChance;
}

void CCritHack::GetDamageTilUnban(CBaseEntity* pLocal)
{
	DamageTilUnban = 0;

	if (!CritBanned)
		return;

	const auto divCritDamage = CritDamage / TF_DAMAGE_CRIT_MULTIPLIER;

	DamageTilUnban = divCritDamage / CritChance + 2 * divCritDamage - AllDamage;
}

bool CCritHack::WeaponCanCrit(CBaseCombatWeapon* pWeapon)
{
	if (Utils::AttribHookValue(1.f, "mult_crit_chance", pWeapon) <= 0.f)
		return false;

	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_PDA:
	case TF_WEAPON_PDA_ENGINEER_BUILD:
	case TF_WEAPON_PDA_ENGINEER_DESTROY:
	case TF_WEAPON_PDA_SPY:
	case TF_WEAPON_PDA_SPY_BUILD:
	case TF_WEAPON_BUILDER:
	case TF_WEAPON_INVIS:
	case TF_WEAPON_GRAPPLINGHOOK:
	case TF_WEAPON_JAR_MILK:
	case TF_WEAPON_LUNCHBOX:
	case TF_WEAPON_BUFF_ITEM:
	case TF_WEAPON_FLAME_BALL:
	case TF_WEAPON_ROCKETPACK:
	case TF_WEAPON_JAR_GAS:
	case TF_WEAPON_LASER_POINTER:
	case TF_WEAPON_MEDIGUN:
	case TF_WEAPON_SNIPERRIFLE:
	case TF_WEAPON_SNIPERRIFLE_DECAP:
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
	case TF_WEAPON_COMPOUND_BOW:
	case TF_WEAPON_JAR:
	case TF_WEAPON_KNIFE:
		return false;
	}

	return true;
}



void CCritHack::ResetWeapons(CBaseEntity* pLocal)
{
	std::unordered_map<int, bool> mWeapons = {};
	const auto hWeapons = pLocal->GetMyWeapons();
	if (!hWeapons)
		return;

	for (int i = 0; hWeapons[i]; i++)
	{
		if (Utils::HandleToIDX(hWeapons[i]) < 0 || Utils::HandleToIDX(hWeapons[i]) >= 2048)
			continue;

		if (const auto pWeapon = reinterpret_cast<CBaseCombatWeapon*>(I::ClientEntityList->GetClientEntityFromHandle(hWeapons[i])))
		{
			const int iSlot = pWeapon->GetSlot();
			const int iDefIndex = pWeapon->m_iItemDefinitionIndex();
			mWeapons[iSlot] = true;

			if (Storage[iSlot].DefIndex == iDefIndex)
				continue;

			Storage[iSlot] = {};
			Storage[iSlot].EntIndex = pWeapon->GetIndex();
			Storage[iSlot].DefIndex = iDefIndex;

			Utils::ConLog("Crithack", std::format("Resetting weapon {}", iDefIndex).c_str(), { 0, 255, 255, 255 }, Vars::Debug::Logging.Value);
		}
	}

	for (auto& [iSlot, _] : Storage)
	{
		if (!mWeapons[iSlot])
			Storage[iSlot] = {};
	}
}

void CCritHack::Reset()
{
	Storage = {};

	CritDamage = 0;
	AllDamage = 0;

	CritBanned = false;
	DamageTilUnban = 0;
	CritChance = 0.f;
	ProtectData = false;

	Utils::ConLog("Crithack", "Resetting all", { 0, 255, 255, 255 }, Vars::Debug::Logging.Value);
}



void CCritHack::Run(CUserCmd* pCmd)
{
	const auto& pLocal = g_EntityCache.GetLocal();
	const auto& pWeapon = g_EntityCache.GetWeapon();
	if (!pLocal || !pWeapon || !pLocal->IsAlive())
		return;

	ResetWeapons(pLocal);
	GetTotalCrits(pLocal, pWeapon);
	CanFireCritical(pLocal, pWeapon);
	GetDamageTilUnban(pLocal);
	Fill(pWeapon, pCmd, 15);
	if (pLocal->IsCritBoosted() || !WeaponCanCrit(pWeapon))
		return;

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && pCmd->buttons & IN_ATTACK)
		pCmd->buttons &= ~IN_ATTACK2;

	bool bAttacking = G::IsAttacking && (G::RocketJumping ? F::AutoRocketJump.iFrame < 1 : true);
	if (G::CurWeaponType == EWeaponType::MELEE)
	{
		bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK;
		if (!bAttacking && pWeapon->GetWeaponID() == TF_WEAPON_FISTS)
			bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK2;
	}
	if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER || pWeapon->GetWeaponID() == TF_WEAPON_STICKY_BALL_LAUNCHER || pWeapon->GetWeaponID() == TF_WEAPON_GRENADE_STICKY_BALL
		|| pWeapon->GetWeaponID() == TF_WEAPON_CANNON)
	{
		static float flBegin = -1.f;
		if (pCmd->buttons& IN_ATTACK&& flBegin < 0.f && G::CanPrimaryAttack)
			flBegin = I::GlobalVars->curtime;

		const float flCharge = flBegin > 0.f ? I::GlobalVars->curtime - flBegin : 0.f;
		const float flAmount = pWeapon->GetWeaponID() != TF_WEAPON_CANNON
			? Math::RemapValClamped(flCharge, 0.f, Utils::AttribHookValue(4.f, "stickybomb_charge_rate", pWeapon), 0.f, 1.f)
			: Math::RemapValClamped(flCharge, 0.f, Utils::AttribHookValue(0.f, "grenade_launcher_mortar_mode", pWeapon), 0.f, 1.f);

		const bool bUnheld = !(pCmd->buttons & IN_ATTACK) && flAmount > 0.f;
		const bool bSwapping = pCmd->weaponselect;
		const bool bFull = flAmount == 1.f; // possibly add exception to skip when full with cannon

		bAttacking = (bUnheld || bFull) && !bSwapping;
		if (bAttacking || !G::CanPrimaryAttack || bSwapping)
			flBegin = -1.f;
	}
	if ((pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN || pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER) && !(G::LastUserCmd->buttons & IN_ATTACK)) // silly
		bAttacking = false;

	const int iSlot = pWeapon->GetSlot();
	if (Storage[iSlot].StreamWait <= I::GlobalVars->tickcount - 1)
		Storage[pWeapon->GetSlot()].StreamWait = -1;

	const bool bRapidFire = pWeapon->IsRapidFire();
	const bool bStreamWait = Storage[iSlot].StreamWait > 0;
	//const bool bStreamWait = pWeapon->IsRapidFire() && pWeapon->LastRapidfireCritCheckTime() + 1.f > TICK_INTERVAL * pLocal->m_nTickBase();

	const int closestCrit = FirstGoodCommand(Storage[iSlot].CritCommands, pCmd);
	const int closestSkip = FirstGoodCommand(Storage[iSlot].SkipCommands, pCmd);

	const bool bPressed = Vars::CritHack::ForceCrits.Value || pWeapon->GetSlot() == SLOT_MELEE && Vars::CritHack::AlwaysMelee.Value;

	static bool bFirstTimePredicted = false;
	if (!I::ClientState->chokedcommands)
		bFirstTimePredicted = false;
	if (bAttacking && !pWeapon->IsInReload() && !bFirstTimePredicted)
	{
		bFirstTimePredicted = true;

		const bool bCanCrit = Storage[pWeapon->GetSlot()].AvailableCrits > 0 && (!CritBanned || pWeapon->GetSlot() == SLOT_MELEE) && !bStreamWait;
		static auto tf_weapon_criticals = g_ConVars.FindVar("tf_weapon_criticals");
		const bool bWeaponCriticals = tf_weapon_criticals ? tf_weapon_criticals->GetBool() : true;
		if (I::EngineClient->IsInGame() && bWeaponCriticals)
		{
			if (bPressed && closestCrit && bCanCrit)
				pCmd->command_number = closestCrit;
			else if (Vars::CritHack::AvoidRandom.Value && closestSkip)
				pCmd->command_number = closestSkip;
		}

		if (bRapidFire && !bStreamWait)
			Storage[pWeapon->GetSlot()].StreamWait = I::GlobalVars->tickcount + 1 / TICK_INTERVAL;
	}
	else if (Vars::CritHack::AvoidRandom.Value && closestSkip)
		pCmd->command_number = closestSkip;

	//if (pCmd->command_number == closestCrit || pCmd->command_number == closestSkip)
	WishRandomSeed = MD5_PseudoRandom(pCmd->command_number) & 0x7FFFFFFF;

	if (pCmd->command_number == closestCrit)
		Storage[iSlot].CritCommands.pop_front();
	else if (pCmd->command_number == closestSkip)
		Storage[iSlot].SkipCommands.pop_front();
}

bool CCritHack::CalcIsAttackCriticalHandler(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	if (!I::Prediction->m_bFirstTimePredicted || !pLocal || !pWeapon)
		return false;

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN || pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER)
	{
		static int iOldAmmo = pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType());
		const int iNewAmmo = pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType());

		const bool bFired = iOldAmmo != iNewAmmo;

		iOldAmmo = iNewAmmo;

		if (!bFired)
			return false;
	}

	if (WishRandomSeed)
	{
		*I::RandomSeed = WishRandomSeed;
		WishRandomSeed = 0;
	}

	return true;
}

void CCritHack::Event(CGameEvent* pEvent, FNV1A_t uNameHash)
{
	if (uNameHash == FNV1A::HashConst("player_hurt"))
	{
		const auto& pLocal = g_EntityCache.GetLocal();
		if (!pLocal)
			return;

		const auto attacked = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		const auto attacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
		auto	   crit = pEvent->GetInt("crit");
		auto       damage = pEvent->GetInt("damageamount");
		const auto health = pEvent->GetInt("health");
		const auto weaponid = pEvent->GetInt("weaponid");

		if (attacker == pLocal->GetIndex() && attacked != attacker)
		{
			const auto& pWeapon = g_EntityCache.GetWeapon();
			if (!pWeapon || pWeapon->GetWeaponID() == weaponid && pWeapon->GetSlot() == SLOT_MELEE || pLocal->InCond(TF_COND_CRITBOOSTED))
				return;

			AllDamage += damage;
			if (crit)
				CritDamage += damage;
		}
	}
	else if (uNameHash == FNV1A::HashConst("teamplay_round_start"))
	{
		CritDamage = 0.f;
		AllDamage = 0.f;
	}
	else if (uNameHash == FNV1A::HashConst("client_beginconnect") || uNameHash == FNV1A::HashConst("client_disconnect") ||
		uNameHash == FNV1A::HashConst("game_newmap"))
	{
		Reset();
	}
	else if (uNameHash == FNV1A::HashConst("player_changeclass"))
	{
		const bool bLocal = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid")) == I::EngineClient->GetLocalPlayer();
		if (bLocal)
			Storage = {};
	}
}

void CCritHack::Draw()
{
	static auto tf_weapon_criticals = g_ConVars.FindVar("tf_weapon_criticals");
	const bool bWeaponCriticals = tf_weapon_criticals ? tf_weapon_criticals->GetBool() : true;
	if (!(Vars::Menu::Indicators.Value & (1 << 1)) || !I::EngineClient->IsInGame() || !bWeaponCriticals || !G::CurrentUserCmd)
		return;

	const auto& pLocal = g_EntityCache.GetLocal();
	if (!pLocal || !pLocal->IsAlive() || pLocal->IsAGhost())
		return;

	const auto& pWeapon = pLocal->GetActiveWeapon();
	if (!pWeapon)
		return;

	int x = Vars::Menu::CritsDisplay.Value.x;
	int y = Vars::Menu::CritsDisplay.Value.y + 8;

	const auto& fFont = g_Draw.GetFont(FONT_INDICATORS);

	EAlign align = ALIGN_TOP;
	if (x <= (100 + 50 * Vars::Menu::DPI.Value))
	{
		x -= 42 * Vars::Menu::DPI.Value;
		align = ALIGN_TOPLEFT;
	}
	else if (x >= g_ScreenSize.w - (100 + 50 * Vars::Menu::DPI.Value))
	{
		x += 42 * Vars::Menu::DPI.Value;
		align = ALIGN_TOPRIGHT;
	}

	if (WeaponCanCrit(pWeapon))
	{
		const auto iSlot = pWeapon->GetSlot();
		const auto bRapidFire = pWeapon->IsRapidFire();

		if (Storage[iSlot].Damage > 0)
		{
			if (pLocal->IsCritBoosted())
				g_Draw.String(fFont, x, y, { 100, 255, 255, 255 }, align, "Crit Boosted");
			else if (pWeapon->CritTime() > TICK_INTERVAL * pLocal->m_nTickBase())
			{
				const float flTime = pWeapon->CritTime() - TICK_INTERVAL * pLocal->m_nTickBase();
				g_Draw.String(fFont, x, y, { 100, 255, 255, 255 }, align, std::format("Streaming crits {:.1f}s", flTime).c_str());
			}
			else if (!CritBanned)
			{
				if (Storage[iSlot].AvailableCrits > 0)
				{
					if (bRapidFire && Storage[iSlot].StreamWait > 0)
					{
						//const float flTime = std::max((TICKS_TO_TIME(Storage[iSlot].StreamWait - pLocal->m_nTickBase())), 0.f);
						const float flTime = pWeapon->LastRapidfireCritCheckTime() + 1.f - TICK_INTERVAL * pLocal->m_nTickBase();
						g_Draw.String(fFont, x, y, Vars::Menu::Theme::Active.Value, align, std::format("Wait {:.1f}s", flTime).c_str());
					}
					else
						g_Draw.String(fFont, x, y, { 150, 255, 150, 255 }, align, "Crit Ready");
				}
				else
				{
					const int shots = -Storage[iSlot].AvailableCrits;
					g_Draw.String(fFont, x, y, { 255, 150, 150, 255 }, align, shots == 1 ? std::format("Crit in {} shot", shots).c_str() : std::format("Crit in {}{} shots", shots, shots == 100 ? "+" : "").c_str());
				}
			}
			else
				g_Draw.String(fFont, x, y, { 255, 150, 150, 255 }, align, std::format("Deal {} damage", DamageTilUnban).c_str());

			g_Draw.String(fFont, x, y + fFont.nTall + 1, Vars::Menu::Theme::Active.Value, align, std::format("{} / {} potential crits", std::max(Storage[iSlot].AvailableCrits, 0), Storage[iSlot].PotentialCrits).c_str());
		}
		else
			g_Draw.String(fFont, x, y, Vars::Menu::Theme::Active.Value, align, "Calculating");

		if (Vars::Debug::Info.Value)
		{
			g_Draw.String(fFont, x, y + fFont.nTall * 3, { 255, 255, 255, 255 }, align, std::format("AllDamage: {}, CritDamage: {}", AllDamage, CritDamage).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 4, { 255, 255, 255, 255 }, align, std::format("Bucket: {}", pWeapon->CritTokenBucket()).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 5, { 255, 255, 255, 255 }, align, std::format("Damage: {}, Cost: {}", Storage[iSlot].Damage, Storage[iSlot].Cost).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 6, { 255, 255, 255, 255 }, align, std::format("Shots: {}, Crits: {}", pWeapon->CritChecks(), pWeapon->CritSeedRequests()).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 7, { 255, 255, 255, 255 }, align, std::format("CritBanned: {}, DamageTilUnban: {}", CritBanned, DamageTilUnban).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 8, { 255, 255, 255, 255 }, align, std::format("CritChance: {:.2f} ({:.2f})", CritChance, CritChance - 0.1f).c_str());
			g_Draw.String(fFont, x, y + fFont.nTall * 9, { 255, 255, 255, 255 }, align, std::format("Force: {}, Skip: {}", Storage[iSlot].CritCommands.size(), Storage[iSlot].SkipCommands.size()).c_str());

			g_Draw.String(fFont, x, y + fFont.nTall * 11, { 255, 255, 255, 255 }, align, L"G::CanPrimaryAttack %d (%d)", G::CanPrimaryAttack, I::GlobalVars->tickcount % 66);
			g_Draw.String(fFont, x, y + fFont.nTall * 12, { 255, 255, 255, 255 }, align, L"G::IsAttacking %d", G::IsAttacking);
		}
	}
}
