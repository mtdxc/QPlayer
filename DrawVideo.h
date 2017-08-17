//绘制图像类
//JC 2013-07-31
#ifndef AVLIB_DRAWVIDEO_H_
#define AVLIB_DRAWVIDEO_H_
#include <Vfw.h>
class CDrawVideo
{
//Attributes
protected:
  HWND m_hWnd;
  HDRAWDIB m_hDib;
  BITMAPINFO bih;
  RECT m_rect;
  BOOL m_bUseGDI;
  BOOL m_bFit;
  // 背景色
  COLORREF m_clrBK;
public:
  CDrawVideo();
  ~CDrawVideo();

  void SetFitMode(BOOL bFit);
  void SetBkColor(COLORREF color){ m_clrBK = color; }

  BOOL Init(HWND hWnd);
  void Release(){}

  //绘制RGB数据  数据长度为width*height*3
  void Draw(BYTE *pBuf,int width, int height, LPCTSTR szText=NULL);
};

#endif //AVLIB_DRAWVIDEO_H_