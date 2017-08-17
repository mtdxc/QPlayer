// VideoWnd.cpp : implementation file
//
#include "StdAfx.h"
#include "VideoWnd.h"
#include <atlimage.h>
#include <atltypes.h>
#include <atlconv.h>
#include <VFW.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

static const wchar_t kWindowBaseClassName[] = L"QWindowClass";
HINSTANCE Win32Window::instance_ = NULL;
ATOM Win32Window::window_class_ = 0;

Win32Window::Win32Window() : wnd_(NULL) {
}

Win32Window::~Win32Window() {
  ASSERT(NULL == wnd_);
}

bool Win32Window::Create(HWND parent, const wchar_t* title, DWORD style,
  DWORD exstyle, int x, int y, int cx, int cy) {
  if (wnd_) {
    // Window already exists.
    return false;
  }

  if (!window_class_) {
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&Win32Window::WndProc),
      &instance_)) {
      return false;
    }

    // Class not registered, register it.
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.hInstance = instance_;
    wcex.lpfnWndProc = &Win32Window::WndProc;
    wcex.lpszClassName = kWindowBaseClassName;
    window_class_ = ::RegisterClassEx(&wcex);
    if (!window_class_) {
      return false;
    }
  }
  wnd_ = ::CreateWindowEx(exstyle, kWindowBaseClassName, title, style,
    x, y, cx, cy, parent, NULL, instance_, this);
  return (NULL != wnd_);
}

void Win32Window::Destroy() {
  ::DestroyWindow(wnd_);
}

void Win32Window::Shutdown() {
  if (window_class_) {
    ::UnregisterClass(MAKEINTATOM(window_class_), instance_);
    window_class_ = 0;
  }
}

bool Win32Window::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
  LRESULT& result) {
  switch (uMsg) {
  case WM_CLOSE:
    if (!OnClose()) {
      result = 0;
      return true;
    }
    break;
  }
  return false;
}

LRESULT Win32Window::WndProc(HWND hwnd, UINT uMsg,
  WPARAM wParam, LPARAM lParam) {
  Win32Window* that = reinterpret_cast<Win32Window*>(
    ::GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (!that && (WM_CREATE == uMsg)) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    that = static_cast<Win32Window*>(cs->lpCreateParams);
    that->wnd_ = hwnd;
    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
  }
  if (that) {
    LRESULT result;
    bool handled = that->OnMessage(uMsg, wParam, lParam, result);
    if (WM_DESTROY == uMsg) {
      for (HWND child = ::GetWindow(hwnd, GW_CHILD); child;
        child = ::GetWindow(child, GW_HWNDNEXT)) {
      }
    }
    if (WM_NCDESTROY == uMsg) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
      that->wnd_ = NULL;
      that->OnNcDestroy();
    }
    if (handled) {
      return result;
    }
  }
  return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

const static int dpixeladd = 24 / 8;
inline int GetDIBColor(int nWidth, int nHeight, byte *Buffer, int X, int Y)
{
  BYTE *pRgb = Buffer + dpixeladd * (X + (nHeight - 1 - Y) * nWidth);
  return RGB(pRgb[2], pRgb[1], pRgb[0]);
}

inline int SetDIBColor(int nWidth, int nHeight, byte *Buffer, int X, int Y, int value)
{
  BYTE *ptr = Buffer + dpixeladd*(X + (nHeight - 1 - Y) * nWidth);
  *ptr = value >> 16;
  *(ptr + 1) = value >> 8;
  *(ptr + 2) = value & 0xFF;
  return 1;
}

VOID RGBRotate90(BYTE *pSrc, BYTE* pDst, int &nHeight, int& nWidth)
{
  for (int w = 0; w<nWidth; w++)
  {
    for (int h = 0; h<nHeight; h++)
    {
      int SrcR = GetDIBColor(nWidth, nHeight, pSrc, w, h);
      SetDIBColor(nHeight, nWidth, pDst, nHeight - 1 - h, w, SrcR);
    }
  }
  std::swap(nHeight, nWidth);
}

