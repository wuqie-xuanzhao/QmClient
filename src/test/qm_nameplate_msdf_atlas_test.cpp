// 名牌 MSDF 图集资产测试：生成产物必须自洽，避免发布一个「能加载但画不对」的图集。
//
// 图集由 qmclient_scripts/qm_nameplate_msdf_build.py 离线生成（msdfgen + FreeType），
// 运行时不参与生成，因此这里直接校验提交进仓库的产物本身。
#include <engine/shared/json.h>

#include <game/client/components/qmclient/nameplate_msdf/qm_nameplate_msdf_manifest.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// PNG 是二进制文件，不能走 ReadTestSourceFile（它会剥除回车换行，破坏 PNG 签名）。
	std::string ReadBinaryTestFile(const char *pRelativePath)
	{
		const std::string Path = std::string(DDNET_TEST_SOURCE_DIR) + "/" + pRelativePath;
		std::ifstream File(Path, std::ios::binary);
		EXPECT_TRUE(File.good()) << Path;
		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		return Buffer.str();
	}

	constexpr const char *kBaseManifest = "data/qmclient/nameplate_msdf/nameplate_base_msdf.json";
	constexpr const char *kBaseImage = "data/qmclient/nameplate_msdf/nameplate_base_msdf.png";
	constexpr int kPublishedProfileCount = 0;
	constexpr const char *aProfiles[] = {nullptr};

	bool PublishedAtlasAvailable()
	{
		std::ifstream Manifest(std::string(DDNET_TEST_SOURCE_DIR) + "/" + kBaseManifest, std::ios::binary);
		std::ifstream Image(std::string(DDNET_TEST_SOURCE_DIR) + "/" + kBaseImage, std::ios::binary);
		return Manifest.good() && Image.good();
	}

	// PNG IHDR：宽高为偏移 16/20 的大端 32 位整数
	bool ReadPngSize(const std::string &Bytes, uint32_t &Width, uint32_t &Height)
	{
		static const unsigned char s_aSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
		if(Bytes.size() < 24)
			return false;
		if(mem_comp(Bytes.data(), s_aSignature, sizeof(s_aSignature)) != 0)
			return false;
		auto ReadBigEndian = [&](size_t Offset) {
			return (uint32_t)((unsigned char)Bytes[Offset] << 24) | ((uint32_t)(unsigned char)Bytes[Offset + 1] << 16) |
			       ((uint32_t)(unsigned char)Bytes[Offset + 2] << 8) | (uint32_t)(unsigned char)Bytes[Offset + 3];
		};
		Width = ReadBigEndian(16);
		Height = ReadBigEndian(20);
		return Width > 0 && Height > 0;
	}

	struct SAtlasFacts
	{
		int m_PxRange = 0;
		int m_EmPixels = 0;
		int m_Padding = 0;
		double m_Ascent = 0.0;
		double m_Descent = 0.0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		std::string m_Kind;
		std::string m_DistanceField;
		std::string m_Image;
	};

	// 校验单页 manifest 的结构与不变量，返回该页事实（解析失败时返回 false）
	bool CheckPage(const char *pManifestPath, const char *pImagePath, SAtlasFacts &Facts, size_t &GlyphCount)
	{
		const std::string Json = ReadTestSourceFile(pManifestPath);
		if(Json.empty())
			return false;

		json_value *pRoot = JsonParse(Json.c_str(), Json.size());
		if(pRoot == nullptr)
			return false;

		bool Ok = true;
		auto IntField = [&](const char *pName) {
			const json_value *pValue = json_object_get(pRoot, pName);
			if(pValue == nullptr || pValue->type != json_integer)
			{
				Ok = false;
				return 0;
			}
			return (int)pValue->u.integer;
		};
		auto NumField = [&](const char *pName) {
			const json_value *pValue = json_object_get(pRoot, pName);
			if(pValue == nullptr || (pValue->type != json_integer && pValue->type != json_double))
			{
				Ok = false;
				return 0.0;
			}
			return pValue->type == json_integer ? (double)pValue->u.integer : pValue->u.dbl;
		};
		Facts.m_PxRange = IntField("px_range");
		Facts.m_EmPixels = IntField("em_pixels");
		Facts.m_Padding = IntField("padding");
		// 行盒度量：基线排版（与内容无关的 ascent/descent）依赖这两个字段
		Facts.m_Ascent = NumField("ascent");
		Facts.m_Descent = NumField("descent");

		const json_value *pKind = json_object_get(pRoot, "kind");
		if(pKind != nullptr && pKind->type == json_string)
			Facts.m_Kind = pKind->u.string.ptr;
		const json_value *pDistanceField = json_object_get(pRoot, "distance_field");
		if(pDistanceField != nullptr && pDistanceField->type == json_string)
			Facts.m_DistanceField = pDistanceField->u.string.ptr;

		const json_value *pAtlas = json_object_get(pRoot, "atlas");
		const json_value *pGlyphs = json_object_get(pRoot, "glyphs");
		if(pAtlas == nullptr || pAtlas->type != json_object || pGlyphs == nullptr || pGlyphs->type != json_object)
			return false;

		const json_value *pWidth = json_object_get(pAtlas, "width");
		const json_value *pHeight = json_object_get(pAtlas, "height");
		const json_value *pImage = json_object_get(pAtlas, "image");
		if(pWidth == nullptr || pWidth->type != json_integer || pHeight == nullptr || pHeight->type != json_integer)
			return false;
		Facts.m_Width = (uint32_t)pWidth->u.integer;
		Facts.m_Height = (uint32_t)pHeight->u.integer;
		if(pImage != nullptr && pImage->type == json_string)
			Facts.m_Image = pImage->u.string.ptr;

		// 图形上传的是 RGBA 页，PNG 尺寸必须与 manifest 声明一致
		const std::string PngBytes = ReadBinaryTestFile(pImagePath);
		uint32_t PngWidth = 0;
		uint32_t PngHeight = 0;
		EXPECT_TRUE(ReadPngSize(PngBytes, PngWidth, PngHeight)) << pImagePath;
		EXPECT_EQ(PngWidth, Facts.m_Width) << pImagePath;
		EXPECT_EQ(PngHeight, Facts.m_Height) << pImagePath;

		GlyphCount = 0;
		for(unsigned i = 0; i < pGlyphs->u.object.length; ++i)
		{
			const json_value *pEntry = pGlyphs->u.object.values[i].value;
			if(pEntry == nullptr || pEntry->type != json_object)
			{
				Ok = false;
				continue;
			}
			const json_value *pOutline = json_object_get(pEntry, "outline");
			const bool HasOutline = pOutline != nullptr && pOutline->type == json_boolean && pOutline->u.boolean != 0;
			auto EntryInt = [&](const char *pName) {
				const json_value *pValue = json_object_get(pEntry, pName);
				return pValue != nullptr && pValue->type == json_integer ? (int)pValue->u.integer : 0;
			};
			if(HasOutline)
			{
				const int X = EntryInt("x");
				const int Y = EntryInt("y");
				const int W = EntryInt("w");
				const int H = EntryInt("h");
				if(X < 0 || Y < 0 || W <= 0 || H <= 0)
				{
					Ok = false;
					continue;
				}
				// 字形矩形必须完整落在图集内，否则采样会整块取错
				if((uint32_t)(X + W) > Facts.m_Width || (uint32_t)(Y + H) > Facts.m_Height)
				{
					Ok = false;
					continue;
				}
			}
			++GlyphCount;
		}
		json_value_free(pRoot);
		return Ok;
	}
}

