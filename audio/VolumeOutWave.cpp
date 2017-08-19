//NetTalk
/*------------------------------------------------------------------------------*\
 =============================
   Module Name: VolumeOutWave.cpp
 =============================
 
   Alex Chmut
                                              
\*------------------------------------------------------------------------------*/

#include <Windows.h>
#include <MMSystem.h>
#include "VolumeOutWave.h"
#include "malloc.h"
#include <tchar.h>

/*------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////
// 		Defines
#define	BAD_DWORD	(DWORD)-1
#define	WND_CLASS_NAME	_T("Wave Output Volume Msg Wnd Class")
#define	WND_NAME		_T("Wave Output Volume Msg Wnd")
/*------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////
// 		Globals
PCVolumeOutWave g_pThis = NULL;

////////////////////////////////////////////////////////////

//{{{ Audio specific functions
#define AUDFREQ			22050	// Frequency
#define AUDCHANNELS		1		// Number of channels
#define AUDBITSMPL		16		// Number of bits per sample
/*------------------------------------------------------------------------------*/
inline
void SetDeviceType( WAVEFORMATEX* pwfe )
{
	memset( pwfe, 0, sizeof(WAVEFORMATEX) );
	WORD  nBlockAlign = (AUDCHANNELS*AUDBITSMPL)/8;
	DWORD nSamplesPerSec = AUDFREQ;
	pwfe->wFormatTag = WAVE_FORMAT_PCM;
	pwfe->nChannels = AUDCHANNELS;
	pwfe->nBlockAlign = nBlockAlign;
	pwfe->nSamplesPerSec = nSamplesPerSec;
	pwfe->wBitsPerSample = AUDBITSMPL;
	pwfe->nAvgBytesPerSec = nSamplesPerSec*nBlockAlign;
}
//}}} Audio specific functions
/*------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////
// 		Implementation
//////////////
CVolumeOutWave::CVolumeOutWave()
	:	m_bOK(false),
		m_bInitialized(false),
		m_bAvailable(false),

		m_uMixerID(0L),
		m_dwMixerHandle(0L),
		m_hWnd(NULL),

		m_dwMinimalVolume(BAD_DWORD),
		m_dwMaximalVolume(BAD_DWORD),

		m_pfUserSink(NULL),
		m_dwUserValue(0L)
{
	if ( m_bOK = Init() )
	{
		g_pThis = this;
		if ( !Initialize() )
		{
			Done();
			g_pThis = NULL;
		}
	}
}
/*------------------------------------------------------------------------------*/
//////////////
CVolumeOutWave::~CVolumeOutWave()
{
	if ( m_bOK )
		Done();
	g_pThis = NULL;
}
/*------------------------------------------------------------------------------*/
//////////////
bool CVolumeOutWave::Init()
{
	if ( !mixerGetNumDevs() )
		return false;
	// Getting Mixer ID
	HWAVEOUT hwaveOut;
	MMRESULT mmResult;
	WAVEFORMATEX WaveFmt;
	SetDeviceType( &WaveFmt );
	mmResult = waveOutOpen( &hwaveOut, WAVE_MAPPER, &WaveFmt, 0L, 0L, CALLBACK_NULL );
	if ( mmResult != MMSYSERR_NOERROR )
	{
		
		return false;
	} else {
		mmResult = mixerGetID( (HMIXEROBJ)hwaveOut, &m_uMixerID, MIXER_OBJECTF_HWAVEOUT );
		waveOutClose( hwaveOut );
		if ( mmResult != MMSYSERR_NOERROR )
		{
			
			return false;
		}
	}
	// Exposing Window to Mixer
	WNDCLASSEX wcx;
	memset( &wcx, 0, sizeof(WNDCLASSEX) );	
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.lpszClassName = WND_CLASS_NAME;
	wcx.lpfnWndProc = (WNDPROC)MixerWndProc;
	::RegisterClassEx(&wcx);
	m_hWnd = CreateWindow(	WND_CLASS_NAME,
							WND_NAME,
							WS_POPUP | WS_DISABLED,
							0, 0, 0, 0,
							NULL, NULL, NULL, NULL );
	if ( !m_hWnd )
	{
	
		return false;
	}
	::ShowWindow(m_hWnd, SW_HIDE);
	mmResult = mixerOpen( (LPHMIXER)&m_dwMixerHandle, m_uMixerID, (DWORD)m_hWnd, 0L, CALLBACK_WINDOW );
	if ( mmResult != MMSYSERR_NOERROR )
	{
	
		::DestroyWindow( m_hWnd );
		return false;
	}
	return true;
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::Done()
{
	if ( mixerClose( (HMIXER)m_dwMixerHandle ) != MMSYSERR_NOERROR )
	{
	
	}
	::DestroyWindow( m_hWnd );
	m_bInitialized = false;
	m_bOK = false;
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::OnControlChanged( DWORD dwControlID )
{
	if ( m_dwVolumeControlID == dwControlID )
	{
		DWORD dwVolume = GetCurrentVolume();
		if ( (dwVolume!=BAD_DWORD) && (m_pfUserSink) )
		{
			(*m_pfUserSink)( dwVolume, m_dwUserValue );
		}
	}
}
/*------------------------------------------------------------------------------*/
//////////////
bool CVolumeOutWave::Initialize()
{
	MMRESULT mmResult;
	if ( !m_bOK )
		return false;
	
	MIXERLINE MixerLine;
	memset( &MixerLine, 0, sizeof(MIXERLINE) );
	MixerLine.cbStruct = sizeof(MIXERLINE);
	MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
	// user master
	// MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;

	mmResult = mixerGetLineInfo( (HMIXEROBJ)m_dwMixerHandle, &MixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE );
	if ( mmResult != MMSYSERR_NOERROR )
	{
	
		return false;
	}

	MIXERCONTROL Control;
	memset( &Control, 0, sizeof(MIXERCONTROL) );
	Control.cbStruct = sizeof(MIXERCONTROL);

	MIXERLINECONTROLS LineControls;
	memset( &LineControls, 0, sizeof(MIXERLINECONTROLS) );
	LineControls.cbStruct = sizeof(MIXERLINECONTROLS);

	LineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	LineControls.dwLineID = MixerLine.dwLineID;
	LineControls.cControls = 1;
	LineControls.cbmxctrl = sizeof(MIXERCONTROL);
	LineControls.pamxctrl = &Control;
	mmResult = mixerGetLineControls( (HMIXEROBJ)m_dwMixerHandle, &LineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE );
	if ( mmResult == MMSYSERR_NOERROR )
	{
		if ( !(Control.fdwControl & MIXERCONTROL_CONTROLF_DISABLED) )
		{
			m_bAvailable = true;
		} else {
		}
	} else {
	}
	
	m_nChannelCount = MixerLine.cChannels;
	m_dwLineID = LineControls.dwLineID;
	m_dwVolumeControlID = Control.dwControlID;
	m_dwMinimalVolume = Control.Bounds.dwMinimum;
	m_dwMaximalVolume = Control.Bounds.dwMaximum;
	m_dwVolumeStep = Control.Metrics.cSteps;

	m_bInitialized = true;
	return true;
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::EnableLine( bool bEnable )
{
	if ( !m_bInitialized )
		return;
	bool bAnyEnabled = false;
	MMRESULT mmResult;

	MIXERLINE lineDestination;
	memset( &lineDestination, 0, sizeof(MIXERLINE) );
	lineDestination.cbStruct = sizeof(MIXERLINE);
	lineDestination.dwLineID = m_dwLineID;
	mmResult = mixerGetLineInfo( (HMIXEROBJ)m_dwMixerHandle, &lineDestination, MIXER_GETLINEINFOF_LINEID );
	if ( mmResult != MMSYSERR_NOERROR )
	{
		if ( bEnable )
		{
		} else {
		}
		return;
	}
	// Getting all line's controls
	int nControlCount = lineDestination.cControls;
	int nChannelCount = lineDestination.cChannels;
	MIXERLINECONTROLS LineControls;
	memset( &LineControls, 0, sizeof(MIXERLINECONTROLS) );
	MIXERCONTROL* aControls = (MIXERCONTROL*)malloc( nControlCount * sizeof(MIXERCONTROL) );
	if ( !aControls )
	{
		if ( bEnable )
		{
		} else {
		}
		return;
	}
	memset( &aControls[0], 0, sizeof(nControlCount * sizeof(MIXERCONTROL)) );
	for ( int i = 0; i < nControlCount; i++ )
	{
		aControls[i].cbStruct = sizeof(MIXERCONTROL);
	}
	LineControls.cbStruct = sizeof(MIXERLINECONTROLS);
	LineControls.dwLineID = lineDestination.dwLineID;
	LineControls.cControls = nControlCount;
	LineControls.cbmxctrl = sizeof(MIXERCONTROL);
	LineControls.pamxctrl = &aControls[0];
	mmResult = mixerGetLineControls( (HMIXEROBJ)m_dwMixerHandle, &LineControls, MIXER_GETLINECONTROLSF_ALL );
	if ( mmResult == MMSYSERR_NOERROR )
	{
		for (int i = 0; i < nControlCount; i++ )
		{
			LONG lValue;
			bool bReadyToSet = false;
			switch (aControls[i].dwControlType)
			{
			case MIXERCONTROL_CONTROLTYPE_MUTE:
				lValue = (BOOL)!bEnable;
				bReadyToSet = true;
				break;
			case MIXERCONTROL_CONTROLTYPE_SINGLESELECT:
				lValue = (BOOL)bEnable;
				bReadyToSet = true;
				break;
			case MIXERCONTROL_CONTROLTYPE_MUX:
				lValue = (BOOL)bEnable;
				bReadyToSet = true;
				break;
			case MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT:
				lValue = (BOOL)bEnable;
				bReadyToSet = true;
				break;
			case MIXERCONTROL_CONTROLTYPE_MIXER:
				lValue = (BOOL)bEnable;
				bReadyToSet = true;
				break;
			}
			if ( bReadyToSet )
			{
				MIXERCONTROLDETAILS_BOOLEAN* aDetails = NULL;
				int nMultipleItems = aControls[i].cMultipleItems;
				int nChannels = nChannelCount;
				// MIXERCONTROLDETAILS
				MIXERCONTROLDETAILS ControlDetails;
				memset( &ControlDetails, 0, sizeof(MIXERCONTROLDETAILS) );
				ControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
				ControlDetails.dwControlID = aControls[i].dwControlID;
				if ( aControls[i].fdwControl & MIXERCONTROL_CONTROLF_UNIFORM )
				{
					nChannels = 1;
				}
				if ( aControls[i].fdwControl & MIXERCONTROL_CONTROLF_MULTIPLE )
				{
					nMultipleItems = aControls[i].cMultipleItems;
					aDetails = (MIXERCONTROLDETAILS_BOOLEAN*)malloc(nMultipleItems*nChannels*sizeof(MIXERCONTROLDETAILS_BOOLEAN));
					if ( !aDetails )
					{
						continue;
					}
					for ( int nItem = 0; nItem < nMultipleItems; nItem++ )
					{
						for ( int nChannel = 0; nChannel < nChannels; nChannel++ )
						{
							aDetails[nItem+nChannel].fValue = lValue;
						}
					}
				} else {
					nMultipleItems = 0;
					aDetails = (MIXERCONTROLDETAILS_BOOLEAN*)malloc(nChannels*sizeof(MIXERCONTROLDETAILS_BOOLEAN));
					if ( !aDetails )
					{
						if ( bEnable )
						{
						} else {
						}
						continue;
					}
					for ( int nChannel = 0; nChannel < nChannels; nChannel++ )
					{
						aDetails[nChannel].fValue = (LONG)lValue;
					}
				}
				ControlDetails.cChannels = nChannels;
				ControlDetails.cMultipleItems = nMultipleItems;
				ControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
				ControlDetails.paDetails = &aDetails[0];
				mmResult = mixerSetControlDetails( (HMIXEROBJ)m_dwMixerHandle, &ControlDetails, 0L );
				if ( mmResult == MMSYSERR_NOERROR )
				{
					if ( bEnable )
					{
					} else {
					}
					bAnyEnabled = true;
				}
				free( aDetails );
			}
		}
	} else {
		if ( bEnable )
		{
		} else {
		}
	}
	free( aControls );
	if ( !bAnyEnabled )
	{
		if ( bEnable )
		{
		} else {
		}
	}
}
/*------------------------------------------------------------------------------*/
//////////////////////////////////////////////
// IVolume interface
//////////////
bool CVolumeOutWave::IsAvailable()
{
	return m_bAvailable;
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::Enable()
{
	EnableLine( true );
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::Disable()
{
	EnableLine( false );
}
/*------------------------------------------------------------------------------*/
//////////////
DWORD CVolumeOutWave::GetVolumeMetric()
{
	if ( !m_bAvailable )
		return BAD_DWORD;
	return m_dwVolumeStep;
}
/*------------------------------------------------------------------------------*/
//////////////
DWORD CVolumeOutWave::GetMinimalVolume()
{
	if ( !m_bAvailable )
		return BAD_DWORD;
	return m_dwMinimalVolume;
}
/*------------------------------------------------------------------------------*/
//////////////
DWORD CVolumeOutWave::GetMaximalVolume()
{
	if ( !m_bAvailable )
		return BAD_DWORD;
	return m_dwMaximalVolume;
}
/*------------------------------------------------------------------------------*/
//////////////
DWORD CVolumeOutWave::GetCurrentVolume()
{
	if ( !m_bAvailable )
		return BAD_DWORD;
	MIXERCONTROLDETAILS_UNSIGNED* aDetails = (MIXERCONTROLDETAILS_UNSIGNED*)malloc(m_nChannelCount*sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	if ( !aDetails )
		return BAD_DWORD;
	MIXERCONTROLDETAILS ControlDetails;
	memset( &ControlDetails, 0, sizeof(MIXERCONTROLDETAILS) );
	ControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
	ControlDetails.dwControlID = m_dwVolumeControlID;
	ControlDetails.cChannels = m_nChannelCount;
	ControlDetails.cMultipleItems = 0;
	ControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	ControlDetails.paDetails = &aDetails[0];
	MMRESULT mmResult = mixerGetControlDetails( (HMIXEROBJ)m_dwMixerHandle, &ControlDetails, MIXER_GETCONTROLDETAILSF_VALUE );
	DWORD dw = aDetails[0].dwValue;
	free( aDetails );
	if ( mmResult != MMSYSERR_NOERROR )
	{
		return BAD_DWORD;
	}
	return dw;
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::SetCurrentVolume( DWORD dwValue )
{
	if ( !m_bAvailable || (dwValue<m_dwMinimalVolume) || (dwValue>m_dwMaximalVolume) )
		return;
	MIXERCONTROLDETAILS_UNSIGNED* aDetails = (MIXERCONTROLDETAILS_UNSIGNED*)malloc(m_nChannelCount*sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	if ( !aDetails )
		return;
	for ( int i = 0; i < m_nChannelCount; i++ )
	{
		aDetails[i].dwValue = dwValue;
	}
	MIXERCONTROLDETAILS ControlDetails;
	memset( &ControlDetails, 0, sizeof(MIXERCONTROLDETAILS) );
	ControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
	ControlDetails.dwControlID = m_dwVolumeControlID;
	ControlDetails.cChannels = m_nChannelCount;
	ControlDetails.cMultipleItems = 0;
	ControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	ControlDetails.paDetails = &aDetails[0];
	MMRESULT mmResult = mixerSetControlDetails( (HMIXEROBJ)m_dwMixerHandle, &ControlDetails, MIXER_SETCONTROLDETAILSF_VALUE );
	free( aDetails );
	if ( mmResult != MMSYSERR_NOERROR )
	{
	}
}
/*------------------------------------------------------------------------------*/
//////////////
void CVolumeOutWave::RegisterNotificationSink( PONMICVOULUMECHANGE pfUserSink, DWORD dwUserValue )
{
	m_pfUserSink = pfUserSink;
	m_dwUserValue = dwUserValue;
}
/*------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK CVolumeOutWave::MixerWndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( uMsg == MM_MIXM_CONTROL_CHANGE )
	{
		if ( g_pThis )
		{
			g_pThis->OnControlChanged( (DWORD)lParam );
		}
	}
	return ::DefWindowProc( hwnd, uMsg, wParam, lParam);
}
