#include "qm_nameplate_msdf_renderer.h"

#include "qm_nameplate_msdf_gate.h"
#include "qm_nameplate_msdf_manifest.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace
{
	constexpr const char *kProfileManifestPrefix = "qmclient/nameplate_msdf/profiles/nameplate_";
	constexpr const char *kProfileManifestSuffix = ".json";
	// 渲染器内部的短名：与门控/诊断工具共用同一份 UTF-8 解码实现。
	uint32_t DecodeUtf8(const char *&p)
	{
		return QmNameplateMsdfDecodeUtf8(p);
	}

	ColorRGBA RainbowAt(float Time)
	{
		const float Hue = std::fmod(Time * 0.15f, 1.0f);
		return color_cast<ColorRGBA>(ColorHSLA(Hue, 0.7f, 0.65f, 1.0f));
	}

	// 渲染器内部的短名：manifest 取值统一走 qm_nameplate_msdf_manifest.h 的共享实现，
	// 保证生产解析器与图集契约测试用的是同一套分隔符处理（曾因两者不一致导致整包解析失败）。
	const char *FindSpan(const char *pBegin, const char *pEnd, const char *pNeedle)
	{
		return QmNameplateMsdfFindSpan(pBegin, pEnd, pNeedle);
	}

	const char *FindKey(const char *pBegin, const char *pEnd, const char *pKey)
	{
		return QmNameplateMsdfFindKey(pBegin, pEnd, pKey);
	}

	bool ReadDouble(const char *p, const char *pEnd, double &Out)
	{
		return QmNameplateMsdfReadDouble(p, pEnd, Out);
	}

	bool ReadString(const char *p, const char *pEnd, std::string &Out)
	{
		return QmNameplateMsdfReadString(p, pEnd, Out);
	}

	bool ReadBool(const char *p, const char *pEnd, bool &Out)
	{
		return QmNameplateMsdfReadBool(p, pEnd, Out);
	}

	bool ReadJsonStringAt(const char *&p, const char *pEnd, std::string &Out)
	{
		while(p < pEnd && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ','))
			++p;
		if(p >= pEnd || *p != '"')
			return false;
		++p;
		const char *pBegin = p;
		while(p < pEnd && *p != '"')
			++p;
		if(p >= pEnd)
			return false;
		Out.assign(pBegin, p);
		++p;
		return true;
	}
}

CQmNameplateMsdfRenderer::~CQmNameplateMsdfRenderer()
{
	Shutdown();
}

CQmNameplateMsdfRenderer &QmNameplateMsdf()
{
	static CQmNameplateMsdfRenderer s_Renderer;
	return s_Renderer;
}

void CQmNameplateMsdfRenderer::UnloadPages()
{
	for(SPage &Page : m_vPages)
	{
		if(m_pGraphics != nullptr && Page.m_Texture.IsValid())
			m_pGraphics->UnloadTexture(&Page.m_Texture);
		Page.m_Texture = IGraphics::CTextureHandle();
	}
	m_vPages.clear();
}

void CQmNameplateMsdfRenderer::Shutdown()
{
	UnloadPages();
	m_Glyphs.clear();
	m_Ready = false;
	m_OptionalPagePending = false;
	// 刻意保留 m_InitAttempted：否则每帧的 EnsureInitialized 会在图集缺失/后端不支持时反复读盘重试
	m_NextInitAttempt = 0;
	m_RefEmPixels = 0.0f;
	m_RefAscent = 0.0f;
	m_RefDescent = 0.0f;
	m_Error.clear();
	m_pStorage = nullptr;
	m_pGraphics = nullptr;
}

void CQmNameplateMsdfRenderer::OnGraphicsResourcesReset()
{
	// 设备重建后旧纹理句柄全部失效：丢弃页与字形表，让下一次 EnsureInitialized 在新设备上重建。
	// m_FatalError（后端不支持等硬失败）保持不重试；m_pStorage/m_pGraphics 对象未变，保持不动。
	const bool WasLoaded = m_Ready;
	UnloadPages();
	m_Glyphs.clear();
	m_Ready = false;
	m_OptionalPagePending = false;
	m_InitAttempted = false;
	m_NextInitAttempt = 0;
	m_RefEmPixels = 0.0f;
	m_RefAscent = 0.0f;
	m_RefDescent = 0.0f;
	m_Error.clear();
	if(WasLoaded)
		log_info("nameplate_msdf", "atlas released for graphics resources reset, will reload on next frame");
}

