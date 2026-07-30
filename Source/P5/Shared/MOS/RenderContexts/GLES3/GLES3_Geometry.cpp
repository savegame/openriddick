#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_Geometry.h"

// Full CXR_VBContext definition + CRC_RIP_*/VB_GETFLAGS_* constants come
// in transitively through MSystem_Core.h -> XR/XRVBContext.h -> XR.h ->
// XR/XRUtil.h -> XR/XRVertexBuffer.h -> XR/XRVertexBuffer_VPUShared.h
// (same chain MDisplaySDL2.cpp already relies on for m_pVBCtx->VB_Get()).
#include "../../MSystem/MSystem_Core.h"
#include "../../MSystem/Raster/MRCCore.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// Same byte swap MDisplaySDL2.cpp's PackColorBGRA_to_RGBA does: the
// engine's N4_COL registers are BGRA in memory (MImage.h convention);
// GL_UNSIGNED_BYTE normalized reads memory order, so channel 0/2 need
// swapping to land red/blue correctly. Duplicated here (rather than
// exported) because the original is a private static inside the
// CRC_GLES3 class nested in MDisplaySDL2.cpp.
static inline uint32_t GLES3Geom_SwapBR(uint32_t _bgra)
{
	return ( _bgra & 0xff00ff00u)
		 | ((_bgra & 0x00ff0000u) >> 16)
		 | ((_bgra & 0x000000ffu) << 16);
}

// One-shot log gates, so a persistently-unsupported VBID or a stalled
// GL-context wait doesn't flood stderr every time Ensure() is called.
static bool GLES3Geom_DbgEnabled()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_DBG_GL");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_SKINNING=1 (opt-in, default OFF) -- see Docs/HacksAndHooks.md
// "Skinning (matrix palette)". Same gate MDisplaySDL2.cpp's
// GLES3_SkinningEnabled() checks; duplicated here (rather than shared)
// because this TU has no visibility into the CRC_GLES3 class nested in
// that file, same reasoning as GLES3Geom_SwapBR above. When off, Build()
// keeps the pre-existing behaviour: any VBID carrying matrix-palette
// registers is permanently skipped (m_bSkip), same as before this feature
// existed -- RIDDICK_SKIP_SKINNED remains the emergency killswitch on top
// of that (see MDisplaySDL2.cpp's own copies of the skip check for the
// non-cached streaming path, which this flag does NOT affect).
// RIDDICK_HWSKIN=1 implies it as well -- that flag makes the engine send
// animated meshes down the VBID path, and skipping them here would delete
// every character from the frame. Same duplicated-gate reasoning as above.
static bool GLES3Geom_SkinningEnabled()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_SKINNING");
		const char* eHW = getenv("RIDDICK_HWSKIN");
		s = ((e && *e && *e != '0') || (eHW && *eHW && *eHW != '0')) ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_SKIP_SKINNED=1 -- the killswitch that hides matrix-palette
// geometry outright.
//
// It used to work because EVERY draw went through MDisplaySDL2.cpp's
// streaming paths (BuildVertsFromVBB / the m_Geom path), each of which
// checks this flag itself. Since the VBID geometry cache (R0) took over
// the hot path, those checks stopped covering the draws that matter, and
// the flag silently became a no-op for exactly the geometry it exists to
// hide -- with RIDDICK_SKINNING=1 the cache built and drew palette meshes
// regardless (observed 2026-07-28, Pa1_TheDream: the flag no longer
// removed the scattered polygons). Honour it here too, and independently
// of RIDDICK_SKINNING: "skip skinned" must mean skip, whether or not GPU
// skinning is compiled into the draw.
static bool GLES3Geom_SkipSkinned()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_SKIP_SKINNED");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

CGLES3GeometryCache::CGLES3GeometryCache()
	: m_pVBCtx(0), m_pRC(0), m_VBIDCapacity(0), m_nBuilt(0), m_nBytesV(0), m_nBytesI(0)
{
}

