/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_FRAME_SCHEDULER_H
#define GAME_CLIENT_FRAME_SCHEDULER_H

#include <engine/kernel.h>

#include <game/client/components/settings_resource_jobs.h>

#include <cstddef>
#include <cstdint>

enum class EFrameSchedulerConsumer : int
{
	SettingsText = 0,
	IngameText = 1,
	Assets = 2,
	DemoBrowser = 3,
	IngameServerInfo = 4,
	SettingsTee = 5,
	Count
};

constexpr size_t FRAME_SCHEDULER_CONSUMER_COUNT = static_cast<size_t>(EFrameSchedulerConsumer::Count);

class IFrameScheduler : public IInterface
{
	MACRO_INTERFACE("frame_scheduler")
public:
	virtual SSettingsAdaptiveBudgetOutput ComputeBudget(
		EFrameSchedulerConsumer Consumer,
		const SSettingsAdaptiveBudgetInput &Input) = 0;
	virtual void Reset() = 0;
	virtual const SSettingsAdaptiveBudgetState &State(EFrameSchedulerConsumer Consumer) const = 0;
	virtual const SSettingsAdaptiveBudgetOutput &LastOutput(EFrameSchedulerConsumer Consumer) const = 0;
	// CGameClient::OnRender 调用帧入口/出口；EndFrame 当前不需要清理。
	virtual void BeginFrame(int FrameId) = 0;
	virtual void EndFrame() = 0;
	virtual int CurrentFrameId() const = 0;
};

IFrameScheduler *CreateFrameScheduler();

#endif
