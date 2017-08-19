#pragma once
#include "Duilib/UIlib.h"
#include "Utils/WinImplBase.h"
#include "VideoWnd.h"
#include "FFPlayer.h"
#include "audio/AudioPlay.h"
#include "audio/VolumeCtrl.h"

using DuiLib::CDuiString;

class QPlayer : public FFEvent,
  public DuiLib::WindowImplBase
{
public:
  QPlayer(bool bTop=false);
  virtual ~QPlayer();

  void OpenFile(LPCTSTR path);
  void CloseFile();
  static QPlayer* Instance();
protected:
  void UpdateUI();
  void Mute(bool mute);
  void FullScreen(bool bFull);
  bool m_bFullScreenMode;
  WINDOWPLACEMENT m_OldWndPlacement;  // 保存窗口原来的位置

  bool top_window_;
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

  // 视频控件
  CVideoWnd video_wnd_;

  DuiLib::CControlUI* CreateControl(LPCTSTR pstrClass);
  virtual CDuiString GetSkinFolder() override;
  virtual CDuiString GetSkinFile() override;
  virtual LPCTSTR GetWindowClassName(void) const override;

  virtual void InitWindow() override;
  virtual void OnFinalMessage(HWND hWnd) override;
  virtual void Notify(DuiLib::TNotifyUI& msg) override;
  virtual LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

  virtual void OnProgress(uint32_t cur, uint32_t duration);
  virtual void OnStat(int jitter, int speed);
  virtual void OnOpen(const char* url);
  virtual void OnClose(int conn);

  virtual bool onAudioStream(int steam_id, int codec, int samplerate, int channel);
  virtual bool onVideoStream(int steam_id, int codec, int width, int height);
  virtual bool onVideoFrame(VideoPicture* vp);
  bool seek(double incr);
  CAudioPlay audio_player;
  CVolumeCtrl audio_volctrl;
};

