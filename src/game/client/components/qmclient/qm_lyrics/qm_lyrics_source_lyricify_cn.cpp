#include "qm_lyrics_source_lyricify_cn.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/external/zlib/zlib.h>
#include <engine/http.h>
#include <engine/shared/http.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

namespace QmLyrics
{

	namespace
	{

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage != nullptr ? pMessage : "", ErrSize);
		}

		void UrlEncode(std::string_view In, std::string *pOut)
		{
			for(unsigned char C : In)
			{
				const bool Unreserved = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
							(C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~';
				if(Unreserved)
					pOut->push_back((char)C);
				else
				{
					char aBuf[4];
					str_format(aBuf, sizeof(aBuf), "%%%02X", (int)C);
					pOut->append(aBuf);
				}
			}
		}

		void JsonEscape(std::string_view In, std::string *pOut)
		{
			for(char C : In)
			{
				switch(C)
				{
				case '"': pOut->append("\\\""); break;
				case '\\': pOut->append("\\\\"); break;
				case '\b': pOut->append("\\b"); break;
				case '\f': pOut->append("\\f"); break;
				case '\n': pOut->append("\\n"); break;
				case '\r': pOut->append("\\r"); break;
				case '\t': pOut->append("\\t"); break;
				default:
					if((unsigned char)C < 0x20)
					{
						char aBuf[8];
						str_format(aBuf, sizeof(aBuf), "\\u%04x", (unsigned)(unsigned char)C);
						pOut->append(aBuf);
					}
					else
					{
						pOut->push_back(C);
					}
				}
			}
		}

		std::string BuildKeyword(const SSourceQuery &Query)
		{
			std::string Keyword = Query.m_Title;
			if(!Query.m_Artist.empty())
			{
				if(!Keyword.empty())
					Keyword.push_back(' ');
				Keyword.append(Query.m_Artist);
			}
			return Keyword;
		}

		bool ContainsNoCase(std::string_view Haystack, const char *pNeedle)
		{
			if(Haystack.empty() || pNeedle == nullptr || pNeedle[0] == '\0')
				return false;
			const std::string Copy(Haystack);
			return str_find_nocase(Copy.c_str(), pNeedle) != nullptr;
		}

		bool StartsWithNoCase(std::string_view Haystack, const char *pNeedle)
		{
			if(Haystack.empty() || pNeedle == nullptr || pNeedle[0] == '\0')
				return false;
			const std::string Copy(Haystack);
			return str_startswith_nocase(Copy.c_str(), pNeedle) != nullptr;
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int64_t JsonInt(const json_value *pVal, int64_t Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return pVal->u.integer;
			if(pVal->type == json_double)
				return (int64_t)pVal->u.dbl;
			if(pVal->type == json_string)
			{
				char *pEnd = nullptr;
				const int64_t Value = std::strtoll(pVal->u.string.ptr, &pEnd, 10);
				return pEnd != pVal->u.string.ptr ? Value : Default;
			}
			return Default;
		}

		std::string JsonId(const json_value *pVal)
		{
			if(pVal == nullptr)
				return {};
			if(pVal->type == json_string)
				return pVal->u.string.ptr;
			if(pVal->type == json_integer)
			{
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%lld", (long long)pVal->u.integer);
				return aBuf;
			}
			return {};
		}

		std::string JoinNameArray(const json_value &Items)
		{
			if(Items.type != json_array)
				return {};
			std::string Out;
			for(unsigned int i = 0; i < Items.u.array.length; ++i)
			{
				const json_value *pItem = Items.u.array.values[i];
				if(pItem == nullptr || pItem->type != json_object)
					continue;
				const char *pName = JsonString(&(*pItem)["name"]);
				if(pName[0] == '\0')
					pName = JsonString(&(*pItem)["title"]);
				if(pName[0] == '\0')
					continue;
				if(!Out.empty())
					Out.append(" / ");
				Out.append(pName);
			}
			return Out;
		}

		bool Base64Decode(std::string_view In, std::string *pOut)
		{
			static unsigned char s_aDecode[256];
			static bool s_Init = false;
			if(!s_Init)
			{
				std::memset(s_aDecode, 0xFF, sizeof(s_aDecode));
				const char *pAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				for(unsigned char i = 0; i < 64; ++i)
					s_aDecode[(unsigned char)pAlphabet[i]] = i;
				s_Init = true;
			}
			std::vector<unsigned char> vOut;
			vOut.reserve(In.size() * 3 / 4);
			unsigned int Acc = 0;
			int Bits = 0;
			for(char C : In)
			{
				if(C == '=' || C == '\n' || C == '\r' || C == ' ' || C == '\t')
					continue;
				const unsigned char V = s_aDecode[(unsigned char)C];
				if(V == 0xFF)
					return false;
				Acc = (Acc << 6) | V;
				Bits += 6;
				if(Bits >= 8)
				{
					Bits -= 8;
					vOut.push_back((unsigned char)((Acc >> Bits) & 0xFF));
				}
			}
			pOut->assign((const char *)vOut.data(), vOut.size());
			if(pOut->size() >= 3 && (unsigned char)(*pOut)[0] == 0xEF &&
				(unsigned char)(*pOut)[1] == 0xBB && (unsigned char)(*pOut)[2] == 0xBF)
				pOut->erase(0, 3);
			return true;
		}

		bool ParseJsonRoot(const char *pBody, size_t BodyLen, json_value **ppRoot, char *pErr, size_t ErrSize)
		{
			json_settings Settings{};
			char aJsonErr[json_error_max];
			*ppRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
			if(*ppRoot == nullptr)
			{
				SetError(pErr, ErrSize, aJsonErr);
				return false;
			}
			if((*ppRoot)->type != json_object)
			{
				json_value_free(*ppRoot);
				*ppRoot = nullptr;
				SetError(pErr, ErrSize, "root is not object");
				return false;
			}
			return true;
		}

		std::string ExtractJsonp(std::string_view Body)
		{
			const size_t LParen = Body.find('(');
			const size_t RParen = Body.rfind(')');
			if(LParen == std::string_view::npos || RParen == std::string_view::npos || RParen <= LParen)
				return std::string(Body);
			return std::string(Body.substr(LParen + 1, RParen - LParen - 1));
		}

		int HexValue(char C)
		{
			if(C >= '0' && C <= '9')
				return C - '0';
			if(C >= 'a' && C <= 'f')
				return C - 'a' + 10;
			if(C >= 'A' && C <= 'F')
				return C - 'A' + 10;
			return -1;
		}

		bool HexDecode(std::string_view In, std::vector<unsigned char> *pOut)
		{
			if(pOut == nullptr || In.size() % 2 != 0)
				return false;
			pOut->clear();
			pOut->reserve(In.size() / 2);
			for(size_t i = 0; i < In.size(); i += 2)
			{
				const int Hi = HexValue(In[i]);
				const int Lo = HexValue(In[i + 1]);
				if(Hi < 0 || Lo < 0)
					return false;
				pOut->push_back((unsigned char)((Hi << 4) | Lo));
			}
			return true;
		}

		constexpr unsigned char AES_SBOX[256] = {
			0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
			0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
			0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
			0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
			0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
			0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
			0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
			0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
			0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
			0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
			0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
			0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
			0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
			0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
			0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
			0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

		constexpr unsigned char AES_RCON[11] = {
			0x00,
			0x01,
			0x02,
			0x04,
			0x08,
			0x10,
			0x20,
			0x40,
			0x80,
			0x1b,
			0x36};

		unsigned char AesXtime(unsigned char Value)
		{
			return (unsigned char)((Value << 1) ^ ((Value & 0x80) != 0 ? 0x1b : 0x00));
		}

		void AesKeyExpansion(const unsigned char *pKey, std::array<unsigned char, 176> *pRoundKey)
		{
			std::copy(pKey, pKey + 16, pRoundKey->begin());
			int BytesGenerated = 16;
			int RconIteration = 1;
			unsigned char aTemp[4];
			while(BytesGenerated < (int)pRoundKey->size())
			{
				for(int i = 0; i < 4; ++i)
					aTemp[i] = (*pRoundKey)[BytesGenerated - 4 + i];
				if(BytesGenerated % 16 == 0)
				{
					const unsigned char First = aTemp[0];
					aTemp[0] = (unsigned char)(AES_SBOX[aTemp[1]] ^ AES_RCON[RconIteration++]);
					aTemp[1] = AES_SBOX[aTemp[2]];
					aTemp[2] = AES_SBOX[aTemp[3]];
					aTemp[3] = AES_SBOX[First];
				}
				for(unsigned char Value : aTemp)
				{
					(*pRoundKey)[BytesGenerated] = (unsigned char)((*pRoundKey)[BytesGenerated - 16] ^ Value);
					++BytesGenerated;
				}
			}
		}

		void AesAddRoundKey(unsigned char *pState, const std::array<unsigned char, 176> &RoundKey, int Round)
		{
			for(int i = 0; i < 16; ++i)
				pState[i] ^= RoundKey[Round * 16 + i];
		}

		void AesSubBytes(unsigned char *pState)
		{
			for(int i = 0; i < 16; ++i)
				pState[i] = AES_SBOX[pState[i]];
		}

		void AesShiftRows(unsigned char *pState)
		{
			unsigned char Temp;
			Temp = pState[1];
			pState[1] = pState[5];
			pState[5] = pState[9];
			pState[9] = pState[13];
			pState[13] = Temp;

			Temp = pState[2];
			pState[2] = pState[10];
			pState[10] = Temp;
			Temp = pState[6];
			pState[6] = pState[14];
			pState[14] = Temp;

			Temp = pState[15];
			pState[15] = pState[11];
			pState[11] = pState[7];
			pState[7] = pState[3];
			pState[3] = Temp;
		}

		void AesMixColumns(unsigned char *pState)
		{
			for(int Col = 0; Col < 4; ++Col)
			{
				unsigned char *pColumn = pState + Col * 4;
				const unsigned char T = pColumn[0] ^ pColumn[1] ^ pColumn[2] ^ pColumn[3];
				const unsigned char U = pColumn[0];
				pColumn[0] ^= T ^ AesXtime((unsigned char)(pColumn[0] ^ pColumn[1]));
				pColumn[1] ^= T ^ AesXtime((unsigned char)(pColumn[1] ^ pColumn[2]));
				pColumn[2] ^= T ^ AesXtime((unsigned char)(pColumn[2] ^ pColumn[3]));
				pColumn[3] ^= T ^ AesXtime((unsigned char)(pColumn[3] ^ U));
			}
		}

		void AesEncryptBlock(unsigned char *pBlock, const std::array<unsigned char, 176> &RoundKey)
		{
			AesAddRoundKey(pBlock, RoundKey, 0);
			for(int Round = 1; Round <= 9; ++Round)
			{
				AesSubBytes(pBlock);
				AesShiftRows(pBlock);
				AesMixColumns(pBlock);
				AesAddRoundKey(pBlock, RoundKey, Round);
			}
			AesSubBytes(pBlock);
			AesShiftRows(pBlock);
			AesAddRoundKey(pBlock, RoundKey, 10);
		}

		std::string Base64Encode(const std::vector<unsigned char> &vData)
		{
			if(vData.empty())
				return {};
			const int OutSize = (int)((vData.size() + 2) / 3 * 4 + 1);
			std::string Out(OutSize, '\0');
			str_base64(Out.data(), OutSize, vData.data(), (int)vData.size());
			Out.resize(str_length(Out.c_str()));
			return Out;
		}

		std::string Aes128CbcPkcs7Base64(std::string_view Plain, const char *pKey)
		{
			constexpr char NETEASE_WEAPI_IV[] = "0102030405060708";
			if(pKey == nullptr || str_length(pKey) != 16)
				return {};
			std::array<unsigned char, 176> RoundKey;
			AesKeyExpansion((const unsigned char *)pKey, &RoundKey);

			std::vector<unsigned char> vBuffer(Plain.begin(), Plain.end());
			const size_t Pad = 16 - (vBuffer.size() % 16);
			vBuffer.insert(vBuffer.end(), Pad, (unsigned char)Pad);
			std::vector<unsigned char> vOut;
			vOut.resize(vBuffer.size());
			std::array<unsigned char, 16> Prev{};
			std::copy(NETEASE_WEAPI_IV, NETEASE_WEAPI_IV + 16, Prev.begin());
			for(size_t Offset = 0; Offset < vBuffer.size(); Offset += 16)
			{
				unsigned char aBlock[16];
				for(int i = 0; i < 16; ++i)
					aBlock[i] = (unsigned char)(vBuffer[Offset + i] ^ Prev[i]);
				AesEncryptBlock(aBlock, RoundKey);
				std::copy(aBlock, aBlock + 16, vOut.begin() + Offset);
				std::copy(aBlock, aBlock + 16, Prev.begin());
			}
			return Base64Encode(vOut);
		}

		constexpr int DES_IP[64] = {
			58,
			50,
			42,
			34,
			26,
			18,
			10,
			2,
			60,
			52,
			44,
			36,
			28,
			20,
			12,
			4,
			62,
			54,
			46,
			38,
			30,
			22,
			14,
			6,
			64,
			56,
			48,
			40,
			32,
			24,
			16,
			8,
			57,
			49,
			41,
			33,
			25,
			17,
			9,
			1,
			59,
			51,
			43,
			35,
			27,
			19,
			11,
			3,
			61,
			53,
			45,
			37,
			29,
			21,
			13,
			5,
			63,
			55,
			47,
			39,
			31,
			23,
			15,
			7,
		};

		constexpr int DES_FP[64] = {
			40,
			8,
			48,
			16,
			56,
			24,
			64,
			32,
			39,
			7,
			47,
			15,
			55,
			23,
			63,
			31,
			38,
			6,
			46,
			14,
			54,
			22,
			62,
			30,
			37,
			5,
			45,
			13,
			53,
			21,
			61,
			29,
			36,
			4,
			44,
			12,
			52,
			20,
			60,
			28,
			35,
			3,
			43,
			11,
			51,
			19,
			59,
			27,
			34,
			2,
			42,
			10,
			50,
			18,
			58,
			26,
			33,
			1,
			41,
			9,
			49,
			17,
			57,
			25,
		};

		constexpr int DES_E[48] = {
			32,
			1,
			2,
			3,
			4,
			5,
			4,
			5,
			6,
			7,
			8,
			9,
			8,
			9,
			10,
			11,
			12,
			13,
			12,
			13,
			14,
			15,
			16,
			17,
			16,
			17,
			18,
			19,
			20,
			21,
			20,
			21,
			22,
			23,
			24,
			25,
			24,
			25,
			26,
			27,
			28,
			29,
			28,
			29,
			30,
			31,
			32,
			1,
		};

		constexpr int DES_P[32] = {
			16,
			7,
			20,
			21,
			29,
			12,
			28,
			17,
			1,
			15,
			23,
			26,
			5,
			18,
			31,
			10,
			2,
			8,
			24,
			14,
			32,
			27,
			3,
			9,
			19,
			13,
			30,
			6,
			22,
			11,
			4,
			25,
		};

		constexpr int DES_PC1[56] = {
			57,
			49,
			41,
			33,
			25,
			17,
			9,
			1,
			58,
			50,
			42,
			34,
			26,
			18,
			10,
			2,
			59,
			51,
			43,
			35,
			27,
			19,
			11,
			3,
			60,
			52,
			44,
			36,
			63,
			55,
			47,
			39,
			31,
			23,
			15,
			7,
			62,
			54,
			46,
			38,
			30,
			22,
			14,
			6,
			61,
			53,
			45,
			37,
			29,
			21,
			13,
			5,
			28,
			20,
			12,
			4,
		};

		constexpr int DES_PC2[48] = {
			14,
			17,
			11,
			24,
			1,
			5,
			3,
			28,
			15,
			6,
			21,
			10,
			23,
			19,
			12,
			4,
			26,
			8,
			16,
			7,
			27,
			20,
			13,
			2,
			41,
			52,
			31,
			37,
			47,
			55,
			30,
			40,
			51,
			45,
			33,
			48,
			44,
			49,
			39,
			56,
			34,
			53,
			46,
			42,
			50,
			36,
			29,
			32,
		};

		constexpr int DES_SHIFTS[16] = {
			1,
			1,
			2,
			2,
			2,
			2,
			2,
			2,
			1,
			2,
			2,
			2,
			2,
			2,
			2,
			1,
		};

		constexpr unsigned char DES_SBOX[8][64] = {
			{
				14,
				4,
				13,
				1,
				2,
				15,
				11,
				8,
				3,
				10,
				6,
				12,
				5,
				9,
				0,
				7,
				0,
				15,
				7,
				4,
				14,
				2,
				13,
				1,
				10,
				6,
				12,
				11,
				9,
				5,
				3,
				8,
				4,
				1,
				14,
				8,
				13,
				6,
				2,
				11,
				15,
				12,
				9,
				7,
				3,
				10,
				5,
				0,
				15,
				12,
				8,
				2,
				4,
				9,
				1,
				7,
				5,
				11,
				3,
				14,
				10,
				0,
				6,
				13,
			},
			{
				15,
				1,
				8,
				14,
				6,
				11,
				3,
				4,
				9,
				7,
				2,
				13,
				12,
				0,
				5,
				10,
				3,
				13,
				4,
				7,
				15,
				2,
				8,
				14,
				12,
				0,
				1,
				10,
				6,
				9,
				11,
				5,
				0,
				14,
				7,
				11,
				10,
				4,
				13,
				1,
				5,
				8,
				12,
				6,
				9,
				3,
				2,
				15,
				13,
				8,
				10,
				1,
				3,
				15,
				4,
				2,
				11,
				6,
				7,
				12,
				0,
				5,
				14,
				9,
			},
			{
				10,
				0,
				9,
				14,
				6,
				3,
				15,
				5,
				1,
				13,
				12,
				7,
				11,
				4,
				2,
				8,
				13,
				7,
				0,
				9,
				3,
				4,
				6,
				10,
				2,
				8,
				5,
				14,
				12,
				11,
				15,
				1,
				13,
				6,
				4,
				9,
				8,
				15,
				3,
				0,
				11,
				1,
				2,
				12,
				5,
				10,
				14,
				7,
				1,
				10,
				13,
				0,
				6,
				9,
				8,
				7,
				4,
				15,
				14,
				3,
				11,
				5,
				2,
				12,
			},
			{
				7,
				13,
				14,
				3,
				0,
				6,
				9,
				10,
				1,
				2,
				8,
				5,
				11,
				12,
				4,
				15,
				13,
				8,
				11,
				5,
				6,
				15,
				0,
				3,
				4,
				7,
				2,
				12,
				1,
				10,
				14,
				9,
				10,
				6,
				9,
				0,
				12,
				11,
				7,
				13,
				15,
				1,
				3,
				14,
				5,
				2,
				8,
				4,
				3,
				15,
				0,
				6,
				10,
				1,
				13,
				8,
				9,
				4,
				5,
				11,
				12,
				7,
				2,
				14,
			},
			{
				2,
				12,
				4,
				1,
				7,
				10,
				11,
				6,
				8,
				5,
				3,
				15,
				13,
				0,
				14,
				9,
				14,
				11,
				2,
				12,
				4,
				7,
				13,
				1,
				5,
				0,
				15,
				10,
				3,
				9,
				8,
				6,
				4,
				2,
				1,
				11,
				10,
				13,
				7,
				8,
				15,
				9,
				12,
				5,
				6,
				3,
				0,
				14,
				11,
				8,
				12,
				7,
				1,
				14,
				2,
				13,
				6,
				15,
				0,
				9,
				10,
				4,
				5,
				3,
			},
			{
				12,
				1,
				10,
				15,
				9,
				2,
				6,
				8,
				0,
				13,
				3,
				4,
				14,
				7,
				5,
				11,
				10,
				15,
				4,
				2,
				7,
				12,
				9,
				5,
				6,
				1,
				13,
				14,
				0,
				11,
				3,
				8,
				9,
				14,
				15,
				5,
				2,
				8,
				12,
				3,
				7,
				0,
				4,
				10,
				1,
				13,
				11,
				6,
				4,
				3,
				2,
				12,
				9,
				5,
				15,
				10,
				11,
				14,
				1,
				7,
				6,
				0,
				8,
				13,
			},
			{
				4,
				11,
				2,
				14,
				15,
				0,
				8,
				13,
				3,
				12,
				9,
				7,
				5,
				10,
				6,
				1,
				13,
				0,
				11,
				7,
				4,
				9,
				1,
				10,
				14,
				3,
				5,
				12,
				2,
				15,
				8,
				6,
				1,
				4,
				11,
				13,
				12,
				3,
				7,
				14,
				10,
				15,
				6,
				8,
				0,
				5,
				9,
				2,
				6,
				11,
				13,
				8,
				1,
				4,
				10,
				7,
				9,
				5,
				0,
				15,
				14,
				2,
				3,
				12,
			},
			{
				13,
				2,
				8,
				4,
				6,
				15,
				11,
				1,
				10,
				9,
				3,
				14,
				5,
				0,
				12,
				7,
				1,
				15,
				13,
				8,
				10,
				3,
				7,
				4,
				12,
				5,
				6,
				11,
				0,
				14,
				9,
				2,
				7,
				11,
				4,
				1,
				9,
				12,
				14,
				2,
				0,
				6,
				10,
				13,
				15,
				3,
				5,
				8,
				2,
				1,
				14,
				7,
				4,
				10,
				8,
				13,
				15,
				12,
				9,
				0,
				3,
				5,
				6,
				11,
			},
		};

		uint64_t LoadBe64(const unsigned char *pData)
		{
			uint64_t Value = 0;
			for(int i = 0; i < 8; ++i)
				Value = (Value << 8) | pData[i];
			return Value;
		}

		void StoreBe64(uint64_t Value, unsigned char *pOut)
		{
			for(int i = 7; i >= 0; --i)
			{
				pOut[i] = (unsigned char)(Value & 0xFF);
				Value >>= 8;
			}
		}

		uint64_t DesPermute(uint64_t Input, const int *pTable, size_t TableSize, int InputBits)
		{
			uint64_t Out = 0;
			for(size_t i = 0; i < TableSize; ++i)
			{
				Out <<= 1;
				Out |= (Input >> (InputBits - pTable[i])) & 1ULL;
			}
			return Out;
		}

		std::array<uint64_t, 16> BuildDesSubkeys(const unsigned char *pKey, bool Encrypt)
		{
			std::array<uint64_t, 16> aSubkeys{};
			const uint64_t Key56 = DesPermute(LoadBe64(pKey), DES_PC1, std::size(DES_PC1), 64);
			uint32_t C = (uint32_t)((Key56 >> 28) & 0x0FFFFFFF);
			uint32_t D = (uint32_t)(Key56 & 0x0FFFFFFF);
			for(int Round = 0; Round < 16; ++Round)
			{
				C = ((C << DES_SHIFTS[Round]) | (C >> (28 - DES_SHIFTS[Round]))) & 0x0FFFFFFF;
				D = ((D << DES_SHIFTS[Round]) | (D >> (28 - DES_SHIFTS[Round]))) & 0x0FFFFFFF;
				const uint64_t CD = ((uint64_t)C << 28) | D;
				aSubkeys[Encrypt ? Round : 15 - Round] = DesPermute(CD, DES_PC2, std::size(DES_PC2), 56);
			}
			return aSubkeys;
		}

		uint32_t DesFeistel(uint32_t R, uint64_t Subkey)
		{
			const uint64_t Expanded = DesPermute((uint64_t)R, DES_E, std::size(DES_E), 32) ^ Subkey;
			uint32_t SboxOut = 0;
			for(int i = 0; i < 8; ++i)
			{
				const unsigned char SixBits = (unsigned char)((Expanded >> (42 - i * 6)) & 0x3F);
				const int Row = ((SixBits & 0x20) >> 4) | (SixBits & 0x01);
				const int Col = (SixBits >> 1) & 0x0F;
				SboxOut = (SboxOut << 4) | DES_SBOX[i][Row * 16 + Col];
			}
			return (uint32_t)DesPermute((uint64_t)SboxOut, DES_P, std::size(DES_P), 32);
		}

		void DesCryptBlock(const unsigned char *pIn, unsigned char *pOut, const std::array<uint64_t, 16> &Subkeys)
		{
			const uint64_t Permuted = DesPermute(LoadBe64(pIn), DES_IP, std::size(DES_IP), 64);
			uint32_t L = (uint32_t)(Permuted >> 32);
			uint32_t R = (uint32_t)(Permuted & 0xFFFFFFFF);
			for(int Round = 0; Round < 16; ++Round)
			{
				const uint32_t NewL = R;
				const uint32_t NewR = L ^ DesFeistel(R, Subkeys[Round]);
				L = NewL;
				R = NewR;
			}
			StoreBe64(DesPermute(((uint64_t)R << 32) | L, DES_FP, std::size(DES_FP), 64), pOut);
		}

		struct SQqTripleDesDecryptSchedule
		{
			std::array<uint64_t, 16> m_K1Decrypt;
			std::array<uint64_t, 16> m_K2Encrypt;
			std::array<uint64_t, 16> m_K3Decrypt;
		};

		SQqTripleDesDecryptSchedule BuildQqTripleDesDecryptSchedule(const unsigned char *pKey)
		{
			SQqTripleDesDecryptSchedule Schedule;
			Schedule.m_K1Decrypt = BuildDesSubkeys(pKey, false);
			Schedule.m_K2Encrypt = BuildDesSubkeys(pKey + 8, true);
			Schedule.m_K3Decrypt = BuildDesSubkeys(pKey + 16, false);
			return Schedule;
		}

		void TripleDesQqDecryptBlock(const unsigned char *pIn, unsigned char *pOut, const SQqTripleDesDecryptSchedule &Schedule)
		{
			unsigned char aTmp1[8];
			unsigned char aTmp2[8];
			DesCryptBlock(pIn, aTmp1, Schedule.m_K3Decrypt);
			DesCryptBlock(aTmp1, aTmp2, Schedule.m_K2Encrypt);
			DesCryptBlock(aTmp2, pOut, Schedule.m_K1Decrypt);
		}

		bool InflateZlib(const unsigned char *pData, size_t DataSize, std::string *pOut, char *pErr, size_t ErrSize)
		{
			if(pData == nullptr || DataSize == 0 || pOut == nullptr)
			{
				SetError(pErr, ErrSize, "bad zlib input");
				return false;
			}

			std::vector<unsigned char> vInflated;
			vInflated.resize(DataSize * 8 + 1024);
			z_stream Stream{};
			Stream.next_in = const_cast<Bytef *>(pData);
			Stream.avail_in = (uInt)DataSize;
			Stream.next_out = vInflated.data();
			Stream.avail_out = (uInt)vInflated.size();
			if(inflateInit(&Stream) != Z_OK)
			{
				SetError(pErr, ErrSize, "inflateInit failed");
				return false;
			}
			int Ret = inflate(&Stream, Z_FINISH);
			while(Ret == Z_BUF_ERROR || Ret == Z_OK)
			{
				const size_t Used = vInflated.size() - Stream.avail_out;
				vInflated.resize(vInflated.size() * 2);
				Stream.next_out = vInflated.data() + Used;
				Stream.avail_out = (uInt)(vInflated.size() - Used);
				Ret = inflate(&Stream, Z_FINISH);
			}
			const size_t Total = vInflated.size() - Stream.avail_out;
			inflateEnd(&Stream);
			if(Ret != Z_STREAM_END)
			{
				SetError(pErr, ErrSize, "inflate failed");
				return false;
			}

			pOut->assign((const char *)vInflated.data(), Total);
			if(pOut->size() >= 3 && (unsigned char)(*pOut)[0] == 0xEF &&
				(unsigned char)(*pOut)[1] == 0xBB && (unsigned char)(*pOut)[2] == 0xBF)
				pOut->erase(0, 3);
			return true;
		}

		std::string_view TrimAscii(std::string_view Text)
		{
			while(!Text.empty() && (Text.front() == ' ' || Text.front() == '\t' || Text.front() == '\r' || Text.front() == '\n'))
				Text.remove_prefix(1);
			while(!Text.empty() && (Text.back() == ' ' || Text.back() == '\t' || Text.back() == '\r' || Text.back() == '\n'))
				Text.remove_suffix(1);
			return Text;
		}

		bool ParseI64(std::string_view Text, int64_t *pOut)
		{
			if(pOut == nullptr)
				return false;
			Text = TrimAscii(Text);
			if(Text.empty())
				return false;
			int64_t Value = 0;
			for(char C : Text)
			{
				if(C < '0' || C > '9')
					return false;
				Value = Value * 10 + (C - '0');
			}
			*pOut = Value;
			return true;
		}

		std::string RemoveXmlComments(std::string_view Body)
		{
			std::string Out;
			Out.reserve(Body.size());
			for(size_t Pos = 0; Pos < Body.size();)
			{
				const size_t Begin = Body.find("<!--", Pos);
				if(Begin == std::string_view::npos)
				{
					Out.append(Body.substr(Pos));
					break;
				}
				Out.append(Body.substr(Pos, Begin - Pos));
				Pos = Begin + 4;
				const size_t End = Body.find("-->", Pos);
				if(End == std::string_view::npos)
					continue;
				Out.append(Body.substr(Pos, End - Pos));
				Pos = End + 3;
			}
			return Out;
		}

		std::string XmlUnescape(std::string_view Text)
		{
			std::string Out;
			Out.reserve(Text.size());
			for(size_t i = 0; i < Text.size(); ++i)
			{
				if(Text[i] != '&')
				{
					Out.push_back(Text[i]);
					continue;
				}
				if(Text.substr(i, 5) == "&amp;")
				{
					Out.push_back('&');
					i += 4;
				}
				else if(Text.substr(i, 4) == "&lt;")
				{
					Out.push_back('<');
					i += 3;
				}
				else if(Text.substr(i, 4) == "&gt;")
				{
					Out.push_back('>');
					i += 3;
				}
				else if(Text.substr(i, 6) == "&quot;")
				{
					Out.push_back('"');
					i += 5;
				}
				else if(Text.substr(i, 6) == "&apos;")
				{
					Out.push_back('\'');
					i += 5;
				}
				else
				{
					Out.push_back(Text[i]);
				}
			}
			return Out;
		}

		bool ExtractXmlElementText(std::string_view Xml, const char *pTag, std::string *pOut)
		{
			if(pTag == nullptr || pOut == nullptr)
				return false;
			std::string Open = "<";
			Open.append(pTag);
			std::string Close = "</";
			Close.append(pTag);
			Close.push_back('>');
			const size_t TagBegin = Xml.find(Open);
			if(TagBegin == std::string_view::npos)
				return false;
			const size_t OpenEnd = Xml.find('>', TagBegin + Open.size());
			if(OpenEnd == std::string_view::npos)
				return false;
			const size_t CloseBegin = Xml.find(Close, OpenEnd + 1);
			if(CloseBegin == std::string_view::npos)
				return false;
			std::string_view Inner = Xml.substr(OpenEnd + 1, CloseBegin - OpenEnd - 1);
			const std::string_view TrimmedInner = TrimAscii(Inner);
			if(TrimmedInner.substr(0, 9) == "<![CDATA[" && TrimmedInner.size() >= 12 && TrimmedInner.substr(TrimmedInner.size() - 3) == "]]>")
				Inner = TrimmedInner.substr(9, TrimmedInner.size() - 12);
			*pOut = XmlUnescape(Inner);
			return true;
		}

		bool ExtractXmlAttribute(std::string_view Xml, const char *pTag, const char *pAttribute, std::string *pOut)
		{
			if(pTag == nullptr || pAttribute == nullptr || pOut == nullptr)
				return false;
			std::string Open = "<";
			Open.append(pTag);
			const size_t TagBegin = Xml.find(Open);
			if(TagBegin == std::string_view::npos)
				return false;
			const size_t TagEnd = Xml.find('>', TagBegin + Open.size());
			if(TagEnd == std::string_view::npos)
				return false;
			std::string Attr = pAttribute;
			Attr.push_back('=');
			std::string_view Header = Xml.substr(TagBegin, TagEnd - TagBegin);
			const size_t AttrBegin = Header.find(Attr);
			if(AttrBegin == std::string_view::npos)
				return false;
			size_t ValueBegin = AttrBegin + Attr.size();
			if(ValueBegin >= Header.size() || (Header[ValueBegin] != '"' && Header[ValueBegin] != '\''))
				return false;
			const char Quote = Header[ValueBegin++];
			const size_t ValueEnd = Header.find(Quote, ValueBegin);
			if(ValueEnd == std::string_view::npos)
				return false;
			*pOut = XmlUnescape(Header.substr(ValueBegin, ValueEnd - ValueBegin));
			return true;
		}

		bool LooksLikeLrc(std::string_view Text)
		{
			return Text.find("[00:") != std::string_view::npos || Text.find("[0:") != std::string_view::npos;
		}

		bool LooksLikeQrc(std::string_view Text)
		{
			return Text.find('(') != std::string_view::npos && Text.find(',') != std::string_view::npos && Text.find(')') != std::string_view::npos;
		}

		bool DecodeQqQrcXmlPayload(std::string_view Text, std::string *pOut, char *pErr, size_t ErrSize)
		{
			if(pOut == nullptr)
			{
				SetError(pErr, ErrSize, "null output");
				return false;
			}

			std::string Decoded;
			if(DecryptQqMusicQrcHex(Text, &Decoded, pErr, ErrSize))
			{
				if(Decoded.find("<?xml") != std::string::npos)
				{
					const std::string Xml = RemoveXmlComments(Decoded);
					if(ExtractXmlAttribute(Xml, "Lyric_1", "LyricContent", pOut))
						return !pOut->empty();
				}
				*pOut = std::move(Decoded);
				return !pOut->empty();
			}

			if(LooksLikeLrc(Text) || LooksLikeQrc(Text))
			{
				*pOut = std::string(Text);
				return true;
			}
			return false;
		}

		void ConsiderQqSong(const json_value &Song, const SSourceQuery &Query, float *pBestScore, SQqMusicSearchHit *pBest)
		{
			if(Song.type != json_object || pBestScore == nullptr || pBest == nullptr)
				return;
			SQqMusicSearchHit Hit;
			Hit.m_Id = JsonId(&Song["id"]);
			Hit.m_Mid = JsonString(&Song["mid"]);
			if(Hit.m_Mid.empty() && Hit.m_Id.empty())
				return;
			Hit.m_Metadata.m_Title = JsonString(&Song["title"]);
			if(Hit.m_Metadata.m_Title.empty())
				Hit.m_Metadata.m_Title = JsonString(&Song["name"]);
			Hit.m_Metadata.m_Artist = JoinNameArray(Song["singer"]);
			const json_value &Album = Song["album"];
			if(Album.type == json_object)
			{
				Hit.m_Metadata.m_Album = JsonString(&Album["title"]);
				if(Hit.m_Metadata.m_Album.empty())
					Hit.m_Metadata.m_Album = JsonString(&Album["name"]);
			}
			Hit.m_Metadata.m_DurationSec = (int)JsonInt(&Song["interval"]);
			const float S = Score(Query, Hit.m_Metadata);
			if(S > *pBestScore)
			{
				*pBestScore = S;
				*pBest = std::move(Hit);
			}
		}

		bool ReadResponseBody(CHttpRequest *pReq, std::vector<unsigned char> *pOut)
		{
			if(pReq == nullptr || pReq->State() != EHttpState::DONE || pReq->StatusCode() != 200)
				return false;
			unsigned char *pBody = nullptr;
			size_t BodyLen = 0;
			pReq->Result(&pBody, &BodyLen);
			if(pBody == nullptr || BodyLen == 0)
				return false;
			pOut->assign(pBody, pBody + BodyLen);
			return true;
		}

		void PrepareRequest(CHttpRequest *pReq, int TimeoutMs, bool AllowHttp)
		{
			pReq->Timeout(CTimeout{5000, TimeoutMs > 0 ? TimeoutMs : 8000, 0, 0});
			pReq->LogProgress(HTTPLOG::FAILURE);
			pReq->AllowInsecureProtocol(AllowHttp);
			pReq->HeaderString("User-Agent", "QmClient (https://github.com/Q1menG)");
		}

		void PrepareNeteaseRequest(CHttpRequest *pReq, int TimeoutMs, bool AllowHttp)
		{
			pReq->Timeout(CTimeout{5000, TimeoutMs > 0 ? TimeoutMs : 8000, 0, 0});
			pReq->LogProgress(HTTPLOG::FAILURE);
			pReq->AllowInsecureProtocol(AllowHttp);
			pReq->HeaderString("User-Agent", "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.132 Safari/537.36");
			pReq->HeaderString("Referer", "https://music.163.com/");
			pReq->HeaderString("Cookie", "os=pc;osver=Microsoft-Windows-10-Professional-build-16299.125-64bit;appver=2.0.3.131777;channel=netease;__remember_me=true");
		}

	} // anonymous namespace

	std::string BuildQqMusicSearchJson(const SSourceQuery &Query)
	{
		std::string Json = "{\"req_1\":{\"method\":\"DoSearchForQQMusicDesktop\",\"module\":\"music.search.SearchCgiService\",\"param\":{\"num_per_page\":\"20\",\"page_num\":\"1\",\"query\":\"";
		JsonEscape(BuildKeyword(Query), &Json);
		Json.append("\",\"search_type\":0}}}");
		return Json;
	}

	std::string BuildQqMusicQrcPostBody(const std::string &SongId)
	{
		std::string Body = "version=15&miniversion=82&lrctype=4&musicid=";
		UrlEncode(SongId, &Body);
		return Body;
	}

	std::string BuildQqMusicLyricUrl(const std::string &SongMid, int64_t CurrentMillis)
	{
		std::string Url = "https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg?callback=MusicJsonCallback_lrc&pcachetime=";
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%lld", (long long)CurrentMillis);
		Url.append(aBuf);
		Url.append("&songmid=");
		UrlEncode(SongMid, &Url);
		Url.append("&g_tk=5381&jsonpCallback=MusicJsonCallback_lrc&loginUin=0&hostUin=0&format=jsonp&inCharset=utf8&outCharset=utf8&notice=0&platform=yqq&needNewCode=0");
		return Url;
	}

	bool IsQqMusicFamilyPlayerId(std::string_view PlayerId)
	{
		return ContainsNoCase(PlayerId, "QQMusic.exe");
	}

	bool ShouldUseQqMusicDirectSongId(const SSourceQuery &Query)
	{
		return !Query.m_QqMusicSongId.empty() && IsQqMusicFamilyPlayerId(Query.m_PlayerId);
	}

	bool ParseQqMusicSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SQqMusicSearchHit *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		json_value *pRoot = nullptr;
		if(!ParseJsonRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;

		const json_value &Items = (*pRoot)["req_1"]["data"]["body"]["song"]["list"];
		if(Items.type != json_array || Items.u.array.length == 0)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "no qq songs");
			return false;
		}

		float BestScore = -1.0f;
		SQqMusicSearchHit Best;
		for(unsigned int i = 0; i < Items.u.array.length; ++i)
		{
			const json_value *pItem = Items.u.array.values[i];
			if(pItem == nullptr)
				continue;
			ConsiderQqSong(*pItem, Query, &BestScore, &Best);
			const json_value &Group = (*pItem)["grp"];
			if(Group.type == json_array)
			{
				for(unsigned int j = 0; j < Group.u.array.length; ++j)
				{
					if(Group.u.array.values[j] != nullptr)
						ConsiderQqSong(*Group.u.array.values[j], Query, &BestScore, &Best);
				}
			}
		}
		json_value_free(pRoot);
		if(Best.m_Mid.empty())
		{
			SetError(pErr, ErrSize, "no qq match");
			return false;
		}
		*pOut = std::move(Best);
		return true;
	}

	bool DecryptQqMusicQrcHex(std::string_view Hex, std::string *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		Hex = TrimAscii(Hex);
		std::vector<unsigned char> vEncrypted;
		if(!HexDecode(Hex, &vEncrypted) || vEncrypted.empty() || vEncrypted.size() % 8 != 0)
		{
			SetError(pErr, ErrSize, "bad qrc hex");
			return false;
		}
		static constexpr unsigned char QQ_KEY[24] = {
			'!',
			'@',
			'#',
			')',
			'(',
			'*',
			'$',
			'%',
			'1',
			'2',
			'3',
			'Z',
			'X',
			'C',
			'!',
			'@',
			'!',
			'@',
			'#',
			')',
			'(',
			'N',
			'H',
			'L',
		};
		const SQqTripleDesDecryptSchedule Schedule = BuildQqTripleDesDecryptSchedule(QQ_KEY);
		std::vector<unsigned char> vDecrypted(vEncrypted.size());
		for(size_t i = 0; i < vEncrypted.size(); i += 8)
			TripleDesQqDecryptBlock(vEncrypted.data() + i, vDecrypted.data() + i, Schedule);
		return InflateZlib(vDecrypted.data(), vDecrypted.size(), pOut, pErr, ErrSize);
	}

	bool ParseQqMusicQrcText(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr || pText == nullptr || TextSize == 0)
		{
			SetError(pErr, ErrSize, "bad qrc input");
			return false;
		}
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';

		pOut->m_Format = EFormat::QRC;
		pOut->m_OffsetMs = 0;
		pOut->m_vLines.clear();

		const char *p = pText;
		const char *pEnd = pText + TextSize;
		while(p < pEnd)
		{
			const char *pLineStart = p;
			while(p < pEnd && *p != '\n')
				++p;
			std::string_view Line(pLineStart, (size_t)(p - pLineStart));
			if(p < pEnd)
				++p;
			Line = TrimAscii(Line);
			if(Line.empty())
				continue;
			if(Line.front() == '[')
			{
				const size_t BracketEnd = Line.find(']');
				if(BracketEnd != std::string_view::npos)
					Line.remove_prefix(BracketEnd + 1);
			}

			SLyricsLine OutLine;
			size_t SegmentBegin = 0;
			while(SegmentBegin < Line.size())
			{
				const size_t Paren = Line.find('(', SegmentBegin);
				if(Paren == std::string_view::npos)
					break;
				const size_t Close = Line.find(')', Paren + 1);
				if(Close == std::string_view::npos)
					break;
				const size_t Comma = Line.find(',', Paren + 1);
				if(Comma == std::string_view::npos || Comma > Close)
				{
					SegmentBegin = Close + 1;
					continue;
				}
				int64_t StartMs = 0;
				int64_t DurationMs = 0;
				if(!ParseI64(Line.substr(Paren + 1, Comma - Paren - 1), &StartMs) ||
					!ParseI64(Line.substr(Comma + 1, Close - Comma - 1), &DurationMs))
				{
					SegmentBegin = Close + 1;
					continue;
				}

				SLyricsWord Word;
				Word.m_StartMs = StartMs;
				Word.m_EndMs = StartMs + std::max<int64_t>(0, DurationMs);
				Word.m_Text.assign(Line.substr(SegmentBegin, Paren - SegmentBegin));
				if(!Word.m_Text.empty())
					OutLine.m_RawText.append(Word.m_Text);
				OutLine.m_vWords.push_back(std::move(Word));
				SegmentBegin = Close + 1;
			}
			if(OutLine.m_vWords.empty())
				continue;
			OutLine.m_StartMs = OutLine.m_vWords.front().m_StartMs;
			OutLine.m_EndMs = OutLine.m_vWords.front().m_EndMs;
			for(const SLyricsWord &Word : OutLine.m_vWords)
				OutLine.m_EndMs = std::max(OutLine.m_EndMs, Word.m_EndMs);
			pOut->m_vLines.push_back(std::move(OutLine));
		}

		std::sort(pOut->m_vLines.begin(), pOut->m_vLines.end(), [](const SLyricsLine &A, const SLyricsLine &B) {
			return A.m_StartMs < B.m_StartMs;
		});
		if(pOut->m_vLines.empty())
		{
			SetError(pErr, ErrSize, "empty qrc lyric");
			return false;
		}
		return true;
	}

	bool ParseQqMusicQrcDownloadResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		const std::string Xml = RemoveXmlComments(std::string_view(pBody, BodyLen));
		std::string Orig;
		std::string Trans;
		std::string Roma;
		std::string Enc;
		if(ExtractXmlElementText(Xml, "content", &Enc))
			DecodeQqQrcXmlPayload(Enc, &Orig, nullptr, 0);
		if(ExtractXmlElementText(Xml, "contentts", &Enc))
			DecodeQqQrcXmlPayload(Enc, &Trans, nullptr, 0);
		if(ExtractXmlElementText(Xml, "contentroma", &Enc))
			DecodeQqQrcXmlPayload(Enc, &Roma, nullptr, 0);

		if(Orig.empty() && !Trans.empty())
		{
			Orig = Trans;
			Trans.clear();
		}
		if(Orig.empty())
		{
			SetError(pErr, ErrSize, "empty qq qrc lyric");
			return false;
		}

		SSourceCandidate Candidate;
		Candidate.m_RawText = std::move(Orig);
		Candidate.m_TranslationText = std::move(Trans);
		Candidate.m_TransliterationText = std::move(Roma);
		Candidate.m_FormatHint = LooksLikeQrc(Candidate.m_RawText) ? EFormat::QRC : EFormat::LRC_STANDARD;
		Candidate.m_SourceId = "qq";
		Candidate.m_SourceScore = 1.0f;
		*pOut = std::move(Candidate);
		return true;
	}

	bool ParseQqMusicLyricResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		const std::string Json = ExtractJsonp(std::string_view(pBody, BodyLen));
		json_value *pRoot = nullptr;
		if(!ParseJsonRoot(Json.c_str(), Json.size(), &pRoot, pErr, ErrSize))
			return false;
		const char *pLyric = JsonString(&(*pRoot)["lyric"]);
		const char *pTrans = JsonString(&(*pRoot)["trans"]);
		SSourceCandidate Candidate;
		if(pLyric[0] != '\0')
			Base64Decode(pLyric, &Candidate.m_RawText);
		if(pTrans[0] != '\0')
			Base64Decode(pTrans, &Candidate.m_TranslationText);
		json_value_free(pRoot);
		if(Candidate.m_RawText.empty())
		{
			SetError(pErr, ErrSize, "empty qq lyric");
			return false;
		}
		Candidate.m_FormatHint = EFormat::LRC_STANDARD;
		Candidate.m_SourceId = "qq";
		Candidate.m_SourceScore = 1.0f;
		*pOut = std::move(Candidate);
		return true;
	}

	std::string BuildNeteaseSearchUrl(const SSourceQuery &Query)
	{
		std::string Url = "http://music.163.com/api/search/get/web?csrf_token=hlpretag=&hlposttag=&s=";
		UrlEncode(BuildKeyword(Query), &Url);
		Url.append("&type=1&offset=0&total=true&limit=20");
		return Url;
	}

	std::string BuildNeteaseLyricUrl(const std::string &SongId)
	{
		(void)SongId;
		return "https://music.163.com/weapi/song/lyric?csrf_token=";
	}

	std::string BuildNeteaseLyricPostBody(const std::string &SongId)
	{
		constexpr char NETEASE_WEAPI_NONCE[] = "0CoJUm6Qyw8W8jud";
		constexpr char NETEASE_WEAPI_SECRET[] = "QmLyricsWeapiKey";
		constexpr char NETEASE_WEAPI_ENC_SEC_KEY[] =
			"c76aa624ba1d2a7676339d94fa890b7510d33bf21d270f2e21d81bcb5a8a299fe8cf7303c98128fc"
			"9de8a87742f186db1b02be275feea7dddd4a71e5ac0965ad3ffd776e8b6537adcf67db8a2f8566"
			"346519806ecd0aadea28247b6d891af3791ac466ee7ba88e6520006ded154cde1787e644269"
			"6a819d1924af6d0fa402e30";

		std::string Json = "{\"id\":\"";
		JsonEscape(SongId, &Json);
		Json.append("\",\"os\":\"pc\",\"lv\":\"-1\",\"kv\":\"-1\",\"tv\":\"-1\",\"rv\":\"-1\",\"csrf_token\":\"\"}");
		const std::string FirstPass = Aes128CbcPkcs7Base64(Json, NETEASE_WEAPI_NONCE);
		if(FirstPass.empty())
			return {};
		const std::string Params = Aes128CbcPkcs7Base64(FirstPass, NETEASE_WEAPI_SECRET);
		if(Params.empty())
			return {};
		std::string Body = "params=";
		UrlEncode(Params, &Body);
		Body.append("&encSecKey=");
		Body.append(NETEASE_WEAPI_ENC_SEC_KEY);
		return Body;
	}

	bool IsNeteaseFamilyPlayerId(std::string_view PlayerId)
	{
		return ContainsNoCase(PlayerId, "cloudmusic.exe") ||
		       StartsWithNoCase(PlayerId, "17588BrandonWong.LyricEase_") ||
		       StartsWithNoCase(PlayerId, "48848aaaaaaccd.HyPlayer_");
	}

	bool ShouldUseNeteaseDirectSongId(const SSourceQuery &Query)
	{
		return !Query.m_NeteaseSongId.empty() && IsNeteaseFamilyPlayerId(Query.m_PlayerId);
	}

	bool ParseNeteaseSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SNeteaseSearchHit *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		json_value *pRoot = nullptr;
		if(!ParseJsonRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;

		const json_value &Items = (*pRoot)["result"]["songs"];
		if(Items.type != json_array || Items.u.array.length == 0)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "no netease songs");
			return false;
		}

		float BestScore = -1.0f;
		SNeteaseSearchHit Best;
		for(unsigned int i = 0; i < Items.u.array.length; ++i)
		{
			const json_value *pItem = Items.u.array.values[i];
			if(pItem == nullptr || pItem->type != json_object)
				continue;
			SNeteaseSearchHit Hit;
			Hit.m_Id = JsonId(&(*pItem)["id"]);
			if(Hit.m_Id.empty())
				continue;
			Hit.m_Metadata.m_Title = JsonString(&(*pItem)["name"]);
			Hit.m_Metadata.m_Artist = JoinNameArray((*pItem)["artists"]);
			const json_value &Album = (*pItem)["album"];
			if(Album.type == json_object)
				Hit.m_Metadata.m_Album = JsonString(&Album["name"]);
			Hit.m_Metadata.m_DurationSec = (int)(JsonInt(&(*pItem)["duration"]) / 1000);
			const float S = Score(Query, Hit.m_Metadata);
			if(S > BestScore)
			{
				BestScore = S;
				Best = std::move(Hit);
			}
		}
		json_value_free(pRoot);
		if(Best.m_Id.empty())
		{
			SetError(pErr, ErrSize, "no netease match");
			return false;
		}
		*pOut = std::move(Best);
		return true;
	}

	bool ParseNeteaseLyricResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		json_value *pRoot = nullptr;
		if(!ParseJsonRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;
		SSourceCandidate Candidate;
		Candidate.m_RawText = JsonString(&(*pRoot)["lrc"]["lyric"]);
		Candidate.m_TranslationText = JsonString(&(*pRoot)["tlyric"]["lyric"]);
		Candidate.m_TransliterationText = JsonString(&(*pRoot)["romalrc"]["lyric"]);
		json_value_free(pRoot);
		if(Candidate.m_RawText.empty())
		{
			SetError(pErr, ErrSize, "empty netease lyric");
			return false;
		}
		Candidate.m_FormatHint = EFormat::LRC_STANDARD;
		Candidate.m_SourceId = "netease";
		Candidate.m_SourceScore = 1.0f;
		*pOut = std::move(Candidate);
		return true;
	}

	struct CLyricsSourceQqMusic::SImpl
	{
		enum class EStage
		{
			IDLE,
			SEARCH,
			QRC,
			LYRIC,
		};

		IHttp *m_pHttp = nullptr;
		int m_TimeoutMs = 8000;
		EStage m_Stage = EStage::IDLE;
		std::shared_ptr<CHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_Query;
		SQqMusicSearchHit m_Hit;

		bool StartQrcLyricRequest()
		{
			if(m_pHttp == nullptr || m_Hit.m_Id.empty())
				return false;
			const std::string Body = BuildQqMusicQrcPostBody(m_Hit.m_Id);
			m_Stage = EStage::QRC;
			m_pRequest = std::make_shared<CHttpRequest>("https://c.y.qq.com/qqmusic/fcgi-bin/lyric_download.fcg");
			PrepareRequest(m_pRequest.get(), m_TimeoutMs, false);
			m_pRequest->HeaderString("Referer", "https://c.y.qq.com/");
			m_pRequest->HeaderString("Origin", "https://y.qq.com");
			m_pRequest->PostForm((const unsigned char *)Body.data(), Body.size());
			m_pHttp->Run(m_pRequest);
			return true;
		}

		bool StartLegacyLyricRequest()
		{
			if(m_pHttp == nullptr || m_Hit.m_Mid.empty())
				return false;
			const int64_t NowMs = time_timestamp() * 1000LL;
			m_Stage = EStage::LYRIC;
			m_pRequest = std::make_shared<CHttpRequest>(BuildQqMusicLyricUrl(m_Hit.m_Mid, NowMs).c_str());
			PrepareRequest(m_pRequest.get(), m_TimeoutMs, false);
			m_pRequest->HeaderString("Referer", "https://c.y.qq.com/");
			m_pHttp->Run(m_pRequest);
			return true;
		}
	};

	CLyricsSourceQqMusic::CLyricsSourceQqMusic(IHttp *pHttp, int TimeoutMs) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
	}

	CLyricsSourceQqMusic::~CLyricsSourceQqMusic()
	{
		Cancel();
	}

	void CLyricsSourceQqMusic::Cancel()
	{
		if(m_pImpl->m_pRequest)
		{
			m_pImpl->m_pRequest->Abort();
			m_pImpl->m_pRequest.reset();
		}
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		m_pImpl->m_Hit = {};
	}

	void CLyricsSourceQqMusic::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		if(m_pImpl->m_pHttp == nullptr)
		{
			if(Error)
				Error("no IHttp");
			return;
		}
		const bool UseDirectSongId = ShouldUseQqMusicDirectSongId(Query);
		if(Query.m_Title.empty() && !UseDirectSongId)
		{
			if(Done)
				Done({});
			return;
		}
		m_pImpl->m_Query = Query;
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		if(UseDirectSongId)
		{
			m_pImpl->m_Hit.m_Id = Query.m_QqMusicSongId;
			m_pImpl->m_Hit.m_Mid.clear();
			m_pImpl->m_Hit.m_Metadata.m_Title = Query.m_Title;
			m_pImpl->m_Hit.m_Metadata.m_Artist = Query.m_Artist;
			m_pImpl->m_Hit.m_Metadata.m_Album = Query.m_Album;
			m_pImpl->m_Hit.m_Metadata.m_DurationSec = Query.m_DurationSec;
			if(m_pImpl->StartQrcLyricRequest())
				return;
			FSourceDoneCallback DoneCallback = std::move(m_pImpl->m_Done);
			m_pImpl->m_Error = nullptr;
			if(DoneCallback)
				DoneCallback({});
			return;
		}
		m_pImpl->m_Stage = SImpl::EStage::SEARCH;
		const std::string Json = BuildQqMusicSearchJson(Query);
		m_pImpl->m_pRequest = std::make_shared<CHttpRequest>("https://u.y.qq.com/cgi-bin/musicu.fcg");
		PrepareRequest(m_pImpl->m_pRequest.get(), m_pImpl->m_TimeoutMs, false);
		m_pImpl->m_pRequest->HeaderString("Referer", "https://c.y.qq.com/");
		m_pImpl->m_pRequest->PostJson(Json.c_str());
		m_pImpl->m_pHttp->Run(m_pImpl->m_pRequest);
	}

	void CLyricsSourceQqMusic::Tick()
	{
		if(m_pImpl->m_Stage == SImpl::EStage::IDLE || !m_pImpl->m_pRequest || !m_pImpl->m_pRequest->Done())
			return;

		std::vector<unsigned char> vBody;
		const bool Ok = ReadResponseBody(m_pImpl->m_pRequest.get(), &vBody);
		m_pImpl->m_pRequest.reset();
		if(!Ok)
		{
			if(m_pImpl->m_Stage == SImpl::EStage::QRC && m_pImpl->StartLegacyLyricRequest())
				return;
			FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
			m_pImpl->m_Error = nullptr;
			m_pImpl->m_Stage = SImpl::EStage::IDLE;
			if(Done)
				Done({});
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::SEARCH)
		{
			SQqMusicSearchHit Hit;
			if(ParseQqMusicSearchResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Query, &Hit))
			{
				m_pImpl->m_Hit = std::move(Hit);
				if(m_pImpl->StartQrcLyricRequest() || m_pImpl->StartLegacyLyricRequest())
					return;
			}
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::QRC)
		{
			SSourceCandidate Candidate;
			if(ParseQqMusicQrcDownloadResponse((const char *)vBody.data(), vBody.size(), &Candidate))
			{
				Candidate.m_Metadata = m_pImpl->m_Hit.m_Metadata;
				Candidate.m_SourceId = Id();
				FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(Done)
					Done({std::move(Candidate)});
				return;
			}
			if(m_pImpl->StartLegacyLyricRequest())
				return;
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::LYRIC)
		{
			SSourceCandidate Candidate;
			if(ParseQqMusicLyricResponse((const char *)vBody.data(), vBody.size(), &Candidate))
			{
				Candidate.m_Metadata = m_pImpl->m_Hit.m_Metadata;
				Candidate.m_SourceId = Id();
				FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(Done)
					Done({std::move(Candidate)});
				return;
			}
		}

		FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		if(Done)
			Done({});
	}

	struct CLyricsSourceNetease::SImpl
	{
		enum class EStage
		{
			IDLE,
			SEARCH,
			LYRIC,
		};

		IHttp *m_pHttp = nullptr;
		int m_TimeoutMs = 8000;
		EStage m_Stage = EStage::IDLE;
		std::shared_ptr<CHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_Query;
		SNeteaseSearchHit m_Hit;

		bool StartLyricRequest()
		{
			if(m_pHttp == nullptr || m_Hit.m_Id.empty())
				return false;
			const std::string Body = BuildNeteaseLyricPostBody(m_Hit.m_Id);
			if(Body.empty())
				return false;
			m_Stage = EStage::LYRIC;
			m_pRequest = std::make_shared<CHttpRequest>(BuildNeteaseLyricUrl(m_Hit.m_Id).c_str());
			PrepareNeteaseRequest(m_pRequest.get(), m_TimeoutMs, false);
			m_pRequest->PostForm((const unsigned char *)Body.data(), Body.size());
			m_pHttp->Run(m_pRequest);
			return true;
		}
	};

	CLyricsSourceNetease::CLyricsSourceNetease(IHttp *pHttp, int TimeoutMs) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
	}

	CLyricsSourceNetease::~CLyricsSourceNetease()
	{
		Cancel();
	}

	void CLyricsSourceNetease::Cancel()
	{
		if(m_pImpl->m_pRequest)
		{
			m_pImpl->m_pRequest->Abort();
			m_pImpl->m_pRequest.reset();
		}
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		m_pImpl->m_Hit = {};
	}

	void CLyricsSourceNetease::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		if(m_pImpl->m_pHttp == nullptr)
		{
			if(Error)
				Error("no IHttp");
			return;
		}
		const bool UseDirectSongId = ShouldUseNeteaseDirectSongId(Query);
		if(Query.m_Title.empty() && !UseDirectSongId)
		{
			if(Done)
				Done({});
			return;
		}
		m_pImpl->m_Query = Query;
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		if(UseDirectSongId)
		{
			m_pImpl->m_Hit.m_Id = Query.m_NeteaseSongId;
			m_pImpl->m_Hit.m_Metadata.m_Title = Query.m_Title;
			m_pImpl->m_Hit.m_Metadata.m_Artist = Query.m_Artist;
			m_pImpl->m_Hit.m_Metadata.m_Album = Query.m_Album;
			m_pImpl->m_Hit.m_Metadata.m_DurationSec = Query.m_DurationSec;
			if(m_pImpl->StartLyricRequest())
				return;
			FSourceDoneCallback DoneCallback = std::move(m_pImpl->m_Done);
			m_pImpl->m_Error = nullptr;
			if(DoneCallback)
				DoneCallback({});
			return;
		}
		m_pImpl->m_Stage = SImpl::EStage::SEARCH;
		m_pImpl->m_pRequest = std::make_shared<CHttpRequest>(BuildNeteaseSearchUrl(Query).c_str());
		PrepareNeteaseRequest(m_pImpl->m_pRequest.get(), m_pImpl->m_TimeoutMs, true);
		m_pImpl->m_pHttp->Run(m_pImpl->m_pRequest);
	}

	void CLyricsSourceNetease::Tick()
	{
		if(m_pImpl->m_Stage == SImpl::EStage::IDLE || !m_pImpl->m_pRequest || !m_pImpl->m_pRequest->Done())
			return;

		std::vector<unsigned char> vBody;
		const bool Ok = ReadResponseBody(m_pImpl->m_pRequest.get(), &vBody);
		m_pImpl->m_pRequest.reset();
		if(!Ok)
		{
			FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
			m_pImpl->m_Error = nullptr;
			m_pImpl->m_Stage = SImpl::EStage::IDLE;
			if(Done)
				Done({});
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::SEARCH)
		{
			SNeteaseSearchHit Hit;
			if(ParseNeteaseSearchResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Query, &Hit))
			{
				m_pImpl->m_Hit = std::move(Hit);
				if(m_pImpl->StartLyricRequest())
					return;
			}
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::LYRIC)
		{
			SSourceCandidate Candidate;
			if(ParseNeteaseLyricResponse((const char *)vBody.data(), vBody.size(), &Candidate))
			{
				Candidate.m_Metadata = m_pImpl->m_Hit.m_Metadata;
				Candidate.m_SourceId = Id();
				FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(Done)
					Done({std::move(Candidate)});
				return;
			}
		}

		FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		if(Done)
			Done({});
	}

} // namespace QmLyrics
