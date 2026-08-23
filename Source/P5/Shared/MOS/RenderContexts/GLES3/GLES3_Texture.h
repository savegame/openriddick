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
	//
	// _FaceTarget/_ExistingTex exist for UploadCube: pass one of
	// GL_TEXTURE_CUBE_MAP_POSITIVE_X.. plus the cube's texture name and the
	// image lands on that face of that cube instead of in a new 2D texture.
	// Everything else (format table, DXT decode, swizzle) is shared.
	static GLuint Upload2D(CImage* _pImage, bool _bGenerateMipmaps = true,
		GLenum _FaceTarget = GL_TEXTURE_2D, GLuint _ExistingTex = 0);

	// Build a GL_TEXTURE_CUBE_MAP from six face images, in the engine's
	// chain order (which is also GL's: +X, -X, +Y, -Y, +Z, -Z). A NULL
	// entry is allowed only if _pFaces[0] is valid -- it is then reused for
	// the remaining faces, which is exactly CTC_TEXTUREFLAGS_CUBEMAP
	// ("this texture is used on all 6 faces", MTexture.h:103).
	// Returns 0 on failure.
	static GLuint UploadCube(CImage* const _pFaces[6]);

	// Small helper: swap R and B in-place on an RGBA8 (or BGRA8) buffer.
	static void   SwizzleBGRA_RGBA(unsigned char* _pPixels, int _nPixels);
};

#endif // PLATFORM_LINUX
