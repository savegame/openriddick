#include "PCH.h"

#include "../MSystem.h"
#include "MSound.h"

// -------------------------------------------------------------------
//  CSC_SFXDesc
// -------------------------------------------------------------------
int16 CSC_SFXDesc::CNormalParams::GetRandomSound()
{
	if(m_lWaves.Len() > 1)
	{
		int16 Sound = Min((int32)(m_lWaves.Len()-1), (int32)(Random*(fp32)(m_lWaves.Len()))); 
		
		if(Sound == m_LastSound)
		{
			Sound++;
			Sound %= m_lWaves.Len();
		}

		m_LastSound = Sound;
		return Sound;
	}

	return 0;
}

//
void CSC_SFXDesc::CMaterialParams::CMaterialWaveHolder::AddWave(int16 _WaveID)
{
	// Increase size and add wave
	int32 Len = m_lWaves.Len();
	m_lWaves.SetLen(Len+1);
	m_lWaves[Len] = _WaveID;
}

//
//
//

//
//
//
CSC_SFXDesc::CMaterialParams::CMaterialWaveHolder *CSC_SFXDesc::CMaterialParams::GetMaterial(uint8 _ID, int &_HashPlace)
{
	int32 HashIndex = _ID%m_HolderHashSize;
	
	// Search the holder for the material
	for(int32 i = 0; i < m_alMaterialWaveHolders[HashIndex].Len(); i++)
		if(m_alMaterialWaveHolders[HashIndex][i]->m_iMaterial == _ID)
		{
			_HashPlace = i;
			return m_alMaterialWaveHolders[HashIndex][i];
		}

	return NULL;
}

//
//
//
CSC_SFXDesc::CMaterialParams::CMaterialWaveHolder *CSC_SFXDesc::CMaterialParams::CreateMaterial(uint8 _ID)
{
	int32 HashIndex = _ID%m_HolderHashSize;

	// Create material
	CMaterialWaveHolder *pMaterialWaveHolder = DNew(CMaterialWaveHolder) CMaterialWaveHolder;

	// Add to the list
	int32 Len = m_alMaterialWaveHolders[HashIndex].Len();
	m_alMaterialWaveHolders[HashIndex].SetLen(Len+1);
	m_alMaterialWaveHolders[HashIndex][Len] = pMaterialWaveHolder;

	// Set Material
	pMaterialWaveHolder->m_iMaterial = _ID;

	// Return the ew object
	return pMaterialWaveHolder;
}

//
//
//
CSC_SFXDesc::CMaterialParams::CMaterialWaveHolder *CSC_SFXDesc::CMaterialParams::GetOrCreateMaterial(uint8 _ID)
{
	if(!_ID)
		return NULL;

	int _HashPlace;
	CMaterialWaveHolder *pMaterialWaveHolder = GetMaterial(_ID, _HashPlace);
	
	if(!pMaterialWaveHolder)
		return CreateMaterial(_ID);

	return pMaterialWaveHolder;
}

//
//
//
void CSC_SFXDesc::CMaterialParams::AddWave(uint8 _MaterialID, int16 _WaveID)
{
	if(!_MaterialID || _WaveID < 0)
		return;

	GetOrCreateMaterial(_MaterialID)->AddWave(_WaveID);
}

//
// Just sets the mode to normal
//
CSC_SFXDesc::CSC_SFXDesc()
{
	// Init Mode
	m_Datas.m_Mode = ENONE;
	m_Datas.m_Category = 0;
	m_pParams = NULL;
	m_pWaveContainer = NULL;
	
}

//
// 
//
//
//
//
void CSC_SFXDesc::SetWaveContatiner(class CWaveContainer_Plain *_pWaveContainer)
{
	MAUTOSTRIP( CSC_SFXDesc_SetWaveContatiner, MAUTOSTRIP_VOID);
	m_pWaveContainer = _pWaveContainer;
}

//
//
//
int16 CSC_SFXDesc::GetWaveId(int16 _iLocal)
{
	MAUTOSTRIP( CSC_SFXDesc_GetWaveId, 0);

	if(GetMode() == ENORMAL)
	{
		if(GetNormalParams()->m_lWaves.Len() <= _iLocal)
			return -1;
		return m_pWaveContainer->GetWaveID(GetNormalParams()->m_lWaves[_iLocal]);
	}
	if(GetMode() == EMATERIAL)
	{
		uint8 HashID = (_iLocal>>6)&0x3;
		int8 Index = (_iLocal)&0x3F;
		int8 LocalID = _iLocal>>8;

		if(HashID >= CMaterialParams::m_HolderHashSize)
			return -1;

		if(Index >= GetMaterialParams()->m_alMaterialWaveHolders[HashID].Len())
			return -1;

		CMaterialParams::CMaterialWaveHolder *pHolder = GetMaterialParams()->m_alMaterialWaveHolders[HashID][Index];
		
		if(!pHolder)
			return -1;

		if(pHolder->m_lWaves.Len() <= LocalID)
			return -1;

		return m_pWaveContainer->GetWaveID(pHolder->m_lWaves[LocalID]);
	}

	return -1;
}