void CQmNameplateMsdfRenderer::EnsureInitialized(IStorage *pStorage, IGraphics *pGraphics)
{
	EnsureInitialized(pStorage, pGraphics, m_Profile.empty() ? nullptr : m_Profile.c_str());
}

void CQmNameplateMsdfRenderer::EnsureInitialized(IStorage *pStorage, IGraphics *pGraphics, const char *pProfile)
{
	if(pProfile == nullptr || pProfile[0] == '\0')
		return;
	if(m_Profile != pProfile)
	{
		Shutdown();
		m_Profile = pProfile;
	}
	if(m_FatalError)
		return;
	if(!m_Ready)
	{
		// 失败退避：图集缺失或损坏时不要每帧重试读盘
		if(m_InitAttempted && time_get() < m_NextInitAttempt)
			return;
		m_InitAttempted = false;
		if(!Init(pStorage, pGraphics, pProfile))
			m_NextInitAttempt = time_get() + time_freq() * 10;
		return;
	}
}

bool CQmNameplateMsdfRenderer::Init(IStorage *pStorage, IGraphics *pGraphics, const char *pProfile)
{
	if(m_InitAttempted)
		return m_Ready;
	m_InitAttempted = true;
	m_pStorage = pStorage;
	m_pGraphics = pGraphics;
	if(m_pStorage == nullptr || m_pGraphics == nullptr)
	{
		m_Error = "storage or graphics unavailable";
		return false;
	}
	if(!m_pGraphics->HasTexturedMsdf())
	{
		// 该后端不支持 MSDF 管线（例如 GLES），静默回退到 FreeType 名牌
		m_Error = "backend has no textured MSDF pipeline";
		m_FatalError = true;
		log_info("nameplate_msdf", "TexturedMsdf unsupported, nameplate MSDF disabled");
		return false;
	}

	if(!LoadProfile(pProfile))
	{
		log_error("nameplate_msdf", "failed to load profile '%s': %s", pProfile, m_Error.c_str());
		Shutdown();
		return false;
	}
	// 主字体只负责自身字形；语言缺口由客户端随包的通用 profile 补齐。
	// 先加载主 profile，ParseManifest 使用 emplace 保证主字体字形优先，
	// 因而英文仍保持用户选择的字体，中文/日文/韩文则来自内置 CJK 字体。
	const char *pFallbackProfiles[] = {"dejavu", "noto_glow_cjk"};
	for(const char *pFallback : pFallbackProfiles)
	{
		if(str_comp(pFallback, pProfile) == 0)
			continue;
		const std::string PreviousError = m_Error;
		if(!LoadProfile(pFallback))
		{
			// 补充 profile 缺失不应让主 profile 失效；保留主 profile 的错误状态。
			m_Error = PreviousError;
		}
	}

	m_Ready = !m_vPages.empty() && !m_Glyphs.empty();
	if(m_Ready)
	{
		// 旧图集无行盒度量时的近似：ascent 取大写 H 的 cap 高度（基线一致性仍成立），
		// descent 取 0.25em（与常见拉丁字体的 hhea 比例接近）。重建图集后即为真实度量。
		if(m_RefAscent <= 0.0f)
		{
			const auto HCap = m_Glyphs.find(0x48);
			m_RefAscent = HCap != m_Glyphs.end() && HCap->second.m_BearingY > 0.0f ? HCap->second.m_BearingY : m_RefEmPixels * 0.75f;
		}
		if(m_RefDescent <= 0.0f)
			m_RefDescent = m_RefEmPixels * 0.25f;
		log_info("nameplate_msdf", "Nameplate MSDF ready: %zu page(s), %zu glyphs, refEm=%.0f pxRange=%.1f ascent=%.2f descent=%.2f",
			m_vPages.size(), m_Glyphs.size(), m_RefEmPixels, m_vPages.empty() ? 0.0f : m_vPages[0].m_PxRange, m_RefAscent, m_RefDescent);
	}
	else
	{
		Shutdown();
	}
	return m_Ready;
}

