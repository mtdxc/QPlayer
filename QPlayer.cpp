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

void QPlayer::InitWindow()
{
  ::GetWindowPlacement(*this, &m_OldWndPlacement);

  slide_player_ = (DuiLib::CSliderUI*)m_PaintManager.FindControl(_T("sliderPlay"));
  slide_vol_ = (DuiLib::CSliderUI*)m_PaintManager.FindControl(_T("sliderVol"));
  if (slide_vol_) {
    slide_vol_->SetMinValue(0);
    slide_vol_->SetMaxValue(255);
    //slide_vol_->SetValue(player_.volume());
  }
  lbStatus = (DuiLib::CSliderUI*)m_PaintManager.FindControl(_T("lbStatus"));
  btnOpen = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnOpen"));
  btnStop = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnStop"));
  btnPause = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnPause"));
  btnPlay = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnPlay"));
  player_.set_event(this);
}

void QPlayer::OnFinalMessage(HWND hWnd)
{
  player_.Close();
  if (top_window_)
    ::PostMessage(NULL, WM_QUIT, 0, 0);
  delete this;
}

void QPlayer::Mute(bool mute) {
  DuiLib::CControlUI* btnMute = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnVolumeZero"));
  DuiLib::CControlUI* btnUnMute = (DuiLib::CControlUI*)m_PaintManager.FindControl(_T("btnVolume"));
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
      if (DuiLib::CControlUI* pUI = m_PaintManager.FindControl(_T("edUrl"))) {
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
      if (player_.isOpen()) {
        player_.Close();
        video_wnd_.Clear();
        //KillTimer(GetHWND(), UM_RENDER);
      }
      UpdateUI();
    }
    else if (msg.pSender->GetName() == _T("btnPause")) {
      player_.pause();
      btnPlay->SetVisible(true);
      btnPause->SetVisible(false);
    }
    else if (msg.pSender->GetName() == _T("btnPlay")) {
      player_.resume();
      btnPlay->SetVisible(false);
      btnPause->SetVisible(true);
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
      double pos = player_.get_master_clock();
      double val = slide_player_->GetValue() / 1000.0;
      player_.seek(val, pos - val);
      player_.resume();
    }
    else if (msg.pSender == slide_vol_) {
      //player_.set_volume(slide_vol_->GetValue());
    }
  }
  __super::Notify(msg);
}

void QPlayer::OpenFile(LPCTSTR szFile) {
  USES_CONVERSION;
  //player_.OpenFile(T2A(szFile));
  audio_player.CloseOutput();
  player_.Close();
  player_.set_event(this);
  player_.Open(T2A(szFile));
  UpdateUI();
}

void QPlayer::FullScreen(bool bFull)
{
  DuiLib::CControlUI* pbtnFull = m_PaintManager.FindControl(_T("btnScreenFull"));
  DuiLib::CControlUI* pbtnNormal = m_PaintManager.FindControl(_T("btnScreenNormal"));
  DuiLib::CControlUI* pUICaption = m_PaintManager.FindControl(_T("ctnCaption"));
  int iBorderX = GetSystemMetrics(SM_CXFIXEDFRAME) + GetSystemMetrics(SM_CXBORDER);
  int iBorderY = GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYBORDER);

  if (pbtnFull && pbtnNormal && pUICaption)
  {
    m_bFullScreenMode = bFull;

    if (bFull)
    {
      ::GetWindowPlacement(*this, &m_OldWndPlacement);

      if (::IsZoomed(*this))
      {
        ::ShowWindow(*this, SW_SHOWDEFAULT);
      }

      ::SetWindowPos(*this, HWND_TOPMOST, -iBorderX, -iBorderY, GetSystemMetrics(SM_CXSCREEN) + 2 * iBorderX, GetSystemMetrics(SM_CYSCREEN) + 2 * iBorderY, 0);
    }
    else
    {
      ::SetWindowPlacement(*this, &m_OldWndPlacement);
      ::SetWindowPos(*this, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }

    pbtnNormal->SetVisible(bFull);
    pUICaption->SetVisible(!bFull);
    pbtnFull->SetVisible(!bFull);
  }
}

void QPlayer::UpdateUI() {
  if (player_.isOpen()) {
    if (player_.duration() && lbStatus) {
      slide_player_->SetMaxValue(player_.duration());
    }
    SetTimer(GetHWND(), UM_PROGRESS, 500, NULL);
  }
  else {
    if (lbStatus) {
      lbStatus->SetText(_T(""));
    }
    KillTimer(GetHWND(), UM_PROGRESS);
  }
  if (btnStop) {
    btnStop->SetEnabled(player_.isOpen());
  }
}

void QPlayer::OnProgress(uint32_t cur, uint32_t duration)
{
  if (lbStatus) {
    CDuiString  strTime;
    struct tm   tmTotal, tmCurrent;
    time_t      timeTotal = cur / 1000;
    time_t      timeCurrent = duration / 1000;
    TCHAR       szTotal[MAX_PATH], szCurrent[MAX_PATH];

    gmtime_s(&tmTotal, &timeTotal);
    gmtime_s(&tmCurrent, &timeCurrent);
    _tcsftime(szTotal, MAX_PATH, _T("%X"), &tmTotal);
    _tcsftime(szCurrent, MAX_PATH, _T("%X"), &tmCurrent);
    strTime.Format(_T("%s / %s"), szCurrent, szTotal);

    lbStatus->SetText(strTime);
  }

  slide_player_->SetValue(cur);
  if (duration > 0)
    slide_player_->SetMaxValue(duration);
}

void QPlayer::OnOpen(const char* url)
{
  //video_wnd_.SetText(url, RGB(255,255,255));
}

void QPlayer::OnClose(int conn)
{
  //video_wnd_.SetText("连接失败", RGB(255, 0, 0));
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
  audio_player.WaveInitFormat(channel, samplerate, 16);
  audio_player.m_NbMaxSamples = SDL_AUDIO_BUFFER_SIZE;
  audio_player.SetCallBack(&PcmFillFunc, &player_);
  audio_player.OpenOutput();
  return true;
}

bool QPlayer::onVideoStream(int steam_id, int codec, int width, int height)
{
  return true;
}

bool QPlayer::onVideoFrame(VideoPicture* vp)
{
  BYTE* p = video_wnd_.GetBuffer(vp->width, vp->height);
  if (p) {
    memcpy(p, vp->bmp, vp->width * vp->height * 3);
    video_wnd_.ReleaseBuffer();
  }
  //video_wnd_.Draw(vp->bmp, vp->width, vp->height);
  return true;
}

bool QPlayer::seek(double incr)
{
  if (player_.isOpen()) {
    double pos = player_.get_master_clock();
    pos += incr;
    player_.seek(pos, incr);
    return true;
  }
  return false;
}
