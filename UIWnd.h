#pragma once
#include <UIlib.h>
//#include <Core\UIControl.h>

// 将带句柄HWND的控件显示到CControlUI上面
class CWndUI : public DuiLib::CControlUI
{
public:
  CWndUI() : m_hWnd(NULL){}
  // add by caiqm 必须加上这句,否则在tab控件中无法使用
  virtual void SetVisible(bool bVisible /* = true */){
    DuiLib::CControlUI::SetInternVisible(bVisible);
    if (m_hWnd)
      ::ShowWindow(m_hWnd, bVisible);
  }
  virtual void SetInternVisible(bool bVisible = true)
  {
    DuiLib::CControlUI::SetInternVisible(bVisible);
    if (m_hWnd)
      ::ShowWindow(m_hWnd, bVisible);
  }

  virtual void SetPos(RECT rc)
  {
    DuiLib::CControlUI::SetPos(rc);
    if (m_hWnd)
      ::SetWindowPos(m_hWnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
  }

  BOOL Attach(HWND hWndNew, BOOL bSize=FALSE)
  {
    if (!::IsWindow(hWndNew))
    {
      return FALSE;
    }

    m_hWnd = hWndNew;
    if (bSize){
      const RECT& rc = GetPos();
      ::SetWindowPos(m_hWnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return TRUE;
  }

  HWND Detach()
  {
    HWND hRet = m_hWnd;
    m_hWnd = NULL;
    return hRet;
  }
  
protected:
  HWND m_hWnd;
};
