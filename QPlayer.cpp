#include "QPlayer.h"
#include "UIWnd.h"
#include <atlbase.h>
#include <CommDlg.h>

#define UM_PROGRESS 101
static QPlayer* gPlayer = NULL;
QPlayer* QPlayer::Instance()
{
  return gPlayer;
}

QPlayer::QPlayer(bool top) :top_window_(top)
{
  gPlayer = this;
  m_bFullScreenMode = false;
}


QPlayer::~QPlayer()
{
  if (gPlayer == this)
    gPlayer = NULL;
}

DuiLib::CControlUI* QPlayer::CreateControl(LPCTSTR pstrClass)
{
  if (!_tcscmp(pstrClass, _T("VideoWnd"))) {
    CWndUI* pRet = new CWndUI();
    video_wnd_.Create(GetHWND(), _T("LocalVideo"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, 0);
    video_wnd_.SetFitMode(FALSE); video_wnd_.SetBkColor(0);
    pRet->Attach(video_wnd_.handle());
    return pRet;
  }
  return NULL;
}

CDuiString QPlayer::GetSkinFolder()
{
  return _T("skin");
}

CDuiString QPlayer::GetSkinFile()
{
  return _T("player.xml");
}

LPCTSTR QPlayer::GetWindowClassName(void) const
{
  return _T("QPlayer");
}

void CALLBACK OutVolumeChanged(DWORD dwCurrentVolume, DWORD dwUserValue)
{
  DuiLib::CSliderUI* p = (DuiLib::CSliderUI*)dwUserValue;
  p->SetValue(dwCurrentVolume);
}

void QPlayer::InitWindow()
{
  ::GetWindowPlacement(*this, &m_OldWndPlacement);

  slide_player_ = (DuiLib::CSliderUI*)m_pm.FindControl(_T("sliderPlay"));
  slide_vol_ = (DuiLib::CSliderUI*)m_pm.FindControl(_T("sliderVol"));
  if (slide_vol_) {
    slide_vol_->SetMinValue(audio_vol.GetMinimalVolume());
    slide_vol_->SetMaxValue(audio_vol.GetMaximalVolume());
    slide_vol_->SetValue(audio_vol.GetCurrentVolume());
    audio_vol.RegisterNotificationSink(OutVolumeChanged, (DWORD)slide_vol_);
  }
  lbStatus = (DuiLib::CSliderUI*)m_pm.FindControl(_T("lbStatus"));
  btnOpen = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnOpen"));
  btnStop = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnStop"));
  btnPause = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnPause"));
  btnPlay = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnPlay"));
  player_.set_event(this);
}

void QPlayer::OnFinalMessage(HWND hWnd)
{
  CloseFile();
  if (top_window_)
    ::PostMessage(NULL, WM_QUIT, 0, 0);
  __super::OnFinalMessage(hWnd);
  //delete this;
}

void QPlayer::Mute(bool mute) {
  DuiLib::CControlUI* btnMute = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnVolumeZero"));
  DuiLib::CControlUI* btnUnMute = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnVolume"));
  if (btnMute)
    btnMute->SetVisible(mute);
  if (btnUnMute)
    btnUnMute->SetVisible(!mute);
  player_.set_muted(mute);
}

void QPlayer::Notify(DuiLib::TNotifyUI& msg)
{
  if (msg.sType == DUI_MSGTYPE_CLICK) {
    if (msg.pSender->GetName() == _T("btnOpen")) {
      video_wnd_.SetText("", RGB(255, 255, 255));
      DuiLib::CDuiString url;
      if (DuiLib::CControlUI* pUI = m_pm.FindControl(_T("edUrl"))) {
        url = pUI->GetText();
      }
      if (url.GetLength()) {
        OpenFile(url.GetData());
      }
      else {
        TCHAR szFile[1024] = { 0 };
        OPENFILENAME ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        // must !
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        // ofn.lpstrFilter = _T("Flv file(*.flv)\0*.flv\0");
        if (GetOpenFileName(&ofn)) {
          OpenFile(szFile);
        }
      }
      UpdateUI();
    }
    if (msg.pSender->GetName() == _T("btnStop")) {
      video_wnd_.SetText("", RGB(255, 255, 255));
      CloseFile();
      UpdateUI();
    }
    else if (msg.pSender->GetName() == _T("btnPause")) {
      player_.pause();
      UpdateUI();
    }
    else if (msg.pSender->GetName() == _T("btnPlay")) {
      player_.resume();
      UpdateUI();
    }
    else if (msg.pSender->GetName() == _T("btnVolumeZero")) {
      Mute(false);
    }
    else if (msg.pSender->GetName() == _T("btnVolume")) {
      Mute(true);
    }
    else if (msg.pSender->GetName() == _T("btnScreenFull")) {
      FullScreen(true);
    }
    else if (msg.pSender->GetName() == _T("btnScreenNormal")) {
      FullScreen(false);
    }
    else if (msg.pSender->GetName() == _T("btnFastBackward")) {
      seek(-30);
    }
    else if (msg.pSender->GetName() == _T("btnFastForward")) {
      seek(30);
    }
    else if (msg.pSender->GetName() == _T("btnPrevious")) {
    }
    else if (msg.pSender->GetName() == _T("btnNext")) {
    }
  }
  else if (msg.sType == DUI_MSGTYPE_VALUECHANGED) {
    if (msg.pSender == slide_player_) {
      double val = slide_player_->GetValue() / 1000.0;
      player_.seek(val);
      player_.resume();
      UpdateUI();
    }
    else if (msg.pSender == slide_vol_) {
      audio_vol.SetCurrentVolume(slide_vol_->GetValue());
    }
  }
  __super::Notify(msg);
}

void QPlayer::OpenFile(LPCTSTR szFile) {
  USES_CONVERSION;
  CloseFile();
  //player_.OpenFile(T2A(szFile));
  player_.set_event(this);
  player_.Open(T2A(szFile));
  UpdateUI();
}

void QPlayer::CloseFile()
{
  if (player_.isOpen()) {
    player_.Close();
    video_wnd_.Clear();
    audio_player.Stop();
  }
}

void QPlayer::FullScreen(bool bFull)
{
  DuiLib::CControlUI* pbtnFull = m_pm.FindControl(_T("btnScreenFull"));
  DuiLib::CControlUI* pbtnNormal = m_pm.FindControl(_T("btnScreenNormal"));
  DuiLib::CControlUI* pUICaption = m_pm.FindControl(_T("ctnCaption"));
  int iBorderX = GetSystemMetrics(SM_CXFIXEDFRAME) + GetSystemMetrics(SM_CXBORDER);
  int iBorderY = GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYBORDER);
  static LONG oldStyle;  //记录下原窗口style
  static RECT rc;        //记录下还原时的位置
  if (pbtnFull && pbtnNormal && pUICaption)
  {
    m_bFullScreenMode = bFull;

    if (bFull)
    {
      GetWindowRect(m_hWnd, &rc);
      oldStyle = SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP);
      int w = GetSystemMetrics(SM_CXSCREEN);
      int h = GetSystemMetrics(SM_CYSCREEN);
      SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, w, h, SWP_SHOWWINDOW);
      /*
      ::GetWindowPlacement(*this, &m_OldWndPlacement);
      if (::IsZoomed(*this))
        ::ShowWindow(*this, SW_SHOWDEFAULT);
      ::SetWindowPos(*this, HWND_TOPMOST, -iBorderX, -iBorderY, GetSystemMetrics(SM_CXSCREEN) + 2 * iBorderX, GetSystemMetrics(SM_CYSCREEN) + 2 * iBorderY, 0);
      */
    }
    else
    {
      SetWindowLong(m_hWnd, GWL_STYLE, oldStyle);
      //还原后，使用参数HWND_NOTOPMOST，取消窗口置顶，否则摄像头选择等窗口看不见
      SetWindowPos(m_hWnd, HWND_NOTOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
      /*
      ::SetWindowPlacement(*this, &m_OldWndPlacement);
      ::SetWindowPos(*this, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
      */
    }

    pbtnNormal->SetVisible(bFull);
    pUICaption->SetVisible(!bFull);
    pbtnFull->SetVisible(!bFull);
  }
}