CGLES3GeometryCache::~CGLES3GeometryCache()
{
	DestroyAll();
}

void CGLES3GeometryCache::Init(CXR_VBContext* _pVBCtx, CRC_Core* _pRC)
{
	m_pVBCtx = _pVBCtx;
	m_pRC = _pRC;
	m_VBIDCapacity = m_pVBCtx ? m_pVBCtx->GetIDCapacity() : 0;
	m_lEntries.SetLen(0);
	if (m_VBIDCapacity > 0)
		EnsureCapacity(m_VBIDCapacity - 1);
}

CRC_VBIDInfo* CGLES3GeometryCache::FreshInfo(int _VBID)
{
	if (!m_pRC || _VBID < 0 || _VBID >= m_VBIDCapacity) return NULL;
	CRC_VBIDInfo* pInfo = m_pRC->VB_GetVBIDInfo();
	return pInfo ? (pInfo + _VBID) : NULL;
}

void CGLES3GeometryCache::ResetEntry(SGLES3GeomEntry& _E)
{
	_E.m_VBO = 0;
	_E.m_IBO = 0;
	_E.m_nV = 0;
	_E.m_Stride = 0;
	_E.m_nIdx = 0;
	for (int i = 0; i < CRC_MAXVERTEXREG; ++i)
	{
		_E.m_lRegOffset[i] = -1;
		_E.m_lRegFormat[i] = CRC_VREGFMT_VOID;
	}
	_E.m_bValid = false;
	_E.m_bSkip = false;
}

void CGLES3GeometryCache::EnsureCapacity(int _VBID)
{
	if (_VBID < 0) return;
	if (_VBID < m_lEntries.Len()) return;
	const int Old = m_lEntries.Len();
	m_lEntries.SetLen(_VBID + 1);
	for (int i = Old; i < m_lEntries.Len(); ++i)
		ResetEntry(m_lEntries[i]);
}

const SGLES3GeomEntry* CGLES3GeometryCache::Ensure(int _VBID)
{
	if (_VBID <= 0 || !m_pVBCtx) return NULL;
	CRC_VBIDInfo* pInfo = FreshInfo(_VBID);
	if (!pInfo) return NULL;
	EnsureCapacity(_VBID);
	if (m_lEntries[_VBID].m_bSkip) return NULL;
	if (!(pInfo->m_Fresh & 1) || !m_lEntries[_VBID].m_bValid)
	{
		if (!Build(_VBID))
			return NULL;
	}
	// Re-index rather than holding a reference across Build(): the entry
	// array can reallocate inside it.
	SGLES3GeomEntry& E = m_lEntries[_VBID];
	return E.m_bValid ? &E : NULL;
}

void CGLES3GeometryCache::Destroy(int _VBID)
{
	if (_VBID <= 0 || _VBID >= m_lEntries.Len()) return;
	SGLES3GeomEntry& E = m_lEntries[_VBID];
	if (E.m_VBO) { glDeleteBuffers(1, &E.m_VBO); E.m_VBO = 0; }
	if (E.m_IBO) { glDeleteBuffers(1, &E.m_IBO); E.m_IBO = 0; }
	E.m_nV = 0;
	E.m_Stride = 0;
	E.m_nIdx = 0;
	for (int i = 0; i < CRC_MAXVERTEXREG; ++i)
	{
		E.m_lRegOffset[i] = -1;
		E.m_lRegFormat[i] = CRC_VREGFMT_VOID;
	}
	E.m_bValid = false;
	// Note: m_bSkip deliberately left untouched here -- see header comment.
	if (CRC_VBIDInfo* pInfo = FreshInfo(_VBID))
		pInfo->m_Fresh &= ~1;
}

void CGLES3GeometryCache::DestroyAll()
{
	for (int i = 0; i < m_lEntries.Len(); ++i)
	{
		SGLES3GeomEntry& E = m_lEntries[i];
		if (E.m_VBO) { glDeleteBuffers(1, &E.m_VBO); E.m_VBO = 0; }
		if (E.m_IBO) { glDeleteBuffers(1, &E.m_IBO); E.m_IBO = 0; }
		E.m_bValid = false;
	}
}

