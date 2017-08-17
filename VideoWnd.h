#if !defined(AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_)
#define AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// VideoWnd.h : header file
//
#include <mutex>
typedef HANDLE HDRAWDIB;
//Video buffer class
class CVideoBuffer
{
public:
  CVideoBuffer();
  ~CVideoBuffer();

  BYTE *GetBuffer(int w = 0, int h = 0);
  void ReleaseBuffer();
  void CleanUpBuffer();
  int stride() const { return (m_w * 3 + 3) / 4 * 4; }
  int width() const { return m_w; }
  int height()const { return m_h; }
protected:
  void *m_buf;
  int m_w;
  int m_h;
  std::mutex m_mutex;
};

///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

class Win32Window {
public:
  Win32Window();
  virtual ~Win32Window();

  HWND handle() const { return wnd_; }

  bool Create(HWND parent, const wchar_t* title, DWORD style, DWORD exstyle,
    int x, int y, int cx, int cy);
  void Destroy();

  // Call this when your DLL unloads.
  static void Shutdown();

protected:
  virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
    LRESULT& result);

  virtual bool OnClose() { return true; }
  virtual void OnNcDestroy() { }

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);

  HWND wnd_;
  static HINSTANCE instance_;
  static ATOM window_class_;
};
/**
CVideoWnd window
由于CImage的Bits指向RGB数据底部，因此CImage调用方式
CImage img("videoback.jpg");
pWnd->FillBuffer((BYTE*)img.GetPixelAddress(0, img.GetHeight()-1), img.GetWidth(), img.GetHeight());
*/
class CVideoWnd : public Win32Window
{
  // Construction
public:
  CVideoWnd();
  virtual ~CVideoWnd();

  static bool SetDefaultImage(const char* path);

  // implement from IVideoWnd
  BYTE* GetBuffer(int w, int h);
  BOOL ReleaseBuffer();

  void Clear();
  bool SetVisible(bool bShow);
  bool GetVisible();
  /*
  填充RGB.
  @arg pRgb 原始RGB数据
  @arg width 原始宽度
  @arg height 原始高度
  @arg orgin 旋转标记,为如下
  - 0 不旋转
  - 1 90度
  - 2 180度
  - 3 270度
  @return BOOL
  @retval
  */
  BOOL FillBuffer(BYTE* pRgb, int width, int height, int orgin);
  BOOL FillBuffer(BYTE* pRgb, int width, int height) {
    return FillBuffer(pRgb, width, height, m_nDefOrgin);
  }
  // 设置缺省旋转方向
  void SetDefOrgin(int orgin) { m_nDefOrgin = orgin; }

  // 设置窗口适应模式
  void SetFitMode(BOOL bFit);
  void SetBkColor(COLORREF color);
  BOOL TaskSnap(LPCTSTR path);
  /*
  设置文本.
  @arg text 内容
  @arg clr 颜色
  @arg mode 绘制模式
  */
  void SetText(const char* text, COLORREF clr, DWORD mode = DT_BOTTOM | DT_SINGLELINE);
  void Refresh();

  // Attributes
public:
  // Operations
public:

  // Implementation
  BOOL m_bShowVol;
  void SetPoint(std::vector<POINT>& pts);
protected:
  std::vector<POINT> points;
  int m_nVol;
  BOOL m_bUseGDI;
  // 背景色
  COLORREF m_clrBK;
  // 适应窗口
  BOOL m_bFit;
  // 缺省旋转方向
  int m_nDefOrgin;

  // 文本
  std::string m_szText;
  // 格式 DrawText格式
  DWORD m_nTextFmt;
  // 文本颜色
  COLORREF m_clrText;

  BOOL m_bSizeChanged;

  BITMAPINFO m_bmi;
  HDRAWDIB m_hDrawDIB;

  CVideoBuffer m_buffer;

  // Generated message map functions
protected:
  virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& result);
  BOOL OnEraseBkgnd(HDC pDC);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_)