//
//
//
int16 CSC_SFXDesc::GetContainerWaveId(int16 _iLocal)
{
	MAUTOSTRIP(CSC_SFXDesc_GetContainerWaveId, 0);

	if(GetMode() == ENORMAL)
	{
		if(GetNormalParams()->m_lWaves.Len() <= _iLocal)
			return -1;
		return GetNormalParams()->m_lWaves[_iLocal];
	}
	if(GetMode() == EMATERIAL)
	{
		uint8 HashID = (_iLocal>>6)&0x3;
		int8 Index = (_iLocal)&0x3F;
		int8 LocalID = _iLocal>>8;

		if(HashID >= CMaterialParams::m_HolderHashSize)
			return -1;

		if(Index >= GetMaterialParams()->m_alMaterialWaveHolders[HashID].Len())
			return -1;

		CMaterialParams::CMaterialWaveHolder *pHolder = GetMaterialParams()->m_alMaterialWaveHolders[HashID][Index];
		
		if(!pHolder)
			return -1;

		if(pHolder->m_lWaves.Len() <= LocalID)
			return -1;

		return pHolder->m_lWaves[LocalID];
	}

	return -1;
}

//
//
//
int16 CSC_SFXDesc::GetNumWaves()
{
	MAUTOSTRIP( CSC_SFXDesc_GetNumWaves, 0);

	if(GetMode() == ENORMAL)
		return GetNormalParams()->m_lWaves.Len();
	if(GetMode() == EMATERIAL)
	{
		CMaterialParams *pParams = GetMaterialParams();
		int32 Count = 0;

		for(int32 h = 0; h < pParams->m_HolderHashSize; h++)
			for(int32 i = 0; i < pParams->m_alMaterialWaveHolders[h].Len(); i++)
				Count += pParams->m_alMaterialWaveHolders[h][i]->m_lWaves.Len();

		return Count;
	}

	return 0;
}

//
//
//
int32 CSC_SFXDesc::EnumWaves(int16 *_paWaves, int32 _MaxWaves)
{
	if(_MaxWaves <= 0)
		return 0;

	int32 Current = 0;

	if(GetMode() == ENORMAL)
	{
		CNormalParams *pParams = GetNormalParams();
		while(Current < pParams->m_lWaves.Len() && Current < _MaxWaves)
		{
			_paWaves[Current] = Current;
			++Current;
		}

		return Current;
	}

	if(GetMode() == EMATERIAL)
	{
		CMaterialParams *pParams = GetMaterialParams();
//		int32 Count = 0;

		for(int32 h = 0; h < CMaterialParams::m_HolderHashSize; h++)
			for(int32 i = 0; i < pParams->m_alMaterialWaveHolders[h].Len(); i++)
			{
				CMaterialParams::CMaterialWaveHolder *pHolder = pParams->m_alMaterialWaveHolders[h][i];

				for(int32 w = 0; w < pHolder->m_lWaves.Len(); w++)
				{
					// this should be a function
//					int16 iLocal = (pHolder->m_lWaves.Len() > 1) ? Min((int32)(pHolder->m_lWaves.Len()-1), (int32)(Random*(fp32)(pHolder->m_lWaves.Len()))) : 0;
					int16 Ret = (w<<8) | ((pHolder->m_iMaterial%4)<<6) | i;

					_paWaves[Current++] = Ret;

					if(Current == _MaxWaves)
						return Current;
				}
			}

		return Current;
	}

	return 0;
}

int16 CSC_SFXDesc::GetPlayWaveId(uint8 _iMaterial)
{
	if(GetMode() == ENORMAL)
	{
		CNormalParams *pParams = GetNormalParams();
		return pParams->GetRandomSound();
	}
	else if(GetMode() == EMATERIAL)
	{
		int HashPlace = 0; 
		CMaterialParams::CMaterialWaveHolder *pHolder = GetMaterialParams()->GetMaterial(_iMaterial, HashPlace);

		M_ASSERT(HashPlace >= 0 && HashPlace < 63, "Too many materials");
		
		// if we didn't found the holder for the material. try the base10 of the material id. it should contain the fallback sound
		if(!pHolder)
		{
			_iMaterial = _iMaterial / 10 * 10;
			pHolder = GetMaterialParams()->GetMaterial(_iMaterial, HashPlace);
		}

		if(!pHolder)
		{
			CMaterialParams *pParams = GetMaterialParams();
			if(!pParams)
				return -1;

			for(int i = 0; i < pParams->m_HolderHashSize; i++)
				if(pParams->m_alMaterialWaveHolders[i].Len())
					pHolder = pParams->m_alMaterialWaveHolders[i][0];
		}

		if(!pHolder)
			return -1;

		int16 iLocal = (pHolder->m_lWaves.Len() > 1) ? Min((int32)(pHolder->m_lWaves.Len()-1), (int32)(Random*(fp32)(pHolder->m_lWaves.Len()))) : 0;
		int16 Ret = (iLocal<<8) | ((_iMaterial%4)<<6);
		int16 Ret2 = HashPlace;
		Ret |= Ret2;
		return Ret;
	}
	return -1;
}