bool CQmNameplateMsdfRenderer::LoadProfile(const char *pProfile)
{
	char aManifestPath[IO_MAX_PATH_LENGTH];
	str_format(aManifestPath, sizeof(aManifestPath), "%s%s%s", kProfileManifestPrefix, pProfile, kProfileManifestSuffix);
	void *pData = nullptr;
	unsigned Size = 0;
	if(!m_pStorage->ReadFile(aManifestPath, IStorage::TYPE_ALL, &pData, &Size))
	{
		m_Error = "profile manifest missing: ";
		m_Error += aManifestPath;
		return false;
	}
	std::string Text((const char *)pData, Size);
	free(pData);
	const char *pEnd = Text.c_str() + Text.size();
	const char *pPages = FindKey(Text.c_str(), pEnd, "\"pages\"");
	if(pPages == nullptr)
	{
		m_Error = "profile manifest has no pages: ";
		m_Error += aManifestPath;
		return false;
	}
	const char *p = pPages;
	while(p < pEnd && *p != '[')
		++p;
	if(p >= pEnd)
		return false;
	++p;
	int Loaded = 0;
	while(p < pEnd)
	{
		while(p < pEnd && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ','))
			++p;
		if(p >= pEnd || *p == ']')
			break;
		std::string PagePath;
		if(!ReadJsonStringAt(p, pEnd, PagePath))
			break;
		if(LoadPage(PagePath.c_str()))
			++Loaded;
		else
			log_info("nameplate_msdf", "profile '%s' page unavailable (%s), continuing with remaining pages", pProfile, m_Error.c_str());
	}
	if(Loaded == 0)
	{
		m_Error = "profile has no loadable pages: ";
		m_Error += pProfile;
		return false;
	}
	return true;
}

bool CQmNameplateMsdfRenderer::LoadPage(const char *pManifestPath)
{
	void *pData = nullptr;
	unsigned Size = 0;
	if(!m_pStorage->ReadFile(pManifestPath, IStorage::TYPE_ALL, &pData, &Size))
	{
		m_Error = "manifest missing: ";
		m_Error += pManifestPath;
		return false;
	}
	std::string Text((const char *)pData, Size);
	free(pData);
	return ParseManifest(Text.c_str(), pManifestPath);
}