void CGLES3GeometryCache::FlushUnused()
{
	if (!m_pVBCtx) return;
	for (int i = 1; i < m_lEntries.Len(); ++i)
	{
		SGLES3GeomEntry& E = m_lEntries[i];
		if (!E.m_bValid && !E.m_bSkip) continue;
		const int Flags = m_pVBCtx->VB_GetFlags(i);
		if (Flags & (CXR_VBFLAGS_PRECACHE | CXR_VBFLAGS_ALLOCATED)) continue;
		Destroy(i);
		// Unlike the plain per-VBID Destroy(), a flush means the slot
		// may be handed to entirely different geometry later (it's no
		// longer PRECACHE'd/ALLOCATED) -- clear the sticky skip flag too.
		E.m_bSkip = false;
	}
}

// Mirrors CContext_Geometry::Build (RenderContexts/PS3GCM/MRenderPS3_Geometry.cpp).
bool CGLES3GeometryCache::Build(int _VBID)
{
	Destroy(_VBID);
	EnsureCapacity(_VBID);
	SGLES3GeomEntry& E = m_lEntries[_VBID];

	CRC_BuildVertexBuffer VBB;
	VBB.Clear();
	m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);

	// Nothing here yet at all (neither vertex nor index data) -- not a
	// permanent failure, the engine may populate this VBID later.
	// NOTE: unlike a first draft of this cache, we do NOT bail out just
	// because VBB.m_nV <= 0. CBSP2_SLCIBContainer::Get (WBSP2Misc.cpp)
	// hands out VBIDs that are index-only (m_nV == 0, only m_piPrim
	// filled in) -- that's exactly the shared index pool half of
	// Render_VertexBuffer_IndexBufferTriangles, i.e. the BSP2 world path
	// this cache exists to speed up. Bailing here would mean that half
	// of the pair never caches and the whole feature is a no-op for
	// world geometry.
	if (VBB.m_nV <= 0 && !VBB.m_piPrim)
	{
		m_pVBCtx->VB_Release(_VBID);
		return false;
	}

	// Skinned meshes (matrix-palette blend regs present). RIDDICK_SKINNING=1
	// (opt-in, default off): let these through -- the MI0/MI1 registers get
	// the passthrough DstFormat fix just below (so bone-index bytes survive
	// the interleave untouched) and MW0/MW1 already flow through the
	// generic float-register path unchanged; the actual palette blend
	// happens GPU-side in the vertex shader (see MDisplaySDL2.cpp
	// SetVertexAttribPointersFromEntry / kGLES3_SkinningGLSL). Default
	// (flag off): unchanged pre-existing behaviour -- permanently skip, same
	// limitation as the streaming BuildVertsFromVBB path under
	// RIDDICK_SKIP_SKINNED.
	const bool bSkinning = GLES3Geom_SkinningEnabled();
	if (VBB.m_nV > 0 &&
	    (VBB.m_lpVReg[CRC_VREG_MI0] || VBB.m_lpVReg[CRC_VREG_MW0] ||
	     VBB.m_lpVReg[CRC_VREG_MI1] || VBB.m_lpVReg[CRC_VREG_MW1]))
	{
		if (!bSkinning || GLES3Geom_SkipSkinned())
		{
			if (GLES3Geom_DbgEnabled())
				fprintf(stderr, "[GLES3-GEOM] VBID=%d skip (skinned, %s)\n", _VBID,
					!bSkinning ? "RIDDICK_SKINNING=0" : "RIDDICK_SKIP_SKINNED=1");
			E.m_bSkip = true;
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}
		if (GLES3Geom_DbgEnabled())
			fprintf(stderr, "[GLES3-GEOM] VBID=%d skinned: MI0=%d MW0=%d MI1=%d MW1=%d\n",
				_VBID, VBB.m_lpVReg[CRC_VREG_MI0] ? 1 : 0, VBB.m_lpVReg[CRC_VREG_MW0] ? 1 : 0,
				VBB.m_lpVReg[CRC_VREG_MI1] ? 1 : 0, VBB.m_lpVReg[CRC_VREG_MW1] ? 1 : 0);
	}

	void* pMem = NULL;
	// First vertex's UV0, sampled while the CPU buffer is still alive (it
	// is freed right after upload) -- see the diagnostic at the end.
	float DbgU0 = 0.0f, DbgV0 = 0.0f;
	int Stride = 0;
	int lRegOffset[CRC_MAXVERTEXREG];
	uint8 lRegFormat[CRC_MAXVERTEXREG];
	for (int i = 0; i < CRC_MAXVERTEXREG; ++i) { lRegOffset[i] = -1; lRegFormat[i] = CRC_VREGFMT_VOID; }

	if (VBB.m_nV > 0)
	{
		// Destination format: F32 for everything except a native BGRA
		// colour register, which stays packed (ConvertToInterleaved only
		// processes registers where the SOURCE format != VOID -- see
		// MRender.cpp:2210 -- so registers absent from m_Format must stay
		// VOID here too or the stride computation would disagree with
		// what ConvertToInterleaved actually writes).
		CRC_VertexFormat DstFmt;
		DstFmt.Clear();
		int Offset = 0;
		for (int reg = 0; reg < CRC_MAXVERTEXREG; ++reg)
		{
			const int SrcFmt = VBB.m_Format.GetFormat(reg);
			if (SrcFmt == CRC_VREGFMT_VOID)
				continue;

			int DstFormat;
			if (reg == CRC_VREG_COLOR && SrcFmt == CRC_VREGFMT_N4_COL)
				DstFormat = CRC_VREGFMT_N4_COL;
			else if ((reg == CRC_VREG_MI0 || reg == CRC_VREG_MI1) &&
			         (SrcFmt == CRC_VREGFMT_N4_UI8_P32 || SrcFmt == CRC_VREGFMT_N4_UI8_P32_NORM))
			{
				// Passthrough, same trick as the CRC_VREG_COLOR case above:
				// CRC_VertexFormat::ConvertRegisterFormat decodes N4_UI8_P32
				// to a raw 0..255 float and N4_UI8_P32_NORM to a 0..1 float
				// (byte/255), then re-encodes on the way out -- picking the
				// SAME enum for Dst as Src makes decode+encode an identity
				// on the underlying byte, whichever convention the model
				// used (the engine's own Geometry_MatrixIndex0 setter always
				// uses the _NORM tag -- MRender_Classes.h:1150 -- but forcing
				// one specific DstFormat here regardless of SrcFmt would NOT
				// round-trip for the other convention: encoding a 0..1 float
				// as _P32 rounds every index down to 0 or 1). The result is
				// that our vertex buffer always ends up holding the raw
				// unnormalized bone-index bytes, read back as an unsigned
				// integer attribute (see BindEntryAttrib/glVertexAttribIPointer
				// in MDisplaySDL2.cpp) -- see Docs/VP_Reference.md §5.2.
				DstFormat = SrcFmt;
			}
			else
			{
				int nComp = CRC_VertexFormat::GetRegisterComponents(SrcFmt);
				if (nComp < 1) nComp = 1;
				if (nComp > 4) nComp = 4;
				DstFormat = CRC_VREGFMT_V1_F32 + (nComp - 1);
			}
			DstFmt.SetFormat(reg, DstFormat);
			lRegOffset[reg] = Offset;
			lRegFormat[reg] = (uint8)DstFormat;
			Offset += CRC_VertexFormat::GetRegisterSize(DstFormat);
		}

		Stride = DstFmt.GetStride();
		if (Stride != Offset || Stride <= 0)
		{
			fprintf(stderr, "[GLES3-GEOM] VBID=%d stride mismatch (computed=%d, GetStride=%d) -- skip\n",
				_VBID, Offset, Stride);
			E.m_bSkip = true;
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}

		pMem = malloc((size_t)Stride * (size_t)VBB.m_nV);
		if (!pMem)
		{
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}

		CRC_VRegTransform DstScale[CRC_MAXVERTEXREGSCALE];
		for (int i = 0; i < CRC_MAXVERTEXREGSCALE; ++i)
		{
			DstScale[i].m_Scale = CVec4Dfp32(1.0f);
			DstScale[i].m_Offset = CVec4Dfp32(0.0f);
		}
		VBB.ConvertToInterleaved(pMem, DstFmt, DstScale, 0, VBB.m_nV);

		// BGRA -> RGBA byte swap for the packed colour register (see
		// GLES3Geom_SwapBR comment above).
		if (lRegOffset[CRC_VREG_COLOR] >= 0 && lRegFormat[CRC_VREG_COLOR] == CRC_VREGFMT_N4_COL)
		{
			uint8* pBase = (uint8*)pMem + lRegOffset[CRC_VREG_COLOR];
			for (int i = 0; i < VBB.m_nV; ++i)
			{
				uint32_t* pCol = (uint32_t*)(pBase + (size_t)i * Stride);
				*pCol = GLES3Geom_SwapBR(*pCol);
			}
		}

		const int UVOff = lRegOffset[CRC_VREG_TEXCOORD0];
		const int UVFmt = lRegFormat[CRC_VREG_TEXCOORD0];
		if (UVOff >= 0 && UVFmt >= CRC_VREGFMT_V1_F32 && UVFmt <= CRC_VREGFMT_V4_F32)
		{
			const float* p = (const float*)((const uint8*)pMem + UVOff);
			DbgU0 = p[0];
			if (CRC_VertexFormat::GetRegisterComponents(UVFmt) >= 2) DbgV0 = p[1];
		}
	}

	// --- Indices -----------------------------------------------------
	uint16* piIdx = NULL;
	int nIdx = 0;
	bool bIdxOk = true;

	if (VBB.m_piPrim)
	{
		if (VBB.m_PrimType == CRC_RIP_TRIANGLES)
		{
			// Raw index pool, no per-primitive header words (see
			// CBSP2_SLCIBContainer::Get, WBSP2Misc.cpp).
			nIdx = (int)VBB.m_nPrim * 3;
			if (nIdx > 0)
			{
				piIdx = (uint16*)malloc(sizeof(uint16) * (size_t)nIdx);
				if (piIdx)
					memcpy(piIdx, VBB.m_piPrim, sizeof(uint16) * (size_t)nIdx);
				else
					bIdxOk = false;
			}
		}
		else if (VBB.m_PrimType == CRC_RIP_STREAM)
		{
			// Nested primitive stream (tristrip/trifan/triangle mix) --
			// flatten to one triangle list, chunked exactly like
			// CContext_Geometry::Build (PS3) does.
			CRCPrimStreamIterator CountIter(VBB.m_piPrim, VBB.m_nPrim);
			int nMaxIdx = CRC_Core::Geometry_BuildTriangleListFromPrimitivesCount(CountIter);
			if (nMaxIdx > 0)
			{
				piIdx = (uint16*)malloc(sizeof(uint16) * (size_t)nMaxIdx);
				if (piIdx)
				{
					CRCPrimStreamIterator Iter(VBB.m_piPrim, VBB.m_nPrim);
					int iDst = 0;
					uint16 lChunk[1024 * 3];
					while (Iter.IsValid())
					{
						int nChunk = 1024 * 3;
						bool bDone = CRC_Core::Geometry_BuildTriangleListFromPrimitives(Iter, lChunk, nChunk);
						if (nChunk > 0 && iDst + nChunk <= nMaxIdx)
						{
							memcpy(piIdx + iDst, lChunk, sizeof(uint16) * (size_t)nChunk);
							iDst += nChunk;
						}
						if (bDone) break;
					}
					nIdx = iDst;
				}
				else
					bIdxOk = false;
			}
		}
		else
		{
			// CRC_RIP_WIRES or anything else: not a triangle draw we
			// support in the cache path (wireframe debug chains etc).
			if (GLES3Geom_DbgEnabled())
				fprintf(stderr, "[GLES3-GEOM] VBID=%d skip (unsupported m_PrimType=%d)\n",
					_VBID, (int)VBB.m_PrimType);
			if (pMem) free(pMem);
			E.m_bSkip = true;
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}
	}

	if (!bIdxOk)
	{
		if (pMem) free(pMem);
		if (piIdx) free(piIdx);
		m_pVBCtx->VB_Release(_VBID);
		return false;
	}

	if (Stride <= 0 && nIdx <= 0)
	{
		// Genuinely empty draw (no vertex registers, no indices) -- not
		// a permanent skip, just nothing to do yet.
		if (pMem) free(pMem);
		if (piIdx) free(piIdx);
		m_pVBCtx->VB_Release(_VBID);
		return false;
	}

	// --- Upload --------------------------------------------------------
	GLuint VBO = 0, IBO = 0;
	if (Stride > 0 && VBB.m_nV > 0)
	{
		glGenBuffers(1, &VBO);
		if (!VBO)
		{
			// No current GL context on this thread (e.g. a loader/
			// precache thread) -- retry later from the draw path, which
			// only ever runs on the GL thread. Not a permanent skip.
			if (pMem) free(pMem);
			if (piIdx) free(piIdx);
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)Stride * VBB.m_nV, pMem, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	if (nIdx > 0)
	{
		glGenBuffers(1, &IBO);
		if (!IBO)
		{
			if (VBO) glDeleteBuffers(1, &VBO);
			if (pMem) free(pMem);
			if (piIdx) free(piIdx);
			m_pVBCtx->VB_Release(_VBID);
			return false;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16) * (GLsizeiptr)nIdx, piIdx, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	if (pMem) free(pMem);
	if (piIdx) free(piIdx);
	m_pVBCtx->VB_Release(_VBID);

	E.m_VBO = VBO;
	E.m_IBO = IBO;
	E.m_nV = (Stride > 0) ? VBB.m_nV : 0;
	E.m_Stride = Stride;
	E.m_nIdx = nIdx;
	for (int i = 0; i < CRC_MAXVERTEXREG; ++i)
	{
		E.m_lRegOffset[i] = lRegOffset[i];
		E.m_lRegFormat[i] = lRegFormat[i];
	}
	E.m_bValid = true;
	if (CRC_VBIDInfo* pInfo = FreshInfo(_VBID))
		pInfo->m_Fresh |= 1;

	++m_nBuilt;
	m_nBytesV += (long long)Stride * E.m_nV;
	m_nBytesI += (long long)sizeof(uint16) * nIdx;

	if (GLES3Geom_DbgEnabled())
	{
		// Which texcoord sets this VBID actually carries, plus a sample of
		// the first vertex's UV0/position. If a world cluster comes out
		// with no UV register at all (or with all-zero UVs), the diffuse
		// texture samples one texel and the surface renders flat/black --
		// this line distinguishes "UV data missing at the source" from
		// "UV lost somewhere in our attribute plumbing".
		char UVSets[64]; UVSets[0] = 0;
		{
			int n = 0;
			for (int t = 0; t < CRC_MAXTEXCOORDS && n < 60; ++t)
				if (lRegOffset[CRC_VREG_TEXCOORD0 + t] >= 0)
					n += snprintf(UVSets + n, sizeof(UVSets) - n, "%d,", t);
		}
		fprintf(stderr, "[GLES3-GEOM] VBID=%d nV=%d stride=%d nIdx=%d uvsets=[%s] v0uv=(%.4f,%.4f)\n",
			_VBID, E.m_nV, Stride, nIdx, UVSets, DbgU0, DbgV0);
	}

	return true;
}

#endif // PLATFORM_LINUX