VOID RGBRotate180_(BYTE *pSrc, BYTE* pDst, int nHeight, int nWidth)
{
  memcpy(pDst, pSrc, nHeight * nWidth * 3);
  for (int h = 0; h<nHeight / 2; h++)  //(nHeight/2)
  {
    for (int w = 0; w<nWidth; w++)//nWidth
    {
      int SrcR = GetDIBColor(nWidth, nHeight, pDst, w, h);
      int DesR = GetDIBColor(nWidth, nHeight, pDst, nWidth - w - 1, nHeight - h - 1);
      SetDIBColor(nWidth, nHeight, pDst, w, h, DesR);
      SetDIBColor(nWidth, nHeight, pDst, nWidth - w - 1, nHeight - h - 1, SrcR);
    }
  }
}

VOID RGBRotate180(BYTE *pSrc, BYTE* pDst, int nHeight, int nWidth)
{
  for (int h = 0; h<nHeight; h++)
  {
    for (int w = 0; w<nWidth; w++)//nWidth
    {
      int SrcR = GetDIBColor(nWidth, nHeight, pSrc, w, h);
      SetDIBColor(nWidth, nHeight, pDst, nWidth - w - 1, nHeight - h - 1, SrcR);
    }
  }
}

VOID RGBRotate270(BYTE *pSrc, BYTE* pDst, int &nHeight, int &nWidth)
{
  for (int w = 0; w<nWidth; w++)
  {
    for (int h = 0; h<nHeight; h++)
    {
      int SrcR = GetDIBColor(nWidth, nHeight, pSrc, w, h);
      SetDIBColor(nHeight, nWidth, pDst, h, nWidth - w - 1, SrcR);
    }
  }
  std::swap(nHeight, nWidth);
}

CVideoBuffer::CVideoBuffer()
{
  m_buf = NULL;
  m_w = 0;
  m_h = 0;
}

CVideoBuffer::~CVideoBuffer()
{
  CleanUpBuffer();
}

//分配符合视频大小的内存
BYTE *CVideoBuffer::GetBuffer(int w, int h)
{
  m_mutex.lock();
  void *ptr = NULL;
  if (w == 0 || (w == m_w&&h == m_h))
  {
    //已经分配了要求大小的内存
    ptr = m_buf;
    goto RET;
  }
  int stride = (w * 3 + 3) / 4 * 4;
  if (m_buf)
  {
    //
    ptr = realloc(m_buf, stride*h);
    if (ptr == NULL)
    {
      CleanUpBuffer();
    }
  }
  else
  {
    ptr = malloc(stride*h);
  }
  if (ptr == NULL)
    goto RET;

  m_buf = ptr;

  m_w = w;
  m_h = h;

  memset(m_buf, 0, stride*h);

RET:
  if (!ptr)
    m_mutex.unlock();
  return (BYTE*)ptr;
}

void CVideoBuffer::ReleaseBuffer()
{
  if (m_buf)
    m_mutex.unlock();
}

void CVideoBuffer::CleanUpBuffer()
{
  m_mutex.lock();
  if (m_buf)
  {
    free(m_buf);
    m_buf = NULL;
    m_w = 0;
    m_h = 0;
  }
  m_mutex.unlock();
}


/////////////////////////////////////////////////////////////////////////////
// CVideoWnd
static CImage defImage;
CVideoWnd::CVideoWnd()
{
  memset(&m_bmi, 0, sizeof(m_bmi));
  m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  m_bmi.bmiHeader.biPlanes = 1;
  m_bmi.bmiHeader.biBitCount = 24;
  m_bShowVol = FALSE;
  m_bSizeChanged = FALSE;
  m_bFit = TRUE;
  m_nDefOrgin = 0;
  m_clrBK = 0;
  m_clrText = 0xFFFFFF;
  m_nTextFmt = DT_BOTTOM | DT_SINGLELINE;

  m_bUseGDI = TRUE;
  m_hDrawDIB = NULL;
  if (!m_bUseGDI) {
    m_hDrawDIB = DrawDibOpen();
  }
}

CVideoWnd::~CVideoWnd()
{
  if (m_hDrawDIB) {
    DrawDibClose(m_hDrawDIB);
    m_hDrawDIB = NULL;
  }
  if (handle()) {
    Destroy();
  }
}

