#pragma once
#include "../../Utils/Feature/Feature.h"
#include <string>
class CFedworking
{
private:
	void ConsoleLog(const std::string& pMessage);

public:
	void HandleMessage(const char* pMessage);
	void SendMessageFed(const std::string& pData);
};

ADD_FEATURE(CFedworking, Fedworking)