//
//
//
void CSC_SFXDesc::operator= (CSC_SFXDesc& _s)
{
	MAUTOSTRIP( CSC_SFXDesc_operator_equal, MAUTOSTRIP_VOID);

	// Set Mode
	SetMode(_s.GetMode());

	// Copy parameter options
	if(GetMode() == ENORMAL)
	{
		GetNormalParams()->m_lWaves.SetLen(_s.GetNormalParams()->m_lWaves.Len());

		for(int32 i = 0; i < GetNormalParams()->m_lWaves.Len(); i++)
			GetNormalParams()->m_lWaves[i] = _s.GetNormalParams()->m_lWaves[i];
	}
	else if (GetMode() == EMATERIAL)
	{
		// Get parameters
		CMaterialParams *pParams = _s.GetMaterialParams();
		CMaterialParams *pParamsDest = GetMaterialParams();

		// Copy materials
		for(int32 h = 0; h < CMaterialParams::m_HolderHashSize; h++)
		{
			pParamsDest->m_alMaterialWaveHolders[h].SetLen(pParams->m_alMaterialWaveHolders[h].Len());

			for(int32 m = 0; m < pParams->m_alMaterialWaveHolders[h].Len(); m++)
			{
				CMaterialParams::CMaterialWaveHolder *pHolder = pParams->m_alMaterialWaveHolders[h][m];
				CMaterialParams::CMaterialWaveHolder *pHolderDest = DNew(CMaterialParams::CMaterialWaveHolder) CMaterialParams::CMaterialWaveHolder;
				pParamsDest->m_alMaterialWaveHolders[h][m] = pHolderDest;

				pHolderDest->m_lWaves.SetLen(pHolder->m_lWaves.Len());
				pHolderDest->m_iMaterial = pHolder->m_iMaterial;

				// Write wave names for the material
				for(int32 w = 0; w < pHolder->m_lWaves.Len(); w++)
					pHolderDest->m_lWaves[w] = pHolder->m_lWaves[w];
			}
		}
	}
	else
		Error("operator=", "Can't copy SFXDesc in this mode");

	// Copy name
	#ifdef USE_HASHED_SFXDESC
		m_SoundIndex = _s.m_SoundIndex;
	#else
		m_SoundName = _s.m_SoundName;
	#endif

	SetWaveContatiner(m_pWaveContainer);

	// Copy data
	SetPriority(_s.GetPriority());
	SetPitch(_s.GetPitch());
	SetPitchRandAmp(_s.GetPitchRandAmp());
	SetVolume(_s.GetVolume());
	SetVolumeRandAmp(_s.GetVolumeRandAmp());
	SetAttnMaxDist(_s.GetAttnMaxDist());
	SetAttnMinDist(_s.GetAttnMinDist());
	m_Datas.m_Category = _s.m_Datas.m_Category; // direct copy, SetCategory() truncates to uint8 (PC scripts use >255)
	
}

//
//
//