TEST(QmNameplateMsdfAtlas, BuiltinProfilesReferenceExistingPages)
{
	if(kPublishedProfileCount == 0)
	{
		GTEST_SKIP() << "No published built-in MSDF profile is present yet";
	}
	for(int ProfileIndex = 0; ProfileIndex < kPublishedProfileCount; ++ProfileIndex)
	{
		const char *pProfile = aProfiles[ProfileIndex];
		SCOPED_TRACE(pProfile);
		char aProfilePath[IO_MAX_PATH_LENGTH];
		str_format(aProfilePath, sizeof(aProfilePath), "data/qmclient/nameplate_msdf/profiles/nameplate_%s.json", pProfile);
		const std::string ProfileJson = ReadTestSourceFile(aProfilePath);
		ASSERT_FALSE(ProfileJson.empty()) << aProfilePath;
		json_value *pRoot = JsonParse(ProfileJson.c_str(), ProfileJson.size());
		ASSERT_NE(pRoot, nullptr) << aProfilePath;
		const json_value *pPages = json_object_get(pRoot, "pages");
		ASSERT_NE(pPages, nullptr) << aProfilePath;
		ASSERT_EQ(pPages->type, json_array) << aProfilePath;
		ASSERT_GT(pPages->u.array.length, 0u) << aProfilePath;
		for(unsigned i = 0; i < pPages->u.array.length; ++i)
		{
			ASSERT_NE(pPages->u.array.values[i], nullptr) << aProfilePath;
			ASSERT_EQ(pPages->u.array.values[i]->type, json_string) << aProfilePath;
			const std::string PagePath = pPages->u.array.values[i]->u.string.ptr;
			const std::string SourcePagePath = PagePath.rfind("data/", 0) == 0 ? PagePath : "data/" + PagePath;
			EXPECT_FALSE(ReadTestSourceFile(SourcePagePath.c_str()).empty()) << SourcePagePath;
		}
		json_value_free(pRoot);
	}
}

