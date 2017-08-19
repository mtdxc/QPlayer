//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: AudioPlay.h
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#ifndef _AUDIOPLAY_H_
#define _AUDIOPLAY_H_

#include "WaveOut.h"
#include "WaveBase.h"

/// 音频播放封装类
class CAudioPlay:public CWaveOut, public WaveBase
{
public:
	
	CAudioPlay();
	virtual  ~CAudioPlay();
	/// 创建
	BOOL Create(WAVEFORMATEX *pwf,DWORD dwCallBack,DWORD dwInst,DWORD fdwOpen );
	/// 播放
	BOOL Play(char* buf,UINT uSize);
	/// 释放
	BOOL Destroy();
	
  // add by caiqm
  BOOL Create(int channel, int samplerate, DWORD dwBuffSize);
  virtual void OnWaveDone(WAVEHDR* hdr);
  virtual bool OnWaveAlloc(WAVEHDR* hdr);
  virtual void OnWaveFree(WAVEHDR* hdr);

  /// 开始播放
  BOOL Start();
  /// 停止播放
  BOOL Stop();
protected:
  bool m_bStarted;
};


#endif