//
// Load a descriptor from a file
//
void CSC_SFXDesc::Read(CCFile* _pFile, class CWaveContainer_Plain *_pWaveContainer)
{
	MAUTOSTRIP( CSC_SFXDesc_Read, MAUTOSTRIP_VOID);
	MSCOPE(CSC_SFXDesc::Read, IGNORE);

	SetWaveContatiner(_pWaveContainer);

	uint32 Ver = 0x0000;
	_pFile->ReadLE(Ver);

	if (Ver > CSC_SFXDESC_VERSION)
	{
		Error("CSC_SFXDesc", CStrF("Unsupported SFX-Description version (%.4x)", Ver));
	}
	else if (Ver >= 0x0103)
	{
		// Read Sound Name
		CStr TempStr;
		TempStr.Read(_pFile);

		#ifdef USE_HASHED_SFXDESC
			m_SoundIndex = StringToHash(TempStr);
		#else
			m_SoundName = TempStr;
		#endif
		
		// Read mode
		uint8 CurrentMode;
		_pFile->ReadLE(CurrentMode);

		if(CurrentMode != ENORMAL && CurrentMode != EMATERIAL)
			CurrentMode = CurrentMode;

		SetMode(CurrentMode);

		if(CurrentMode == ENORMAL)
		{
			// Read wave names
			int16 NumWaves;
			_pFile->ReadLE(NumWaves);
			GetNormalParams()->m_lWaves.SetLen(NumWaves);
			
			int32 Count = 0;
			for(int32 i = 0; i < NumWaves; i++)
			{
				CStr TempStr;
				TempStr.Read(_pFile);
				#ifdef USE_HASHED_WAVENAME
					int32 iLocalWave = _pWaveContainer->GetIndex(StringToHash(TempStr));
				#else
					int32 iLocalWave = _pWaveContainer->GetIndex(TempStr);
				#endif

				int32 iTemp = (iLocalWave >= 0) ? _pWaveContainer->GetWaveID(iLocalWave) : -1;
				
				if(iTemp < 0)
					ConOutL(CStr("§cf80WARNING: Sound references undefined waveform ."));
				else
					GetNormalParams()->m_lWaves[Count++] = iLocalWave;
			}

			// Optimize List size
			GetNormalParams()->m_lWaves.SetLen(Count);
		}
		else if(CurrentMode == EMATERIAL)
		{
			// Get parameters
			CMaterialParams *pParams = GetMaterialParams();

			// Calculate the total number of materials
			int16 NumMaterials;
			_pFile->ReadLE(NumMaterials);

			// Read materials
			for(int32 m = 0; m < NumMaterials; m++)
			{
				uint8 MaterialID;
				int16 NumWaves;
				_pFile->ReadLE(MaterialID);
				_pFile->ReadLE(NumWaves);

				CMaterialParams::CMaterialWaveHolder *pHolder = pParams->GetOrCreateMaterial(MaterialID);
				for(int32 w = 0; w < NumWaves; w++)
				{
					CStr TempStr;
					TempStr.Read(_pFile);

					#ifdef USE_HASHED_WAVENAME
						int32 iLocalWave = _pWaveContainer->GetIndex(StringToHash(TempStr));
					#else
						int32 iLocalWave = _pWaveContainer->GetIndex(TempStr);
					#endif

					int32 iTemp = (iLocalWave >= 0) ? _pWaveContainer->GetWaveID(iLocalWave) : -1;
					
					if(iTemp < 0)
						ConOutL(CStr("§cf80WARNING: Sound references undefined waveform."));
					else
						pHolder->AddWave(iLocalWave);
				}
			}
		}
		else
			Error("(CSC_SFXDesc::Read)", "Strange mode requested to sfxdesc, bailing.");



		if (Ver == CSC_SFXDESC_VERSION_VER3)
		{
			// Read Data
			uint16 Temp;
			_pFile->ReadLE(Temp);
			m_Datas.m_AttnMaxDist = Temp;
			_pFile->ReadLE(Temp);
			m_Datas.m_AttnMinDist = Temp;
			uint8 Temp8;
			_pFile->ReadLE(Temp8);
			m_Datas.m_Priority = Temp8;
			_pFile->ReadLE(m_Datas.m_Pitch);
			_pFile->ReadLE(m_Datas.m_PitchRandAmp);
			_pFile->ReadLE(m_Datas.m_Volume);
			_pFile->ReadLE(m_Datas.m_VolumeRandAmp);
		}
		else
		{
			uint32 Data1;
			_pFile->ReadLE(Data1);
			m_Datas.m_AttnMinDist = (Data1 >> 2) & ((1 << 12) - 1);
			m_Datas.m_AttnMaxDist = (Data1 >> 14) & ((1 << 12) - 1);
			m_Datas.m_Priority = (Data1 >> 26) & ((1 << 6) - 1);

			_pFile->ReadLE(m_Datas.m_Pitch);
			_pFile->ReadLE(m_Datas.m_PitchRandAmp);
			_pFile->ReadLE(m_Datas.m_Volume);
			_pFile->ReadLE(m_Datas.m_VolumeRandAmp);
		}

		if (Ver >= 0x0105)
		{
			if (Ver >= 0x0106)
				_pFile->ReadLE(m_Datas.m_Category);
			else
			{
				uint8 Category;
				_pFile->ReadLE(Category);
				m_Datas.m_Category = Category;				
			}
		}

	}
	else
	{
		switch(Ver)
		{
		case CSC_SFXDESC_VERSION_VER2:  // Old Version
			{
				// Set mode
				SetMode(ENORMAL);

				// Read Sound Name
				CStr TempStr;
				TempStr.Read(_pFile);

				#ifdef USE_HASHED_SFXDESC
					m_SoundIndex = StringToHash(TempStr);
				#else
					m_SoundName = TempStr;
				#endif

				// Read wave names
				int16 NumWaves;
				_pFile->ReadLE(NumWaves);
				GetNormalParams()->m_lWaves.SetLen(NumWaves);
				
				int32 Count = 0;
				for(int32 i = 0; i < NumWaves; i++)
				{
					int32 m_iLocalWave = _pWaveContainer->GetIndex(TempStr);
					int32 iTemp = (m_iLocalWave >= 0) ? _pWaveContainer->GetWaveID(m_iLocalWave) : -1;
					
					if(iTemp < 0)
						ConOutL(CStrF("§cf80WARNING: Sound references undefined waveform '%s'.", TempStr.Str()));
					else
						GetNormalParams()->m_lWaves[Count++] = m_iLocalWave;
				}

				// Optimize List size
				GetNormalParams()->m_lWaves.SetLen(Count);

				// Read Data
				int16 Crap16;
				int8 Crap8;
				uint16 Temp;
				_pFile->ReadLE(Temp);
				m_Datas.m_AttnMaxDist = Temp;

				_pFile->ReadLE(Temp);
				m_Datas.m_AttnMinDist = Temp;
				
				_pFile->ReadLE(Crap16);
				_pFile->ReadLE(Crap16);

				_pFile->ReadLE(Crap8);
				m_Datas.m_Priority = Crap8;
				_pFile->ReadLE(m_Datas.m_Pitch);
				_pFile->ReadLE(m_Datas.m_PitchRandAmp);
				_pFile->ReadLE(m_Datas.m_Volume);
				_pFile->ReadLE(m_Datas.m_VolumeRandAmp);

				_pFile->ReadLE(Crap8);
				_pFile->ReadLE(Crap8);
			}
			break;
		default:
			Error("CSC_SFXDesc", CStrF("Unsupported SFX-Description version (%.4x)", Ver));
		}
	}
}

