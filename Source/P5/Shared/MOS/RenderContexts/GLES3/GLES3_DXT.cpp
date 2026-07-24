#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_DXT.h"

#include <cstring>

// -- RGB565 -> 8-bit R,G,B expansion (with low-bit replication). ---------
static inline void Expand565(unsigned short _c, unsigned char& _R, unsigned char& _G, unsigned char& _B)
{
	_R = (unsigned char)(((_c >> 11) & 0x1f) * 255 / 31);
	_G = (unsigned char)(((_c >>  5) & 0x3f) * 255 / 63);
	_B = (unsigned char)(( _c        & 0x1f) * 255 / 31);
}

// -- Common: decode one DXT1 color block (8 bytes) into 4x4 RGBA. --------
// _pOut: 4x4 output block; _RowStride bytes between output rows.
// _bDXT1Punchthrough: true only for DXT1 (indexed alpha 0 for the
// "c0<=c1" bit; ignored on DXT5, where the alpha block wins).
static void DecodeColorBlock(const unsigned char* _pSrc,
	unsigned char* _pOut, int _RowStride, bool _bDXT1Punchthrough)
{
	unsigned short c0 = (unsigned short)(_pSrc[0] | (_pSrc[1] << 8));
	unsigned short c1 = (unsigned short)(_pSrc[2] | (_pSrc[3] << 8));
	unsigned int   idx =
		((unsigned int)_pSrc[4])       |
		((unsigned int)_pSrc[5] <<  8) |
		((unsigned int)_pSrc[6] << 16) |
		((unsigned int)_pSrc[7] << 24);

	unsigned char R[4], G[4], B[4], A[4];
	Expand565(c0, R[0], G[0], B[0]); A[0] = 255;
	Expand565(c1, R[1], G[1], B[1]); A[1] = 255;

	if (c0 > c1 || !_bDXT1Punchthrough)
	{
		R[2] = (unsigned char)((2 * R[0] + R[1]) / 3);
		G[2] = (unsigned char)((2 * G[0] + G[1]) / 3);
		B[2] = (unsigned char)((2 * B[0] + B[1]) / 3);
		A[2] = 255;
		R[3] = (unsigned char)((R[0] + 2 * R[1]) / 3);
		G[3] = (unsigned char)((G[0] + 2 * G[1]) / 3);
		B[3] = (unsigned char)((B[0] + 2 * B[1]) / 3);
		A[3] = 255;
	}
	else
	{
		R[2] = (unsigned char)((R[0] + R[1]) / 2);
		G[2] = (unsigned char)((G[0] + G[1]) / 2);
		B[2] = (unsigned char)((B[0] + B[1]) / 2);
		A[2] = 255;
		R[3] = G[3] = B[3] = 0;
		A[3] = 0; // punchthrough transparent
	}

	for (int y = 0; y < 4; ++y)
	{
		unsigned char* pRow = _pOut + y * _RowStride;
		for (int x = 0; x < 4; ++x)
		{
			unsigned int k = (idx >> (2 * (y * 4 + x))) & 0x3;
			pRow[x * 4 + 0] = R[k];
			pRow[x * 4 + 1] = G[k];
			pRow[x * 4 + 2] = B[k];
			pRow[x * 4 + 3] = A[k];
		}
	}
}

// -- Decode one DXT5 alpha block (8 bytes) into 4x4 alphas. --------------
static void DecodeAlphaBlockDXT5(const unsigned char* _pSrc,
	unsigned char* _pOut, int _RowStride)
{
	unsigned char a0 = _pSrc[0];
	unsigned char a1 = _pSrc[1];

	unsigned char A[8];
	A[0] = a0;
	A[1] = a1;
	if (a0 > a1)
	{
		for (int i = 1; i < 7; ++i)
			A[i + 1] = (unsigned char)(((7 - i) * a0 + i * a1) / 7);
	}
	else
	{
		for (int i = 1; i < 5; ++i)
			A[i + 1] = (unsigned char)(((5 - i) * a0 + i * a1) / 5);
		A[6] = 0;
		A[7] = 255;
	}

	// 48 bits of 3-bit indices packed into bytes 2..7.
	unsigned long long bits =
		((unsigned long long)_pSrc[2])       |
		((unsigned long long)_pSrc[3] <<  8) |
		((unsigned long long)_pSrc[4] << 16) |
		((unsigned long long)_pSrc[5] << 24) |
		((unsigned long long)_pSrc[6] << 32) |
		((unsigned long long)_pSrc[7] << 40);

	for (int y = 0; y < 4; ++y)
	{
		unsigned char* pRow = _pOut + y * _RowStride;
		for (int x = 0; x < 4; ++x)
		{
			unsigned int k = (unsigned int)((bits >> (3 * (y * 4 + x))) & 0x7);
			pRow[x * 4 + 3] = A[k];
		}
	}
}

