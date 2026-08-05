#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_Texture.h"
#include "GLES3_DXT.h"

#include "../../MSystem/Raster/MImage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

int g_GLES3_UploadRGBA = 0;
int g_GLES3_UploadDXT1 = 0;
int g_GLES3_UploadDXT3 = 0;
int g_GLES3_UploadDXT5 = 0;
int g_GLES3_UploadFail = 0;

// RIDDICK_DUMP_GL_TEX=<width>: writes the CPU-side pixel buffer that
// will be uploaded to GL (post-swizzle for raw, post-DXT-decode for
// S3TC) as a PPM in the current working directory. Rides on width
// since the CImage carries no engine texture ID here. Cap 8 across
// both upload paths. Set wantW=0 to dump ANY width.
static void MaybeDumpPPM(const char* _tag, const unsigned char* _pixels,
                         int W, int H, int _bpp, unsigned _fmt)
{
	if (_bpp < 3 || !_pixels) return;
	const char* env = getenv("RIDDICK_DUMP_GL_TEX");
	if (!env) return;
	static int sDumped = 0;
	if (sDumped >= 8) return;
	const int wantW = atoi(env);
	if (wantW != 0 && wantW != W) return;
	char path[256];
	snprintf(path, sizeof(path), "openriddick_tex_%s_%dx%d_%d.ppm",
		_tag, W, H, sDumped);
	FILE* fp = fopen(path, "wb");
	if (!fp) { fprintf(stderr, "[GLES3-TEX-DUMP] fopen('%s') failed: %s\n", path, strerror(errno)); return; }
	fprintf(fp, "P6\n%d %d\n255\n", W, H);
	for (int i = 0; i < W*H; ++i)
		fwrite(_pixels + i*_bpp, 1, 3, fp);
	fclose(fp);
	fprintf(stderr, "[GLES3-TEX-DUMP] wrote %s  (fmt=0x%x bpp=%d)\n",
		path, _fmt, _bpp);
	fflush(stderr);
	++sDumped;
}

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