bool CQmNameplateMsdfRenderer::ParseManifest(const char *pText, const std::string &ManifestPath)
{
	const char *pEnd = pText + str_length(pText);

	const char *pPxRange = FindKey(pText, pEnd, "px_range");
	const char *pEmPixels = FindKey(pText, pEnd, "em_pixels");
	const char *pAlphaSdf = FindKey(pText, pEnd, "alpha_sdf");
	const char *pImage = FindKey(pText, pEnd, "image");
	double PxRange = 0.0;
	double EmPixels = 0.0;
	bool AlphaSdf = false;
	if(pPxRange == nullptr || pEmPixels == nullptr || pImage == nullptr ||
		!ReadDouble(pPxRange, pEnd, PxRange) || !ReadDouble(pEmPixels, pEnd, EmPixels) || PxRange <= 0.0 || EmPixels <= 0.0)
	{
		m_Error = "manifest malformed: ";
		m_Error += ManifestPath;
		return false;
	}
	if(pAlphaSdf != nullptr)
		ReadBool(pAlphaSdf, pEnd, AlphaSdf);

	// image 字段为相对 data/ 的路径，取其文件名与本 manifest 同目录拼接
	std::string ImageField;
	if(!ReadString(pImage, pEnd, ImageField))
	{
		m_Error = "manifest image field malformed: ";
		m_Error += ManifestPath;
		return false;
	}
	const size_t Slash = ImageField.find_last_of('/');
	const std::string ImageName = Slash == std::string::npos ? ImageField : ImageField.substr(Slash + 1);
	const size_t DirEnd = ManifestPath.find_last_of('/');
	const std::string ImagePath = (DirEnd == std::string::npos ? std::string() : ManifestPath.substr(0, DirEnd + 1)) + ImageName;

	IGraphics::CTextureHandle Texture = m_pGraphics->LoadTexture(ImagePath.c_str(), IStorage::TYPE_ALL, IGraphics::TEXLOAD_NO_MIPMAPS);
	if(!Texture.IsValid())
	{
		m_Error = "atlas image missing: ";
		m_Error += ImagePath;
		return false;
	}

	const char *pAtlasKey = FindKey(pText, pEnd, "\"atlas\"");
	if(pAtlasKey == nullptr)
	{
		m_Error = "manifest has no atlas block: ";
		m_Error += ManifestPath;
		m_pGraphics->UnloadTexture(&Texture);
		return false;
	}
	// atlas 块内的 width/height 紧跟其后，避免误取 glyphs 里的同名字段
	const char *pAtlasEnd = pAtlasKey;
	while(pAtlasEnd < pEnd && *pAtlasEnd != '}')
		++pAtlasEnd;
	const char *pWidth = FindKey(pAtlasKey, pAtlasEnd, "width");
	const char *pHeight = FindKey(pAtlasKey, pAtlasEnd, "height");
	double AtlasWidth = 0.0;
	double AtlasHeight = 0.0;
	if(pWidth == nullptr || pHeight == nullptr ||
		!ReadDouble(pWidth, pAtlasEnd, AtlasWidth) || !ReadDouble(pHeight, pAtlasEnd, AtlasHeight) ||
		AtlasWidth <= 0.0 || AtlasHeight <= 0.0)
	{
		m_Error = "manifest atlas block malformed: ";
		m_Error += ManifestPath;
		m_pGraphics->UnloadTexture(&Texture);
		return false;
	}

	const int PageIndex = (int)m_vPages.size();
	const float PreviousRefEmPixels = m_RefEmPixels;
	const float PreviousRefAscent = m_RefAscent;
	const float PreviousRefDescent = m_RefDescent;
	SPage Page;
	Page.m_Texture = Texture;
	Page.m_Info.m_ImageName = ImagePath;
	Page.m_Info.m_Width = (int)AtlasWidth;
	Page.m_Info.m_Height = (int)AtlasHeight;
	Page.m_PxRange = (float)PxRange;
	m_vPages.push_back(Page);
	auto RollbackPage = [&]() {
		SPage &Back = m_vPages.back();
		if(Back.m_Texture.IsValid())
			m_pGraphics->UnloadTexture(&Back.m_Texture);
		m_vPages.pop_back();
		m_RefEmPixels = PreviousRefEmPixels;
		m_RefAscent = PreviousRefAscent;
		m_RefDescent = PreviousRefDescent;
	};
	if(m_RefEmPixels <= 0.0f)
		m_RefEmPixels = (float)EmPixels;
	const float PageMetricScale = EmPixels > 0.0 ? m_RefEmPixels / (float)EmPixels : 1.0f;
	// Profile 中允许混用不同烘焙分辨率。把本页距离范围和字形度量
	// 归一到首个页面的参考 em，避免高分辨率中文页被整体缩小或描边变薄。
	m_vPages.back().m_PxRange = (float)PxRange * PageMetricScale;
	// 行盒度量（可选字段，旧图集没有）：字体真实的 ascent/descent（参考 em 像素），
	// 来自 FreeType hhea 度量。基线排版必须与字符串内容无关，见 Measure()/Draw()。
	// 键带引号定位："ascent" 不是 "descent" 的子串（带前引号），不会互相误匹配。
	if(m_RefAscent <= 0.0f)
	{
		double Ascent = 0.0;
		double Descent = 0.0;
		const char *pAscent = FindKey(pText, pEnd, "\"ascent\"");
		const char *pDescent = FindKey(pText, pEnd, "\"descent\"");
		if(pAscent != nullptr)
			ReadDouble(pAscent, pEnd, Ascent);
		if(pDescent != nullptr)
			ReadDouble(pDescent, pEnd, Descent);
		if(Ascent > 0.0)
			m_RefAscent = (float)Ascent;
		if(Descent > 0.0)
			m_RefDescent = (float)Descent;
	}

	// glyphs 块：逐个解析 "<codepoint>": { ... }（扫描逻辑与图集契约测试共用同一份实现）
	const char *pGlyphs = FindKey(pText, pEnd, "\"glyphs\"");
	if(pGlyphs == nullptr)
	{
		m_Error = "manifest has no glyphs block: ";
		m_Error += ManifestPath;
		RollbackPage();
		return false;
	}
	const int Parsed = QmNameplateMsdfForEachGlyphObject(pGlyphs, pEnd,
		[&](uint32_t Codepoint, const char *pObjectBegin, const char *pObjectEnd) {
			SGlyph Glyph;
			Glyph.m_Page = PageIndex;
			double Value = 0.0;
			const char *pField = FindKey(pObjectBegin, pObjectEnd, "\"x\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_X = (int)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"y\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_Y = (int)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"w\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_W = (int)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"h\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_H = (int)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"adv\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_Advance = (float)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"bx\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_BearingX = (float)Value;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"by\"");
			if(pField != nullptr && ReadDouble(pField, pObjectEnd, Value))
				Glyph.m_BearingY = (float)Value;
			bool Outline = false;
			pField = FindKey(pObjectBegin, pObjectEnd, "\"outline\"");
			if(pField != nullptr && ReadBool(pField, pObjectEnd, Outline))
				Glyph.m_HasOutline = Outline;
			// 只有新生成且明确声明 alpha_sdf 的图集才能走 Alpha 真 SDF。
			// 当前 Noto CJK 页的 Alpha 通道在部分字形上会被 PNG/上传链判成
			// 实心 tile，导致中文变成白色方块；它们仍是 MTSDF 资源，改用
			// RGB median 采样可保留距离场边缘，同时避开这个 Alpha 伪影。
			const bool IsNotoCjkPage = ManifestPath.find("noto_glow") != std::string::npos;
			Glyph.m_UseTrueSdf = AlphaSdf && !IsNotoCjkPage;
			Glyph.m_Valid = true;

			// 同名码位跨页时以先加载的页为准（基础页优先），避免 CJK 页覆盖拉丁字形
			Glyph.m_MetricScale = PageMetricScale;
			m_Glyphs.emplace(Codepoint, Glyph);
			return true;
		});
	if(Parsed == 0)
	{
		// 回滚本页，保持 m_vPages 与 m_Glyphs 一致
		RollbackPage();
		m_Error = "manifest glyphs block empty: ";
		m_Error += ManifestPath;
		return false;
	}
	return true;
}

