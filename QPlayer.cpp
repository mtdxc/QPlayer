#include "QPlayer.h"
#include "UIWnd.h"
#include <atlbase.h>
#include <CommDlg.h>
#include "Ini.h"
#include "resource.h"

#define UM_PROGRESS 101
#define UM_CALLUI 1000
#define IDM_DEVICE_START 1001
#define IDM_DEVICE_END 1010

#define IDM_ONVIF_START 1011
#define IDM_ONVIF_END 1020
#define IDC_CAMERA 10030
#define IDC_PROFILE 10031
#define STACK_ARRAY(TYPE, LEN) \
  static_cast<TYPE*>(::alloca((LEN) * sizeof(TYPE)))
namespace rtc {
  inline std::string ToUtf8(const wchar_t* wide, size_t len = -1) {
    int len8 = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len),
      nullptr, 0, nullptr, nullptr);
    char* ns = STACK_ARRAY(char, len8);
    ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len), ns, len8,
      nullptr, nullptr);
    return std::string(ns, len8);
  }

  inline std::wstring ToUtf16(const char* utf8, size_t len) {
    int len16 = ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len),
      NULL, 0);
    wchar_t* ws = STACK_ARRAY(wchar_t, len16);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len), ws, len16);
    return std::wstring(ws, len16);
  }
  inline std::wstring ToUtf16(const std::string& str) {
    return ToUtf16(str.data(), str.length());
  }
}

static QPlayer* gPlayer = NULL;
QPlayer* QPlayer::Instance()
{
  return gPlayer;
}

