/*
	Bring-up stubs for the SPU/VPU job entry points on Linux. The real CPU
	implementation (VPUWorkers.cpp) assumes 32-bit pointers in the
	PPU<->SPU job ABI and needs a dedicated 64-bit port; until then VPU
	jobs are no-ops.

	Deliberately does not include the VPU headers (they redefine the M_*
	float primitives for the SPU compile context); only the signatures
	matter here.
*/

class CVPU_ContextData;
class CVPU_JobDefData;
class CVPU_JobInfo;

int VPU_Main(CVPU_ContextData& _ContextData, const CVPU_JobDefData* _pJobDefData, bool _IsAsync)
{
	return 0;
}

unsigned int VPU_Worker(unsigned int _JobHash, CVPU_JobInfo& _JobInfo)
{
	return 0;
}

unsigned int VPU_Worker_NavGrid(unsigned int _JobHash, CVPU_JobInfo& _JobInfo)
{
	return 0;
}