// pxRange 之外的 padding 必须留足，否则字形边缘的双线性采样会串到邻居字形上

// 基础页必须覆盖 ASCII（含空格），否则最常见的名字会整条回退
TEST(QmNameplateMsdfAtlas, BasePageCoversAscii)
{
	if(!PublishedAtlasAvailable())
		GTEST_SKIP() << "No published built-in MSDF profile is present yet";
	const std::string Json = ReadTestSourceFile(kBaseManifest);
	ASSERT_FALSE(Json.empty());
	json_value *pRoot = JsonParse(Json.c_str(), Json.size());
	ASSERT_NE(pRoot, nullptr);
	const json_value *pGlyphs = json_object_get(pRoot, "glyphs");
	ASSERT_NE(pGlyphs, nullptr);

	for(uint32_t Codepoint = 0x20; Codepoint <= 0x7E; ++Codepoint)
	{
		char aKey[16];
		str_format(aKey, sizeof(aKey), "%u", Codepoint);
		const json_value *pEntry = json_object_get(pGlyphs, aKey);
		EXPECT_NE(pEntry, nullptr) << "missing U+" << std::hex << Codepoint;
		if(pEntry == nullptr || pEntry->type != json_object)
			continue;
		// 空格没有轮廓，但必须有推进宽度，否则排版会塌缩
		const json_value *pAdvance = json_object_get(pEntry, "adv");
		ASSERT_NE(pAdvance, nullptr) << aKey;
		EXPECT_EQ(pAdvance->type, json_double) << aKey;
		EXPECT_GT(pAdvance->u.dbl, 0.0) << aKey;
		if(Codepoint != 0x20)
		{
			const json_value *pOutline = json_object_get(pEntry, "outline");
			ASSERT_NE(pOutline, nullptr) << aKey;
			EXPECT_EQ(pOutline->u.boolean, 1) << aKey;
		}
	}
	json_value_free(pRoot);
}

TEST(QmNameplateMsdfAtlas, ManifestsAreSelfConsistent)
{
	if(!PublishedAtlasAvailable())
		GTEST_SKIP() << "No published built-in MSDF profile is present yet";
	SAtlasFacts BaseFacts;
	size_t BaseGlyphs = 0;
	EXPECT_TRUE(CheckPage(kBaseManifest, kBaseImage, BaseFacts, BaseGlyphs));
	EXPECT_EQ(BaseFacts.m_Kind, "msdf-glyphs");
	EXPECT_EQ(BaseFacts.m_DistanceField, "mtsdf");
	EXPECT_GT(BaseFacts.m_PxRange, 0);
	EXPECT_GT(BaseFacts.m_EmPixels, 0);
	EXPECT_GE(BaseFacts.m_Padding, BaseFacts.m_PxRange + 1);
	EXPECT_GT(BaseFacts.m_Ascent, 0.0);
	EXPECT_GT(BaseFacts.m_Descent, 0.0);
	EXPECT_GT(BaseFacts.m_Ascent, BaseFacts.m_Descent);
	EXPECT_GT(BaseGlyphs, 700u);
}

namespace
{
	struct SRuntimeManifestScan
	{
		bool m_Found = false;
		double m_PxRange = 0.0;
		double m_EmPixels = 0.0;
		double m_AtlasWidth = 0.0;
		double m_AtlasHeight = 0.0;
		double m_Ascent = 0.0;
		double m_Descent = 0.0;
		std::string m_Image;
	};

