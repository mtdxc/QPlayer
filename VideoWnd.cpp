// VideoWnd.cpp : implementation file
//

#include "VideoWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const static int dpixeladd = 24 / 8;
inline int GetDIBColor(int nWidth,int nHeight,byte *Buffer,int X, int Y)
{
	BYTE *pRgb=Buffer+ dpixeladd * (X + (nHeight - 1 - Y) * nWidth);
	return RGB(pRgb[2],pRgb[1],pRgb[0]);
}

inline int SetDIBColor(int nWidth,int nHeight,byte *Buffer, int X, int Y,int value)
{
	BYTE *ptr=Buffer+ dpixeladd*(X + (nHeight - 1 - Y) * nWidth);
	*ptr = value>>16;
	*(ptr + 1)=value>>8;
	*(ptr + 2)=value&0xFF;
	return 1;
}

VOID RGBRotate90(BYTE *pSrc,BYTE* pDst, int &nHeight,int& nWidth) 
{
	for(int w=0;w<nWidth;w++)
	{
		for(int h=0;h<nHeight;h++)
		{
			int SrcR=GetDIBColor(nWidth,nHeight,pSrc,w,h);
			SetDIBColor(nHeight,nWidth,pDst,nHeight-1-h,w,SrcR);
		}
	}
	std::swap(nHeight, nWidth);
}

VOID RGBRotate180_(BYTE *pSrc,BYTE* pDst, int nHeight,int nWidth) 
{
	memcpy(pDst, pSrc, nHeight * nWidth * 3);
	for(int h=0;h<nHeight/2;h++)  //(nHeight/2)
	{
		for( int w=0;w<nWidth;w++)//nWidth
		{
			int SrcR=GetDIBColor(nWidth,nHeight,pDst,w,h);
			int DesR=GetDIBColor(nWidth,nHeight,pDst,nWidth-w-1,nHeight-h-1);
			SetDIBColor(nWidth,nHeight,pDst,w,h,DesR);	
			SetDIBColor(nWidth,nHeight,pDst,nWidth-w-1,nHeight-h-1,SrcR);
		} 
	}
}

VOID RGBRotate180(BYTE *pSrc,BYTE* pDst, int nHeight,int nWidth) 
{
	for(int h=0;h<nHeight;h++)
	{
		for( int w=0;w<nWidth;w++)//nWidth
		{
			int SrcR=GetDIBColor(nWidth,nHeight,pSrc,w,h);
			SetDIBColor(nWidth,nHeight,pDst,nWidth-w-1,nHeight-h-1,SrcR);
		} 
	}
}

VOID RGBRotate270(BYTE *pSrc,BYTE* pDst,int &nHeight,int &nWidth) 
{
	for(int w=0;w<nWidth;w++)
	{
		for(int h=0;h<nHeight;h++)
		{
			int SrcR=GetDIBColor(nWidth,nHeight,pSrc,w,h);
			SetDIBColor(nHeight,nWidth,pDst,h,nWidth-w-1,SrcR);
		}
	}
	std::swap(nHeight, nWidth);
}

CVideoBuffer::CVideoBuffer()
{
	m_buf=NULL;
	m_w=0;
	m_h=0;
}

CVideoBuffer::~CVideoBuffer()
{
	CleanUpBuffer();
}

//分配符合视频大小的内存
BYTE *CVideoBuffer::GetBuffer(int w,int h)
{
	m_mutex.lock();
	void *ptr=NULL;
	if(w==0||(w==m_w&&h==m_h))
	{
		//已经分配了要求大小的内存
		ptr=m_buf;
		goto RET;
	}
	if(m_buf)
	{
		//
		ptr=realloc(m_buf,w*h*3);		
		if(ptr==NULL)
		{
			CleanUpBuffer();
		}
	}
	else
	{
		ptr=malloc(w*h*3);
	}
	if(ptr==NULL)
		goto RET;
	
	m_buf=ptr;
	
	m_w=w;
	m_h=h;
	
	memset(m_buf,0,w*h*3);	

RET:
	if(!ptr)
		m_mutex.unlock();
	return (BYTE*)ptr;
}

