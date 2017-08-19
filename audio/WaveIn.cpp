//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: wavein.cpp
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#include <Windows.h>
#include "WaveIn.h"
#pragma comment(lib, "Winmm")
/*------------------------------------------------------------------------------*/
/// MsgBox出mmr错误
void waveInErrorMsg(MMRESULT mmr, TCHAR* szTitle)
{
	TCHAR txt[MAXERRORLENGTH];
	waveInGetErrorText(mmr, txt, MAXERRORLENGTH);
	MessageBox(0, txt, szTitle, MB_OK|MB_ICONWARNING);
}
/*------------------------------------------------------------------------------*/
CWaveIn::CWaveIn()
{
	m_mmr = 0;
	m_hIn = 0;	
}

/*------------------------------------------------------------------------------*/
/// 打开录音设备
BOOL CWaveIn::OpenDev(WAVEFORMATEX* pfmt, DWORD dwCallback, DWORD dwCallbackInstance, DWORD fdwOpen)
{
	BOOL bRet = FALSE;
	WAVEFORMATEX wf;
	if(!pfmt)//默认格式
	{
		wf.wFormatTag	 =  WAVE_FORMAT_PCM;
		wf.cbSize		 =  0;
		wf.wBitsPerSample = 16;
		wf.nSamplesPerSec = 8000;
		wf.nChannels = 1;
		wf.nAvgBytesPerSec	 =  wf.nSamplesPerSec*(wf.wBitsPerSample/8);
		wf.nBlockAlign		 =  wf.nChannels     *(wf.wBitsPerSample/8);
		pfmt = &wf;
	}

	MMRESULT mmr;
	mmr = waveInOpen(0, WAVE_MAPPER, pfmt, 0, 0, WAVE_FORMAT_QUERY);
	if(mmr)
	{
		SetLastMMError(mmr);
		goto RET;
	}

	mmr = waveInOpen(&m_hIn, WAVE_MAPPER, pfmt, dwCallback, dwCallbackInstance, fdwOpen);
	if(mmr)
	{
		SetLastMMError(mmr);
		goto RET;
	}
	bRet = TRUE;
RET:
	return bRet;
}
/*------------------------------------------------------------------------------*/
CWaveIn::operator HWAVEIN() const
{
	return m_hIn;
}
/*------------------------------------------------------------------------------*/
CWaveIn::~CWaveIn()
{
	
}
/*------------------------------------------------------------------------------*/
MMRESULT CWaveIn::GetLastMMError()
{
	return m_mmr;
}
/*------------------------------------------------------------------------------*/
void CWaveIn::SetLastMMError(MMRESULT mmr)
{
	m_mmr = mmr;
}
/*------------------------------------------------------------------------------*/
//该死的死锁!就出现在这里
BOOL CWaveIn::CloseDev()
{
	BOOL bRet = FALSE;
	if(!m_hIn)
		goto RET;
	m_mmr = waveInReset(m_hIn);
	if(m_mmr)
		goto RET;
	m_mmr = waveInClose(m_hIn);
	if(m_mmr)
		goto RET;
	m_hIn = 0;
	bRet = TRUE;
RET:
	return bRet;
}
/*------------------------------------------------------------------------------*/
HWAVEIN CWaveIn::GetHandle()
{
	return m_hIn;
}
