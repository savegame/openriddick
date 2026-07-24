#include "PCH.h"

#include "../MSystem.h"
#include "MSound_Core.h"

/*************************************************************************************************\
|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
| CSCC_Codec
|__________________________________________________________________________________________________
\*************************************************************************************************/


MRTC_IMPLEMENT(CSCC_Codec, CReferenceCount);

CSCC_Codec::CSCC_Codec()
{
	m_pFile = NULL;
}

CSCC_Codec::~CSCC_Codec()
{
	
}

// Compressor interface
bool CSCC_Codec::CreateEncoderInternal(CSCC_CodecFormat *_pFormat, CCFile *_pFile)
{
	m_Format = *_pFormat;
	m_pFile = _pFile;
	
	return true;
}

// Decompressor interface
bool CSCC_Codec::CreateDecoderInternal(CCFile *_pFile)
{
	m_pFile = _pFile;
	
	return true;
}

int32 CSCC_Codec::GetDataSize()
{
	return m_Format.m_TotalSize;
}

bool CSCC_Codec::GetDataNonInterleaved(void **_pData, mint &_nBytes, bool _bLooping, bool &_bReadSomething)
{
	M_ASSERT(0, "The codec did not override this function");

	return true;
}

void * CSCC_Codec::GetCodecInfo()
{
//	M_ASSERT(0, "The codec did not override this function");

	return NULL;
}

/*************************************************************************************************\
|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
| CSCC_Codec_RAW
|__________________________________________________________________________________________________
\*************************************************************************************************/


CSCC_Codec_RAW::CSCC_Codec_RAW()
{	
	m_FileStart = 0;
}

CSCC_Codec_RAW::~CSCC_Codec_RAW()
{
	Close();	
}

bool CSCC_Codec_RAW::CreateEncoder(float _Quality)
{
	Error("CSCC_Codec_RAW::CreateEncoder", "This interface is not supported by this codec, you can only use CreateRAW");
	return false;
}

bool CSCC_Codec_RAW::AddData(void *&_pData, int &_nBytes)
{
	Error("CSCC_Codec_RAW::AddData", "This interface is not supported by this codec, you can only use CreateRAW");
	return false;
}

bool CSCC_Codec_RAW::CreateDecoder(int _Flags)
{
	Error("CSCC_Codec_RAW::CreateDecoder", "This interface is not supported by this codec, you can only use CreateRAW");
	return false;
}

bool CSCC_Codec_RAW::CreateRAW(CImage *_pFormat)
{
	m_FileStart = m_pFile->Pos();

	m_Format.m_Data.SetChannels(_pFormat->GetWidth());
	m_Format.m_Data.SetNumSamples(_pFormat->GetHeight());
	m_Format.m_Data.SetSampleRate(22050);
	m_Format.m_Data.SetSampleSize(_pFormat->GetPixelSize());

	return true;
}


bool CSCC_Codec_RAW::GetData(void *&_pData, mint &_nBytes, bool _bLoop, bool &_bReadSomething)
{
	_bReadSomething = true;
	while (_nBytes > 0)
	{
		int Size = m_Format.m_Data.GetNumSamples() * m_Format.m_Data.GetChannels() * m_Format.m_Data.GetSampleSize();
		int Pos = m_pFile->Pos();
		int ToRead = _nBytes;
		if (Pos + ToRead > (Size + m_FileStart))
			ToRead = (Size + m_FileStart) - Pos;

		if (!ToRead)
		{
			if (_bLoop)
			{
				SeekData(0);
				return true;
			}
			else
				return false;
		}
		else
		{
			int ReadBytes = -m_pFile->Pos();
			m_pFile->Read(_pData, ToRead);
			ReadBytes += m_pFile->Pos();

			*((uint8 **)&_pData) += ReadBytes;
			_nBytes -= ReadBytes;
		}
	}	

	return true;
}

mint CSCC_Codec_RAW::GetData(fp32 *_pDest, mint _nMaxSamples, bint _bLooping)
{
	// Decode raw PCM to fp32 (mixer packet streaming path, Linux/SDL2 port)
	int nChannels = m_Format.m_Data.GetChannels();
	int SampleSize = m_Format.m_Data.GetSampleSize();
	int nSamples = m_Format.m_Data.GetNumSamples();
	if (!nChannels || !SampleSize || !nSamples)
		return 0;

	int64 DataSize = (int64)nSamples * nChannels * SampleSize;
	mint nDecoded = 0;
	uint8 Temp[4096];
	while (nDecoded < _nMaxSamples)
	{
		int64 Pos = m_pFile->Pos() - m_FileStart;
		int64 BytesLeft = DataSize - Pos;
		if (BytesLeft <= 0)
		{
			if (_bLooping)
			{
				m_pFile->Seek(m_FileStart);
				continue;
			}
			break;
		}

		mint nValues = (_nMaxSamples - nDecoded) * nChannels;
		{
			int64 FileValues = BytesLeft / SampleSize;
			if (FileValues < nValues)
				nValues = (mint)FileValues;
			int64 ChunkValues = sizeof(Temp) / SampleSize;
			if (ChunkValues < nValues)
				nValues = (mint)ChunkValues;
		}
		if (!nValues)
			break;

		m_pFile->Read(Temp, nValues * SampleSize);
		if (SampleSize == 2)
		{
			const int16 *pSrc = (const int16 *)Temp;
			for (mint i = 0; i < nValues; ++i)
				*_pDest++ = pSrc[i] * (1.0f / 32768.0f);
		}
		else if (SampleSize == 1)
		{
			const uint8 *pSrc = (const uint8 *)Temp;
			for (mint i = 0; i < nValues; ++i)
				*_pDest++ = ((int)pSrc[i] - 128) * (1.0f / 128.0f);
		}
		else
		{
			break; // Unsupported sample size
		}
		nDecoded += nValues / nChannels;
	}

	return nDecoded;
}

bool CSCC_Codec_RAW::SeekData(int _SampleOffset)
{
	int SampleOffset = _SampleOffset % m_Format.m_Data.GetNumSamples();

	m_pFile->Seek(SampleOffset * m_Format.m_Data.GetChannels() * m_Format.m_Data.GetSampleSize());

	return true;
}

void CSCC_Codec_RAW::Close()
{
	
}
