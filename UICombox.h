#pragma once
#include "UIWnd.h"

/*
��MFC��CComboBox�Ƶ�CWndUI
@see https://blog.csdn.net/u011471873/article/details/50997512
��Ͽ���ࣺ
����Ͽ� - CBS_SIMPLE
����ʽ��Ͽ� - CBS_DROPDOWN���������룬Ҳ��������ѡ��
����ʽ�б���Ͽ� - CBS_DROPDOWNLIST��ֻ����ѡ�񣬲���������

֪ͨ��Ϣ:
CBN_SELCHANGE - ��ѡ������仯�󣬲�������Ϣ��
CBN_EDITCHANGE - �����뷢���仯�󣬲�������Ϣ������Ϣ����Windowsˢ����Ļ֮�󷢳��ġ�
CBN_EDITUPDATE - �����뷢���仯�󣬲�������Ϣ����֪ͨ��Ϣ�ڿؼ��Ѿ���ʽ�����ı���û����ʾʱ���͡�
CBN_CLOSEUP - ��Ͽ���б��ѱ��رա����ڷ��ΪCBS_SIMPLE����Ͽ���˵�����ᷢ�͸�֪ͨ��Ϣ��
CBN_DROPDOWN - �����������б�(��Ϊ�ɼ�)...
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