//
// Writes the descriptor to a file
//
void CSC_SFXDesc::Write(CCFile* _pFile, class CWaveContainer_Plain *_pWaveContainer)
{
#ifdef USE_HASHED_SFXDESC
	Error("CSC_SFXDesc", "Can't save sound description that has a diffrent mode than normal or material");
	return;
#else

	MAUTOSTRIP( CSC_SFXDesc_Write, MAUTOSTRIP_VOID);

	if(GetMode() != ENORMAL && GetMode() != EMATERIAL)
	{
		Error("CSC_SFXDesc", "Can't save sound description that has a diffrent mode than normal or material");
		return;
	}

	// Write Version
	uint32 Ver = CSC_SFXDESC_VERSION;
	_pFile->WriteLE(Ver);

	// Write name of description
	m_SoundName.Write(_pFile);

	// Write mode
	uint8 CurrentMode = GetMode();
	_pFile->WriteLE(CurrentMode);

	if(CurrentMode == ENORMAL)
	{
		// Normal mode
		int16 NumWaves = (int16)GetNormalParams()->m_lWaves.Len();
		_pFile->WriteLE(NumWaves);
		
		for(int i = 0; i < GetNormalParams()->m_lWaves.Len(); i++)
		{
			/*
			#ifdef USE_HASHED_WAVENAME
				int16 id = GetNormalParams()->m_lWaves[i];
				if(id == -1)
					ConOutL(CStrF("§cf80WARNING: Waveform with no good id.", id));

				uint32 nameid = _pWaveContainer->GetNameID(id);
				_pFile->WriteLE(nameid);
				
			#else
			*/
			CStr TempStr;
			TempStr = _pWaveContainer->GetName(GetNormalParams()->m_lWaves[i]);
			TempStr.Write(_pFile);
			//#endif
		}
	}
	else if(CurrentMode == EMATERIAL)
	{
		// Get parameters
		CMaterialParams *pParams = GetMaterialParams();

		// Calculate the total number of materials
		int16 NumMaterials = 0;
		for(int32 i = 0; i < CMaterialParams::m_HolderHashSize; i++)
			NumMaterials += pParams->m_alMaterialWaveHolders[i].Len();

		_pFile->WriteLE(NumMaterials);

		// Write materials
		for(int32 h = 0; h < CMaterialParams::m_HolderHashSize; h++)
		{
			for(int32 m = 0; m < pParams->m_alMaterialWaveHolders[h].Len(); m++)
			{
				CMaterialParams::CMaterialWaveHolder *pHolder = pParams->m_alMaterialWaveHolders[h][m];

				// Write material ID and num waves for that material
				_pFile->WriteLE((uint8)pHolder->m_iMaterial);
				_pFile->WriteLE((int16)pHolder->m_lWaves.Len());
				
				// Write wave names for the material
				for(int32 w = 0; w < pHolder->m_lWaves.Len(); w++)
				{
					/*
					#ifdef USE_HASHED_WAVENAME
						//int16 id = m_pWaveContainer->GetWaveID(GetNormalParams()->m_lWaves[i]);
						uint32 nameid = _pWaveContainer->GetNameID(pHolder->m_lWaves[w]);
						_pFile->WriteLE(nameid);

						//_pFile->WriteLE(_pWaveContainer->GetNameID(m_pWaveContainer->GetWaveID(GetNormalParams()->m_lWaves[i])));
					#else
					*/
					CStr TempStr;
					TempStr = _pWaveContainer->GetName(pHolder->m_lWaves[w]);
					TempStr.Write(_pFile);
                    //#endif
				}
			}
		}
	}
	uint32 Data1 = m_Datas.m_Mode | m_Datas.m_AttnMinDist<<2 | m_Datas.m_AttnMaxDist<<14 | m_Datas.m_Priority << 26;
	_pFile->WriteLE(Data1);
	_pFile->WriteLE(m_Datas.m_Pitch);
	_pFile->WriteLE(m_Datas.m_PitchRandAmp);
	_pFile->WriteLE(m_Datas.m_Volume);
	_pFile->WriteLE(m_Datas.m_VolumeRandAmp);
	_pFile->WriteLE(m_Datas.m_Category);
#endif
}