void CVideoBuffer::ReleaseBuffer()
{
	if(m_buf)
		m_mutex.unlock();
}

void CVideoBuffer::CleanUpBuffer()
{
	m_mutex.lock();
	if(m_buf)
	{
		free(m_buf);	
		m_buf=NULL;
		m_w=0;
		m_h=0;
	}
	m_mutex.unlock();
}


/////////////////////////////////////////////////////////////////////////////
// CVideoWnd

CVideoWnd::CVideoWnd()
{	
	memset(&m_bmi,0,sizeof(m_bmi));
	m_bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	m_bmi.bmiHeader.biPlanes=1;	
	m_bmi.bmiHeader.biBitCount=24;
	m_bSizeChanged=FALSE;
	m_bFit = TRUE;
	m_nDefOrgin = 0;
	m_clrBK = 0;
	m_bUseGDI=FALSE;
	m_nTextFmt = DT_BOTTOM|DT_SINGLELINE;
	m_hDrawDIB = NULL;
}

CVideoWnd::~CVideoWnd()
{
	if(m_hDrawDIB)
		DrawDibClose(m_hDrawDIB);
}

/////////////////////////////////////////////////////////////////////////////
// CVideoWnd message handlers
void FillSolidRect(HDC hDC, int x, int y, int cx, int cy, COLORREF clr)
{
  ::SetBkColor(hDC, clr);
  RECT rect{ x, y, x + cx, y + cy };
  ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}

bool CVideoWnd::OnEraseBkgnd(HDC pDC) 
{
	BYTE *pBuffer=m_buffer.GetBuffer();
	if(pBuffer)
	{
		if(m_bSizeChanged)
		{
			m_bSizeChanged=FALSE;
			RECT rcWnd,rcClient;
			GetWindowRect(m_hWnd, &rcWnd); 
			GetClientRect(m_hWnd, &rcClient);
			//Make the client area fit video size
			SetWindowPos(m_hWnd, 0, 0, 0,
				m_bmi.bmiHeader.biWidth + (rcWnd.right - rcWnd.left)-(rcClient.right - rcClient.left),
				m_bmi.bmiHeader.biHeight + (rcWnd.bottom - rcWnd.top) - (rcClient.bottom - rcClient.top),
        SWP_NOMOVE);
		}
		else
		{
			COLORREF clr = m_clrBK;//pDC->GetBkColor();
			RECT rc;
			GetClientRect(m_hWnd, &rc);
      int height = rc.bottom - rc.top;
      int width = rc.right - rc.left;
			if(!m_bFit){
				float fRate = m_bmi.bmiHeader.biWidth * 1.0 / m_bmi.bmiHeader.biHeight;
				int newW = height * fRate;
				if(newW>width){
					int newH = width / fRate;
					int oldH = height;
					rc.top = (oldH - newH)/2;
					rc.bottom = rc.top + newH;

          ::SetBkColor(pDC, clr);
					FillSolidRect(pDC, 0, 0, width, rc.top, clr);
					FillSolidRect(pDC, 0, rc.bottom, width, rc.top, clr);
				}else{
					int oldW = width;
					rc.left = (oldW-newW)/2;
					rc.right = rc.left + newW;
					FillSolidRect(pDC, 0, 0, rc.left, height, clr);
					FillSolidRect(pDC, rc.right, 0, rc.left, height, clr);
				}
			}
			if(m_bUseGDI){
				int oldMode = SetStretchBltMode(pDC, COLORONCOLOR);

				StretchDIBits(pDC, rc.left, rc.top, width, height, 
					0,0,m_bmi.bmiHeader.biWidth,m_bmi.bmiHeader.biHeight, 
					pBuffer, &m_bmi, DIB_RGB_COLORS, SRCCOPY);
				/*
				HDC hMemDc = CreateCompatibleDC(hDC);
				HBITMAP hBitmap = CreateDIBitmap(hDC, &m_bmi.bmiHeader, CBM_INIT, pBuffer, (BITMAPINFO*)&m_bmi, DIB_RGB_COLORS);
				HGDIOBJ hOldBmp = SelectObject(hMemDc, hBitmap);
				StretchBlt(hDC, rc.left, rc.top, rc.Width(), rc.Height(), 
					hMemDc, 0, 0, m_bmi.bmiHeader.biWidth,m_bmi.bmiHeader.biHeight, SRCCOPY);
				DeleteObject(SelectObject(hMemDc, hOldBmp));
				DeleteDC(hMemDc);
				*/
				SetStretchBltMode(pDC, oldMode);
			}else
			DrawDibDraw(m_hDrawDIB,pDC,
				rc.left,rc.top,width,height,
				&m_bmi.bmiHeader,pBuffer,
				0,0,m_bmi.bmiHeader.biWidth,m_bmi.bmiHeader.biHeight,
				DDF_NOTKEYFRAME);
			if(m_szText.GetLength()){ 
				SetBkMode(pDC, TRANSPARENT);
				clr = SetTextColor(pDC, m_clrText);
				DrawText(pDC, m_szText,m_szText.GetLength(), &rc, m_nTextFmt);
				SetTextColor(pDC, clr);
			}
		}
		m_buffer.ReleaseBuffer();
		return TRUE;
	}
	// else return CWnd::OnEraseBkgnd(pDC);
}