QPlayer::QPlayer(bool top) :top_window_(top)
{
  gPlayer = this;
  m_bFullScreenMode = false;
  m_ResourceType = DuiLib::UILIB_ZIPRESOURCE;
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
  else if (!_tcscmp(pstrClass, _T("Combox"))){
    auto lstCamera = new UICombox();
    lstCamera->Create(CBS_DROPDOWN | WS_VISIBLE, RECT(), GetHWND(), IDC_CAMERA);
    return lstCamera;
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

LPCTSTR QPlayer::GetResourceID() const
{
  return MAKEINTRESOURCE(IDR_ZIPRES_SKIN);
}

void CALLBACK OutVolumeChanged(DWORD dwCurrentVolume, DWORD dwUserValue)
{
  DuiLib::CSliderUI* p = (DuiLib::CSliderUI*)dwUserValue;
  p->SetValue(dwCurrentVolume);
}

void QPlayer::InitWindow()
{
  ::GetWindowPlacement(*this, &m_OldWndPlacement);
  Upnp::Instance()->start();
  Upnp::Instance()->addListener(this);
  slide_player_ = (DuiLib::CSliderUI*)m_pm.FindControl(_T("sliderPlay"));
  slide_vol_ = (DuiLib::CSliderUI*)m_pm.FindControl(_T("sliderVol"));
  UpdateVol();

  lbStatus = (DuiLib::CSliderUI*)m_pm.FindControl(_T("lbStatus"));
  btnOpen = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnOpen"));
  btnStop = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnStop"));
  btnPause = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnPause"));
  btnPlay = (DuiLib::CControlUI*)m_pm.FindControl(_T("btnPlay"));
  lstCamera = (UICombox*)m_pm.FindControl(_T("listCamera"));
  lstProfile = (DuiLib::CComboUI*)m_pm.FindControl(_T("listProfile"));
  if (edUser = (DuiLib::CControlUI*)m_pm.FindControl(_T("edUser"))) {
    edUser->SetText(rtc::ToUtf16(GetIniStr("App", "User", "")).c_str());
  }
  if (edPwd = (DuiLib::CControlUI*)m_pm.FindControl(_T("edPassword"))){
    edPwd->SetText(rtc::ToUtf16(GetIniStr("App", "Pwd", "")).c_str());
  }

  if (edUrl = FindControl(_T("edUrl"))) {
    edUrl->SetText(rtc::ToUtf16(GetIniStr("App", "Url", "http://janus.97kid.com/264.flv")).c_str());
    // auto url = Upnp::Instance()->setWindowId(long(m_hWnd));
    // edUrl->SetText(rtc::ToUtf16(url).c_str());
  }
  if (edRate = FindControl(_T("edRate"))) {
    edRate->SetText(_T("1.0"));
  }

  player_.set_event(this);
}

void QPlayer::UpdateVol()
{
  if (!slide_vol_)
    return;
  if (cur_dev_.empty()) {
    slide_vol_->SetMinValue(audio_vol.GetMinimalVolume());
    slide_vol_->SetMaxValue(audio_vol.GetMaximalVolume());
    slide_vol_->SetValue(audio_vol.GetCurrentVolume());
    audio_vol.RegisterNotificationSink(OutVolumeChanged, (DWORD)slide_vol_);
  }
  else {
    audio_vol.RegisterNotificationSink(NULL, 0);
    slide_vol_->SetMinValue(0);
    slide_vol_->SetMaxValue(100);
    Upnp::Instance()->getVolume(cur_dev_.c_str(), [this](int code, int val) {
      slide_vol_->SetValue(val);
    });
  }
}

void QPlayer::OnFinalMessage(HWND hWnd)
{
  CloseFile();
  Upnp::Instance()->delListener(this);
  Upnp::Instance()->stop();
  if (top_window_)
    ::PostMessage(NULL, WM_QUIT, 0, 0);
  __super::OnFinalMessage(hWnd);
  //delete this;
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
      Stop();
    }
    else if (msg.pSender->GetName() == _T("btnPause")) {
      Pause(true);
    }
    else if (msg.pSender->GetName() == _T("btnPlay")) {
      Pause(false);
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
      seek(slide_player_->GetValue() / 1000.0 - 30);
    }
    else if (msg.pSender->GetName() == _T("btnFastForward")) {
      seek(slide_player_->GetValue() / 1000.0 + 30);
    }
    else if (msg.pSender->GetName() == _T("btnShare")) {
      Upnp::Instance()->search();
      hMenu = CreatePopupMenu();
      AppendMenu(hMenu, MF_STRING, IDM_DEVICE_START, _T("本地"));
      if (cur_dev_.empty()) {
        CheckMenuItem(hMenu, IDM_DEVICE_START, MF_CHECKED);
      }
      RECT pos = msg.pSender->GetPos();
      POINT pt = { pos.left, pos.bottom };
      ClientToScreen(GetHWND(), &pt);
      TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, GetHWND(), NULL);
      //RefreshUpnpDevices();
    }
  }
  else if (msg.pSender->GetName() == _T("btnSearchCamera")) {
    // Upnp::Instance()->sendProbe("tds:Device");
    auto upnp = Upnp::Instance();
    auto user = edUser->GetText().GetStringA();
    auto pwd = edPwd->GetText().GetStringA();
    SetIniStr("App", "User", user.c_str());
    SetIniStr("App", "Pwd", pwd.c_str());
    upnp->setOnvifPwd(user, pwd);
    upnp->sendProbe("dn:NetworkVideoTransmitter");
  }
  else if (msg.pSender->GetName() == _T("btnAddCamera")) {
    std::wstring buff;
    buff.resize(256);
    lstCamera->GetWindowText(&buff[0], buff.size());
		if (buff.empty()) return;
    std::string ip = rtc::ToUtf8(buff.data(), buff.size());
    if (!Upnp::Instance()->getOnvif(ip.c_str())){
      char url[256];
      sprintf(url, "http://%s/onvif/device_service", ip.c_str());
      Upnp::Instance()->addOnvif(url);
    }
  }
  else if (msg.pSender->GetName() == _T("btnPlayCamera")) {
    if (camera_){
      auto profile = lstProfile->GetText();
      camera_->GetStreamUri(profile.GetStringA(), [this](int code, std::string resp){
        if (code) return;
        callInUI([this, resp]{
          edUrl->SetText(rtc::ToUtf16(resp).c_str());
          OpenFile(resp.c_str());
        });
      });
    }
  }
  else if (msg.sType == DUI_MSGTYPE_ITEMSELECT){
    if (msg.pSender == lstCamera) {
      int sel = lstCamera->GetCurSel();
      if (sel == -1) return;
      /*
      auto p = lstCamera->GetItemAt(sel);
      std::string id = p->p->GetName().GetStringA();
      selectCamera(id);
      */
    }
  }
  else if (msg.sType == DUI_MSGTYPE_VALUECHANGED) {
    if (msg.pSender == slide_player_) {
      double val = slide_player_->GetValue() / 1000.0;
      seek(val);
    }
    else if (msg.pSender == slide_vol_) {
      setVolume(slide_vol_->GetValue());
    }
  }
  __super::Notify(msg);
}

void QPlayer::selectCamera(const std::string &id)
{
  lstProfile->RemoveAll();
  camera_ = Upnp::Instance()->getOnvif(id.c_str());
  if (nullptr != camera_) {
    camera_->GetProfiles(true, [this](int, std::string){
      callInUI([this]{
        updateProfiles();
      });
    });
  }
}

void QPlayer::Stop()
{
  CloseFile();
  video_wnd_.SetText("", RGB(255, 255, 255));
  slide_player_->SetValue(0);
  UpdateUI();
}

void QPlayer::OpenFile(const wchar_t* szFile) {
  USES_CONVERSION;
  OpenFile(T2A(szFile));
}