bool CQmNameplateMsdfRenderer::SupportsText(const char *pText) const
{
	if(!m_Ready || pText == nullptr)
		return false;
	return FindUnsupportedCodepoint(pText) == 0;
}

uint32_t CQmNameplateMsdfRenderer::FindUnsupportedCodepoint(const char *pText) const
{
	if(!m_Ready || pText == nullptr)
		return 0;
	return QmNameplateMsdfFirstMissingCodepoint(pText, [this](uint32_t Codepoint) {
		return m_Glyphs.find(Codepoint) != m_Glyphs.end();
	});
}

vec2 CQmNameplateMsdfRenderer::Measure(const char *pText, float FontSize) const
{
	if(!m_Ready || pText == nullptr || FontSize <= 0.0f)
		return vec2(0.0f, 0.0f);
	const float Scale = FontSize / m_RefEmPixels;
	const char *p = pText;
	float Width = 0.0f;
	while(*p != '\0')
	{
		const uint32_t Cp = DecodeUtf8(p);
		if(Cp == 0)
			break;
		// msdf-atlas-gen 不会为没有可见轮廓的空格写入 glyph quad，
		// 但空格的 advance 仍属于排版度量，必须保留，否则英文文本会被拼接。
		if(Cp == ' ')
		{
			Width += FontSize * 0.25f;
			continue;
		}
		// 制表符同样没有字形；宽度必须与 DrawText 的 0.5em 推进一致，
		// 否则同一串文本的测量宽度与绘制宽度不等，铭牌底板会错位。
		if(Cp == '\t')
		{
			Width += FontSize * 0.5f;
			continue;
		}
		// 其余零宽/不可见码点（换行、变体选择符、ZWJ 等）既没有字形也不推进笔位。
		// 与门控共用同一判定，保证「测出来的宽度」与「画出来的宽度」永远一致。
		if(!QmNameplateMsdfCodepointNeedsGlyph(Cp))
			continue;
		auto It = m_Glyphs.find(Cp);
		if(It == m_Glyphs.end())
		{
			// 字体链全部没有该码点时不切换整条铭牌到 FreeType，
			// 使用 ASCII '?' 保持 MSDF 路径、宽度和布局稳定。
			It = m_Glyphs.find('?');
			if(It == m_Glyphs.end())
				continue;
		}
		Width += It->second.m_Advance * Scale;
	}
	// 高度固定为行盒（ascent+descent），与字符串内容无关。
	// 若按墨迹包围盒居中，纯小写、含大写/带变音符的字符串基线各不相同，
	// 混排时小写与大写会被「居中对齐」而非「基线对齐」，明显违背排版。
	return vec2(Width, (m_RefAscent + m_RefDescent) * Scale);
}

