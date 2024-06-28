#pragma once
#include "../Utils/Feature/Feature.h"

class CCore
{
public:
	void Load();

	bool bUnload = false;
};

ADD_FEATURE_CUSTOM(CCore, Core, U);
