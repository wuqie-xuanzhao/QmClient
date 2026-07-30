// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "settings_warmup.h"

#include <algorithm>
#include <utility>

static constexpr double MIN_WARMUP_SECTION_BUDGET_MS = 1.0;

void CSettingsWarmupScheduler::RegisterSection(SSettingsWarmupSection Section)
{
	m_vSections.push_back(std::move(Section));
	m_Sorted = false;
}

void CSettingsWarmupScheduler::SetLastSessionPage(EClassicSettingsPage Page)
{
	m_LastSessionPage = Page;
	m_Sorted = false;
}

void CSettingsWarmupScheduler::SetEnabled(bool Enabled)
{
	m_Enabled = Enabled;
}

bool CSettingsWarmupScheduler::WarmupFrame(double BudgetMs)
{
	if(!m_Enabled)
		return true;

	if(!m_Sorted)
	{
		std::stable_sort(m_vSections.begin(), m_vSections.end(), [&](const SSettingsWarmupSection &Left, const SSettingsWarmupSection &Right) {
			const bool LeftLastPage = Left.m_Page == m_LastSessionPage;
			const bool RightLastPage = Right.m_Page == m_LastSessionPage;
			if(LeftLastPage != RightLastPage)
				return LeftLastPage;
			return Left.m_Priority < Right.m_Priority;
		});
		m_Sorted = true;
	}

	double UsedBudgetMs = 0.0;
	for(SSettingsWarmupSection &Section : m_vSections)
	{
		if(Section.m_Warmed)
			continue;
		if(UsedBudgetMs > 0.0 && UsedBudgetMs + MIN_WARMUP_SECTION_BUDGET_MS > BudgetMs)
			return false;

		const double CostMs = Section.m_WarmupFn ? Section.m_WarmupFn() : 0.0;
		Section.m_Warmed = true;
		UsedBudgetMs += CostMs;
		if(UsedBudgetMs >= BudgetMs)
			return false;
	}
	return true;
}

void CSettingsWarmupScheduler::Reset()
{
	for(SSettingsWarmupSection &Section : m_vSections)
		Section.m_Warmed = false;
}
