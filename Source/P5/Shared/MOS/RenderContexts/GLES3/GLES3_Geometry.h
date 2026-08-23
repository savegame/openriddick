// GPU-resident geometry cache for VBID-addressed vertex/index data
// (Phase 4 milestone M6). Mirrors PS3GCM's CContext_Geometry
// (RenderContexts/PS3GCM/MRenderPS3_Geometry.cpp): once a VBID has been
// built, its interleaved vertex data + index list live in a pair of
// GL_STATIC_DRAW buffers until the engine marks the VBID dirty again
// (CXR_VBContext::VB_MakeDirty clears bit 0 of CRC_VBIDInfo::m_Fresh --
// see XR/XRVBContext.cpp). This removes the per-frame malloc + scalar
// VRegFetch conversion + full re-upload that BuildVertsFromVBB /
// DrawUserVerts (MDisplaySDL2.cpp) otherwise do on every draw call, for
// the common case of static, unskinned geometry submitted by VBID
// (world BSP2 clusters, static props).
//
// The actual vertex-register -> interleaved-buffer conversion is
// delegated to the engine's own CRC_BuildVertexBuffer::ConvertToInterleaved
// (MSystem/Raster/MRender.cpp) -- this cache only decides the destination
// vertex layout and flattens the primitive stream into a uint16 triangle
// list, exactly like CContext_Geometry::Build does for PS3.

#pragma once

#ifdef PLATFORM_LINUX

#include <GLES3/gl3.h>
#include "../../MSystem/Raster/MRender_Classes.h"

class CXR_VBContext;
class CRC_VBIDInfo;
class CRC_Core;

// One cached VBID. Either half (vertex or index) may be empty depending
// on how the VBID is used: a self-contained draw (Render_VertexBuffer)
// has both; the shared index pool used by
// Render_VertexBuffer_IndexBufferTriangles (CBSP2_SLCIBContainer -- see
// WBSP2Misc.cpp) is index-only (m_nV == 0, m_VBO == 0) and the matching
// vertex VBID it pairs with is vertex-only w.r.t. its OWN primitive
// stream (may or may not carry one). DrawCachedVB() takes two entries
// (vertex-supplying + index-supplying) so both shapes work uniformly.
struct SGLES3GeomEntry
{
	GLuint m_VBO = 0;        // interleaved vertices, GL_STATIC_DRAW. 0 = none.
	GLuint m_IBO = 0;        // uint16 index data, GL_STATIC_DRAW. 0 = none.
	int    m_nV = 0;
	int    m_Stride = 0;
	int    m_nIdx = 0;
	int    m_lRegOffset[CRC_MAXVERTEXREG];  // byte offset inside vertex, -1 = absent
	uint8  m_lRegFormat[CRC_MAXVERTEXREG];  // destination CRC_VREGFMT_*, VOID if absent
	bool   m_bValid = false; // GPU buffers reflect the current source data
	bool   m_bSkip  = false; // permanently unsupported (bad prim type, or skinned while RIDDICK_SKINNING=0) -- never draw, never rebuild

	SGLES3GeomEntry()
	{
		for (int i = 0; i < CRC_MAXVERTEXREG; ++i)
		{
			m_lRegOffset[i] = -1;
			m_lRegFormat[i] = CRC_VREGFMT_VOID;
		}
	}
};

class CGLES3GeometryCache
{
public:
	CGLES3GeometryCache();
	~CGLES3GeometryCache();

	// _pRC is the owning render context: the CRC_VBIDInfo array lives in
	// CRC_Core (m_lVBIDInfo, sized from CXR_VBContext::GetIDCapacity())
	// and is reached through CRC_Core::VB_GetVBIDInfo(). We deliberately
	// re-query that pointer on every access instead of caching it --
	// TArray::GetBasePtr() moves if the array is ever resized, and Init()
	// may run before CRC_Core has sized it.
	void Init(CXR_VBContext* _pVBCtx, CRC_Core* _pRC);

	// Returns the ready-to-draw entry for _VBID, (re)building it first if
	// the engine's CRC_VBIDInfo::m_Fresh bit 0 is clear or it was never
	// built. Returns NULL if _VBID is out of range, the VBID currently
	// carries no data, the geometry is permanently unsupported (exotic
	// primitive type, or a skinned mesh while RIDDICK_SKINNING=0 -- see
	// GLES3Geom_SkinningEnabled in the .cpp -- m_bSkip either way), or GL buffer creation had
	// to be deferred (no current GL context on this thread; retried on
	// the next Ensure() call -- the draw path only calls this from the
	// GL thread, so it always succeeds there once data is available).
	const SGLES3GeomEntry* Ensure(int _VBID);

	// Drops the GL buffers for one VBID and clears the engine's fresh
	// bit for it so a later Ensure() rebuilds from scratch. Does not
	// clear m_bSkip -- a VBID whose geometry we can't handle stays
	// skipped until FlushUnused() recycles the slot (map change etc.).
	void Destroy(int _VBID);

	void DestroyAll();

	// CRC_Core::Geometry_PrecacheFlush(): drop cached entries for VBIDs
	// the engine no longer flags PRECACHE|ALLOCATED (see
	// XR/XRVBContext.h) -- mirrors PS3's CContext_Geometry flush at map
	// change. These slots may be reused for different geometry, so this
	// also clears m_bSkip (unlike the plain per-VBID Destroy()).
	void FlushUnused();

	// Diagnostics (folded into the [GL-DBG] 60-frame log in MDisplaySDL2.cpp).
	int       m_nBuilt;
	long long m_nBytesV;
	long long m_nBytesI;

private:
	bool Build(int _VBID);
	// Fresh-bit accessor: NULL when the RC has no VBID-info array yet or
	// _VBID is beyond its length (the engine's own VB_MakeDirty indexes
	// the same array, so staying inside it is what keeps us in sync).
	CRC_VBIDInfo* FreshInfo(int _VBID);
	// Grows m_lEntries to at least _VBID+1, resetting new slots to their
	// default (empty, non-skip) state. TArray::SetLen does not run
	// constructors on the newly-added elements (see the identical
	// pattern for m_lGLTex/m_lFBO in MDisplaySDL2.cpp), so this is done
	// by hand rather than relying on SGLES3GeomEntry's default members.
	void EnsureCapacity(int _VBID);
	static void ResetEntry(SGLES3GeomEntry& _E);

	CXR_VBContext*         m_pVBCtx;
	CRC_Core*              m_pRC;
	int                    m_VBIDCapacity;
	TArray<SGLES3GeomEntry> m_lEntries;

	CGLES3GeometryCache(const CGLES3GeometryCache&);
	CGLES3GeometryCache& operator=(const CGLES3GeometryCache&);
};

#endif // PLATFORM_LINUX
