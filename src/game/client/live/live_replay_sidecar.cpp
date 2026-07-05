#include "live_replay_sidecar.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>

#include <game/teamscore.h>

namespace
{

	const json_value *JsonObjectField(const json_value *pObject, const char *pName)
	{
		if(pObject == nullptr || pObject->type != json_object)
			return &json_value_none;
		return json_object_get(pObject, pName);
	}

	bool JsonIntField(const json_value *pObject, const char *pName, int &Out)
	{
		const json_value *pValue = JsonObjectField(pObject, pName);
		if(pValue == &json_value_none || pValue->type != json_integer)
			return false;
		Out = json_int_get(pValue);
		return true;
	}

	bool JsonStringField(const json_value *pObject, const char *pName, char *pOut, int OutSize)
	{
		const json_value *pValue = JsonObjectField(pObject, pName);
		if(pValue == &json_value_none || pValue->type != json_string)
			return false;
		str_copy(pOut, json_string_get(pValue), OutSize);
		return true;
	}

	void SetError(char *pError, int ErrorSize, const char *pMessage)
	{
		if(pError != nullptr && ErrorSize > 0)
			str_copy(pError, pMessage, ErrorSize);
	}

	bool ValidClientId(int ClientId)
	{
		return ClientId >= 0 && ClientId < MAX_CLIENTS;
	}

	bool ValidRecordedTeam(int Team)
	{
		return Team >= TEAM_FLOCK && Team <= TEAM_SUPER;
	}

	bool ValidFinishEvent(const SLiveReplayFinishEvent &Event)
	{
		return Event.m_Tick >= 0 &&
		       Event.m_Team > TEAM_FLOCK && Event.m_Team < TEAM_SUPER &&
		       Event.m_Time >= 0 &&
		       ValidClientId(Event.m_ClientId);
	}

	bool ValidTeamEvent(const SLiveReplayTeamEvent &Event)
	{
		return Event.m_Tick >= 0 &&
		       ValidClientId(Event.m_ClientId) &&
		       ValidRecordedTeam(Event.m_OldTeam) &&
		       ValidRecordedTeam(Event.m_NewTeam);
	}

	bool DemoFilenameMatches(const char *pRecordedFilename, const char *pCurrentFilename)
	{
		if(pCurrentFilename == nullptr || pCurrentFilename[0] == '\0')
			return true;
		if(pRecordedFilename == nullptr || pRecordedFilename[0] == '\0')
			return false;
		if(str_comp(pRecordedFilename, pCurrentFilename) == 0)
			return true;

		char aRecordedName[IO_MAX_PATH_LENGTH];
		char aCurrentName[IO_MAX_PATH_LENGTH];
		IStorage::StripPathAndExtension(pRecordedFilename, aRecordedName, sizeof(aRecordedName));
		IStorage::StripPathAndExtension(pCurrentFilename, aCurrentName, sizeof(aCurrentName));
		return aRecordedName[0] != '\0' &&
		       aCurrentName[0] != '\0' &&
		       str_comp(aRecordedName, aCurrentName) == 0;
	}

	void WriteFinishEvent(CJsonWriter &JsonWriter, const SLiveReplayFinishEvent &Event)
	{
		JsonWriter.BeginObject();
		JsonWriter.WriteAttribute("tick");
		JsonWriter.WriteIntValue(Event.m_Tick);
		JsonWriter.WriteAttribute("team");
		JsonWriter.WriteIntValue(Event.m_Team);
		JsonWriter.WriteAttribute("time");
		JsonWriter.WriteIntValue(Event.m_Time);
		JsonWriter.WriteAttribute("time_ms");
		JsonWriter.WriteIntValue(Event.m_Time);
		JsonWriter.WriteAttribute("client_id");
		JsonWriter.WriteIntValue(Event.m_ClientId);
		JsonWriter.EndObject();
	}