// -------------------------------------------------------------------
//  MSound_LoadSFXDescScript
//
//  Loads PC text SFX descriptors (Content/SfxDesc/*.xsfxc) and adds them
//  to the scanned wave containers. The PC wave containers have no binary
//  SFXDESC sections, the Win32 build loaded these scripts instead
//  (CWaveContext::ReadSfxDesc).
//
//  The .xsfxc syntax is the registry script syntax (*KEY value, nested
//  { } scopes, // comments), but CRegistry can not be used for it: it
//  folds ';'-separated values into animated keyframes and we need the
//  raw wave lists, so there's a minimal tokenizer below instead.
// -------------------------------------------------------------------
#ifndef USE_HASHED_SFXDESC
namespace NSFXDescScript
{

class CNode
{
public:
	CStr m_Name;	// Key, upper case
	CStr m_Value;
	TArray<CNode *> m_lpChildren;

	~CNode()
	{
		for(int i = 0; i < m_lpChildren.Len(); i++)
			delete m_lpChildren[i];
	}

	const CNode *FindChild(const char *_pName) const
	{
		for(int i = 0; i < m_lpChildren.Len(); i++)
			if (m_lpChildren[i]->m_Name.CompareNoCase(_pName) == 0)
				return m_lpChildren[i];
		return NULL;
	}