bool CVideoWnd::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& result) {
  switch (uMsg)
  {
  case WM_ERASEBKGND:
  {
    HDC dc = (HDC)wParam;
    OnEraseBkgnd(dc);
    result = 1;
    return true;
  }
  break;
  case WM_CREATE:
  case WM_LBUTTONDBLCLK:
  case WM_RBUTTONDOWN:
  default:
    return __super::OnMessage(uMsg, wParam, lParam, result);
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////
// CVideoWnd message handlers
#include "Core\UIRender.h"
BOOL CVideoWnd::OnEraseBkgnd(HDC hDC)
{
  CRect rc;
  GetClientRect(handle(), rc);
  BYTE *pBuffer = m_buffer.GetBuffer();
  if (pBuffer)
  {
    if (m_bSizeChanged)
    {
      m_bSizeChanged = FALSE;
      CRect rcWnd;
      GetWindowRect(handle(), rcWnd);
      //Make the client area fit video size
      SetWindowPos(handle(), 0, 0, 0,
        m_bmi.bmiHeader.biWidth + rcWnd.Width() - rc.Width(),
        abs(m_bmi.bmiHeader.biHeight) + rcWnd.Height() - rc.Height(), SWP_NOMOVE);
    }
    else
    {
      ::SetBkColor(hDC, m_clrBK);
      RECT rcBK;
      if (!m_bFit) {
        //绘制黑边
        float fRate = m_bmi.bmiHeader.biWidth * 1.0f / abs(m_bmi.bmiHeader.biHeight);
        int newW = rc.Height() * fRate;
        if (newW>rc.Width()) {
          int newH = rc.Width() / fRate;
          int oldH = rc.Height();
          rc.top = (oldH - newH) / 2;
          rc.bottom = rc.top + newH;

          rcBK = { 0, 0, rc.Width(), rc.top };
          ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rcBK, NULL, 0, NULL);
          rcBK = { 0, rc.bottom, rc.Width(), rc.bottom + rc.top };
          ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rcBK, NULL, 0, NULL);
        }
        else {
          int oldW = rc.Width();
          rc.left = (oldW - newW) / 2;
          rc.right = rc.left + newW;
          rcBK = { 0, 0, rc.left, rc.Height() };
          ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rcBK, NULL, 0, NULL);
          rcBK = { rc.right, 0, rc.right + rc.left, rc.Height() };
          ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rcBK, NULL, 0, NULL);
        }
      }

      if (m_bUseGDI) {
        int oldMode = SetStretchBltMode(hDC, COLORONCOLOR);

        StretchDIBits(hDC, rc.left, rc.top, rc.Width(), rc.Height(),
          0, 0, m_bmi.bmiHeader.biWidth, abs(m_bmi.bmiHeader.biHeight),
          pBuffer, &m_bmi, DIB_RGB_COLORS, SRCCOPY);
        SetStretchBltMode(hDC, oldMode);
      }
      else {
        DrawDibDraw(m_hDrawDIB, hDC,
          rc.left, rc.top, rc.Width(), rc.Height(),
          &m_bmi.bmiHeader, pBuffer,
          0, 0, m_bmi.bmiHeader.biWidth, abs(m_bmi.bmiHeader.biHeight),
          DDF_NOTKEYFRAME);
      }
    }
    if (points.size()) {
      float xscale = rc.Width() * 1.0 / m_bmi.bmiHeader.biWidth;
      float yscale = rc.Height() * 1.0 / abs(m_bmi.bmiHeader.biHeight);
#define PT_SIZE 3
      HGDIOBJ bush = CreateSolidBrush(RGB(255, 0, 0));
      HGDIOBJ oldBush = ::SelectObject(hDC, bush);
      for (POINT pt : points) {
        pt.x *= xscale;
        pt.y *= yscale;
        Ellipse(hDC, pt.x - PT_SIZE, pt.y - PT_SIZE, pt.x + PT_SIZE, pt.y + PT_SIZE);
        //SetPixel(hDC, pt.x, pt.y, RGB(255, 0, 0));
      }
      SelectObject(hDC, oldBush);
      DeleteObject(bush);
    }
    m_buffer.ReleaseBuffer();
  }
  else if (defImage) {
    /*
    HBITMAP hBmp = CreateCompatibleBitmap(hDC, rc.Width(), rc.Height());
    HDC hMemDc = CreateCompatibleDC(hDC);
    HGDIOBJ hOld = SelectObject(hMemDc, hBmp);
    int oldMode = SetStretchBltMode(hMemDc, COLORONCOLOR);
    defImage.StretchBlt(hMemDc, rc);
    SetStretchBltMode(hMemDc, oldMode);
    SelectObject(hMemDc, hOld);
    DeleteObject(hMemDc);
    HGDIOBJ hBmpBush = CreatePatternBrush(hBmp);
    DeleteObject(hBmp);

    SelectObject(hDC, hBmpBush);
    SelectObject(hDC, CreatePen(PS_SOLID, 6, 0x00FFFFFF));
    RoundRect(hDC, rc.left, rc.top, rc.right, rc.bottom, rc.Width() / 4, rc.Height() / 4);
    DeleteObject(hBmpBush);
    */
    int oldMode = SetStretchBltMode(hDC, COLORONCOLOR);
    defImage.StretchBlt(hDC, rc);
    SetStretchBltMode(hDC, oldMode);
  }
  else {
    HBRUSH brush = ::CreateSolidBrush(m_clrBK);
    ::FillRect(hDC, &rc, brush);
    ::DeleteObject(brush);
  }

  if (m_szText.length()) {
    HGDIOBJ old_font = ::SelectObject(hDC, ::GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hDC, TRANSPARENT);
    COLORREF clr = SetTextColor(hDC, m_clrText);
    DrawTextA(hDC, m_szText.c_str(), -1, rc, m_nTextFmt);
    SetTextColor(hDC, clr);
    ::SelectObject(hDC, old_font);
  }

  if (m_bShowVol) {
    GetClientRect(handle(), rc);
    rc.bottom = 5;
    rc.right = m_nVol * rc.right / 32727;
    COLORREF ref = RGB(21, 194, 60);
    if (m_nVol > 0.8 * 32727)
      ref = RGB(128, 0, 0);
    HBRUSH brush = ::CreateSolidBrush(ref);
    FillRect(hDC, rc, brush);
    ::DeleteObject(brush);

    /* 渐变色绘制
    GetClientRect(handle(), rc);
    rc.top = rc.bottom - 20;
    rc.right = 100;
    SelectObject(hDC, GetStockObject(NULL_BRUSH));
    ::Rectangle(hDC,rc.left, rc.top, rc.right, rc.bottom);
    rc.right = m_nVol * rc.right / 32727;
    InflateRect(&rc, -1, -1);
    DuiLib::CRenderEngine::DrawGradient(hDC, rc, 0xFF3cc215, 0xFF800000, false, 32);
    */
  }
  return TRUE;
}

