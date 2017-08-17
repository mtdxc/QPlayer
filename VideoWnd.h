#if !defined(AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_)
#define AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_
#include <atlstr.h>
#include <memory>
#include <mutex>
#include <VFW.h>

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// VideoWnd.h : header file
//

//Video buffer class
class CVideoBuffer
{
public:
	CVideoBuffer();
	~CVideoBuffer();

	BYTE *GetBuffer(int w=0,int h=0);
	void ReleaseBuffer();
	void CleanUpBuffer();
protected:
	void *m_buf;
	int m_w;
	int m_h;
	std::mutex m_mutex;
};

/**
CVideoWnd window
由于CImage的Bits指向RGB数据底部，因此CImage调用方式
CImage img("videoback.jpg");
pWnd->FillBuffer((BYTE*)img.GetPixelAddress(0, img.GetHeight()-1), img.GetWidth(), img.GetHeight());
*/
class CVideoWnd
{
  HWND m_hWnd;            // must be first data member
// Construction
public:
	CVideoWnd();	
	virtual ~CVideoWnd();

	bool Create(HWND pParentWnd, DWORD dwExStyle,LPCTSTR lpszWindowName,DWORD dwStyle,
		int x, int y, int nWidth, int nHeight);

	BYTE * GetBuffer(int w,int h);
	BOOL ReleaseBuffer();
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
	BOOL FillBuffer(BYTE* pRgb, int width, int height){
		return FillBuffer(pRgb, width, height, m_nDefOrgin);}
	// 设置缺省旋转方向
	void SetDefOrgin(int orgin);
	// 设置窗口适应模式
	void SetFitMode(BOOL bFit);
	void SetBkColor(COLORREF color){m_clrBK=color;}
	BOOL TaskSnap(LPCTSTR path);
	/*
	设置文本.
	@arg text 内容 
	@arg clr 颜色
	@arg mode 绘制模式
	*/
	void SetText(LPCTSTR text, COLORREF clr, DWORD mode = DT_BOTTOM|DT_SINGLELINE);
	void Clear();
// Attributes
public:
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVideoWnd)
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL m_bUseGDI;
	// 背景色
	COLORREF m_clrBK;
	// 适应窗口
	BOOL m_bFit;
	// 缺省旋转方向
	int m_nDefOrgin;

	// 文本
	CString m_szText;
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
	//{{AFX_MSG(CVideoWnd)
	bool OnEraseBkgnd(HDC pDC);
	void OnLButtonDblClk(UINT nFlags, POINT point);
	int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIDEOWND_H__2D780DDC_26E5_4561_91CC_53BCF9292071__INCLUDED_)
