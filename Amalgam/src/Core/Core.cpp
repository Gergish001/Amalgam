#include "Core.h"

#include "../SDK/SDK.h"
#include "../Features/Visuals/Materials/Materials.h"
#include "../Features/Configs/Configs.h"
#include "../Features/Commands/Commands.h"
#include "../Features/ImGui/Menu/Menu.h"
#include "../Features/Visuals/Visuals.h"

void CCore::Load()
{
	// Check the DirectX version

	U::Signatures.Initialize();
	U::Interfaces.Initialize();
	U::Hooks.Initialize();
	U::ConVars.Initialize();
	F::Materials.LoadMaterials();
	F::Commands.Initialize();

	F::Configs.LoadConfig(F::Configs.sCurrentConfig, false);
	F::Menu.ConfigLoaded = true;

	SDK::Output("Amalgam", "Loaded", { 175, 150, 255, 255 });
}