	// 复刻运行时解析器的取值顺序，但用共享实现扫描真实产物。
	SRuntimeManifestScan ScanManifestWithRuntimeHelpers(const std::string &Json)
	{
		SRuntimeManifestScan Scan;
		const char *pBegin = Json.c_str();
		const char *pEnd = pBegin + Json.size();
		const char *pPxRange = QmNameplateMsdfFindKey(pBegin, pEnd, "px_range");
		const char *pEmPixels = QmNameplateMsdfFindKey(pBegin, pEnd, "em_pixels");
		const char *pImage = QmNameplateMsdfFindKey(pBegin, pEnd, "image");
		if(pPxRange == nullptr || pEmPixels == nullptr || pImage == nullptr)
			return Scan;
		if(!QmNameplateMsdfReadDouble(pPxRange, pEnd, Scan.m_PxRange) ||
			!QmNameplateMsdfReadDouble(pEmPixels, pEnd, Scan.m_EmPixels) ||
			!QmNameplateMsdfReadString(pImage, pEnd, Scan.m_Image))
			return Scan;

		const char *pAtlas = QmNameplateMsdfFindKey(pBegin, pEnd, "\"atlas\"");
		if(pAtlas == nullptr)
			return Scan;
		const char *pAtlasEnd = pAtlas;
		while(pAtlasEnd < pEnd && *pAtlasEnd != '}')
			++pAtlasEnd;
		const char *pWidth = QmNameplateMsdfFindKey(pAtlas, pAtlasEnd, "width");
		const char *pHeight = QmNameplateMsdfFindKey(pAtlas, pAtlasEnd, "height");
		if(pWidth == nullptr || pHeight == nullptr ||
			!QmNameplateMsdfReadDouble(pWidth, pAtlasEnd, Scan.m_AtlasWidth) ||
			!QmNameplateMsdfReadDouble(pHeight, pAtlasEnd, Scan.m_AtlasHeight))
			return Scan;

		// 行盒度量（可选字段）：键必须带引号定位——"ascent" 不是 "descent" 的子串，
		// 带前引号才不会互相误匹配；解析失败/缺失保持 0，运行时按 cap 高度近似。
		const char *pAscent = QmNameplateMsdfFindKey(pBegin, pEnd, "\"ascent\"");
		const char *pDescent = QmNameplateMsdfFindKey(pBegin, pEnd, "\"descent\"");
		if(pAscent != nullptr)
			QmNameplateMsdfReadDouble(pAscent, pEnd, Scan.m_Ascent);
		if(pDescent != nullptr)
			QmNameplateMsdfReadDouble(pDescent, pEnd, Scan.m_Descent);

		Scan.m_Found = true;
		return Scan;
	}

	struct SRuntimeGlyphScan
	{
		bool m_Found = false;
		double m_Advance = 0.0;
		double m_X = 0.0;
		double m_Y = 0.0;
		double m_W = 0.0;
		double m_H = 0.0;
		bool m_HasOutline = false;
	};

	// 用运行时同一个 glyphs 扫描器遍历整块，返回条目数并顺带记录指定码点的度量。
	int ScanGlyphsWithRuntimeHelpers(const std::string &Json, uint32_t WantedCodepoint, SRuntimeGlyphScan &Wanted)
	{
		const char *pBegin = Json.c_str();
		const char *pEnd = pBegin + Json.size();
		const char *pGlyphs = QmNameplateMsdfFindKey(pBegin, pEnd, "\"glyphs\"");
		if(pGlyphs == nullptr)
			return 0;
		return QmNameplateMsdfForEachGlyphObject(pGlyphs, pEnd, [&](uint32_t Codepoint, const char *pObjectBegin, const char *pObjectEnd) {
			auto Field = [&](const char *pName, double &Out) {
				const char *pField = QmNameplateMsdfFindKey(pObjectBegin, pObjectEnd, pName);
				return pField != nullptr && QmNameplateMsdfReadDouble(pField, pObjectEnd, Out);
			};
			if(Codepoint == WantedCodepoint)
			{
				SRuntimeGlyphScan Scan;
				Scan.m_Found = Field("\"adv\"", Scan.m_Advance) && Field("\"x\"", Scan.m_X) &&
					       Field("\"y\"", Scan.m_Y) && Field("\"w\"", Scan.m_W) && Field("\"h\"", Scan.m_H);
				const char *pOutline = QmNameplateMsdfFindKey(pObjectBegin, pObjectEnd, "\"outline\"");
				if(pOutline != nullptr)
					QmNameplateMsdfReadBool(pOutline, pObjectEnd, Scan.m_HasOutline);
				Wanted = Scan;
			}
			return true;
		});
	}
}

