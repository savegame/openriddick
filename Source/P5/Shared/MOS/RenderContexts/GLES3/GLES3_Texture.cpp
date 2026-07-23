#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_Texture.h"
#include "GLES3_DXT.h"

#include "../../MSystem/Raster/MImage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int g_GLES3_UploadRGBA = 0;
int g_GLES3_UploadDXT1 = 0;
int g_GLES3_UploadDXT3 = 0;
int g_GLES3_UploadDXT5 = 0;
int g_GLES3_UploadFail = 0;

SGLES3Format CGLES3TextureUploader::MapFormat(int _ImageFormat)
{
	SGLES3Format F;
	F.InternalFormat = GL_RGBA8;
	F.Format         = GL_RGBA;
	F.Type           = GL_UNSIGNED_BYTE;
	F.BytesPerPixel  = 4;
	F.NeedsSwizzle   = false;
	F.Supported      = true;

	switch (_ImageFormat)
	{
	case IMAGE_FORMAT_RGBA8:
		break;
	case IMAGE_FORMAT_BGRA8:
	case IMAGE_FORMAT_BGRX8:
		// GLES core has no BGRA upload; swizzle on CPU into RGBA8.
		F.NeedsSwizzle = true;
		break;
	case IMAGE_FORMAT_RGB8:
		F.InternalFormat = GL_RGB8;
		F.Format         = GL_RGB;
		F.BytesPerPixel  = 3;
		break;
	case IMAGE_FORMAT_BGR8:
		F.InternalFormat = GL_RGB8;
		F.Format         = GL_RGB;
		F.BytesPerPixel  = 3;
		F.NeedsSwizzle   = true; // 3-byte BGR->RGB swap
		break;
	case IMAGE_FORMAT_I8:
	case IMAGE_FORMAT_A8:
		F.InternalFormat = GL_R8;
		F.Format         = GL_RED;
		F.BytesPerPixel  = 1;
		break;
	case IMAGE_FORMAT_I8A8:
		F.InternalFormat = GL_RG8;
		F.Format         = GL_RG;
		F.BytesPerPixel  = 2;
		break;
	default:
		// DXT/palettised/16bit/float variants: caller must convert.
		F.Supported = false;
		break;
	}
	return F;
}

void CGLES3TextureUploader::SwizzleBGRA_RGBA(unsigned char* _pPixels, int _nPixels)
{
	for (int i = 0; i < _nPixels; ++i)
	{
		unsigned char* p = _pPixels + i * 4;
		unsigned char t = p[0];
		p[0] = p[2];
		p[2] = t;
	}
}

