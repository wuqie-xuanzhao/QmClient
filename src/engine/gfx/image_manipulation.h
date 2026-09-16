#ifndef ENGINE_GFX_IMAGE_MANIPULATION_H
#define ENGINE_GFX_IMAGE_MANIPULATION_H

#include <engine/image.h>

#include <cstdint>

// Destination must have appropriate size for RGBA data
bool ConvertToRgba(uint8_t *pDest, const CImageInfo &SourceImage);
// Allocates appropriate buffer with malloc, must be freed by caller
bool ConvertToRgbaAlloc(uint8_t *&pDest, const CImageInfo &SourceImage);
// Replaces existing image data with RGBA data (unless already RGBA)
bool ConvertToRgba(CImageInfo &Image);

// Changes the image data (not the format)
void ConvertToGrayscale(const CImageInfo &Image);
void ConvertToGrayscaleRect(const CImageInfo &Image, size_t StartX, size_t StartY, size_t Width, size_t Height);

// Color a rectangle inside an image with hue and saturation
void ColorizeWithHueRect(CImageInfo &Image, float Hue, float Sat, size_t StartX, size_t StartY, size_t Width, size_t Height);

// These functions assume that the image data is 4 bytes per pixel RGBA
void DilateImage(uint8_t *pImageBuff, int w, int h);
void DilateImage(const CImageInfo &Image);
void DilateImageSub(uint8_t *pImageBuff, int w, int h, int x, int y, int SubWidth, int SubHeight);

// Returned buffer is allocated with malloc, must be freed by caller
uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP);
// Replaces existing image data with resized buffer
void ResizeImage(CImageInfo &Image, int NewWidth, int NewHeight);

int HighestBit(int OfVar);

// 图集网格换算：把 sprite 的格坐标解析成图集内的像素矩形（OutX/OutY 为左上角）。
// 网格非法、图集尺寸不能被网格整除或 sprite 越界时返回 false。
// pOutOfBounds 非空时额外区分「sprite 越界」与「参数非法」两类失败。
bool ResolveSpritePixelRect(size_t ImageWidth, size_t ImageHeight, int GridX, int GridY,
	int SpriteX, int SpriteY, int SpriteW, int SpriteH,
	size_t &OutX, size_t &OutY, size_t &OutW, size_t &OutH, bool *pOutOfBounds = nullptr);

// 判断图像中指定矩形是否完全透明。仅对带 alpha 通道的格式（FORMAT_R / FORMAT_RA / FORMAT_RGBA）
// 有效，其它格式、空数据或越界矩形都返回 false。
bool IsImageRectFullyTransparent(const CImageInfo &Image, size_t X, size_t Y, size_t Width, size_t Height);

// 空白 sprite 回退：当 Image 的指定矩形完全透明时，用 FallbackImage 同位置的像素覆盖它。
// 两张图必须同尺寸同格式（同一图集的内置默认图），否则不做任何修改。返回是否发生了覆盖。
bool CopyFallbackOverBlankRect(CImageInfo &Image, const CImageInfo &FallbackImage, size_t X, size_t Y, size_t Width, size_t Height);

#endif // ENGINE_GFX_IMAGE_MANIPULATION_H
