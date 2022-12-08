#pragma once
#include "UIWnd.h"

/*
将MFC的CComboBox移到CWndUI
@see https://blog.csdn.net/u011471873/article/details/50997512
组合框分类：
简单组合框 - CBS_SIMPLE
下拉式组合框 - CBS_DROPDOWN，可以输入，也可以下拉选择
下拉式列表组合框 - CBS_DROPDOWNLIST，只可以选择，不可以输入

通知消息:
CBN_SELCHANGE - 当选择项发生变化后，产生的消息。
CBN_EDITCHANGE - 当输入发生变化后，产生的消息。本消息是在Windows刷新屏幕之后发出的。
CBN_EDITUPDATE - 当输入发生变化后，产生的消息。本通知消息在控件已经格式化了文本但没有显示时发送。
CBN_CLOSEUP - 组合框的列表已被关闭。对于风格为CBS_SIMPLE的组合框来说，不会发送该通知消息。
CBN_DROPDOWN - 下拉出下拉列表(变为可见)...
*/
class UICombox : public CWndUI
{
public:
	UICombox();
	~UICombox();

	virtual BOOL Create(DWORD dwStyle, const RECT& rect, HWND pParentWnd, UINT nID);
	virtual BOOL OnChildNotify(UINT, WPARAM, LPARAM, LRESULT*);
	// Overridables (must override draw, measure and compare for owner draw)
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual int CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct);
	virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);


	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int nSelect);
	DWORD GetEditSel() const;
	BOOL LimitText(int nMaxChars);
	BOOL SetEditSel(_In_ int nStartChar, _In_ int nEndChar);
	DWORD_PTR GetItemData(int nIndex) const;
	int SetItemData(int nIndex, DWORD_PTR dwItemData);
	void* GetItemDataPtr(int nIndex) const;
	int SetItemDataPtr(int nIndex, void* pData);
	DuiLib::CDuiString GetText() const;
#pragma warning(push)
#pragma warning(disable: 6001 6054)
	int GetLBText(_In_ int nIndex, _Pre_notnull_ _Post_z_ LPTSTR lpszText) const;
#pragma warning(pop)
	int GetLBTextLen(int nIndex) const;
	void ShowDropDown(BOOL bShowIt);
	int AddString(LPCTSTR lpszString);
	int DeleteString(UINT nIndex);
	int InsertString(_In_ int nIndex, _In_z_ LPCTSTR lpszString);
	void ResetContent();
	int Dir(_In_ UINT attr, _In_ LPCTSTR lpszWildCard);
	int FindString(_In_ int nStartAfter, _In_z_ LPCTSTR lpszString) const;
	int SelectString(int nStartAfter, LPCTSTR lpszString);
	void Clear();
	void Copy();
	void Cut();
	void Paste();
	int SetItemHeight(int nIndex, UINT cyItemHeight);
	int GetItemHeight(int nIndex) const;
	int FindStringExact(int nIndexStart, LPCTSTR lpszFind) const;
	int SetExtendedUI(BOOL bExtended);
	BOOL GetExtendedUI() const;
	void GetDroppedControlRect(LPRECT lprect) const;
	BOOL GetDroppedState() const;
	LCID GetLocale() const;
	LCID SetLocale(LCID nNewLocale);
	int GetTopIndex() const;
	int SetTopIndex(int nIndex);
	int InitStorage(int nItems, UINT nBytes);
	void SetHorizontalExtent(UINT nExtent);
	UINT GetHorizontalExtent() const;
	int SetDroppedWidth(UINT nWidth);
	int GetDroppedWidth() const;
};