void QPlayer::OpenFile(const char* szFile) {
  CloseFile();
  cur_file_ = szFile;
  if (cur_dev_.length()) {
    //player_.OpenFile(T2A(szFile));
    video_wnd_.SetText("投屏中", RGB(255, 255, 255));
    std::string url = cur_file_;
    if (-1 == cur_file_.find("://")) {
      url = Upnp::Instance()->setCurFile(cur_file_.c_str());
    }
    Upnp::Instance()->openUrl(cur_dev_.c_str(), url.c_str());
  }
  else {
    //player_.OpenFile(T2A(szFile));
    video_wnd_.SetText("", RGB(255, 0, 0));
    player_.Open(cur_file_.c_str());
  }
  paused_ = false;
  UpdateUI();
}

void QPlayer::CloseFile()
{
  cur_file_.clear();
  if (cur_dev_.length()) {
    Upnp::Instance()->close(cur_dev_.c_str());
  }
  else if (player_.opened()) {
    player_.Close();
    video_wnd_.Clear();
    audio_player.Stop();
    audio_player.Destroy();
  }
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

void QPlayer::Pause(bool val)
{
  if (val) {
    if (cur_dev_.empty())
      player_.pause();
    else
      Upnp::Instance()->pause(cur_dev_.c_str());
  }
  else {
    if (cur_dev_.empty())
      player_.resume();
    else
      Upnp::Instance()->resume(cur_dev_.c_str());
  }
  paused_ = val;
  UpdateUI();
}

void QPlayer::seek(double pos)
{
  if (cur_dev_.empty())
    player_.seek(pos);
  else
    Upnp::Instance()->seek(cur_dev_.c_str(), pos);
}

void QPlayer::setVolume(int val)
{
  if (cur_dev_.empty()) {
    audio_vol.SetCurrentVolume(val);
  }
  else {
    Upnp::Instance()->setVolume(cur_dev_.c_str(), val);
  }
}

float QPlayer::speed()
{
  if (cur_dev_.empty())
    return 1.0f;
  else
    return Upnp::Instance()->getSpeed(cur_dev_.c_str());
}

void QPlayer::SetCurDev(const std::string& dev)
{
  USES_CONVERSION;
  if (cur_dev_ == dev)
    return;
  std::string file = cur_file_;
  if (!cur_file_.empty()) {
    CloseFile();
    cur_dev_ = dev;
    OpenFile(A2T(file.c_str()));
  }
  else {
    cur_dev_ = dev;
  }
  UpdateVol();
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

void QPlayer::callInUI(std::function<void()> func) {
  PostMessage(UM_CALLUI, (WPARAM) new std::function<void()>(func), 0);
}

void QPlayer::UpdateUI() {
  bool opened = cur_file_.length();
  if (btnStop) {
    btnStop->SetEnabled(opened);
  }

  if (opened && !paused_) {
    SetTimer(GetHWND(), UM_PROGRESS, 500, NULL);
  } else {
    KillTimer(GetHWND(), UM_PROGRESS);
  }
  btnPlay->SetVisible(paused_);
  btnPause->SetVisible(!paused_);
  if (edRate) {
    edRate->SetEnabled(opened);
    TCHAR buf[20] = { 0 };
    _stprintf(buf, _T("%.1f"), speed());
    edRate->SetText(buf);
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
    slide_player_->SetMaxValue(duration * 1000);
}

void QPlayer::OnOpen(const char* url)
{
  //video_wnd_.SetText(url, RGB(255,255,255));
  UpdateUI();
}

void QPlayer::onClose(int conn)
{
  char msg[32];
  sprintf(msg, "关闭%d", conn);
  video_wnd_.SetText(msg, RGB(255, 0, 0));
  UpdateUI();
}

void QPlayer::onSeekDone(float pos, int code)
{
  //TRACE("onSeekDone %f, %d", pos, code);
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
      if (cur_dev_.empty())
        OnProgress(player_.position(), player_.duration());
      else {
        Upnp::Instance()->getPosition(cur_dev_.c_str(), [this](int code, AVPositionInfo pos) {
          if (code) return;
          OnProgress(pos.absTime, pos.trackDuration);
        });
      }
    }
    break;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE && m_bFullScreenMode)
      FullScreen(false);
    break;
  case WM_LBUTTONDBLCLK:
    //FullScreen(!m_bFullScreenMode);
    break;
  case UM_CALLUI:
    if (wParam) {
      auto func = (std::function<void()>*)wParam;
      (*func)();
      delete func;
    }
    break;
  case WM_COMMAND:
  {
    USHORT wmId = LOWORD(wParam);
    USHORT wmEvent = HIWORD(wParam);
    if (wmId == IDM_DEVICE_START) {
      SetCurDev("");
      hMenu = NULL;
      KillTimer(GetHWND(), UM_PROGRESS);
    }
    else if (wmId > IDM_DEVICE_START && wmId < IDM_DEVICE_END)
    {
      auto devs = Upnp::Instance()->getDevices();
      int idx = wmId - IDM_DEVICE_START - 1;
      if (idx < devs.size()) {
        for (auto it : devs)
        {
          if (0 == idx) {
            SetCurDev(it.first.c_str());
            break;
          }
          idx--;
        }
        hMenu = NULL;
      }
    }
    else if (wmId >= IDM_ONVIF_START && wmId < IDM_ONVIF_END) {
      auto devs = Upnp::Instance()->getOnvifs();
      int idx = wmId - IDM_ONVIF_START - 1;
      if (idx < devs.size()) {
        for (auto it : devs) {
          if (0 == idx) {
            // 选中一项
            break;
          }
          idx--;
        }
      }
    }
    else if (wmId == IDC_CAMERA) {
      // process with combox select changes
      // lParam: Handle to the combo box. 
      switch (wmEvent) {
      case CBN_SELCHANGE:
        if (lstCamera){
          int sel = lstCamera->GetCurSel();
          auto ptr = (OnvifDevice*)lstCamera->GetItemData(sel);
          Output("select changed %s", ptr->name.c_str());
          selectCamera(ptr->uuid);
        }
        break;
      case CBN_EDITCHANGE:
        break;
      default:
        break;
      }
    }
    break;
  }
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

void QPlayer::upnpSearchChangeWithResults(const MapDevices& devs)
{
  callInUI([this](){
    RefreshUpnpDevices();
  });
}

void QPlayer::onvifSearchChangeWithResults(const OnvifMap& devs)
{
  callInUI([this](){
    RefreshOnvifDevices();
  });
}

void QPlayer::RefreshOnvifDevices()
{
  auto devs = Upnp::Instance()->getOnvifs();
  if (devs.empty() || !lstCamera) return;
#if 0
  lstCamera->RemoveAll();
  for (auto it : devs) {
    auto dev = it.second;
    auto child = new DuiLib::CListLabelElementUI();
    child->SetText(rtc::ToUtf16(dev->name).c_str());
    child->SetName(rtc::ToUtf16(dev->uuid).c_str());
    lstCamera->Add(child);
  }
  lstCamera->SelectItem(0);
#else
  lstCamera->ResetContent();
  for (auto it : devs) {
    auto dev = it.second;
    int n = lstCamera->AddString(rtc::ToUtf16(dev->name).c_str());
    lstCamera->SetItemDataPtr(n, dev.get());
  }
  lstCamera->SetCurSel(0);
#endif
}

void QPlayer::updateProfiles()
{
  if (!lstProfile) return;
  lstProfile->RemoveAll();
  Output("update %s with %d profiles", camera_->name.c_str(), camera_->profiles.size());
  if (!camera_ || camera_->profiles.empty()) return;
  for (auto it : camera_->profiles)
  {
    auto child = new DuiLib::CListLabelElementUI();
    child->SetText(rtc::ToUtf16(it.first).c_str());
    child->SetName(rtc::ToUtf16(it.first).c_str());
    lstProfile->Add(child);
  }
  lstProfile->SelectItem(0);
}

void QPlayer::RefreshUpnpDevices()
{
  auto devs = Upnp::Instance()->getDevices();
  if (hMenu == NULL) return;
  // LOG(INFO) << "refresh with " << devs.size() << " Devices";
  int count = GetMenuItemCount(hMenu);
  UINT i = 1;
  for (auto it = devs.begin(); it != devs.end(); it++, i++)
  {
    Device::Ptr dev = it->second;
    UINT_PTR id = IDM_DEVICE_START + i;
    if (i < count) {
      ModifyMenu(hMenu, i, MF_BYPOSITION, id, rtc::ToUtf16(dev->friendlyName).c_str());
    }
    else
      AppendMenu(hMenu, MF_STRING, id, rtc::ToUtf16(dev->friendlyName).c_str());
    CheckMenuItem(hMenu, id, cur_dev_ == dev->uuid ? MF_CHECKED : MF_UNCHECKED);
    id++;
  }
  while (i < count) {
    DeleteMenu(hMenu, i, MF_BYPOSITION);
    i++;
  }
}

void QPlayer::upnpPropChanged(const char* id, const char* name, const char* value)
{
  if (!stricmp(name, "TransportState")){
    if (!stricmp(value, "STOPPED")){
      callInUI([this](){
        Stop();
      });
    }
  }
}