void QPlayer::UpdateUI() {
  if (btnStop) {
    btnStop->SetEnabled(player_.isOpen());
  }
  if (player_.isOpen()) {
    if (player_.duration() && lbStatus) {
      slide_player_->SetMaxValue(1000 * player_.duration());
    }
  }
  else {
    if (lbStatus) {
      lbStatus->SetText(_T(""));
    }
  }

  bool paused = player_.paused();
  if (paused){
    KillTimer(GetHWND(), UM_PROGRESS);
    btnPlay->SetVisible(true);
    btnPause->SetVisible(false);
  }
  else{
    SetTimer(GetHWND(), UM_PROGRESS, 500, NULL);
    btnPlay->SetVisible(false);
    btnPause->SetVisible(true);
  }
}

void QPlayer::OnProgress(float cur, float duration)
{
  if (lbStatus) {
    CDuiString  strTime;
    struct tm   tmTotal, tmCurrent;
    time_t      timeTotal = cur;
    time_t      timeCurrent = duration;
    TCHAR       szTotal[MAX_PATH], szCurrent[MAX_PATH];

    gmtime_s(&tmTotal, &timeTotal);
    gmtime_s(&tmCurrent, &timeCurrent);
    _tcsftime(szTotal, MAX_PATH, _T("%X"), &tmTotal);
    _tcsftime(szCurrent, MAX_PATH, _T("%X"), &tmCurrent);
    strTime.Format(_T("%s / %s"), szCurrent, szTotal);

    lbStatus->SetText(strTime);
  }

  slide_player_->SetValue(cur * 1000);
  if (duration > 0)
    slide_player_->SetMaxValue(duration*1000);
}

