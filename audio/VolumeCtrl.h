//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: VolumeCtrl.h
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
#ifndef _VOLUMECTRL_H_
#define _VOLUMECTRL_H_
#include <mmsystem.h>


class CVolumeCtrl
{
public:
	BOOL SetVolume(DWORD dwValue);
	DWORD GetVolume();
	BOOL Close();
	BOOL Open(UINT uDevHandle,BOOL bFlag);
	virtual  ~CVolumeCtrl();
	CVolumeCtrl();

protected:
	HMIXER m_h;
	MIXERLINE           m_mxl;
	MIXERLINECONTROLS   m_mxlc;
	MIXERCONTROL        m_mxctrl;
};


#endif