GLuint CGLES3TextureUploader::Upload2D(CImage* _pImage, bool _bGenerateMipmaps)
{
	if (!_pImage) return 0;
	const int W = _pImage->GetWidth();
	const int H = _pImage->GetHeight();
	if (W <= 0 || H <= 0) return 0;

	// S3TC path: CPU-decode DXT1/DXT5 to RGBA8, then fall through to
	// the normal upload code path with pTmp/pSrc.
	unsigned char* pDecoded = 0;
	if (_pImage->IsCompressed() &&
	    (_pImage->GetMemModel() & IMAGE_MEM_COMPRESSTYPE_S3TC))
	{
		unsigned char* pRaw = (unsigned char*)_pImage->LockCompressed();
		if (!pRaw)
		{
			fprintf(stderr, "[GLES3] Upload2D: LockCompressed()==NULL (%dx%d fmt=0x%x mem=0x%x)\n",
				W, H, (unsigned)_pImage->GetFormat(), (unsigned)_pImage->GetMemModel());
			++g_GLES3_UploadFail;
			return 0;
		}
		const CImage_CompressHeader_S3TC& Hdr =
			*(const CImage_CompressHeader_S3TC*)pRaw;
		// Payload offset comes from the header (PS3 backend does the
		// same: pHeader->getOffsetData()); it is not necessarily
		// sizeof(header).
		unsigned char* pPayload = pRaw + Hdr.getOffsetData();
		// DXT decoders always emit full 4x4 blocks — for W or H not
		// aligned to 4 the last block writes past the WxH region. Allocate
		// the padded (block-aligned) size so those writes stay in-buffer;
		// GL upload uses GL_UNPACK_ROW_LENGTH to skip the padding columns.
		// Root cause of Pa1_TheDream 'free(): invalid next size' abort on
		// 'Special_DepthFogTable' (small W×1 LUT — decoder overran by
		// 3 rows into next heap chunk metadata).
		const int PadW = (W + 3) & ~3;
		const int PadH = (H + 3) & ~3;
		pDecoded = (unsigned char*)malloc((size_t)PadW * PadH * 4);
		if (!pDecoded) return 0;
		const uint32 Sub = Hdr.getCompressType();
		if (Sub == IMAGE_COMPRESSTYPE_S3TC_DXT1)
		{
			GLES3_DecodeDXT1(pPayload, pDecoded, PadW, PadH);
			++g_GLES3_UploadDXT1;
		}
		else if (Sub == IMAGE_COMPRESSTYPE_S3TC_DXT3 ||
		         Sub == IMAGE_COMPRESSTYPE_S3TC_DXT2)
		{
			// DXT2 differs from DXT3 only in "premultiplied alpha" hint;
			// on-disk layout is identical, sampler-side interpretation
			// is up to the caller. Decode the same way.
			GLES3_DecodeDXT3(pPayload, pDecoded, PadW, PadH);
			++g_GLES3_UploadDXT3;
		}
		else if (Sub == IMAGE_COMPRESSTYPE_S3TC_DXT5)
		{
			GLES3_DecodeDXT5(pPayload, pDecoded, PadW, PadH);
			++g_GLES3_UploadDXT5;
		}
		else
		{
			fprintf(stderr, "[GLES3] Upload2D: S3TC subformat %u not decoded yet (%dx%d)\n",
				(unsigned)Sub, W, H);
			++g_GLES3_UploadFail;
			free(pDecoded);
			return 0;
		}
		// Upload as RGBA8 directly, bypassing the format-table + swizzle
		// (already RGBA8 order from the decoders).
		GLuint Tex = 0;
		glGenTextures(1, &Tex);
		if (!Tex)
		{
			// glGenTextures returning 0 means GL errored -- typically no
			// GL context is current on THIS thread (loader thread?).
			fprintf(stderr, "[GLES3] Upload2D: glGenTextures failed, glGetError=0x%x (no ctx on this thread?)\n",
				(unsigned)glGetError());
			++g_GLES3_UploadFail;
			free(pDecoded);
			return 0;
		}
		glBindTexture(GL_TEXTURE_2D, Tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		// Skip padding columns via GL_UNPACK_ROW_LENGTH (=padded row width
		// in pixels). Reset to 0 (=default: tightly packed) after upload
		// so subsequent glTexImage2D calls behave normally.
		glPixelStorei(GL_UNPACK_ROW_LENGTH, PadW);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pDecoded);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		if (_bGenerateMipmaps)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
		free(pDecoded);
		return Tex;
	}

	// Other compression types (3DC/BC5 normal maps, JPG, ...): use the
	// engine's own decompressor into a plain image, then upload that.
	if (_pImage->IsCompressed())
	{
		CImage Tmp;
		_pImage->Decompress(&Tmp);
		if (Tmp.GetWidth() > 0 && !Tmp.IsCompressed())
			return Upload2D(&Tmp, _bGenerateMipmaps);
		fprintf(stderr, "[GLES3] Upload2D: Decompress failed (mem=0x%x fmt=0x%x %dx%d)\n",
			(unsigned)_pImage->GetMemModel(), (unsigned)_pImage->GetFormat(), W, H);
		++g_GLES3_UploadFail;
		return 0;
	}

	SGLES3Format F = MapFormat(_pImage->GetFormat());
	if (!F.Supported)
	{
		fprintf(stderr, "[GLES3] Upload2D: unsupported image format 0x%x (%dx%d)\n",
			(unsigned)_pImage->GetFormat(), W, H);
		++g_GLES3_UploadFail;
		return 0;
	}

	void* pLocked = _pImage->Lock();
	if (!pLocked)
	{
		fprintf(stderr, "[GLES3] Upload2D: Lock()==NULL (%dx%d fmt=0x%x mem=0x%x)\n",
			W, H, (unsigned)_pImage->GetFormat(), (unsigned)_pImage->GetMemModel());
		++g_GLES3_UploadFail;
		return 0;
	}
	++g_GLES3_UploadRGBA;

	// If BGR(A) input on core GLES: copy + swizzle. For BGRX8 alpha byte
	// is undefined, force it to 0xff so the sampler yields opaque.
	unsigned char* pSrc = (unsigned char*)pLocked;
	unsigned char* pTmp = 0;
	const int Pixels = W * H;
	if (F.NeedsSwizzle && F.BytesPerPixel == 4)
	{
		pTmp = (unsigned char*)malloc(Pixels * 4);
		if (pTmp)
		{
			memcpy(pTmp, pSrc, Pixels * 4);
			SwizzleBGRA_RGBA(pTmp, Pixels);
			if (_pImage->GetFormat() == IMAGE_FORMAT_BGRX8)
				for (int i = 0; i < Pixels; ++i) pTmp[i * 4 + 3] = 0xff;
			pSrc = pTmp;
		}
	}
	else if (F.NeedsSwizzle && F.BytesPerPixel == 3)
	{
		pTmp = (unsigned char*)malloc(Pixels * 3);
		if (pTmp)
		{
			for (int i = 0; i < Pixels; ++i)
			{
				pTmp[i * 3 + 0] = pSrc[i * 3 + 2];
				pTmp[i * 3 + 1] = pSrc[i * 3 + 1];
				pTmp[i * 3 + 2] = pSrc[i * 3 + 0];
			}
			pSrc = pTmp;
		}
	}

	GLuint Tex = 0;
	glGenTextures(1, &Tex);
	if (!Tex)
	{
		fprintf(stderr, "[GLES3] Upload2D: glGenTextures failed, glGetError=0x%x (no ctx on this thread?)\n",
			(unsigned)glGetError());
		++g_GLES3_UploadFail;
		if (pTmp) free(pTmp);
		return 0;
	}

	glBindTexture(GL_TEXTURE_2D, Tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// RIDDICK_DUMP_GL_TEX=<width>: if uploaded image matches that width,
	// dump the *post-swizzle* CPU-side buffer (what glTexImage2D actually
	// sees) as a PPM to /tmp/openriddick_tex_<W>x<H>_<n>.ppm. Rides on
	// width because we don't have the engine texture ID here. Cap 8.
	if (F.BytesPerPixel >= 3 && getenv("RIDDICK_DUMP_GL_TEX"))
	{
		static int sDumped = 0;
		const int wantW = atoi(getenv("RIDDICK_DUMP_GL_TEX"));
		if (sDumped < 8 && (wantW == 0 || wantW == W))
		{
			char path[256];
			snprintf(path, sizeof(path), "/tmp/openriddick_tex_%dx%d_%d.ppm", W, H, sDumped);
			FILE* fp = fopen(path, "wb");
			if (fp)
			{
				fprintf(fp, "P6\n%d %d\n255\n", W, H);
				const int bpp = F.BytesPerPixel;
				for (int i = 0; i < W*H; ++i)
					fwrite(pSrc + i*bpp, 1, 3, fp);
				fclose(fp);
				fprintf(stderr, "[GLES3-TEX-DUMP] wrote %s (fmt=0x%x)\n", path, (unsigned)_pImage->GetFormat());
				++sDumped;
			}
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, F.InternalFormat, W, H, 0, F.Format, F.Type, pSrc);

	if (_bGenerateMipmaps)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// GL_REPEAT: world/prop tiling textures use UV outside [0..1] to
	// tile (e.g. wall Aguerra06_0004_C uses V ≈ 7.5). CLAMP_TO_EDGE
	// clamps them to the edge row = uniform darkish stripe on every
	// wall. Compressed-DXT upload path (line ~179) already uses REPEAT
	// — this uncompressed path was inconsistent. Lightmaps and font
	// atlases don't tile: they'd break under REPEAT if sampled off-edge,
	// but engine UV for them stays in [0..1] so REPEAT is safe there too.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);

	// Single/dual channel formats: reconstruct the classic GL semantics
	// via texture swizzle (GLES 3.0 core). Without this a GL_R8 font
	// texture samples as (a,0,0,1) - opaque red text.
	switch (_pImage->GetFormat())
	{
	case IMAGE_FORMAT_A8:		// GL_ALPHA: (1,1,1,a)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
		break;
	case IMAGE_FORMAT_I8:		// GL_INTENSITY: (i,i,i,i)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
		break;
	case IMAGE_FORMAT_I8A8:		// GL_LUMINANCE_ALPHA: (i,i,i,a)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
		break;
	default:
		break;
	}

	if (pTmp) free(pTmp);
	return Tex;
}

#endif // PLATFORM_LINUX