	bool GetValue(const char *_pName, CStr &_Value) const
	{
		const CNode *pChild = FindChild(_pName);
		if (!pChild)
			return false;
		_Value = pChild->m_Value;
		return true;
	}
};

static void SkipWhiteSpace(const char *&_pStr)
{
	for(;;)
	{
		while (*_pStr && (uint8)*_pStr <= 32)
			++_pStr;
		if (_pStr[0] == '/' && _pStr[1] == '/')
		{
			while (*_pStr && *_pStr != '\n')
				++_pStr;
		}
		else if (_pStr[0] == '/' && _pStr[1] == '*')
		{
			_pStr += 2;
			while (*_pStr && !(_pStr[0] == '*' && _pStr[1] == '/'))
				++_pStr;
			if (*_pStr)
				_pStr += 2;
		}
		else
			return;
	}
}

static bint IsKeyChar(char _Chr)
{
	return (_Chr >= 'a' && _Chr <= 'z') || (_Chr >= 'A' && _Chr <= 'Z') ||
		(_Chr >= '0' && _Chr <= '9') || _Chr == '_';
}

// Parses *KEY value / *KEY { children } until the matching '}' or end of data
static void ParseChildren(const char *&_pStr, TArray<CNode *> &_lChildren)
{
	for(;;)
	{
		SkipWhiteSpace(_pStr);
		if (!*_pStr)
			return;
		if (*_pStr == '}')
		{
			++_pStr;
			return;
		}
		if (*_pStr != '*')
		{
			// Unexpected token, skip it to avoid stalling on broken data
			++_pStr;
			continue;
		}
		++_pStr;

		const char *pKey = _pStr;
		while (IsKeyChar(*_pStr))
			++_pStr;
		if (_pStr == pKey)
			continue;

		CNode *pNode = DNew(CNode) CNode;
		if (!pNode) MemError_static("NSFXDescScript::ParseChildren");
		_lChildren.Add(pNode);
		pNode->m_Name.Capture(pKey, _pStr - pKey);
		pNode->m_Name.MakeUpperCase();

		SkipWhiteSpace(_pStr);

		// Optional value
		if (*_pStr == '"')
		{
			++_pStr;
			const char *pValue = _pStr;
			while (*_pStr && *_pStr != '"')
				++_pStr;
			pNode->m_Value.Capture(pValue, _pStr - pValue);
			if (*_pStr)
				++_pStr;
			SkipWhiteSpace(_pStr);
		}
		else if (*_pStr && *_pStr != '{' && *_pStr != '}' && *_pStr != '*')
		{
			const char *pValue = _pStr;
			while (*_pStr && (uint8)*_pStr > 32 && *_pStr != '{' && *_pStr != '}' && *_pStr != '*')
				++_pStr;
			pNode->m_Value.Capture(pValue, _pStr - pValue);
			SkipWhiteSpace(_pStr);
		}

		// Optional child scope
		if (*_pStr == '{')
		{
			++_pStr;
			ParseChildren(_pStr, pNode->m_lpChildren);
		}
	}
}

static char CharToLower(char _Chr)
{
	return (_Chr >= 'A' && _Chr <= 'Z') ? _Chr + 32 : _Chr;
}

// Case-insensitive wildcard match, '*' matches any sequence (including empty)
static bool WildcardMatch(const char *_pPattern, const char *_pStr)
{
	while (*_pPattern)
	{
		if (*_pPattern == '*')
		{
			while (*_pPattern == '*')
				++_pPattern;
			if (!*_pPattern)
				return true;
			for(;;)
			{
				if (WildcardMatch(_pPattern, _pStr))
					return true;
				if (!*_pStr)
					return false;
				++_pStr;
			}
		}
		if (CharToLower(*_pPattern) != CharToLower(*_pStr))
			return false;
		++_pPattern;
		++_pStr;
	}
	return *_pStr == 0;
}

// Splits a ';'-separated wave list, trims whitespace
static void SplitWaveList(const char *_pValue, TArray<CStr> &_lNames)
{
	const char *pStr = _pValue;
	while (*pStr)
	{
		const char *pEnd = pStr;
		while (*pEnd && *pEnd != ';')
			++pEnd;
		int Len = int(pEnd - pStr);
		while (Len && (uint8)*pStr <= 32)
		{
			++pStr;
			--Len;
		}
		while (Len && (uint8)pStr[Len-1] <= 32)
			--Len;
		if (Len)
		{
			int iName = _lNames.Len();
			_lNames.SetLen(iName + 1);
			_lNames[iName].Capture(pStr, Len);
		}
		if (!*pEnd)
			break;
		pStr = pEnd + 1;
	}
}

static void SetAttributes(CSC_SFXDesc &_Desc, const CNode &_Node, int _FileCategory)
{
	CStr Value;

	// Volume and Pitch are multiplicative when the voice is created, so the
	// identity defaults must be set explicitly (the constructor leaves them
	// cleared). Priority and distances are additive/override-if-set, the
	// cleared defaults are correct there.
	_Desc.SetVolume(1.0);
	_Desc.SetPitch(1.0);

	if (_Node.GetValue("VOLUME", Value))
		_Desc.SetVolume((fp32)NStr::StrToFloat(Value.Str(), 1.0f));
	if (_Node.GetValue("VOLUMERANDAMP", Value))
		_Desc.SetVolumeRandAmp((fp32)NStr::StrToFloat(Value.Str(), 0.0f));
	if (_Node.GetValue("PITCH", Value))
		_Desc.SetPitch((fp32)NStr::StrToFloat(Value.Str(), 1.0f));
	if (_Node.GetValue("PITCHRANDAMP", Value))
		_Desc.SetPitchRandAmp((fp32)NStr::StrToFloat(Value.Str(), 0.0f));
	if (_Node.GetValue("PRIORITY", Value))
		_Desc.SetPriority(NStr::StrToInt(Value.Str(), 0));
	if (_Node.GetValue("MINDIST", Value))
		_Desc.SetAttnMinDist((fp32)NStr::StrToFloat(Value.Str(), 0.0f));
	if (_Node.GetValue("MAXDIST", Value))
		_Desc.SetAttnMaxDist((fp32)NStr::StrToFloat(Value.Str(), 0.0f));

	int Category = _FileCategory;
	if (_Node.GetValue("CATEGORY", Value))
		Category = NStr::StrToInt(Value.Str(), _FileCategory);
	// SetCategory() only takes an uint8 but the PC scripts use categories
	// above 255 (m_Category itself is uint16)
	_Desc.m_Datas.m_Category = Category;
}

static void StoreDesc(CWaveContainer_Plain *_pWC, CSC_SFXDesc &_Desc)
{
	_Desc.SetWaveContatiner(_pWC);
	_pWC->AddSFXDesc(_Desc);
	// AddSFXDesc copies the descriptor with operator= which doesn't copy
	// the container pointer, set it on the stored copy as well
	_pWC->GetSFXDesc(_pWC->GetSFXCount() - 1)->SetWaveContatiner(_pWC);
}

static int BuildDescs(const CNode &_Node, int _FileCategory, TArray<spCWaveContainer_Plain> &_lspWC)
{
	CStr Source;
	if (!_Node.GetValue("SOURCE", Source))
		return 0;

	CStr Name;
	bool bHasName = _Node.GetValue("NAME", Name) && Name.Len() > 0;

	TArray<CStr> lPatterns;
	SplitWaveList(Source.Str(), lPatterns);
	if (!lPatterns.Len())
		return 0;

	int nDescs = 0;
	for(int iWC = 0; iWC < _lspWC.Len(); iWC++)
	{
		CWaveContainer_Plain *pWC = _lspWC[iWC];
		if (!pWC)
			continue;

		// Collect the waves of this container matching any of the patterns
		TArray<int16> liWaves;
		int nWaves = pWC->GetWaveCount();
		for(int iWave = 0; iWave < nWaves; iWave++)
		{
			// GetName() is allocation-free; GetWaveName() returns the name
			// hash as a hex string in this build (and allocates per call)
			const char *pWaveName = pWC->GetName(iWave);
			for(int iPattern = 0; iPattern < lPatterns.Len(); iPattern++)
			{
				if (WildcardMatch(lPatterns[iPattern].Str(), pWaveName))
				{
					liWaves.Add((int16)iWave);
					break;
				}
			}
		}

		if (!liWaves.Len())
			continue;

		if (bHasName)
		{
			// One descriptor for all matching waves (random wave at playback)
			CSC_SFXDesc Desc;
			Desc.SetMode(CSC_SFXDesc::ENORMAL);
			Desc.m_SoundName = Name;
			SetAttributes(Desc, _Node, _FileCategory);
			TThinArray<int16> &lWaves = Desc.GetNormalParams()->m_lWaves;
			lWaves.SetLen(liWaves.Len());
			for(int i = 0; i < liWaves.Len(); i++)
				lWaves[i] = liWaves[i];
			StoreDesc(pWC, Desc);
			nDescs++;
		}
		else
		{
			// One descriptor per matching wave, named after the wave
			for(int i = 0; i < liWaves.Len(); i++)
			{
				CSC_SFXDesc Desc;
				Desc.SetMode(CSC_SFXDesc::ENORMAL);
				Desc.m_SoundName = pWC->GetName(liWaves[i]);
				SetAttributes(Desc, _Node, _FileCategory);
				Desc.GetNormalParams()->m_lWaves.SetLen(1);
				Desc.GetNormalParams()->m_lWaves[0] = liWaves[i];
				StoreDesc(pWC, Desc);
				nDescs++;
			}
		}
	}
	return nDescs;
}

static int BuildMaterialDesc(const CNode &_Node, int _FileCategory, TArray<spCWaveContainer_Plain> &_lspWC)
{
	CStr Name;
	if (!_Node.GetValue("NAME", Name) || !Name.Len())
		return 0;
	const CNode *pMaterials = _Node.FindChild("MATERIALS");
	if (!pMaterials)
		return 0;

	int nDescs = 0;
	for(int iWC = 0; iWC < _lspWC.Len(); iWC++)
	{
		CWaveContainer_Plain *pWC = _lspWC[iWC];
		if (!pWC)
			continue;

		CSC_SFXDesc Desc;
		Desc.SetMode(CSC_SFXDesc::EMATERIAL);
		Desc.m_SoundName = Name;
		SetAttributes(Desc, _Node, _FileCategory);

		int nAdded = 0;
		for(int iMat = 0; iMat < pMaterials->m_lpChildren.Len(); iMat++)
		{
			const CNode *pMat = pMaterials->m_lpChildren[iMat];
			int MaterialID = NStr::StrToInt(pMat->m_Name.Str(), -1);
			if (MaterialID < 0 || MaterialID > 255)
				continue;

			TArray<CStr> lWaveNames;
			SplitWaveList(pMat->m_Value.Str(), lWaveNames);
			for(int iWave = 0; iWave < lWaveNames.Len(); iWave++)
			{
				int16 iLocal = pWC->GetLocalWaveID(lWaveNames[iWave].Str());
				if (iLocal >= 0)
				{
					Desc.GetMaterialParams()->AddWave((uint8)MaterialID, iLocal);
					nAdded++;
				}
			}
		}

		if (!nAdded)
			continue;

		StoreDesc(pWC, Desc);
		nDescs++;
	}
	return nDescs;
}

}; // namespace NSFXDescScript
#endif // USE_HASHED_SFXDESC

