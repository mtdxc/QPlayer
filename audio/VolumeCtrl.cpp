//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   模块名称: VolumeCtrl.cpp
 =============================
 
 [版权]
 
   2000-2002  115软件工厂  版权所有
                                              
\*------------------------------------------------------------------------------*/
//not used
#include <Windows.h>
#include "VolumeCtrl.h"

/*------------------------------------------------------------------------------*/
CVolumeCtrl::CVolumeCtrl()
{
	m_h=0;
}
/*------------------------------------------------------------------------------*/
CVolumeCtrl::~CVolumeCtrl()
{
	Close();
}
/*------------------------------------------------------------------------------*/
BOOL CVolumeCtrl::Open(UINT uDevHandle,BOOL bFlag)
{
	BOOL bRet=FALSE;
	Close();
	DWORD f;
	if(bFlag)
		f=MIXER_OBJECTF_HWAVEOUT;
	else
		f=MIXER_OBJECTF_HWAVEIN;
	MMRESULT mmr;
	mmr=mixerOpen(&m_h,uDevHandle,0,0,f);
	if(mmr!=MMSYSERR_NOERROR)
		goto RET;
	
	m_mxl.cbStruct      = sizeof(m_mxl);
	m_mxl.dwDestination = 0;
	mmr = mixerGetLineInfo((HMIXEROBJ)m_h, &m_mxl, MIXER_GETLINEINFOF_DESTINATION);
	if (MMSYSERR_NOERROR != mmr)
		goto RET;
	
	m_mxlc.cbStruct       = sizeof(m_mxlc);
	m_mxlc.dwLineID       = m_mxl.dwLineID;
	m_mxlc.cControls      = m_mxl.cControls;
	m_mxlc.cbmxctrl       = sizeof(m_mxctrl);
	m_mxlc.dwControlType  = MIXERCONTROL_CONTROLTYPE_VOLUME;
	m_mxlc.pamxctrl       = &m_mxctrl;
	mmr = mixerGetLineControls((HMIXEROBJ)m_h, &m_mxlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (MMSYSERR_NOERROR != mmr)
		goto RET;

	bRet=TRUE;
RET:
	if(!bRet)
		Close();
	return bRet;
}
/*------------------------------------------------------------------------------*/
BOOL CVolumeCtrl::Close()
{
	MMRESULT mmr = 0;
	if(m_h)
		mmr=mixerClose(m_h);
	m_h=0;
	if(mmr==MMSYSERR_NOERROR) return TRUE;
	else return FALSE;
}
/*------------------------------------------------------------------------------*/
DWORD CVolumeCtrl::GetVolume()
{
	if(!m_h) return 0;
	MIXERCONTROLDETAILS mxcd;
	MIXERCONTROLDETAILS_UNSIGNED mxcd_u;
	mxcd.cbStruct=sizeof(mxcd);
	mxcd.cChannels=1;
	mxcd.dwControlID=m_mxctrl.dwControlID;
	mxcd.paDetails=&mxcd_u;
	mxcd.cbDetails=4;
	mxcd.cMultipleItems=m_mxctrl.cMultipleItems;
	MMRESULT mmr;
	mmr=mixerGetControlDetails((HMIXEROBJ)m_h,&mxcd,MIXER_SETCONTROLDETAILSF_VALUE);
	if(mmr!=MMSYSERR_NOERROR)
		return 0;
	else
		return (mxcd_u.dwValue-m_mxctrl.Bounds.dwMinimum)*100/(m_mxctrl.Bounds.dwMaximum-m_mxctrl.Bounds.dwMinimum);
}
/*------------------------------------------------------------------------------*/
BOOL CVolumeCtrl::SetVolume(DWORD dwValue)
{
	if(!m_h) return FALSE;

	MIXERCONTROLDETAILS mxcd;
	MIXERCONTROLDETAILS_UNSIGNED mxcd_u;
	mxcd.cbStruct=sizeof(mxcd);
	mxcd.cChannels=1;
	mxcd.dwControlID=m_mxctrl.dwControlID;
	mxcd.paDetails=&mxcd_u;
	mxcd.cbDetails= sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mxcd.cMultipleItems=0;
	mxcd_u.dwValue=dwValue*(m_mxctrl.Bounds.dwMaximum-m_mxctrl.Bounds.dwMinimum)/100+m_mxctrl.Bounds.dwMinimum;
	MMRESULT mmr;
	mmr=mixerSetControlDetails((HMIXEROBJ)m_h,&mxcd,MIXER_SETCONTROLDETAILSF_VALUE);
	if(mmr!=MMSYSERR_NOERROR)
		return FALSE;
	return TRUE;
}