float CQmNameplateMsdfRenderer::CanvasToScreenScale() const
{
	if(m_pGraphics == nullptr)
		return 1.0f;
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	m_pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float CanvasHeight = ScreenY1 - ScreenY0;
	const int ScreenHeightPx = m_pGraphics->ScreenHeight();
	if(CanvasHeight <= 0.0f || ScreenHeightPx <= 0)
		return 1.0f;
	return (float)ScreenHeightPx / CanvasHeight;
}

void CQmNameplateMsdfRenderer::EmitGlyphQuad(const SGlyph &Glyph, float X, float Y, float W, float H, const ColorRGBA &Color, float OutlineWidthPx)
{
	const SPage &Page = m_vPages[Glyph.m_Page];
	IGraphics::STexturedMsdfParams Params;
	Params.m_Texture = Page.m_Texture;
	Params.m_Rect = vec4(X, Y, W, H);
	// 只收紧到首尾 texel 的中心，保留完整 pxRange；整像素 inset 会让
	// 距离场缩放比例与字形四边形不一致，并在边缘制造波纹/白点。
	const float HalfTexelX = 0.5f / (float)Page.m_Info.m_Width;
	const float HalfTexelY = 0.5f / (float)Page.m_Info.m_Height;
	Params.m_UvRect = vec4(
		((float)Glyph.m_X / (float)Page.m_Info.m_Width) + HalfTexelX,
		((float)Glyph.m_Y / (float)Page.m_Info.m_Height) + HalfTexelY,
		((float)(Glyph.m_X + Glyph.m_W) / (float)Page.m_Info.m_Width) - HalfTexelX,
		((float)(Glyph.m_Y + Glyph.m_H) / (float)Page.m_Info.m_Height) - HalfTexelY);
	Params.m_Color = Color;
	Params.m_PxRange = Page.m_PxRange;
	Params.m_AtlasWidth = (float)Page.m_Info.m_Width;
	Params.m_AtlasHeight = (float)Page.m_Info.m_Height;
	Params.m_OutlineWidthPx = std::max(OutlineWidthPx, 0.0f);
	Params.m_UseTrueSdf = Glyph.m_UseTrueSdf;
	m_pGraphics->RenderTexturedMsdf(Params);
}

