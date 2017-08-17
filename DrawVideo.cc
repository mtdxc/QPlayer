#include <windows.h>
#include <atltypes.h>
#include "DrawVideo.h"

#define DBG_INFO
#define GID_DRAWVDO "DrawVdo"
#pragma comment(lib, "vfw32.lib")

CDrawVideo::CDrawVideo()
{
  memset(&m_rect, 0, sizeof m_rect);
  m_bUseGDI = FALSE;
  m_bFit = TRUE;
	m_hDib = DrawDibOpen();
	if(!m_hDib)
	{
		DBG_INFO(GID_DRAWVDO, "***DrawDibOpen error");
	}
	Init(NULL);
}

CDrawVideo::~CDrawVideo()
{
  if (m_hDib){
    DrawDibClose(m_hDib);
    m_hDib = NULL;
  }
}

void CDrawVideo::SetFitMode(BOOL bFit){
  m_bFit = bFit;
}

void CDrawVideo::Draw(BYTE *pBuffer,int width, int height, LPCTSTR szText)
{
	if(!IsWindow(m_hWnd))
	{
		DBG_INFO(GID_DRAWVDO, "no window %X, break draw", m_hWnd);
		return;
	}
	BOOL bVisible = (GetWindowLong(m_hWnd, GWL_STYLE) & WS_VISIBLE);
	if(!bVisible){ //IsWindowVisible(m_hWnd)
		DBG_INFO(GID_DRAWVDO, "m_hWnd %X not Visible skip draw", m_hWnd);
		return;
	}
	//DBG_TRACE("DrawVideo", "Draw");
	HDC hDC=::GetDC(m_hWnd);
	if(!hDC){
		DBG_INFO(GID_DRAWVDO, "unable to GetDC %X, break draw", m_hWnd);
		return ;
	}
	CRect destRect;
	if(!GetClientRect(m_hWnd, &destRect))
	{
		DBG_INFO(GID_DRAWVDO, "unable to GetClientRect %X,%d break draw", m_hWnd, GetLastError());
		ReleaseDC(m_hWnd,hDC);
		return;
	}
	if(destRect.IsRectEmpty())
	{
		DBG_INFO(GID_DRAWVDO, "***rect empty, GWL_STYLE: %X", GetWindowLong(m_hWnd, GWL_STYLE));
		ReleaseDC(m_hWnd,hDC);
		return;
	}
	if(destRect != m_rect)
	{
		DBG_INFO(GID_DRAWVDO, "MoveRect from %d,%d,%d,%d to %d,%d,%d,%d", 
			m_rect.left, m_rect.top, m_rect.right, m_rect.bottom, 
			destRect.left, destRect.top, destRect.right, destRect.bottom);
		m_rect = destRect;
	}
	destRect.DeflateRect(1,1);
  if (pBuffer)
  {
    bih.bmiHeader.biWidth = width;
    bih.bmiHeader.biHeight = height;
    bih.bmiHeader.biSizeImage = width * height * 3;
    if (!m_bFit){
      ::SetBkColor(hDC, m_clrBK);
      ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, destRect, NULL, 0, NULL);
      float fRate = bih.bmiHeader.biWidth * 1.0f / bih.bmiHeader.biHeight;
      int newW = destRect.Height() * fRate;
      if (newW > destRect.Width()){
        int newH = destRect.Width() / fRate;
        int oldH = destRect.Height();
        destRect.top = (oldH - newH) / 2;
        destRect.bottom = destRect.top + newH;
      }
      else{
        int oldW = destRect.Width();
        destRect.left = (oldW - newW) / 2;
        destRect.right = destRect.left + newW;
      }
    }
		if(m_bUseGDI){
			int oldMode = SetStretchBltMode(hDC, COLORONCOLOR);
			StretchDIBits(hDC, destRect.left, destRect.top, destRect.Width(), destRect.Height(), 
				0, 0, width, height, pBuffer, &bih, DIB_RGB_COLORS, SRCCOPY);
			SetStretchBltMode(hDC, oldMode);
		}
		else if(!::DrawDibDraw(m_hDib,
			hDC,
			destRect.left, // dest : left pos
			destRect.top, // dest : top pos
			destRect.Width(), // don't zoom x
			destRect.Height(), // don't zoom y
			&bih.bmiHeader, // bmp header info
			pBuffer,    // bmp data
			0, // dest : left pos
			0, // dest : top pos
			width, // don't zoom x
			height, // don't zoom y
			DDF_NOTKEYFRAME ))   // use prev params....//DDF_SAME_DRAW
				DBG_INFO(GID_DRAWVDO, "DrawDibDraw %X error", m_hWnd);

	}
	// m_desRect.top = m_desRect.bottom - 20;
	if(szText && szText[0]){
		if(1){//GetIniInt("View", "TransText", 0)){
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, 0xffffff);
		}
		// TextOut(hDC, destRect.left+2, destRect.bottom - 20, szText, strlen(szText));
		DrawText(hDC, szText, -1, destRect, DT_BOTTOM|DT_SINGLELINE);
	}
	ReleaseDC(m_hWnd,hDC);

}

BOOL CDrawVideo::Init( HWND hWnd )
{
	m_hWnd = hWnd;
	memset( &bih, 0, sizeof( bih ) );
	bih.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bih.bmiHeader.biPlanes = 1;
	bih.bmiHeader.biBitCount = 24;
	return TRUE;
}
