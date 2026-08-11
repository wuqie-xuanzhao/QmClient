/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
#ifndef ENGINE_CLIENT_PLAUSIBLE_SIZES_H
#define ENGINE_CLIENT_PLAUSIBLE_SIZES_H

// Shared plausibility guards for window sizes / refresh rates. Kept in a header
// so the SDL backend and the threaded graphics backend agree (no duplicated
// file-static copies) and the rules are unit-testable.
inline bool IsPlausibleRefreshRate(int RefreshRate)
{
	return RefreshRate >= 0 && RefreshRate <= 1000;
}

inline bool IsPlausibleWindowSize(int Width, int Height)
{
	return Width >= 320 && Height >= 240 && Width <= 16384 && Height <= 16384;
}

#endif // ENGINE_CLIENT_PLAUSIBLE_SIZES_H
