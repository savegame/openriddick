// CImage -> GLuint uploader for the GLES3 backend (Phase 4 milestone M0).
//
// Format table covers the uncompressed variants the UI/frontend uses;
// DXT/S3TC and cubemaps arrive in M5. Palettised formats are converted
// to RGBA8 on the CPU before upload.

#pragma once

#ifdef PLATFORM_LINUX

#include <GLES3/gl3.h>

class CImage;

struct SGLES3Format
{
	GLint  InternalFormat; // e.g. GL_RGBA8
	GLenum Format;         // e.g. GL_RGBA
	GLenum Type;           // e.g. GL_UNSIGNED_BYTE
	int    BytesPerPixel;  // uncompressed stride hint
	bool   NeedsSwizzle;   // BGR(A) -> RGBA swizzle on the CPU
	bool   Supported;
};

// Diagnostic counters, bumped by Upload2D. Read + reset by the render
// context per-frame diagnostics.
extern int g_GLES3_UploadRGBA;
extern int g_GLES3_UploadDXT1;
extern int g_GLES3_UploadDXT3;
extern int g_GLES3_UploadDXT5;
extern int g_GLES3_UploadFail;

class CGLES3TextureUploader
{
public:
	// Map an IMAGE_FORMAT_* value to a GL upload descriptor. Sets
	// .Supported = false for formats we can't upload directly yet.
	static SGLES3Format MapFormat(int _ImageFormat);

	// Upload the top mip of _pImage into a fresh GL texture. Returns 0
	// on failure. Applies linear filtering + clamp-to-edge by default;
	// caller can rebind and change sampler state after.
	static GLuint Upload2D(CImage* _pImage, bool _bGenerateMipmaps = true);

	// Small helper: swap R and B in-place on an RGBA8 (or BGRA8) buffer.
	static void   SwizzleBGRA_RGBA(unsigned char* _pPixels, int _nPixels);
};

#endif // PLATFORM_LINUX
