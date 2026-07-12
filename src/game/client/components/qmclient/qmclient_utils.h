// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_UTILS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_UTILS_H

#include <engine/shared/client_brand.h>

#include <string>
#include <vector>

typedef struct _json_value json_value;

struct SQmClientServerDistribution
{
	std::string m_ServerAddress;
	int m_UserCount = 0;
	int m_DummyCount = 0;
};

struct SQmClientRecognitionMark
{
	std::string m_Name;
	bool m_FootParticlesEnabled = false;
	bool m_RemoteParticlesEnabled = false;
	bool m_VoiceSupported = false;
	EClientBrand m_ClientBrand = EClientBrand::QM;
	std::string m_Qid;
};

struct SQmClientUsersParseResult
{
	bool m_Parsed = false;
	std::vector<SQmClientServerDistribution> m_vServerDistribution;
	std::vector<SQmClientRecognitionMark> m_vLocalServerMarks;
	int m_OnlineUserCount = 0;
	int m_OnlineDummyCount = 0;
};

bool ParseQmClientUsersJson(const json_value *pRoot, const char *pServerAddress, SQmClientUsersParseResult &OutResult);

#endif
