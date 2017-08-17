#pragma once

#include "qplayer.h"
#include "Ini.h"

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  DuiLib::CPaintManagerUI::SetInstance(hInstance);
  DuiLib::CPaintManagerUI::SetResourcePath(DuiLib::CPaintManagerUI::GetInstancePath() + _T("skin"));
  QPlayer* dlg = new QPlayer(true);
  if (dlg == NULL){
    return 0;
  } 
  dlg->Create(NULL, _T("QPlayer"),
    UI_WNDSTYLE_FRAME, //WS_OVERLAPPEDWINDOW, 
    WS_EX_STATICEDGE | WS_EX_APPWINDOW | WS_EX_ACCEPTFILES);
  dlg->CenterWindow();
  dlg->ShowWindow(true);
  if (lpCmdLine&&lpCmdLine[0])
    dlg->OpenFile(lpCmdLine);
  DuiLib::CPaintManagerUI::MessageLoop();
  return 0;
}