	void WriteTeamEvent(CJsonWriter &JsonWriter, const SLiveReplayTeamEvent &Event)
	{
		JsonWriter.BeginObject();
		JsonWriter.WriteAttribute("tick");
		JsonWriter.WriteIntValue(Event.m_Tick);
		JsonWriter.WriteAttribute("client_id");
		JsonWriter.WriteIntValue(Event.m_ClientId);
		JsonWriter.WriteAttribute("old_team");
		JsonWriter.WriteIntValue(Event.m_OldTeam);
		JsonWriter.WriteAttribute("new_team");
		JsonWriter.WriteIntValue(Event.m_NewTeam);
		JsonWriter.EndObject();
	}

	bool ParseFinishEvent(const json_value *pValue, SLiveReplayFinishEvent &Out)
	{
		const bool HasTime = JsonIntField(pValue, "time", Out.m_Time) ||
				     JsonIntField(pValue, "time_ms", Out.m_Time);
		return JsonIntField(pValue, "tick", Out.m_Tick) &&
		       JsonIntField(pValue, "team", Out.m_Team) &&
		       HasTime &&
		       JsonIntField(pValue, "client_id", Out.m_ClientId);
	}

	bool ParseTeamEvent(const json_value *pValue, SLiveReplayTeamEvent &Out)
	{
		return JsonIntField(pValue, "tick", Out.m_Tick) &&
		       JsonIntField(pValue, "client_id", Out.m_ClientId) &&
		       JsonIntField(pValue, "old_team", Out.m_OldTeam) &&
		       JsonIntField(pValue, "new_team", Out.m_NewTeam);
	}

} // namespace

void CLiveReplaySidecar::Reset()
{
	m_Data = {};
	m_Active = false;
}

void CLiveReplaySidecar::Start(const char *pDemoFilename, const char *pMapName, SHA256_DIGEST MapSha256, unsigned MapCrc, int StartTick)
{
	Reset();
	str_copy(m_Data.m_aDemoFilename, pDemoFilename == nullptr ? "" : pDemoFilename);
	str_copy(m_Data.m_aMapName, pMapName == nullptr ? "" : pMapName);
	m_Data.m_MapSha256 = MapSha256;
	m_Data.m_MapCrc = MapCrc;
	m_Data.m_StartTick = StartTick;
	m_Data.m_EndTick = StartTick;
	m_Active = true;
}

void CLiveReplaySidecar::SetEndTick(int EndTick)
{
	if(m_Active)
		m_Data.m_EndTick = EndTick;
}

bool CLiveReplaySidecar::AddFinishEvent(int Tick, int Team, int Time, int ClientId)
{
	if(!m_Active)
		return false;

	for(const SLiveReplayFinishEvent &Event : m_Data.m_vFinishEvents)
	{
		if(Event.m_Tick == Tick && Event.m_Team == Team && Event.m_Time == Time && Event.m_ClientId == ClientId)
			return false;
	}

	m_Data.m_vFinishEvents.push_back({Tick, Team, Time, ClientId});
	return true;
}

bool CLiveReplaySidecar::AddTeamEvent(int Tick, int ClientId, int OldTeam, int NewTeam)
{
	if(!m_Active || OldTeam == NewTeam)
		return false;

	for(const SLiveReplayTeamEvent &Event : m_Data.m_vTeamEvents)
	{
		if(Event.m_Tick == Tick && Event.m_ClientId == ClientId && Event.m_OldTeam == OldTeam && Event.m_NewTeam == NewTeam)
			return false;
	}

	m_Data.m_vTeamEvents.push_back({Tick, ClientId, OldTeam, NewTeam});
	return true;
}

