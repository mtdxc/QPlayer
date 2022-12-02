#include "UICombox.h"

UICombox::UICombox()
{
}

UICombox::~UICombox()
{
	DestroyWindow(m_hWnd);
}

/////////////////////////////////////////////////////////////////////////////
// CComboBox

BOOL UICombox::Create(DWORD dwStyle, const RECT& rect, HWND pParentWnd,
	UINT nID)
{
	return CWndUI::Create(_T("COMBOBOX"), NULL, dwStyle, rect, pParentWnd, nID);
}

// Derived class is responsible for implementing these handlers
//   for owner/self draw controls (except for the optional DeleteItem)
void UICombox::DrawItem(LPDRAWITEMSTRUCT)
{
	ASSERT(FALSE);
}
void UICombox::MeasureItem(LPMEASUREITEMSTRUCT)
{
	ASSERT(FALSE);
}
int UICombox::CompareItem(LPCOMPAREITEMSTRUCT)
{
	ASSERT(FALSE); return 0;
}
void UICombox::DeleteItem(LPDELETEITEMSTRUCT)
{ /* default to nothing */
}

BOOL UICombox::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam,
	LRESULT* pResult)
{
	switch (message)
	{
	case WM_DRAWITEM:
		ASSERT(pResult == NULL);       // no return value expected
		DrawItem((LPDRAWITEMSTRUCT)lParam);
		break;
	case WM_MEASUREITEM:
		ASSERT(pResult == NULL);       // no return value expected
		MeasureItem((LPMEASUREITEMSTRUCT)lParam);
		break;
	case WM_COMPAREITEM:
		ASSERT(pResult != NULL);       // return value expected
		*pResult = CompareItem((LPCOMPAREITEMSTRUCT)lParam);
		break;
	case WM_DELETEITEM:
		ASSERT(pResult == NULL);       // no return value expected
		DeleteItem((LPDELETEITEMSTRUCT)lParam);
		break;
	default:
		//return CWnd::OnChildNotify(message, wParam, lParam, pResult);
		break;
	}
	return TRUE;
}

int UICombox::GetCount() const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETCOUNT, 0, 0);
}

int UICombox::GetCurSel() const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETCURSEL, 0, 0);
}

int UICombox::SetCurSel(int nSelect)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETCURSEL, nSelect, 0);
}

DWORD UICombox::GetEditSel() const
{
	ASSERT(::IsWindow(m_hWnd)); return DWORD(::SendMessage(m_hWnd, CB_GETEDITSEL, 0, 0));
}

BOOL UICombox::LimitText(int nMaxChars)
{
	ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, CB_LIMITTEXT, nMaxChars, 0);
}

BOOL UICombox::SetEditSel(_In_ int nStartChar, _In_ int nEndChar)
{
	ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, CB_SETEDITSEL, 0, MAKELONG(nStartChar, nEndChar));
}

DWORD_PTR UICombox::GetItemData(int nIndex) const
{
	ASSERT(::IsWindow(m_hWnd)); return ::SendMessage(m_hWnd, CB_GETITEMDATA, nIndex, 0);
}

int UICombox::SetItemData(int nIndex, DWORD_PTR dwItemData)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETITEMDATA, nIndex, (LPARAM)dwItemData);
}

void* UICombox::GetItemDataPtr(int nIndex) const
{
	ASSERT(::IsWindow(m_hWnd)); return (LPVOID)GetItemData(nIndex);
}

int UICombox::SetItemDataPtr(int nIndex, void* pData)
{
	ASSERT(::IsWindow(m_hWnd)); return SetItemData(nIndex, (DWORD_PTR)(LPVOID)pData);
}

int UICombox::GetLBText(_In_ int nIndex, _Pre_notnull_ _Post_z_ LPTSTR lpszText) const
{
	ASSERT(::IsWindow(m_hWnd));
	return (int)::SendMessage(m_hWnd, CB_GETLBTEXT, nIndex, (LPARAM)lpszText);
}

