#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MUSIC_LYRICS_MUSIC_LYRICS_QRC_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MUSIC_LYRICS_MUSIC_LYRICS_QRC_H

#include "music_lyrics_model.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// QQ 音乐 QRC 歌词解密与解析(纯函数,无平台依赖,可单测)。
// 解密链路(已用真实样本实测复现,见 tmp/verify_qrc.py):
//   1. 文件头 11 字节魔数 98 25 B0 AC E3 02 83 68 E8 FC 6C
//   2. 全文件 QMC1 XOR(128 字节 PRIVKEY)
//   3. 去掉魔数后按 8 字节分组做自定义 3DES-EDE 解密(密钥 !@#)(*$%123ZXC!@!@#)(NHL)
//   4. zlib inflate -> UTF-8
// 主歌词 .qrc 解密后是 XML,LyricContent 属性内为 rlrc 逐字文本:
//   [行起始ms,行时长ms]字(字起始ms,字时长ms)...
// 翻译 _qmts.qrc 解密后是纯文本 LRC([mm:ss.xx] 行级时间戳)。
namespace QmMusicLyrics
{
	// 自定义 3DES-EDE 解密(QQ 音乐专用变体,标准 DES 库不兼容)。
	// 供测试构造与上层复用;内部实现为位运算移植(参考 QQMusicDecoder DESHelper.cs)。
	bool QrcTripleDesDecrypt(const uint8_t *pData, size_t Size, const uint8_t *pKey24, std::vector<uint8_t> *pOut);

	// 解密 QRC 文件字节。成功时 pOutText 为解密后的 UTF-8 文本(主歌词为 XML,翻译为 LRC)。
	bool DecryptQrc(std::string_view Data, std::string *pOutText, std::string *pError = nullptr);

	// 从解密后的 XML 提取 LyricContent 属性值(反转义 XML 实体)。
	bool ExtractQrcLyricContent(std::string_view Xml, std::string *pOut, std::string *pError = nullptr);

	// 解析 rlrc 逐字文本: [行起始ms,行时长ms]字(字起始ms,字时长ms)...
	bool ParseQrcRlrc(std::string_view Text, NeteaseLyrics::STimeline *pOut, std::string *pError = nullptr);

	// 组合入口:解密 .qrc 文件字节 + 提取 LyricContent + 解析时间轴。
	bool ParseQrcData(std::string_view Data, SLyricsData *pOut, std::string *pError = nullptr);
} // namespace QmMusicLyrics

#endif