bool CVideoWnd::Create(HWND pParentWnd, DWORD dwExStyle, LPCTSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight)
{
  /*
	CString ClassName = AfxRegisterWndClass(
			CS_VREDRAW | CS_HREDRAW|CS_DBLCLKS,
			::LoadCursor(NULL, IDC_ARROW),
			(HBRUSH) ::GetStockObject(BLACK_BRUSH),
			NULL);
    if(ClassName.IsEmpty())
		return FALSE;
	return CWnd::CreateEx(dwExStyle,ClassName,lpszWindowName,dwStyle,x,y,nWidth,nHeight, pParentWnd,NULL);
  */
}

BYTE * CVideoWnd::GetBuffer(int w, int h)
{
	BYTE *ptr=NULL;
	BOOL bVisible = (GetWindowLong(m_hWnd, GWL_STYLE) & WS_VISIBLE);
	if(m_hWnd&&bVisible)//IsWindowVisible())
	{
		ptr=m_buffer.GetBuffer(w,h);
		if(ptr)
		{
			if(w!=m_bmi.bmiHeader.biWidth||h!=m_bmi.bmiHeader.biHeight)
			{
				m_bmi.bmiHeader.biWidth=w;
				m_bmi.bmiHeader.biHeight=h;
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
	if(m_hWnd)
		InvalidateRect(m_hWnd, NULL, TRUE);
	return TRUE;
}


void CVideoWnd::OnLButtonDblClk(UINT nFlags, POINT point) 
{
	if(m_bmi.bmiHeader.biWidth>0)
	{
		RECT rcWnd,rcClient;
		GetWindowRect(m_hWnd, &rcWnd);
		GetClientRect(m_hWnd, &rcClient);
		//make the client area fit video size
		SetWindowPos(m_hWnd, 0,0,0,
			m_bmi.bmiHeader.biWidth + (rcWnd.right - rcWnd.left)- (rcClient.right - rcClient.left),
			m_bmi.bmiHeader.biHeight + (rcWnd.bottom - rcWnd.top)- (rcClient.bottom - rcClient.top),
			SWP_NOMOVE);
	}
	//CWnd::OnLButtonDblClk(nFlags, point);
}


int CVideoWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	RECT rcWnd,rcClient;
	GetWindowRect(m_hWnd, &rcWnd);
	GetClientRect(m_hWnd, &rcClient);
	//make the client rectangle fit video size
	SetWindowPos(m_hWnd, 0,0,0,
    176+ (rcWnd.right - rcWnd.left) - (rcClient.right - rcClient.left),
    144+ (rcWnd.bottom - rcWnd.top) - (rcClient.bottom - rcClient.top),
    SWP_NOMOVE);

	memset(&m_bmi,0,sizeof(m_bmi));
	m_bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	m_bmi.bmiHeader.biPlanes=1;	
	m_bmi.bmiHeader.biBitCount=24;

	m_hDrawDIB=DrawDibOpen();
	return 0;
}

BOOL CVideoWnd::FillBuffer( BYTE* pRgb, int width, int height, int orgin )
{
	BYTE* pDst = NULL;
	if(orgin%2)
		pDst = GetBuffer(height, width);
	else
		pDst = GetBuffer(width, height);
	if(pDst)
	{
		switch(orgin%4){
		case 0:
			memcpy(pDst, pRgb, width*height*3);
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

void CVideoWnd::SetFitMode( BOOL bFit )
{
	m_bFit = bFit;
	if(m_hWnd)
		InvalidateRect(m_hWnd, NULL, TRUE);
}

void CVideoWnd::SetDefOrgin( int orgin )
{
	m_nDefOrgin = orgin;
	if(m_hWnd)
		InvalidateRect(m_hWnd, NULL, TRUE);
}

void CVideoWnd::SetText( LPCTSTR text, COLORREF cl, DWORD mode )
{
	m_szText = text;
	m_nTextFmt = mode;
	m_clrText = cl;
	if(m_hWnd)
		InvalidateRect(m_hWnd,NULL, TRUE);
}

void CVideoWnd::Clear()
{
	m_buffer.CleanUpBuffer();
	if(m_hWnd)
		InvalidateRect(m_hWnd, NULL, TRUE);
}

//创建位图文件
BOOL SaveBitmap(LPCTSTR bmpPath ,BYTE * pBuffer, int lWidth, int lHeight)
{
	// 生成bmp文件
	HANDLE hf = CreateFile(
		bmpPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
		CREATE_ALWAYS, NULL, NULL );
	if( hf == INVALID_HANDLE_VALUE )return 0;
	// 写文件头 
	BITMAPFILEHEADER bfh;
	memset( &bfh, 0, sizeof( bfh ) );
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof( bfh ) + lWidth*lHeight*3 + sizeof( BITMAPINFOHEADER );
	bfh.bfOffBits = sizeof( BITMAPINFOHEADER ) + sizeof( BITMAPFILEHEADER );
	DWORD dwWritten = 0;
	WriteFile( hf, &bfh, sizeof( bfh ), &dwWritten, NULL );
	// 写位图格式
	BITMAPINFOHEADER bih;
	memset( &bih, 0, sizeof( bih ) );
	bih.biSize = sizeof( bih );
	bih.biWidth = lWidth;
	bih.biHeight = lHeight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	WriteFile( hf, &bih, sizeof( bih ), &dwWritten, NULL );
	// 写位图数据
	WriteFile( hf, pBuffer, lWidth*lHeight*3, &dwWritten, NULL );
	CloseHandle( hf );
	return 0;
}

#include <atlimage.h>
BOOL CVideoWnd::TaskSnap( LPCTSTR path )
{
	BOOL bRet = FALSE;
	IStream* stm = NULL;
  CreateStreamOnHGlobal(NULL, TRUE, &stm);
  BYTE* b = m_buffer.GetBuffer();
	if(b&&stm){
		DWORD size = m_bmi.bmiHeader.biWidth * m_bmi.bmiHeader.biHeight * 3;
		// 写文件头 
		BITMAPFILEHEADER bfh;
		memset( &bfh, 0, sizeof( bfh ) );
		bfh.bfType = 'MB';
		bfh.bfSize = sizeof( bfh ) + size + sizeof( BITMAPINFOHEADER );
		bfh.bfOffBits = sizeof( BITMAPINFOHEADER ) + sizeof( BITMAPFILEHEADER );
		stm->Write(&bfh, sizeof bfh, NULL);
		stm->Write(&m_bmi.bmiHeader, sizeof BITMAPINFOHEADER, NULL);
		stm->Write(b, size, NULL);
		CImage img;
		if(SUCCEEDED(img.Load(stm)))
			bRet = SUCCEEDED(img.Save(path));
    stm->Release();
	}
	if(b) m_buffer.ReleaseBuffer();
	return bRet;
}
