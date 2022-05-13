#pragma once

#include "VirtualListView.h"
#include "Interfaces.h"
#include "resource.h"
#include "ViewBase.h"
#include "ObjectManager.h"
#include "TreeViewHelper.h"
#include "QuickFindEdit.h"
#include "SortedFilteredVector.h"

class CObjectManagerView :
	public CViewBase<CObjectManagerView>,
	public CVirtualListView<CObjectManagerView>,
	public CTreeViewHelper<CObjectManagerView> {
public:
	using CViewBase::CViewBase;

	CString GetDirectoryPath() const;
	void DoSort(const SortInfo* si);
	CString GetColumnText(HWND, int row, int col);
	int GetRowImage(HWND, int row, int col) const;
	CString GetTitle() const override;
	void DoFind(const CString& text, DWORD flags);
	void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	bool JumpToObject(CString const& fullName);
	void OnPageActivated(bool active);

	//
	// treeview overrides
	//
	void OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew);
	bool OnTreeRightClick(HWND tree, HTREEITEM hItem, POINT const& pt);
	bool OnTreeDoubleClick(HWND tree, HTREEITEM hItem);

	BEGIN_MSG_MAP(CObjectManagerView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_CODE_HANDLER(EN_DELAYCHANGE, OnQuickTextChanged)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_JUMPTOTARGET, OnJumpToTarget)
		COMMAND_ID_HANDLER(ID_VIEW_QUICKFIND, OnQuickFind)
		//COMMAND_ID_HANDLER(ID_EDIT_SECURITY, OnEditSecurity)
		CHAIN_MSG_MAP(CVirtualListView<CObjectManagerView>)
		CHAIN_MSG_MAP(CTreeViewHelper<CObjectManagerView>)
		CHAIN_MSG_MAP(CViewBase<CObjectManagerView>)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_OBJECTTREE_COPYFULLDIRECTORYNAME, OnCopyDirectoryName)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_COPYFULLOBJECTPATH, OnCopyFullObjectPath)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_SHOWDIRECTORIESINLIST, OnShowDirectories)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_LISTMODE, OnSwitchToListMode)
	END_MSG_MAP()

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditSecurity(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyDirectoryName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyFullObjectPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnJumpToTarget(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnQuickTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowDirectories(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSwitchToListMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	struct ObjectData {
		CString Name, FullName, Type, SymbolicLinkTarget;
	};

	void InitTree();
	void UpdateList(bool newNode);
	bool ShowProperties(int index) const;
	bool ShowProperties(HTREEITEM hItem) const;
	bool ShowProperties(PCWSTR fullName, PCWSTR type, PCWSTR target = nullptr) const;
	void EnumDirectory(CTreeItem root, const CString& path);
	void EnumAllObjects();
	void EnumObjectsInDirectory(CString const path, SortedFilteredVector<ObjectData>& objects);
	void ApplyFilter();
	void UpdateStatusText();

	static bool CompareItems(const ObjectData& data1, const ObjectData& data2, int col, bool asc);

private:
	CTreeViewCtrlEx m_Tree;
	CListViewCtrl m_List;
	CQuickFindEdit m_QuickFind;
	SortedFilteredVector<ObjectData> m_Objects;
	CSplitterWindow m_Splitter;
	ObjectManager m_mgr;
	CString m_FilterText;
	CString m_SelectedObjectFullName;
	bool m_ShowDirectories{ false };
	bool m_ListMode{ false };
};