vec2 CQmNameplateMsdfRenderer::Draw(const char *pText, float X, float Y, float FontSize, const SQmNameplateMsdfTextStyle &Style)
{
	if(!m_Ready || pText == nullptr || FontSize <= 0.0f || m_RefEmPixels <= 0.0f)
		return vec2(0.0f, 0.0f);
	if(Style.m_TextColor.a <= 0.0f && Style.m_OutlineColor.a <= 0.0f)
		return vec2(0.0f, 0.0f);

	// 先收集字形，确保整条文本都可渲染（调用方负责整名回退判定，这里再兜一层）
	struct SPlaced
	{
		const SGlyph *m_pGlyph = nullptr;
		float m_PenX = 0.0f;
	};
	std::vector<SPlaced> vPlaced;
	vPlaced.reserve(str_length(pText));
	{
		const char *p = pText;
		float PenX = X;
		while(*p != '\0')
		{
			const uint32_t Cp = DecodeUtf8(p);
			if(Cp == 0)
				break;
			if(Cp == '\n' || Cp == '\r')
				break; // 名牌为单行文本，不支持换行
			if(Cp == '\t')
			{
				PenX += FontSize * 0.5f;
				continue;
			}
			if(Cp == ' ')
			{
				PenX += FontSize * 0.25f;
				continue;
			}
			// 零宽/不可见码点（变体选择符、ZWJ、双向控制符等）：不出 quad、不推进笔位。
			// 与 Measure 共用同一判定；否则「⭐️」会因为看不见的 U+FE0F 落到 '?' 上。
			if(!QmNameplateMsdfCodepointNeedsGlyph(Cp))
				continue;
			auto It = m_Glyphs.find(Cp);
			if(It == m_Glyphs.end())
				It = m_Glyphs.find('?');
			if(It == m_Glyphs.end())
				continue;
			if(It->second.m_HasOutline)
				vPlaced.push_back({&It->second, PenX});
			PenX += It->second.m_Advance * (FontSize / m_RefEmPixels);
		}
	}

	const float Scale = FontSize / m_RefEmPixels;
	// 基线固定为 Y + ascent（字体行盒度量），与字符串内容无关：
	// 大写、小写、变音符、中英混排都落在同一条基线上，符合排版。
	// 不能按本串最大 bearingY 推基线——那会让基线随内容漂移（纯小写串整体下沉）。
	const float Baseline = Y + m_RefAscent * Scale;

	const float TotalWidth = std::max(Measure(pText, FontSize).x, FontSize);
	// 辉光：先画一层低透明度的宽距离外扩，再画描边和填充；仍然是同一个 MTSDF quad，
	// 不会产生多份偏移字形造成的重影。
	if(Style.m_GlowEnabled && Style.m_GlowColor.a > 0.0f && Style.m_GlowWidth > 0.0f)
	{
		const float GlowWidthPx = CanvasToScreenScale() * Style.m_GlowWidth;
		for(const SPlaced &Placed : vPlaced)
		{
			const SGlyph &Glyph = *Placed.m_pGlyph;
			const SPage &Page = m_vPages[Glyph.m_Page];
			EmitGlyphQuad(Glyph,
				Placed.m_PenX + (Glyph.m_BearingX - Page.m_PxRange) * Scale,
				Baseline - (Glyph.m_BearingY + Page.m_PxRange) * Scale,
				Glyph.m_W * Scale * Glyph.m_MetricScale, Glyph.m_H * Scale * Glyph.m_MetricScale, Style.m_GlowColor, GlowWidthPx);
		}
	}

	// 描边：单 pass，8 向等距采样的覆盖并集（着色器内实现，半径 = OutlineWidthPx 屏幕像素，
	// 上限 4 texel 防止越采相邻字形/整块饱和）。得到一层均匀、实心、单色的边框，
	// 与位图字体的「自带粗体」观感一致；不再用多份偏移拷贝叠加——半透明拷贝重叠会叠色，边缘也参差。
	if(Style.m_OutlineColor.a > 0.0f && Style.m_OutlineWidth > 0.0f)
	{
		const float OutlineWidthPx = CanvasToScreenScale() * Style.m_OutlineWidth;
		for(const SPlaced &Placed : vPlaced)
		{
			const SGlyph &Glyph = *Placed.m_pGlyph;
			const SPage &Page = m_vPages[Glyph.m_Page];
			EmitGlyphQuad(Glyph,
				Placed.m_PenX + (Glyph.m_BearingX - Page.m_PxRange) * Scale,
				Baseline - (Glyph.m_BearingY + Page.m_PxRange) * Scale,
				Glyph.m_W * Scale * Glyph.m_MetricScale, Glyph.m_H * Scale * Glyph.m_MetricScale, Style.m_OutlineColor, OutlineWidthPx);
		}
	}

	// 主填充
	for(const SPlaced &Placed : vPlaced)
	{
		const SGlyph &Glyph = *Placed.m_pGlyph;
		const SPage &Page = m_vPages[Glyph.m_Page];
		ColorRGBA Color = Style.m_TextColor;
		if(Style.m_GradientEnabled)
		{
			const float Amount = std::clamp((Placed.m_PenX - X) / TotalWidth, 0.0f, 1.0f);
			Color = ColorRGBA(
				Style.m_TextColor.r + (Style.m_GradientColor.r - Style.m_TextColor.r) * Amount,
				Style.m_TextColor.g + (Style.m_GradientColor.g - Style.m_TextColor.g) * Amount,
				Style.m_TextColor.b + (Style.m_GradientColor.b - Style.m_TextColor.b) * Amount,
				Style.m_TextColor.a);
		}
		if(Style.m_RainbowEnabled)
		{
			const ColorRGBA Rb = RainbowAt(Style.m_RainbowTime + Placed.m_PenX * 0.01f);
			Color = ColorRGBA(Rb.r, Rb.g, Rb.b, Color.a);
		}
		EmitGlyphQuad(Glyph,
			Placed.m_PenX + (Glyph.m_BearingX - Page.m_PxRange) * Scale,
			Baseline - (Glyph.m_BearingY + Page.m_PxRange) * Scale,
			Glyph.m_W * Scale * Glyph.m_MetricScale, Glyph.m_H * Scale * Glyph.m_MetricScale, Color);
	}

	return Measure(pText, FontSize);
}

vec2 CQmNameplateMsdfRenderer::DrawCentered(const char *pText, float CenterX, float CenterY, float FontSize, const SQmNameplateMsdfTextStyle &Style)
{
	const vec2 Size = Measure(pText, FontSize);
	return Draw(pText, CenterX - Size.x * 0.5f, CenterY - Size.y * 0.5f, FontSize, Style);
}