BYTE * CVideoWnd::GetBuffer(int w, int h)
{
  BYTE *ptr = NULL;
  BOOL bVisible = (GetWindowLong(handle(), GWL_STYLE) & WS_VISIBLE);
  if (handle() && bVisible)//IsWindowVisible())
  {
    ptr = m_buffer.GetBuffer(w, h);
    if (ptr)
    {
      if (w != m_bmi.bmiHeader.biWidth || h != abs(m_bmi.bmiHeader.biHeight))
      {
        m_bmi.bmiHeader.biWidth = w;
        m_bmi.bmiHeader.biHeight = -h;
        m_bmi.bmiHeader.biSizeImage = w * h * (m_bmi.bmiHeader.biBitCount >> 3);
        // m_bSizeChanged=TRUE;
      }
      //We don't call ReleaseBuffer() here, and call it later
    }
  }
  return ptr;

}

BOOL CVideoWnd::ReleaseBuffer()
{
  m_buffer.ReleaseBuffer();
  Refresh();
  return TRUE;
}

BOOL CVideoWnd::FillBuffer(BYTE* pRgb, int width, int height, int orgin)
{
  BYTE* pDst = NULL;
  if (orgin % 2)
    pDst = GetBuffer(height, width);
  else
    pDst = GetBuffer(width, height);
  if (pDst)
  {
    switch (orgin % 4) {
    case 0:
      memcpy(pDst, pRgb, width*height * 3);
      break;
    case 1:
      RGBRotate90(pRgb, pDst, height, width);
      break;
    case 2:
      RGBRotate180(pRgb, pDst, height, width);
      break;
    case 3:
      RGBRotate270(pRgb, pDst, height, width);
      break;
    }
    ReleaseBuffer();
    return TRUE;
  }
  return FALSE;
}

