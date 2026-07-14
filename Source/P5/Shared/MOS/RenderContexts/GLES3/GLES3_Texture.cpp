#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_Texture.h"

#include "../../MSystem/Raster/MImage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

	SGLES3Format F = MapFormat(_pImage->GetFormat());
	if (!F.Supported)
	{
		fprintf(stderr, "[GLES3] Upload2D: unsupported image format 0x%x (%dx%d)\n",
			(unsigned)_pImage->GetFormat(), W, H);
		return 0;
	}

	void* pLocked = _pImage->Lock();
	if (!pLocked) return 0;

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
	if (!Tex) { if (pTmp) free(pTmp); return 0; }

	glBindTexture(GL_TEXTURE_2D, Tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

	if (pTmp) free(pTmp);
	return Tex;
}

#endif // PLATFORM_LINUX