int MSound_LoadSFXDescScript(const CStr& _Filename, TArray<spCWaveContainer_Plain>& _lspWC)
{
	MAUTOSTRIP( MSound_LoadSFXDescScript, 0 );
#ifdef USE_HASHED_SFXDESC
	// Only the non-hashed build needs the text scripts (PC content)
	return 0;
#else
	CCFile File;
	File.Open(_Filename, CFILE_READ | CFILE_BINARY);
	int Size = File.Length();
	char *pData = DNew(char) char[Size + 1];
	if (!pData) MemError("MSound_LoadSFXDescScript");
	File.Read(pData, Size);
	File.Close();
	pData[Size] = 0;

	NSFXDescScript::CNode *pRoot = DNew(NSFXDescScript::CNode) NSFXDescScript::CNode;
	if (!pRoot) MemError("MSound_LoadSFXDescScript");
	const char *pParse = pData;
	NSFXDescScript::ParseChildren(pParse, pRoot->m_lpChildren);
	delete[] pData;

	int nDescs = 0;
	for(int i = 0; i < pRoot->m_lpChildren.Len(); i++)
	{
		const NSFXDescScript::CNode *pDescs = pRoot->m_lpChildren[i];
		if (pDescs->m_Name.CompareNoCase("SFXDESCS") != 0)
			continue;

		int FileCategory = 0;
		CStr Value;
		if (pDescs->GetValue("CATEGORY", Value))
			FileCategory = NStr::StrToInt(Value.Str(), 0);

		for(int iChild = 0; iChild < pDescs->m_lpChildren.Len(); iChild++)
		{
			const NSFXDescScript::CNode *pChild = pDescs->m_lpChildren[iChild];
			if (pChild->m_Name.CompareNoCase("DESC") == 0)
				nDescs += NSFXDescScript::BuildDescs(*pChild, FileCategory, _lspWC);
			else if (pChild->m_Name.CompareNoCase("MATERIALDESC") == 0)
				nDescs += NSFXDescScript::BuildMaterialDesc(*pChild, FileCategory, _lspWC);
		}
	}

	delete pRoot;

	LogFile(CStrF("%s: %d sfxdescs loaded.", _Filename.Str(), nDescs));
	return nDescs;
#endif
}
