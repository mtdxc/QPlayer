#pragma once
//////////////////////////////////////////////////////////////////////
// SoundOut.h: interface for the CSound class.
//////////////////////////////////////////////////////////////////////////////////////////    

#pragma once

#include <mmsystem.h>

#define MAX_OUTPUT_SAMPLES 8192   
#define MAX_VOIE 2
#define MAX_SIZE_SAMPLES  1  // WORD
#define MAX_SIZE_OUTPUT_BUFFER   (MAX_OUTPUT_SAMPLES*MAX_VOIE*MAX_SIZE_SAMPLES) 
#define MAX_OUT_BUFFERS 20

typedef void(*AudioCallBack)(BYTE* pcm, int length, LPVOID arg);

class CSoundOut
{
protected:

  struct _WAVE_DATA
  {
    WAVEHDR			m_WaveHeader;
    short  OutputBuffer[MAX_SIZE_OUTPUT_BUFFER];
  }m_WaveData[MAX_OUT_BUFFERS];

  WAVEOUTCAPS		m_WaveOutDevCaps;
  HWAVEOUT		  m_WaveOut;
  WAVEFORMATEX	m_WaveFormat;

  BOOL bTerminateFlag;
  UINT m_WaveOutSampleRate;

  AudioCallBack m_cbAudio;
  LPVOID m_cbArgs;
  //////////////////////////////////////////////////////
  // functions members

public:
  /// 每包采样数
  int m_NbMaxSamples;

  void StartOutput();
  void StopOutput();
  void CloseOutput();
  MMRESULT OpenOutput();
public:
  virtual void UpdateSamplesData(SHORT *);
  void NotifyData(BYTE* pcm, int length);
  void SetCallBack(AudioCallBack cb, LPVOID arg) {
    m_cbAudio = cb;
    m_cbArgs = arg;
  }

  void WaveInitFormat(WORD    nCh,		 // number of channels (mono, stereo)
    DWORD   nSampleRate, // sample rate
    WORD    BitsPerSample);

  CSoundOut();
  virtual ~CSoundOut();
public:
  static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

};