int UICombox::GetLBTextLen(int nIndex) const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETLBTEXTLEN, nIndex, 0);
}

void UICombox::ShowDropDown(BOOL bShowIt)
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, CB_SHOWDROPDOWN, bShowIt, 0);
}

int UICombox::AddString(LPCTSTR lpszString)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_ADDSTRING, 0, (LPARAM)lpszString);
}

int UICombox::DeleteString(UINT nIndex)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_DELETESTRING, nIndex, 0);
}

int UICombox::InsertString(_In_ int nIndex, _In_z_ LPCTSTR lpszString)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_INSERTSTRING, nIndex, (LPARAM)lpszString);
}

void UICombox::ResetContent()
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, CB_RESETCONTENT, 0, 0);
}

int UICombox::Dir(_In_ UINT attr, _In_ LPCTSTR lpszWildCard)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_DIR, attr, (LPARAM)lpszWildCard);
}

int UICombox::FindString(_In_ int nStartAfter, _In_z_ LPCTSTR lpszString) const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_FINDSTRING, nStartAfter,
		(LPARAM)lpszString);
}

int UICombox::SelectString(int nStartAfter, LPCTSTR lpszString)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SELECTSTRING,
		nStartAfter, (LPARAM)lpszString);
}

void UICombox::Clear()
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, WM_CLEAR, 0, 0);
}

void UICombox::Copy()
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, WM_COPY, 0, 0);
}

void UICombox::Cut()
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, WM_CUT, 0, 0);
}

void UICombox::Paste()
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, WM_PASTE, 0, 0);
}

int UICombox::SetItemHeight(int nIndex, UINT cyItemHeight)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETITEMHEIGHT, nIndex, MAKELONG(cyItemHeight, 0));
}

int UICombox::GetItemHeight(int nIndex) const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETITEMHEIGHT, nIndex, 0L);
}

int UICombox::FindStringExact(int nIndexStart, LPCTSTR lpszFind) const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_FINDSTRINGEXACT, nIndexStart, (LPARAM)lpszFind);
}

int UICombox::SetExtendedUI(BOOL bExtended)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETEXTENDEDUI, bExtended, 0L);
}

BOOL UICombox::GetExtendedUI() const
{
	ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, CB_GETEXTENDEDUI, 0, 0L);
}

void UICombox::GetDroppedControlRect(LPRECT lprect) const
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)lprect);
}

BOOL UICombox::GetDroppedState() const
{
	ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, CB_GETDROPPEDSTATE, 0, 0L);
}

LCID UICombox::GetLocale() const
{
	ASSERT(::IsWindow(m_hWnd)); return (LCID)::SendMessage(m_hWnd, CB_GETLOCALE, 0, 0);
}

LCID UICombox::SetLocale(LCID nNewLocale)
{
	ASSERT(::IsWindow(m_hWnd)); return (LCID)::SendMessage(m_hWnd, CB_SETLOCALE, (WPARAM)nNewLocale, 0);
}

int UICombox::GetTopIndex() const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETTOPINDEX, 0, 0);
}

int UICombox::SetTopIndex(int nIndex)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETTOPINDEX, nIndex, 0);
}

int UICombox::InitStorage(int nItems, UINT nBytes)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_INITSTORAGE, (WPARAM)nItems, nBytes);
}

void UICombox::SetHorizontalExtent(UINT nExtent)
{
	ASSERT(::IsWindow(m_hWnd)); ::SendMessage(m_hWnd, CB_SETHORIZONTALEXTENT, nExtent, 0);
}

UINT UICombox::GetHorizontalExtent() const
{
	ASSERT(::IsWindow(m_hWnd)); return (UINT)::SendMessage(m_hWnd, CB_GETHORIZONTALEXTENT, 0, 0);
}

int UICombox::SetDroppedWidth(UINT nWidth)
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_SETDROPPEDWIDTH, nWidth, 0);
}

int UICombox::GetDroppedWidth() const
{
	ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, CB_GETDROPPEDWIDTH, 0, 0);
}