void CVideoWnd::SetFitMode(BOOL bFit)
{
  m_bFit = bFit;
  Refresh();
}

void CVideoWnd::SetBkColor(COLORREF color)
{
  m_clrBK = color;
  Refresh();
}

void CVideoWnd::SetText(const char* text, COLORREF cl, DWORD mode)
{
  m_szText = text;
  m_nTextFmt = mode;
  m_clrText = cl;
  Refresh();
}

void CVideoWnd::Clear()
{
  m_buffer.CleanUpBuffer();
  Refresh();
}

void CVideoWnd::Refresh()
{
  if (handle())
    InvalidateRect(handle(), NULL, TRUE);
}


//创建位图文件
bool SaveBitmap(const char* bmpPath, BYTE * pBuffer, int lWidth, int lHeight, int nByte = 3)
{
  // 生成bmp文件
  HANDLE hf = CreateFileA(
    bmpPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
    CREATE_ALWAYS, NULL, NULL);
  if (hf == INVALID_HANDLE_VALUE)return 0;
  // 写文件头 
  BITMAPFILEHEADER bfh;
  memset(&bfh, 0, sizeof(bfh));
  bfh.bfType = 'MB';
  bfh.bfSize = sizeof(bfh) + lWidth*lHeight * nByte + sizeof(BITMAPINFOHEADER);
  bfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
  DWORD dwWritten = 0;
  WriteFile(hf, &bfh, sizeof(bfh), &dwWritten, NULL);
  // 写位图格式
  BITMAPINFOHEADER bih;
  memset(&bih, 0, sizeof(bih));
  bih.biSize = sizeof(bih);
  bih.biWidth = lWidth;
  bih.biHeight = lHeight;// -lHeight;
  bih.biPlanes = 1;
  bih.biBitCount = nByte * 8;
  WriteFile(hf, &bih, sizeof(bih), &dwWritten, NULL);
  // 写位图数据
  WriteFile(hf, pBuffer, lWidth*lHeight*nByte, &dwWritten, NULL);
  CloseHandle(hf);
  return 0;
}

BOOL CVideoWnd::TaskSnap(LPCTSTR path)
{
  BOOL bRet = FALSE;
  LPSTREAM lpStream = NULL;
  BYTE* b = m_buffer.GetBuffer();
  if (b&&SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &lpStream))) {
    DWORD size = m_bmi.bmiHeader.biSizeImage;
    // 写文件头 
    BITMAPFILEHEADER bfh;
    memset(&bfh, 0, sizeof(bfh));
    bfh.bfType = 'MB';
    bfh.bfSize = sizeof(bfh) + size + sizeof(BITMAPINFOHEADER);
    bfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
    ULONG nWritten = 0;
    lpStream->Write(&bfh, sizeof bfh, &nWritten);
    lpStream->Write(&m_bmi.bmiHeader, sizeof BITMAPINFOHEADER, &nWritten);
    lpStream->Write(b, size, &nWritten);
    CImage img;
    if (SUCCEEDED(img.Load(lpStream)))
      bRet = SUCCEEDED(img.Save(path));
    lpStream->Release();
  }
  if (b) m_buffer.ReleaseBuffer();
  return bRet;
}

bool CVideoWnd::SetDefaultImage(const char* path)
{
  USES_CONVERSION;
  return SUCCEEDED(defImage.Load(A2T(path)));
}

bool CVideoWnd::SetVisible(bool bShow)
{
  return ::ShowWindow(handle(), bShow ? SW_SHOW : SW_HIDE);
}

bool CVideoWnd::GetVisible()
{
  return ::IsWindowVisible(handle());
}

void CVideoWnd::SetPoint(std::vector<POINT>& pts)
{
  m_buffer.GetBuffer();
  points = pts;
  m_buffer.ReleaseBuffer();
}
