#ifndef ENGINE_SHARED_CLIENT_BRAND_H
#define ENGINE_SHARED_CLIENT_BRAND_H

#include <base/str.h>

inline constexpr int CLIENT_BRANDS_PROTOCOL_VERSION = 2;

enum class EClientBrand
{
	NONE = 0,
	QM,
	ARG,
};

inline EClientBrand ClientBrandFromVersionString(const char *pVersion)
{
	if(!pVersion)
		return EClientBrand::NONE;
	if(str_find_nocase(pVersion, "QmClient") || str_find_nocase(pVersion, "QmLiveClient"))
		return EClientBrand::QM;
	if(str_find_nocase(pVersion, "Arghena") || str_find_nocase(pVersion, "ArgClient"))
		return EClientBrand::ARG;
	return EClientBrand::NONE;
}

inline EClientBrand ClientBrandFromInt(int Brand)
{
	switch(static_cast<EClientBrand>(Brand))
	{
	case EClientBrand::NONE:
	case EClientBrand::QM:
	case EClientBrand::ARG:
		return static_cast<EClientBrand>(Brand);
	}
	return EClientBrand::NONE;
}

inline const char *ClientBrandPrefix(EClientBrand Brand)
{
	switch(Brand)
	{
	case EClientBrand::QM:
		return "Qm";
	case EClientBrand::ARG:
		return "Arg";
	case EClientBrand::NONE:
		return "";
	}
	return "";
}

#endif // ENGINE_SHARED_CLIENT_BRAND_H
