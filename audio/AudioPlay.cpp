#include "WaveBase.h"
#include "WaveBase.h"
//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   Module Name: AudioPlay.cpp
 =============================
                 
\*------------------------------------------------------------------------------*/
#include <windows.h>
#include <MMREG.H>

#include "WaveOut.h"
#include "AudioPlay.h"

/*------------------------------------------------------------------------------*/


CAudioPlay::CAudioPlay()
{
  m_bStarted = false;
}
/*------------------------------------------------------------------------------*/
//播放一块数据
BOOL CAudioPlay::Play(char *buf, UINT uSize)
{

	BOOL bRet=FALSE;
	char* p;
	LPWAVEHDR pwh=new WAVEHDR;
	if(!pwh)
		goto RET;
	p=new char[uSize];//重新分配一块内存(在播放结束后删除)
	if(!p)
		goto RET;
	CopyMemory(p,buf,uSize);
	ZeroMemory(pwh,sizeof(WAVEHDR));
	pwh->dwBufferLength=uSize;
	pwh->lpData=p;
	//
	m_mmr=waveOutPrepareHeader(m_hOut,pwh,sizeof(WAVEHDR));
    if(m_mmr)
		goto RET;
	m_mmr=waveOutWrite(m_hOut,pwh,sizeof(WAVEHDR));
	if(m_mmr)
		goto RET;
	bRet=TRUE;
	
	
RET:
	return bRet;
}

/*------------------------------------------------------------------------------*/
//打开音频输出设备
BOOL CAudioPlay::Create(WAVEFORMATEX *pwf,DWORD dwCallBack,DWORD dwInst,DWORD fdwOpen )
{
	BOOL bRet=FALSE;
	if(m_hOut)
	{
		bRet=TRUE;
		goto RET;
	}
	//打开设备
	if(!OpenDev(pwf,dwCallBack,dwInst,fdwOpen))
		goto RET;
	bRet=TRUE;
RET:
	return bRet;
}

/*------------------------------------------------------------------------------*/

CAudioPlay::~CAudioPlay()
{
	Destroy();
}

/*------------------------------------------------------------------------------*/

BOOL CAudioPlay::Destroy()
{
	BOOL bRet=FALSE;
	if(!CloseDev())
		goto RET;
	bRet=TRUE;
RET:
	return bRet;

}

void waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
  CAudioPlay *pPlay = (CAudioPlay *)dwInstance;
  // if (pSoundOut->bTerminateFlag) return;
  switch (uMsg)
  {
  case(WOM_DONE):
  {
    WAVEHDR *pWaveHeader = (WAVEHDR*)dwParam1;
    pPlay->OnWaveDone(pWaveHeader);
    break;
  }
  case(WOM_OPEN):
  case(WOM_CLOSE):
  default:
    break;
  }
}

BOOL CAudioPlay::Create(int channel, int samplerate, DWORD dwBuffSize)
{
  BOOL bRet = FALSE;
  if (m_hOut)
  {
    bRet = TRUE;
    goto RET;
  }

  WAVEFORMATEX wf = { 0 };
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.cbSize = 0;
  wf.wBitsPerSample = 16;
  wf.nSamplesPerSec = samplerate;
  wf.nChannels = channel;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec*(wf.wBitsPerSample / 8);
  wf.nBlockAlign = wf.nChannels *(wf.wBitsPerSample / 8);

  if (!OpenDev(&wf, (DWORD)&waveOutProc, (DWORD)this, CALLBACK_FUNCTION))
    goto RET;

  m_nBufSize = dwBuffSize;
  bRet = TRUE;
RET:
  return bRet;
}

void CAudioPlay::OnWaveDone(WAVEHDR * hdr)
{
  if (!m_bStarted)
    return;
  if (cb_func) {
    cb_func((BYTE*)hdr->lpData, hdr->dwBufferLength, cb_data);
  }
  m_mmr = waveOutWrite(m_hOut, hdr, sizeof(WAVEHDR));
}

bool CAudioPlay::OnWaveAlloc(WAVEHDR* hdr) {
  m_mmr = waveOutPrepareHeader(m_hOut, hdr, sizeof(WAVEHDR));
  if (m_mmr) return false;
  m_mmr = waveOutWrite(m_hOut, hdr, sizeof(WAVEHDR));
  if (m_mmr) return false;
  return true;
}
void CAudioPlay::OnWaveFree(WAVEHDR* hdr) {
  waveOutUnprepareHeader(m_hOut, hdr, sizeof(WAVEHDR));
}
/*------------------------------------------------------------------------------*/
/// 开始播放
BOOL CAudioPlay::Start()
{
  BOOL bRet = FALSE;
  if (!m_hOut)
    goto RET;

  if (!AllocBuffer(NUM_BUF))
    goto RET;

  m_mmr = waveOutRestart(m_hOut);
  if (m_mmr)
    goto RET;
  m_bStarted = true;
  bRet = TRUE;

RET:
  return bRet;
}

/*------------------------------------------------------------------------------*/
//停止录音
BOOL CAudioPlay::Stop()
{
  BOOL bRet = FALSE;

  if (!m_hOut)
    goto RET;
  m_bStarted = false;
  m_mmr = waveOutReset(m_hOut);
  if (m_mmr)
    goto RET;
  if (!FreeBuffer())
    goto RET;
  bRet = TRUE;
RET:

  return TRUE;
}
