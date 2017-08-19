//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: AidopRec.cpp
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#include <windows.h>
#include "WaveIn.h"
#include "AudioRec.h"
#include "stdio.h"


/*------------------------------------------------------------------------------*/
CAudioRec::CAudioRec()
{
  m_bStart = false;
}

/*------------------------------------------------------------------------------*/
CAudioRec::~CAudioRec()
{
	Destroy();
}

/*------------------------------------------------------------------------------*/
/// 开始录音
BOOL CAudioRec::Start()
{
	BOOL bRet = FALSE;
	if(!m_hIn)
		goto RET;

	if(!AllocBuffer(NUM_BUF))
		goto RET;

	m_mmr = waveInStart(m_hIn);
	if(m_mmr)
		goto RET;	
  m_bStart = true;
	bRet = TRUE;
	
RET:
	return bRet;
}

/*------------------------------------------------------------------------------*/
//停止录音
BOOL CAudioRec::Stop()
{
	BOOL bRet = FALSE;
	
	if(!m_hIn)
		goto RET;
  m_bStart = false;
	m_mmr = waveInReset(m_hIn);
	if(m_mmr)
		goto RET;
	if(!FreeBuffer())
		goto RET;
	bRet = TRUE;
RET:

	return TRUE;
}


/*------------------------------------------------------------------------------*/
/// 打开录音设备
BOOL CAudioRec::Create(WAVEFORMATEX *pwf, DWORD dwCallBack, DWORD dwInst, DWORD fdwOpen , DWORD dwBufSize)
{
	BOOL bRet = FALSE;
	if(m_hIn)
	{
		bRet = TRUE;
		goto RET;
	}
	if(!OpenDev(pwf, dwCallBack, dwInst, fdwOpen))
		goto RET;

	m_nBufSize = dwBufSize;
	bRet = TRUE;
RET:
	return bRet;
}

/*------------------------------------------------------------------------------*/
BOOL CAudioRec::Destroy()
{
	BOOL bRet = FALSE;
	if(!CloseDev())
		goto RET;
	bRet = TRUE;
RET:
	return bRet;
}

void waveInProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
  CAudioRec *pRec = (CAudioRec *)dwInstance;
  switch (uMsg)
  {
  case(WOM_DONE):
  {
    WAVEHDR *pWaveHeader = (WAVEHDR*)dwParam1;
    pRec->OnWaveDone(pWaveHeader);
    break;
  }
  case(WOM_OPEN):
  case(WOM_CLOSE):
  default:
    break;
  }
}

BOOL CAudioRec::Create(int channel, int samplerate, DWORD dwBuffSize)
{
  BOOL bRet = FALSE;
  if (m_hIn)
  {
    bRet = TRUE;
    goto RET;
  }

  WAVEFORMATEX wf = {0};
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.cbSize = 0;
  wf.wBitsPerSample = 16;
  wf.nSamplesPerSec = samplerate;
  wf.nChannels = channel;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec*(wf.wBitsPerSample / 8);
  wf.nBlockAlign = wf.nChannels *(wf.wBitsPerSample / 8);

  if (!OpenDev(&wf, (DWORD)&waveInProc, (DWORD)this, CALLBACK_FUNCTION))
    goto RET;

  m_nBufSize = dwBuffSize;
  bRet = TRUE;
RET:
  return bRet;
}

bool CAudioRec::OnWaveAlloc(WAVEHDR* hdr) {
  m_mmr = waveInPrepareHeader(m_hIn, hdr, sizeof(WAVEHDR));
  if (m_mmr) return false;

  m_mmr = waveInAddBuffer(m_hIn, hdr, sizeof(WAVEHDR));
  if (m_mmr) return false;
  return true;
}
void CAudioRec::OnWaveFree(WAVEHDR* hdr) {
  waveInUnprepareHeader(m_hIn, hdr, sizeof(WAVEHDR));
}

void CAudioRec::OnWaveDone(WAVEHDR * hdr)
{
  if (!m_bStart) return;
  if (cb_func) {
    cb_func((BYTE*)hdr->lpData, hdr->dwBufferLength, cb_data);
  }
  MMRESULT result = waveInAddBuffer(m_hIn, hdr, sizeof(WAVEHDR));
  if (result != MMSYSERR_NOERROR) {
    //waveInErrorMsg(result, _T("WaveOutWrite"));
    SetLastMMError(result);
  }
}
