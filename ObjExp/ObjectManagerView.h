#pragma once

#include <VirtualListView.h>
#include "Interfaces.h"
#include "resource.h"
#include "ViewBase.h"
#include "ObjectManager.h"
#include <TreeViewHelper.h>
#include <QuickFindEdit.h>
#include <SortedFilteredVector.h>
#include <CustomSplitterWindow.h>
#include "TimerManager.h"

class CObjectManagerView :
	public CViewBase<CObjectManagerView>,
	public CTimerManager<CObjectManagerView>,
	public CVirtualListView<CObjectManagerView>,
	public CTreeViewHelper<CObjectManagerView> {
public:
	using CViewBase::CViewBase;

	CString GetDirectoryPath() const;
	CString GetDirectoryPath(HTREEITEM hItem) const;
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
	void DoTimerUpdate();

	//
	// treeview overrides
	//
	void OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew);
	bool OnTreeRightClick(HWND tree, HTREEITEM hItem, POINT const& pt);
	bool OnTreeDoubleClick(HWND tree, HTREEITEM hItem);

	BEGIN_MSG_MAP(CObjectManagerView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(::RegisterWindowMessage(L"WTLHelperUpdateTheme"), OnUpdateTheme)
		MESSAGE_HANDLER(WM_UPDATE_DARKMODE, OnUpdateTheme)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnListCustomDraw)
		CHAIN_MSG_MAP(CTimerManager<CObjectManagerView>)
		COMMAND_CODE_HANDLER(EN_DELAYCHANGE, OnQuickTextChanged)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_JUMPTOTARGET, OnJumpToTarget)
		COMMAND_ID_HANDLER(ID_VIEW_QUICKFIND, OnQuickFind)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_SECURITY, OnEditSecurity)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_OBJECTTREE_COPYFULLDIRECTORYNAME, OnCopyDirectoryName)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_COPYFULLOBJECTPATH, OnCopyFullObjectPath)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_SHOWDIRECTORIESINLIST, OnShowDirectories)
		COMMAND_ID_HANDLER(ID_OBJECTLIST_LISTMODE, OnSwitchToListMode)
		CHAIN_MSG_MAP(CVirtualListView<CObjectManagerView>)
		CHAIN_MSG_MAP(CTreeViewHelper<CObjectManagerView>)
		CHAIN_MSG_MAP(CViewBase<CObjectManagerView>)
		CHAIN_MSG_MAP_ALT(CTimerManager<CObjectManagerView>, 1)
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
	LRESULT OnUpdateTheme(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnListCustomDraw(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	enum class ObjectState {
		None, New, Deleted,
	};

	struct ObjectData {
		CString Name, FullName, Type, SymbolicLinkTarget;
		ObjectState State{ ObjectState::None };
		DWORD64 TargetTime{ 0 };	// when the New/Deleted highlight expires
	};

	static constexpr DWORD64 HighlightDuration = 2000;

	void InitTree();
	void UpdateList(bool newNode);
	bool ShowProperties(int index) const;
	bool ShowProperties(HTREEITEM hItem) const;
	bool ShowProperties(PCWSTR fullName, PCWSTR type, PCWSTR target = nullptr) const;
	void EnumDirectory(CTreeItem root, const CString& path);
	std::vector<ObjectData> EnumCurrentObjects();
	static void EnumObjectsInDirectory(CString const path, std::vector<ObjectData>& objects);
	void ApplyFilter(PCWSTR filter);
	void UpdateStatusText();

	static bool CompareItems(const ObjectData& data1, const ObjectData& data2, int col, bool asc);

private:
	CTreeViewCtrlEx m_Tree;
	CImageListManaged m_TreeImages;	// the tree doesn't destroy its image list
	CListViewCtrl m_List;
	CQuickFindEdit m_QuickFind;
	SortedFilteredVector<ObjectData> m_Objects;
	CCustomSplitterWindow m_Splitter;
	ObjectManager m_mgr;
	CString m_FilterText;
	CString m_SelectedObjectFullName;
	bool m_ShowDirectories{ false };
	bool m_ListMode{ false };
	COLORREF m_Green, m_Red;
};

