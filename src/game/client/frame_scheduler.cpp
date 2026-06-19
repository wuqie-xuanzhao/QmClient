/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <game/client/frame_scheduler.h>

#include <array>

class CFrameScheduler : public IFrameScheduler
{
	std::array<SSettingsAdaptiveBudgetState, FRAME_SCHEDULER_CONSUMER_COUNT> m_aState{};
	std::array<SSettingsAdaptiveBudgetOutput, FRAME_SCHEDULER_CONSUMER_COUNT> m_aLastOutput{};
	int m_CurrentFrameId = 0;

	static size_t ConsumerIndex(EFrameSchedulerConsumer Consumer)
	{
		const size_t Index = static_cast<size_t>(Consumer);
		dbg_assert(Index < FRAME_SCHEDULER_CONSUMER_COUNT, "invalid EFrameSchedulerConsumer");
		return Index;
	}

public:
	SSettingsAdaptiveBudgetOutput ComputeBudget(
		EFrameSchedulerConsumer Consumer,
		const SSettingsAdaptiveBudgetInput &Input) override
	{
		const size_t Index = ConsumerIndex(Consumer);
		const SSettingsAdaptiveBudgetOutput Output = SettingsAdaptiveBudgetStep(Input, m_aState[Index]);
		m_aLastOutput[Index] = Output;
		return Output;
	}

	const SSettingsAdaptiveBudgetState &State(EFrameSchedulerConsumer Consumer) const override
	{
		return m_aState[ConsumerIndex(Consumer)];
	}

	const SSettingsAdaptiveBudgetOutput &LastOutput(EFrameSchedulerConsumer Consumer) const override
	{
		return m_aLastOutput[ConsumerIndex(Consumer)];
	}

	void BeginFrame(int FrameId) override
	{
		m_CurrentFrameId = FrameId;
	}

	void EndFrame() override
	{
	}

	int CurrentFrameId() const override
	{
		return m_CurrentFrameId;
	}
};

IFrameScheduler *CreateFrameScheduler()
{
	return new CFrameScheduler();
}
