#pragma once
#include "Duilib/UIlib.h"
#include "Utils/WinImplBase.h"
#include "VideoWnd.h"
#include "FFPlayer.h"
#include "audio/AudioPlay.h"
#include "audio/VolumeCtrl.h"
#include "audio/VolumeOutWave.h"
#include "dlna/UpnpServer.h"
#include "UICombox.h"
#include <functional>
using DuiLib::CDuiString;

class QPlayer : public FFEvent, public UpnpListener,
  public DuiLib::WindowImplBase
{
public:
  QPlayer(bool bTop = false);
  virtual ~QPlayer();

  void OpenFile(const wchar_t* path);
  void OpenFile(const char* path);
  void CloseFile();
  void Mute(bool mute);
  void Pause(bool val);
  void seek(double incr);
  void setVolume(int val);
  float speed();
  void Stop();

  static QPlayer* Instance();

protected:
  void callInUI(std::function<void()> func);
  void UpdateUI();
  void FullScreen(bool bFull);


  bool m_bFullScreenMode;
  WINDOWPLACEMENT m_OldWndPlacement;  // 保存窗口原来的位置

  bool top_window_;
  bool paused_;
  std::string cur_file_;
  std::string cur_dev_;
  void SetCurDev(const std::string& dev);

  // 播放器
  FFPlayer player_;
  // 进度条
  DuiLib::CSliderUI* slide_player_;
  // 音量条
  DuiLib::CSliderUI* slide_vol_;
  // 进度文字
  DuiLib::CControlUI* lbStatus;

  DuiLib::CControlUI* btnOpen;
  DuiLib::CControlUI* btnStop;
  DuiLib::CControlUI* btnPause;
  DuiLib::CControlUI* btnPlay;
  DuiLib::CControlUI* edUrl;
  DuiLib::CControlUI* edRate;
  UICombox* lstCamera;
  DuiLib::CComboUI* lstProfile;
  DuiLib::CControlUI* edUser;
  DuiLib::CControlUI* edPwd;

  // 视频控件
  CVideoWnd video_wnd_;

  DuiLib::CControlUI* CreateControl(LPCTSTR pstrClass);
  virtual CDuiString GetSkinFolder() override;
  virtual CDuiString GetSkinFile() override;
  virtual LPCTSTR GetWindowClassName(void) const override;
  virtual LPCTSTR GetResourceID() const;
  virtual void InitWindow() override;

  virtual void OnFinalMessage(HWND hWnd) override;
  virtual void Notify(DuiLib::TNotifyUI& msg) override;

  virtual LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

  virtual void OnProgress(float cur, float duration);
  virtual void OnStat(int jitter, int speed);
  virtual void OnOpen(const char* url);

  virtual void onClose(int conn);
  virtual void onSeekDone(float pos, int code);
  virtual bool onAudioStream(int steam_id, int codec, int samplerate, int channel);
  virtual bool onVideoStream(int steam_id, int codec, int width, int height);
  virtual void onVideoFrame(VideoPicture* vp);
  void upnpPropChanged(const char* id, const char* name, const char* vaule) override;
  void upnpSearchChangeWithResults(const MapDevices& devs) override;
  void RefreshUpnpDevices();
  virtual void onvifSearchChangeWithResults(const OnvifMap& devs) override;
  void RefreshOnvifDevices();
  void selectCamera(const std::string &id);
  void updateProfiles();
  void UpdateVol();

  HMENU hMenu;
  OnvifPtr camera_;
  CAudioPlay audio_player;
  CVolumeOutWave audio_vol;
};

