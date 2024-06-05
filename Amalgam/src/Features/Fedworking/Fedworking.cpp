#include "Fedworking.h"

#include "../../Features/Players/PlayerUtils.h"

enum MessageType
{
	None,
	Marker,	// [ Type, X-Pos, Y-Pos, Z-Pos, Player-IDX ]
	ESP,	// [ Type, X-Pos, Y-Pos, Z-Pos, Player-IDX ]
	Pong
};

void CFedworking::HandleMessage(const char* pMessage)
{
	std::string encMsg(pMessage);
	encMsg.erase(0, 4); // Remove FED@ prefix

	if (encMsg == "TAGS")
	{
		const auto& playerCache = F::PlayerUtils.vPlayerCache;

		for (const auto& player : playerCache)
		{
			for (const auto& sTag : F::PlayerUtils.mPlayerTags[player.FriendsID])
			{
				PriorityLabel_t plTag;
				if (!F::PlayerUtils.GetTag(sTag, &plTag))
					continue;
				SendMessageFed("Player " + (std::string)player.Name + (std::string)" has tag " + (std::string)sTag.c_str());
			}
		}
	}
	
}
void CFedworking::SendMessageFed(const std::string& pData)
{
	if (pData.size() <= 253)
	{
		std::string cmd = "tf_party_chat \"|";
		cmd.append(pData);
		cmd.append("\"");
		I::EngineClient->ClientCmd_Unrestricted(cmd.c_str());
	}
	else
	{
		ConsoleLog("Failed to send message! The message was too long.");
	}
}


void CFedworking::ConsoleLog(const std::string& pMessage)
{
	std::string consoleMsg = "[Fedworking] ";
	consoleMsg.append(pMessage);
	consoleMsg.append("\n");
	I::CVar->ConsoleColorPrintf({ 225, 177, 44, 255 }, consoleMsg.c_str());
}