GLuint CGLES3TextureUploader::Upload2D(CImage* _pImage, bool _bGenerateMipmaps,
	GLenum _FaceTarget, GLuint _ExistingTex)
{
	// _FaceTarget is GL_TEXTURE_2D for an ordinary texture, or one of
	// GL_TEXTURE_CUBE_MAP_POSITIVE_X.. when UploadCube feeds one face of
	// a cube through here -- that reuses every format/DXT/swizzle path
	// below instead of growing a second copy of it. Sampler state and
	// mipmap generation always address the CONTAINER target.
	const GLenum kBindTarget = (_FaceTarget == GL_TEXTURE_2D)
	                         ? (GLenum)GL_TEXTURE_2D : (GLenum)GL_TEXTURE_CUBE_MAP;
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
		GLuint Tex = _ExistingTex;
		if (!Tex) glGenTextures(1, &Tex);
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
		glBindTexture(kBindTarget, Tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		// Skip padding columns via GL_UNPACK_ROW_LENGTH (=padded row width
		// in pixels). Reset to 0 (=default: tightly packed) after upload
		// so subsequent glTexImage2D calls behave normally.
		glPixelStorei(GL_UNPACK_ROW_LENGTH, PadW);
		// Dump post-DXT-decode RGBA8 (padded to PadW; caller trims later).
		MaybeDumpPPM("dxt", pDecoded, PadW, PadH, 4, (unsigned)_pImage->GetFormat());
		glTexImage2D(_FaceTarget, 0, GL_RGBA8, W, H, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pDecoded);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		// RIDDICK_NO_MIPMAP=1: force GL_LINEAR (skip mipmap sampling).
		// Diagnoses "chrome wall" -- if walls appear textured with this
		// on, glGenerateMipmap silently produced incomplete levels and
		// the LINEAR_MIPMAP_LINEAR sampler returns garbage / placeholder.
		static int sNoMipmap = -1;
		if (sNoMipmap < 0)
		{
			const char* e = getenv("RIDDICK_NO_MIPMAP");
			sNoMipmap = (e && *e && *e != '0') ? 1 : 0;
		}
		if (_bGenerateMipmaps && !sNoMipmap)
		{
			glGenerateMipmap(kBindTarget);
			GLenum err = glGetError();
			if (err != GL_NO_ERROR)
				fprintf(stderr, "[GLES3-TEX] glGenerateMipmap failed err=0x%x on %dx%d DXT\n", (unsigned)err, W, H);
			glTexParameteri(kBindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(kBindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		glTexParameteri(kBindTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_S,     GL_REPEAT);
		glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_T,     GL_REPEAT);
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

	GLuint Tex = _ExistingTex;
	if (!Tex) glGenTextures(1, &Tex);
	if (!Tex)
	{
		fprintf(stderr, "[GLES3] Upload2D: glGenTextures failed, glGetError=0x%x (no ctx on this thread?)\n",
			(unsigned)glGetError());
		++g_GLES3_UploadFail;
		if (pTmp) free(pTmp);
		return 0;
	}

	glBindTexture(kBindTarget, Tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	MaybeDumpPPM("bgra", pSrc, W, H, F.BytesPerPixel, (unsigned)_pImage->GetFormat());
	glTexImage2D(_FaceTarget, 0, F.InternalFormat, W, H, 0, F.Format, F.Type, pSrc);

	{
		static int sNoMip = -1;
		if (sNoMip < 0)
		{
			const char* e = getenv("RIDDICK_NO_MIPMAP");
			sNoMip = (e && *e && *e != '0') ? 1 : 0;
		}
		// 1D lookup tables (the depth-fog ramp is 8x1) must never be
		// mipmapped or wrapped: a ramp lookup spans the full 0..1 range
		// across a surface, so the derivative is huge and the sampler
		// drops to a coarse level -- which for an 8x1 texture averages
		// the whole ramp into one flat value. That reads as uniform
		// white-out fog regardless of distance (observed on TheDream,
		// 2026-07-27). Clamp + no mips is what a LUT wants.
		const bool bLUT = (W <= 1 || H <= 1);
		if (bLUT)
		{
			glTexParameteri(kBindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else if (_bGenerateMipmaps && !sNoMip)
		{
			glGenerateMipmap(kBindTarget);
			GLenum err = glGetError();
			if (err != GL_NO_ERROR)
				fprintf(stderr, "[GLES3-TEX] glGenerateMipmap failed err=0x%x on %dx%d BGRA\n", (unsigned)err, W, H);
			glTexParameteri(kBindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(kBindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
	}
	glTexParameteri(kBindTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// GL_REPEAT: world/prop tiling textures use UV outside [0..1] to
	// tile (e.g. wall Aguerra06_0004_C uses V ≈ 7.5). CLAMP_TO_EDGE
	// clamps them to the edge row = uniform darkish stripe on every
	// wall. Compressed-DXT upload path (line ~179) already uses REPEAT
	// — this uncompressed path was inconsistent. Lightmaps and font
	// atlases don't tile: they'd break under REPEAT if sampled off-edge,
	// but engine UV for them stays in [0..1] so REPEAT is safe there too.
	// ...except 1D LUTs (fog ramp), which were already set to CLAMP above
	// and must stay clamped -- wrapping a ramp turns "past the far plane"
	// into "no fog at all".
	if (!(W <= 1 || H <= 1))
	{
		glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_S,     GL_REPEAT);
		glTexParameteri(kBindTarget, GL_TEXTURE_WRAP_T,     GL_REPEAT);
	}

	// Single/dual channel formats: reconstruct the classic GL semantics
	// via texture swizzle (GLES 3.0 core). Without this a GL_R8 font
	// texture samples as (a,0,0,1) - opaque red text.
	switch (_pImage->GetFormat())
	{
	case IMAGE_FORMAT_A8:		// GL_ALPHA: (1,1,1,a)
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_R, GL_ONE);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_G, GL_ONE);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_B, GL_ONE);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_A, GL_RED);
		break;
	case IMAGE_FORMAT_I8:		// GL_INTENSITY: (i,i,i,i)
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_R, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_G, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_A, GL_RED);
		break;
	case IMAGE_FORMAT_I8A8:		// GL_LUMINANCE_ALPHA: (i,i,i,a)
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_R, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_G, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(kBindTarget, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
		break;
	default:
		break;
	}

	if (pTmp) free(pTmp);
	return Tex;
}

// ---------------------------------------------------------------------------
//  Cube maps
// ---------------------------------------------------------------------------
// Needed because the light PROJECTION channel is a cube map, not a 2D texture:
// the engine fills that slot with m_TextureID_Special_Cube_ffffffff by default
// (XRShader_FP20.cpp:663, 1122), the content ships lamp cookies as
// 'Cube_Lamp010_00' chains, and Docs/FP_Reference.md §4.2 has the retail
// program doing textureCube(ProjMap, tc).a. Sampling those as 2D is what
// rotated and hard-edged every flashlight/lamp cookie in the port.
//
// Face order: the engine's chain order maps 1:1 onto GL's
// GL_TEXTURE_CUBE_MAP_POSITIVE_X + i -- same assumption the PS3 backend makes
// when it calls BuildCube(..., i, true) for chain entry i
// (MRenderPS3_Texture.cpp:1084-1092). No vertical flip is applied: PC retail
// rendered through OpenGL (RndrGL), so the shipped faces are already in GL
// orientation. RIDDICK_CUBE_FLIPY=1 negates the lookup's Y in the shader if
// that ever turns out to be wrong -- a one-flag A/B rather than a re-upload.
// Content probe for the cookie itself (RIDDICK_DBG_GL=1). The projection
// factor is ProjMapTexel.a, and for an IMAGE_FORMAT_I8 cookie that alpha IS
// the single stored channel (our swizzle maps A<-RED, matching GL_INTENSITY
// semantics the retail path relies on). So when an isolated 'proj' frame comes
// out uniformly black or uniformly white, the first thing to establish is
// whether the SOURCE is uniform -- otherwise we keep re-testing the sampling
// math against a texture that has nothing in it. Prints min/max/mean of the
// first channel of face 0.
static void GLES3_LogCubeContent(const char* _pName, CImage* _pImg)
{
	static int sLogged = 0;
	if (!_pImg || sLogged >= 8) return;
	const char* e = getenv("RIDDICK_DBG_GL");
	if (!e || !*e || *e == '0') return;
	if (_pImg->IsCompressed()) return;			// probe raw formats only
	const int W = _pImg->GetWidth(), H = _pImg->GetHeight();
	const SGLES3Format F = CGLES3TextureUploader::MapFormat(_pImg->GetFormat());
	if (W <= 0 || H <= 0 || !F.Supported || F.BytesPerPixel <= 0) return;
	const unsigned char* p = (const unsigned char*)_pImg->Lock();
	if (!p) return;
	int mn = 255, mx = 0;
	long long sum = 0;
	const int n = W * H;
	for (int i = 0; i < n; ++i)
	{
		const int v = p[(size_t)i * F.BytesPerPixel];
		if (v < mn) mn = v;
		if (v > mx) mx = v;
		sum += v;
	}
	_pImg->Unlock();
	++sLogged;
	fprintf(stderr, "[GLES3-CUBE-SRC] '%s' %dx%d fmt=0x%x ch0: min=%d max=%d mean=%d\n",
		_pName ? _pName : "?", W, H, (unsigned)_pImg->GetFormat(),
		mn, mx, (int)(sum / (n ? n : 1)));
	fflush(stderr);
}

GLuint CGLES3TextureUploader::UploadCube(CImage* const _pFaces[6])
{
	if (!_pFaces || !_pFaces[0]) return 0;

	GLES3_LogCubeContent("cube face0", _pFaces[0]);

	GLuint Tex = 0;
	glGenTextures(1, &Tex);
	if (!Tex)
	{
		fprintf(stderr, "[GLES3] UploadCube: glGenTextures failed, glGetError=0x%x (no ctx on this thread?)\n",
			(unsigned)glGetError());
		++g_GLES3_UploadFail;
		return 0;
	}

	for (int i = 0; i < 6; ++i)
	{
		// NULL face -> reuse face 0 (CTC_TEXTUREFLAGS_CUBEMAP semantics).
		CImage* pImg = _pFaces[i] ? _pFaces[i] : _pFaces[0];
		// Mipmaps once at the end: glGenerateMipmap on a cube needs all six
		// faces present, so per-face generation would work on an incomplete
		// texture.
		if (!Upload2D(pImg, false, (GLenum)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i), Tex))
		{
			fprintf(stderr, "[GLES3] UploadCube: face %d failed to upload\n", i);
			++g_GLES3_UploadFail;
			glDeleteTextures(1, &Tex);
			return 0;
		}
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, Tex);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	const GLenum MipErr = glGetError();
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
		MipErr ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// Clamp on all three axes: a cookie must not tile across the seams.
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	return Tex;
}

#endif // PLATFORM_LINUX
