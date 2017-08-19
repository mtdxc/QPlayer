/*------------------------------------------------------------------------------*\
 =============================
   模块名称: waveout.h
 =============================

 [目的]
 
     方便waveOutXXX函数族的使用，使其对象化     
     	  
 [描述]
		
	该模块包括CWaveOut类，这是个封装了录音操作的类。   
 
 [用法]
   
    此类是基础类，建议不要直接使用该类		
	 
 [依赖性]
	
	 Winmm.lib 

 [修改记录]
 
  版本:    1.01.01
  日期:    01-11-1         
  作者:    Brant Q
  备注:
  

 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#ifndef _WAVEOUT_H_
#define _WAVEOUT_H_
#include <MMSystem.h>
void waveOutErrorMsg(MMRESULT mmr,char* szTitle);

class CWaveOut
{
public:
	HWAVEOUT GetHandle();
	BOOL CloseDev();
	void SetLastMMError(MMRESULT mmr);
	MMRESULT GetLastMMError();
	
	virtual  ~CWaveOut();

	CWaveOut();
  UINT GetMixerID() {
    UINT uMixerID = 0;
    MMRESULT mmr = mixerGetID((HMIXEROBJ)m_hOut, &uMixerID, MIXER_OBJECTF_HWAVEOUT);
    return uMixerID;
  }
	BOOL OpenDev(WAVEFORMATEX* pfmt,DWORD dwCallback,DWORD dwCallbackInstance,DWORD fdwOpen);
	operator HWAVEOUT() const;
protected:

	MMRESULT m_mmr;

	HWAVEOUT m_hOut;
	
};


#endif