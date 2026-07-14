// CPU DXT1 / DXT5 -> RGBA8 decoder for the GLES3 backend
// (Phase 4 milestone M5-partial).
//
// GLES3 core has no S3TC support without EXT_texture_compression_s3tc,
// so we decompress on the CPU into a RGBA8 buffer before glTexImage2D.
// The decoded buffer is owned by the caller (malloc'd, free with free).

#pragma once

#ifdef PLATFORM_LINUX

#include <cstddef>

// Decode _nBlocks x _nBlocks worth of DXT1 (8 bytes/block) at
// _pSrc into _pDst (RGBA8, _Width * _Height * 4 bytes). Handles the
// 1-bit-alpha variant (c0 <= c1 -> transparent index 3).
void GLES3_DecodeDXT1(const unsigned char* _pSrc, unsigned char* _pDst, int _Width, int _Height);

// Decode DXT5 (16 bytes/block: 8-byte alpha + 8-byte DXT1-style RGB).
void GLES3_DecodeDXT5(const unsigned char* _pSrc, unsigned char* _pDst, int _Width, int _Height);

#endif // PLATFORM_LINUX