// 运行时解析器契约：manifest 是标准 JSON（"key": value）。手写扫描必须在 key 之后
// 跳过结束引号与冒号，否则取值一律失败。这条曾经漏掉，导致「资产测试全绿、
// 运行时整包解析失败、MSDF 从未真正启用」，整屏铭牌退到 FreeType 发虚。
TEST(QmNameplateMsdfAtlas, RuntimeScannerReadsManifestFields)
{
	if(!PublishedAtlasAvailable())
		GTEST_SKIP() << "No published built-in MSDF profile is present yet";
	const std::pair<const char *, const char *> aPages[] = {
		{kBaseManifest, kBaseImage},
	};
	for(const auto &Page : aPages)
	{
		SCOPED_TRACE(Page.first);
		const std::string Json = ReadTestSourceFile(Page.first);
		ASSERT_FALSE(Json.empty()) << Page.first;
		const SRuntimeManifestScan Scan = ScanManifestWithRuntimeHelpers(Json);
		ASSERT_TRUE(Scan.m_Found) << Page.first;
		EXPECT_GT(Scan.m_PxRange, 0.0) << Page.first;
		EXPECT_GT(Scan.m_EmPixels, 0.0) << Page.first;
		// 带引号的键扫描必须分别取到两个度量，且 ascent > descent（基线排版的合理性）
		EXPECT_GT(Scan.m_Ascent, 0.0) << Page.first;
		EXPECT_GT(Scan.m_Descent, 0.0) << Page.first;
		EXPECT_GT(Scan.m_Ascent, Scan.m_Descent) << Page.first;

		// image 字段必须能被整体取出，且文件名与随包页图一致（运行时取其文件名与 manifest 同目录拼接）
		ASSERT_FALSE(Scan.m_Image.empty()) << Page.first;
		const size_t ImageSlash = Scan.m_Image.find_last_of('/');
		const std::string ImageName = ImageSlash == std::string::npos ? Scan.m_Image : Scan.m_Image.substr(ImageSlash + 1);
		const std::string ExpectedName(Page.second);
		EXPECT_EQ(ImageName, ExpectedName.substr(ExpectedName.find_last_of('/') + 1)) << Page.first;

		// 运行时读到的图集尺寸必须与真实页图一致，否则 UV 会整块取错
		const std::string PngBytes = ReadBinaryTestFile(Page.second);
		uint32_t PngWidth = 0;
		uint32_t PngHeight = 0;
		ASSERT_TRUE(ReadPngSize(PngBytes, PngWidth, PngHeight)) << Page.second;
		EXPECT_EQ(Scan.m_AtlasWidth, (double)PngWidth) << Page.second;
		EXPECT_EQ(Scan.m_AtlasHeight, (double)PngHeight) << Page.second;
	}
}

// 字形度量字段同样必须能读出来；读到 0 会让测量塌缩、四边形退化到画不出来。
// 同时用运行时扫描器统计整块条目数，阈值与上面的 JSON 解析口径一致。
TEST(QmNameplateMsdfAtlas, RuntimeScannerReadsGlyphFields)
{
	if(!PublishedAtlasAvailable())
		GTEST_SKIP() << "No published built-in MSDF profile is present yet";
	const std::pair<const char *, size_t> aPages[] = {
		{kBaseManifest, 700u},
	};
	for(const auto &Page : aPages)
	{
		SCOPED_TRACE(Page.first);
		const std::string Json = ReadTestSourceFile(Page.first);
		ASSERT_FALSE(Json.empty()) << Page.first;

		SRuntimeGlyphScan Glyph;
		const int Count = ScanGlyphsWithRuntimeHelpers(Json, 100, Glyph);
		EXPECT_GT(Count, (int)Page.second) << Page.first;

		if(Page.first == std::string(kBaseManifest))
		{
			ASSERT_TRUE(Glyph.m_Found) << Page.first;
			EXPECT_GT(Glyph.m_Advance, 0.0);
			EXPECT_GT(Glyph.m_W, 0.0);
			EXPECT_GT(Glyph.m_H, 0.0);
			EXPECT_GE(Glyph.m_X, 0.0);
			EXPECT_GE(Glyph.m_Y, 0.0);
			EXPECT_TRUE(Glyph.m_HasOutline);
		}
	}
}
