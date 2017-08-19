#include <Windows.h>
#include <MMSystem.h>
#include "WaveBase.h"

void WaveBase::SetPcmCallback(PcmCallback func, void * data) {
  cb_func = func;
  cb_data = data;
}

WaveBase::~WaveBase() {
  FreeBuffer();
}

/// 释放缓冲区

/*------------------------------------------------------------------------------*/
//释放内存

bool WaveBase::FreeBuffer()
{
  bool bRet = false;
  for (int i = 0; i<m_pHdr.size(); i++)
  {
    OnWaveFree(&m_pHdr[i]);
    if (m_pHdr[i].lpData)
      free(m_pHdr[i].lpData);
  }
  m_pHdr.clear();
  bRet = true;
RET:
  return bRet;
}

/// 申请缓冲区
/*------------------------------------------------------------------------------*/
/// 为播放分配一组内存

bool WaveBase::AllocBuffer(int count)
{
  bool bRet = false;
  if (m_pHdr.size()) {
    printf("please call FreeBuffer first");
    return false;
  }
  m_pHdr.resize(count);
  //为了使录音连续，采用多个缓冲区
  for (int i = 0; i<count; i++)
  {
    ZeroMemory(&m_pHdr[i], sizeof(WAVEHDR));
    m_pHdr[i].lpData = (LPSTR)malloc(m_nBufSize);
    memset(m_pHdr[i].lpData, 0, sizeof(m_nBufSize));
    m_pHdr[i].dwBufferLength = m_nBufSize;
    if (!OnWaveAlloc(&m_pHdr[i]))
      goto RET;
  }

  bRet = true;
RET:
  return bRet;
}