void QPlayer::OnOpen(const char* url)
{
  //video_wnd_.SetText(url, RGB(255,255,255));
  UpdateUI();
}

void QPlayer::OnClose(int conn)
{
  //video_wnd_.SetText("连接失败", RGB(255, 0, 0));
  UpdateUI();
}

void QPlayer::OnStat(int jitter, int speed)
{
  char szStat[MAX_PATH];
  sprintf_s(szStat, "Jitter %d ms, Speed %5.2f KB/s", jitter, speed / 1024.0);
  video_wnd_.SetText(szStat, RGB(255, 255, 255));
}

#include <shellapi.h>
LRESULT QPlayer::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  switch (uMsg)
  {
  case WM_TIMER:
    if (wParam == UM_PROGRESS) {
      OnProgress(player_.position(), player_.duration());
    }
    break;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE && m_bFullScreenMode)
      FullScreen(false);
    break;
  case WM_LBUTTONDBLCLK:
    FullScreen(!m_bFullScreenMode);
    break;
  case WM_DROPFILES:
  {
    TCHAR szFileName[MAX_PATH];
    UINT nFileCnt = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0);

    //当你想获取第nFile-1个(以0为起始索引)文件时.
    if (DragQueryFile((HDROP)wParam, nFileCnt - 1, szFileName, MAX_PATH))
    {
      //处理文件方法..自己喜欢怎么干就怎么干吧
      OpenFile(szFileName);
    }
    DragFinish((HDROP)wParam);
    break;
  }
  default:
    break;
  }
  return __super::HandleCustomMessage(uMsg, wParam, lParam, bHandled);
}

// for audio play
void PcmFillFunc(BYTE *stream, int len, void *userdata) {
  FFPlayer *is = (FFPlayer*)userdata;
  if (is)
    is->getAudioFrame(stream, len);
}
bool QPlayer::onAudioStream(int steam_id, int codec, int samplerate, int channel)
{
  audio_player.Create(channel, samplerate, SDL_AUDIO_BUFFER_SIZE);
  audio_player.SetPcmCallback(&PcmFillFunc, &player_);
  audio_player.Start();
  return true;
}

bool QPlayer::onVideoStream(int steam_id, int codec, int width, int height)
{
  return true;
}

void QPlayer::onVideoFrame(VideoPicture* vp)
{
  BYTE* p = video_wnd_.GetBuffer(vp->width, vp->height);
  if (p) {
    memcpy(p, vp->bmp, vp->width * vp->height * 3);
    video_wnd_.ReleaseBuffer();
  }
  //video_wnd_.Draw(vp->bmp, vp->width, vp->height);
}

bool QPlayer::seek(double incr)
{
  if (player_.isOpen()) {
    player_.seek(player_.position() + incr);
    return true;
  }
  return false;
}
