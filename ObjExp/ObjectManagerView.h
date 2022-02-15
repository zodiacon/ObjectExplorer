#pragma once

#include "VirtualListView.h"
#include "Interfaces.h"
#include "resource.h"
#include "ViewBase.h"
#include "ObjectManager.h"

class CObjectManagerView :
	public CViewBase<CObjectManagerView>,
	public CVirtualListView<CObjectManagerView> {
public:
	using CViewBase::CViewBase;

	CString GetDirectoryPath() const;
	void OnFinalMessage(HWND) override;
	void DoSort(const SortInfo* si);
	CString GetColumnText(HWND, int row, int col);
	int GetRowImage(HWND, int row, int col) const;
	CString GetTitle() const override;
	void DoFind(const CString& text, DWORD flags);
	void UpdateUI(CUpdateUIBase& ui, bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;

	BEGIN_MSG_MAP(CObjectManagerView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnTreeSelectionChanged)
		NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnListStateChanged)
		NOTIFY_CODE_HANDLER(LVN_ODSTATECHANGED, OnListStateChanged)
		//COMMAND_ID_HANDLER(ID_EDIT_SECURITY, OnEditSecurity)
		CHAIN_MSG_MAP(CVirtualListView<CObjectManagerView>)
		CHAIN_MSG_MAP(CViewBase<CObjectManagerView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTreeSelectionChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditSecurity(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListStateChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void InitTree();
	void UpdateList(bool newNode);
	bool ShowProperties(int index) const;
	void EnumDirectory(CTreeItem root, const CString& path);

	struct ObjectData {
		CString Name, FullName, Type, SymbolicLinkTarget;
	};
	static bool CompareItems(const ObjectData& data1, const ObjectData& data2, int col, bool asc);

private:
	CTreeViewCtrlEx m_Tree;
	CListViewCtrl m_List;
	std::vector<ObjectData> m_Objects;
	CSplitterWindow m_Splitter;
	ObjectManager m_mgr;
};

