#ifndef ENGINE_SHARED_QM_IME_POLICY_H
#define ENGINE_SHARED_QM_IME_POLICY_H

inline bool QmImeShouldUseSystemCandidateUi()
{
#if defined(CONF_FAMILY_WINDOWS)
	return false;
#else
	return true;
#endif
}

inline bool QmImeShouldRenderCustomCandidateUi()
{
	if(QmImeShouldUseSystemCandidateUi())
		return false;
	return true;
}

inline bool QmImeNotifyFlagsIncludeCandidateList(unsigned CandidateListFlags, unsigned CandidateListIndex)
{
	if(CandidateListIndex >= 32)
		return false;
	if(CandidateListFlags == 0)
		return CandidateListIndex == 0;
	return (CandidateListFlags & (1u << CandidateListIndex)) != 0;
}

inline unsigned QmImeCandidatePageSizeOrCount(unsigned PageSize, unsigned CandidateCount)
{
	return PageSize > 0 ? PageSize : CandidateCount;
}

#endif