// -- Decode one DXT3 alpha block (8 bytes: 4-bit alpha per texel). -------
static void DecodeAlphaBlockDXT3(const unsigned char* _pSrc,
	unsigned char* _pOut, int _RowStride)
{
	// 64 bits, 4 bits per texel, row-major within 4x4 block.
	for (int y = 0; y < 4; ++y)
	{
		unsigned char* pRow = _pOut + y * _RowStride;
		unsigned char b0 = _pSrc[y * 2 + 0];
		unsigned char b1 = _pSrc[y * 2 + 1];
		// Two texels per byte, low nibble is the earlier texel.
		unsigned char a[4] = {
			(unsigned char)((b0 & 0x0f) * 17),
			(unsigned char)(((b0 >> 4) & 0x0f) * 17),
			(unsigned char)((b1 & 0x0f) * 17),
			(unsigned char)(((b1 >> 4) & 0x0f) * 17),
		};
		pRow[0 * 4 + 3] = a[0];
		pRow[1 * 4 + 3] = a[1];
		pRow[2 * 4 + 3] = a[2];
		pRow[3 * 4 + 3] = a[3];
	}
}

void GLES3_DecodeDXT3(const unsigned char* _pSrc, unsigned char* _pDst, int _Width, int _Height)
{
	const int RowStride = _Width * 4;
	const int BlocksX = (_Width  + 3) / 4;
	const int BlocksY = (_Height + 3) / 4;
	for (int by = 0; by < BlocksY; ++by)
	{
		for (int bx = 0; bx < BlocksX; ++bx)
		{
			unsigned char* pOut = _pDst + (by * 4) * RowStride + (bx * 4) * 4;
			DecodeColorBlock(_pSrc + 8, pOut, RowStride, /*punchthrough*/false);
			DecodeAlphaBlockDXT3(_pSrc, pOut, RowStride);
			_pSrc += 16;
		}
	}
}

void GLES3_DecodeDXT1(const unsigned char* _pSrc, unsigned char* _pDst, int _Width, int _Height)
{
	const int RowStride = _Width * 4;
	const int BlocksX = (_Width  + 3) / 4;
	const int BlocksY = (_Height + 3) / 4;
	for (int by = 0; by < BlocksY; ++by)
	{
		for (int bx = 0; bx < BlocksX; ++bx)
		{
			unsigned char* pOut = _pDst + (by * 4) * RowStride + (bx * 4) * 4;
			DecodeColorBlock(_pSrc, pOut, RowStride, /*punchthrough*/true);
			_pSrc += 8;
		}
	}
}

void GLES3_DecodeDXT5(const unsigned char* _pSrc, unsigned char* _pDst, int _Width, int _Height)
{
	const int RowStride = _Width * 4;
	const int BlocksX = (_Width  + 3) / 4;
	const int BlocksY = (_Height + 3) / 4;
	for (int by = 0; by < BlocksY; ++by)
	{
		for (int bx = 0; bx < BlocksX; ++bx)
		{
			unsigned char* pOut = _pDst + (by * 4) * RowStride + (bx * 4) * 4;
			DecodeAlphaBlockDXT5(_pSrc, pOut, RowStride);
			DecodeColorBlock(_pSrc + 8, pOut, RowStride, /*punchthrough*/false);
			// DecodeColorBlock overwrites alpha with 255; rewrite the
			// DXT5 alpha layer on top.
			DecodeAlphaBlockDXT5(_pSrc, pOut, RowStride);
			_pSrc += 16;
		}
	}
}

#endif // PLATFORM_LINUX
