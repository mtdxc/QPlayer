#include <windows.h>
#include "math.h"
#include "SoundOut.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif
#pragma comment(lib, "winmm.lib ")
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
extern void Output(const char* fmt, ...);

void CSoundOut::waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
  CSoundOut *pSoundOut = (CSoundOut *)dwInstance;
  if (pSoundOut->bTerminateFlag)
    return;

  switch (uMsg)
  {
  case(WOM_DONE):
  {
    WAVEHDR *pWaveHeader = (WAVEHDR*)dwParam1;
    pSoundOut->NotifyData((BYTE*)pWaveHeader->lpData, pWaveHeader->dwBufferLength);
    MMRESULT result = waveOutWrite(pSoundOut->m_WaveOut, pWaveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
      Output("Sound output Cannot Add Buffer !");
    break;
  }
  case(WOM_OPEN):
  case(WOM_CLOSE):
  default:
    break;
  }
}

CSoundOut::CSoundOut()
{
  m_NbMaxSamples = 1024;
  // init format 
  m_WaveOutSampleRate = 44100;
  WaveInitFormat(MAX_VOIE, m_WaveOutSampleRate /* khz */, 16 /* bits */);

  memset(&m_WaveData, 0, sizeof(m_WaveData));
  bTerminateFlag = FALSE;
  m_WaveOut = NULL;
  m_cbAudio = NULL;
  m_cbArgs = NULL;
}

CSoundOut::~CSoundOut()
{
  CloseOutput();
}