std::string CLiveReplaySidecar::BuildJson() const
{
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(m_Data.m_MapSha256, aSha256, sizeof(aSha256));

	CJsonStringWriter JsonWriter;
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("format_version");
	JsonWriter.WriteIntValue(m_Data.m_FormatVersion);
	JsonWriter.WriteAttribute("demo_filename");
	JsonWriter.WriteStrValue(m_Data.m_aDemoFilename);
	JsonWriter.WriteAttribute("map");
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("name");
	JsonWriter.WriteStrValue(m_Data.m_aMapName);
	JsonWriter.WriteAttribute("sha256");
	JsonWriter.WriteStrValue(aSha256);
	JsonWriter.WriteAttribute("crc");
	JsonWriter.WriteIntValue(static_cast<int>(m_Data.m_MapCrc));
	JsonWriter.EndObject();
	JsonWriter.WriteAttribute("recording");
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("start_tick");
	JsonWriter.WriteIntValue(m_Data.m_StartTick);
	JsonWriter.WriteAttribute("end_tick");
	JsonWriter.WriteIntValue(m_Data.m_EndTick);
	JsonWriter.EndObject();
	JsonWriter.WriteAttribute("finish_events");
	JsonWriter.BeginArray();
	for(const SLiveReplayFinishEvent &Event : m_Data.m_vFinishEvents)
		WriteFinishEvent(JsonWriter, Event);
	JsonWriter.EndArray();
	JsonWriter.WriteAttribute("team_events");
	JsonWriter.BeginArray();
	for(const SLiveReplayTeamEvent &Event : m_Data.m_vTeamEvents)
		WriteTeamEvent(JsonWriter, Event);
	JsonWriter.EndArray();
	JsonWriter.WriteAttribute("extensions");
	JsonWriter.BeginObject();
	JsonWriter.EndObject();
	JsonWriter.EndObject();
	return JsonWriter.GetOutputString();
}

