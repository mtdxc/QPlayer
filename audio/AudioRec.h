//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: AudioRec.h
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#ifndef _AUDIOREC_H_
#define _AUDIOREC_H_
#include "WaveIn.h"
#include "WaveBase.h"
/**
@brief 音频采集类. 
 
*/
class CAudioRec:public CWaveIn, public WaveBase
{
public:
	CAudioRec();
	virtual ~CAudioRec();

	/// 释放
	BOOL Destroy();
	/// 创建
	BOOL Create(
		WAVEFORMATEX *pwf, ///< 格式
		DWORD dwCallBack, ///< 回调
		DWORD dwInst, ///< 
		DWORD fdwOpen, ///< 
		DWORD dwBufSize ///< 缓冲区大小
		);
	/// 开始采集
	BOOL Start();
	/// 停止采集
	BOOL Stop();

  BOOL Create(int channel, int samplerate, DWORD dwBuffSize);
  void OnWaveDone(WAVEHDR* hdr);
  bool OnWaveAlloc(WAVEHDR* hdr);
  void OnWaveFree(WAVEHDR* hdr);
protected:
  bool m_bStart;
};

#endif