///////////////////////////////////////////////////////////////////
MMRESULT CSoundOut::OpenOutput()
{
  MMRESULT result;

  result = waveOutGetNumDevs();
  if (result == 0)
  {
    Output("No Sound Output Device");
    return result;
  }

  // test for Mic available   
  result = waveOutGetDevCaps(0, &m_WaveOutDevCaps, sizeof(WAVEOUTCAPS));

  if (result != MMSYSERR_NOERROR)
  {
    Output("Sound output Cannot determine card capabilities !");
  }

  // Open Output 
  result = waveOutOpen(&m_WaveOut, 0, &m_WaveFormat, (DWORD)waveOutProc, (DWORD)this, CALLBACK_FUNCTION);
  if (result != MMSYSERR_NOERROR)
  {
    Output("Sound output Cannot Open Device!");
    return result;
  }

  for (int i = 0; i<MAX_OUT_BUFFERS; i++)
  {
    _WAVE_DATA *pWaveOut = &m_WaveData[i];
    //UpdateSamplesData(pWaveOut->OutputBuffer);
    memset(pWaveOut->OutputBuffer, 0, m_NbMaxSamples);
    pWaveOut->m_WaveHeader.lpData = (CHAR *)&pWaveOut->OutputBuffer[0];
    pWaveOut->m_WaveHeader.dwBufferLength = m_NbMaxSamples * 2;
    pWaveOut->m_WaveHeader.dwFlags = 0;

    result = waveOutPrepareHeader(m_WaveOut, &pWaveOut->m_WaveHeader, sizeof(WAVEHDR));
    //MMRESULT waveOutPrepareHeader( HWAVEOUT hwi, LPWAVEHDR pwh, UINT cbwh ); 
    if ((result != MMSYSERR_NOERROR) || (pWaveOut->m_WaveHeader.dwFlags != WHDR_PREPARED))
    {
      Output(" Sound Output Cannot Prepare Header !");
      return result;
    }

    result = waveOutWrite(m_WaveOut, &pWaveOut->m_WaveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
    {
      Output(" Sound Output Cannot Write Buffer !");
      return result;
    }
  }

  bTerminateFlag = false;
  // all is correct now we can start the process
  result = waveOutRestart(m_WaveOut);
  if (result != MMSYSERR_NOERROR)
  {
    Output(" Sound Output Cannot Start Wave Out !");
    return result;
  }
  return result;
}

//在这里以正弦波为例 实际应用中可以是PCM波形数据
void CSoundOut::UpdateSamplesData(SHORT *pBuffer)
{
  static double m_Angle[2] = { 0, 0 };
  static double Freq[2] = { 1000, 5000 };
  static double D_PI = 2.0 * (atan(1.0) * 4.0); //2π

  double dAnagle[2] = { D_PI * Freq[0] / m_WaveOutSampleRate,
    D_PI * Freq[1] / m_WaveOutSampleRate };

  /* init the sound Ouutput buffer with a signal noise */
  for (DWORD i = 0; i<m_NbMaxSamples; i++)
  {
    *pBuffer++ = (SHORT)(32000.0*sin(m_Angle[0])); //ch1
    m_Angle[0] += dAnagle[0];

    if (MAX_VOIE > 1)
    {
      *pBuffer++ = (SHORT)(32000.0*sin(m_Angle[1])); //ch2
      m_Angle[1] += dAnagle[1];
    }
  }
}

void CSoundOut::NotifyData(BYTE * pcm, int length)
{
  if (m_cbAudio) {
    m_cbAudio(pcm, length, m_cbArgs);
  }
}

/*
WAVE_FORMAT_1M08  11.025 kHz, mono, 8-bit
WAVE_FORMAT_1M16  11.025 kHz, mono, 16-bit
WAVE_FORMAT_1S08  11.025 kHz, stereo, 8-bit
WAVE_FORMAT_1S16  11.025 kHz, stereo, 16-bit
WAVE_FORMAT_2M08  22.05 kHz, mono, 8-bit
WAVE_FORMAT_2M16  22.05 kHz, mono, 16-bit
WAVE_FORMAT_2S08  22.05 kHz, stereo, 8-bit
WAVE_FORMAT_2S16  22.05 kHz, stereo, 16-bit
WAVE_FORMAT_4M08  44.1 kHz, mono, 8-bit
WAVE_FORMAT_4M16  44.1 kHz, mono, 16-bit
WAVE_FORMAT_4S08  44.1 kHz, stereo, 8-bit
WAVE_FORMAT_4S16  44.1 kHz, stereo, 16-bit
*/

void CSoundOut::WaveInitFormat(WORD    nCh, // number of channels (mono, stereo)
  DWORD   nSampleRate, // sample rate
  WORD    BitsPerSample)
{
  m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
  m_WaveFormat.nChannels = nCh;
  m_WaveFormat.nSamplesPerSec = nSampleRate;
  m_WaveFormat.nAvgBytesPerSec = nSampleRate * nCh * BitsPerSample / 8;
  m_WaveFormat.nBlockAlign = m_WaveFormat.nChannels * BitsPerSample / 8;
  m_WaveFormat.wBitsPerSample = BitsPerSample;
  m_WaveFormat.cbSize = 0;
}


///////////////////////////////////////////////////////////////////////////
// the comutation for the Output samples need to be calibrated according
// to the SoundOut board  add an Offset and a Mult coef.

void CSoundOut::CloseOutput()
{
  bTerminateFlag = TRUE;
  if (m_WaveOut)
  {
    waveOutReset(m_WaveOut);

    for (int i = 0; i<MAX_OUT_BUFFERS; i++)
    {
      _WAVE_DATA *pWaveOut = &m_WaveData[i];

      MMRESULT result = waveOutUnprepareHeader(m_WaveOut, &pWaveOut->m_WaveHeader, sizeof(WAVEHDR));
      if (result != MMSYSERR_NOERROR)
        Output("Sound output Cannot UnPrepareHeader !");
    }

    waveOutClose(m_WaveOut);
    m_WaveOut = NULL;
  }
}

void CSoundOut::StopOutput()
{
  waveOutPause(m_WaveOut);
}

void CSoundOut::StartOutput()
{
  waveOutRestart(m_WaveOut);
}