bool CLiveReplaySidecar::WriteAtomic(IStorage *pStorage, const char *pFilename) const
{
	if(pStorage == nullptr || pFilename == nullptr || pFilename[0] == '\0')
		return false;

	char aTmpFilename[IO_MAX_PATH_LENGTH];
	IStorage::FormatTmpPath(aTmpFilename, sizeof(aTmpFilename), pFilename);

	const std::string Json = BuildJson();
	IOHANDLE File = pStorage->OpenFile(aTmpFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	io_write(File, Json.c_str(), Json.size());
	io_close(File);

	pStorage->RemoveFile(pFilename, IStorage::TYPE_SAVE);
	if(!pStorage->RenameFile(aTmpFilename, pFilename, IStorage::TYPE_SAVE))
	{
		pStorage->RemoveFile(aTmpFilename, IStorage::TYPE_SAVE);
		return false;
	}
	return true;
}

bool CLiveReplaySidecar::SidecarPathForDemo(const char *pDemoFilename, char *pBuffer, int BufferSize)
{
	if(pDemoFilename == nullptr || pDemoFilename[0] == '\0' || pBuffer == nullptr || BufferSize <= 0)
		return false;

	str_copy(pBuffer, pDemoFilename, BufferSize);
	if(str_endswith(pBuffer, ".demo"))
		pBuffer[str_length(pBuffer) - str_length(".demo")] = '\0';
	str_append(pBuffer, ".qmlive.json", BufferSize);
	return true;
}

bool CLiveReplaySidecar::LoadFromString(const char *pJson, SLiveReplaySidecarData &Out, char *pError, int ErrorSize)
{
	Out = {};
	SetError(pError, ErrorSize, "");

	if(pJson == nullptr)
	{
		SetError(pError, ErrorSize, "missing json");
		return false;
	}

	json_settings Settings{};
	char aJsonError[256] = "";
	json_value *pRoot = JsonParseEx(&Settings, reinterpret_cast<const json_char *>(pJson), str_length(pJson), aJsonError);
	if(pRoot == nullptr)
	{
		SetError(pError, ErrorSize, aJsonError);
		return false;
	}

	bool Success = false;
	do
	{
		if(pRoot->type != json_object)
		{
			SetError(pError, ErrorSize, "root is not an object");
			break;
		}

		if(!JsonIntField(pRoot, "format_version", Out.m_FormatVersion) || Out.m_FormatVersion != SLiveReplaySidecarData::FORMAT_VERSION)
		{
			SetError(pError, ErrorSize, "unsupported format version");
			break;
		}
		if(!JsonStringField(pRoot, "demo_filename", Out.m_aDemoFilename, sizeof(Out.m_aDemoFilename)))
		{
			SetError(pError, ErrorSize, "missing demo filename");
			break;
		}

		const json_value *pMap = JsonObjectField(pRoot, "map");
		if(pMap == &json_value_none || pMap->type != json_object)
		{
			SetError(pError, ErrorSize, "missing map object");
			break;
		}
		char aSha256[SHA256_MAXSTRSIZE] = "";
		int Crc = 0;
		if(!JsonStringField(pMap, "name", Out.m_aMapName, sizeof(Out.m_aMapName)) ||
			!JsonStringField(pMap, "sha256", aSha256, sizeof(aSha256)) ||
			!JsonIntField(pMap, "crc", Crc) ||
			sha256_from_str(&Out.m_MapSha256, aSha256) != 0)
		{
			SetError(pError, ErrorSize, "invalid map object");
			break;
		}
		Out.m_MapCrc = static_cast<unsigned>(Crc);

		const json_value *pRecording = JsonObjectField(pRoot, "recording");
		if(pRecording == &json_value_none || pRecording->type != json_object ||
			!JsonIntField(pRecording, "start_tick", Out.m_StartTick) ||
			!JsonIntField(pRecording, "end_tick", Out.m_EndTick))
		{
			SetError(pError, ErrorSize, "invalid recording object");
			break;
		}
		if(Out.m_StartTick < 0 || Out.m_EndTick < Out.m_StartTick)
		{
			SetError(pError, ErrorSize, "invalid recording tick range");
			break;
		}

		const json_value *pFinishEvents = JsonObjectField(pRoot, "finish_events");
		if(pFinishEvents != &json_value_none)
		{
			if(pFinishEvents->type != json_array)
			{
				SetError(pError, ErrorSize, "finish_events is not an array");
				break;
			}
			bool EventsOk = true;
			for(int i = 0; i < json_array_length(pFinishEvents); ++i)
			{
				SLiveReplayFinishEvent Event;
				if(!ParseFinishEvent(json_array_get(pFinishEvents, i), Event) || !ValidFinishEvent(Event))
				{
					SetError(pError, ErrorSize, "invalid finish event");
					EventsOk = false;
					break;
				}
				Out.m_vFinishEvents.push_back(Event);
			}
			if(!EventsOk)
				break;
		}

		const json_value *pTeamEvents = JsonObjectField(pRoot, "team_events");
		if(pTeamEvents != &json_value_none)
		{
			if(pTeamEvents->type != json_array)
			{
				SetError(pError, ErrorSize, "team_events is not an array");
				break;
			}
			bool EventsOk = true;
			for(int i = 0; i < json_array_length(pTeamEvents); ++i)
			{
				SLiveReplayTeamEvent Event;
				if(!ParseTeamEvent(json_array_get(pTeamEvents, i), Event) || !ValidTeamEvent(Event))
				{
					SetError(pError, ErrorSize, "invalid team event");
					EventsOk = false;
					break;
				}
				Out.m_vTeamEvents.push_back(Event);
			}
			if(!EventsOk)
				break;
		}

		Success = true;
	} while(false);

	json_value_free(pRoot);
	if(!Success)
		Out = {};
	return Success;
}

bool CLiveReplaySidecar::MatchesDemo(const SLiveReplaySidecarData &Data, const char *pDemoFilename, const char *pMapName, SHA256_DIGEST MapSha256, unsigned MapCrc)
{
	if(Data.m_FormatVersion != SLiveReplaySidecarData::FORMAT_VERSION)
		return false;
	if(!DemoFilenameMatches(Data.m_aDemoFilename, pDemoFilename))
		return false;
	if(pMapName != nullptr && pMapName[0] != '\0' && str_comp(Data.m_aMapName, pMapName) != 0)
		return false;
	if(MapSha256 != SHA256_DIGEST{} && Data.m_MapSha256 != MapSha256)
		return false;
	if(MapCrc != 0 && Data.m_MapCrc != MapCrc)
		return false;
	return true;
}
