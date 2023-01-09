#pragma once

/////////////////////////////////////////////////////////////////////////////
// CTreeListView - A TreeView with additional columns
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2001-2005 Bjarke Viksoe.
//
// Partly implemented from a MFC CTreeListView control by Gerolf Kühnel
// available at www.codeproject.com.
// Horizontal scrolling supplied by Oleg Reabciuc (olegr@compudava.com).
// Nail Kaipov fixed the horizontal scrollbar code (roof@crypt.nsk.ru).
// Several improvements by Sergey Solozhentsev - especially around
// getting the selection colors right.
//
// This code may be used in compiled form in any way you desire. This
// source file may be redistributed by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. 
//
// Pavel Yosifovich (2022): code cleanup, modern compiler fixes, safe string functions, bug fixes

// The TreeListView item structure
typedef struct _TLVITEM {
	UINT     mask;
	int      iSubItem;
	UINT     state;
	UINT     stateMask;
	UINT     format;
	LPTSTR   pszText;
	UINT     cchTextMax;
	int      iImage;
	COLORREF clrText;
	COLORREF clrBack;
	LPARAM   lParam;
} TLVITEM, * LPTLVITEM;

// TreeListView mask flags
#define TLVIF_TEXT               0x0001
#define TLVIF_IMAGE              0x0002
#define TLVIF_PARAM              0x0004
#define TLVIF_STATE              0x0008
#define TLVIF_FORMAT             0x0010
#define TLVIF_COLOR              0x0020

// TreeListView format flags
#define TLVIFMT_LEFT             0x00000000
#define TLVIFMT_CENTER           0x00000001
#define TLVIFMT_RIGHT            0x00000002

// TreeListView state flags
#define TLVIS_BOLD               0x0001
#define TLVIS_ITALIC             0x0002
#define TLVIS_UNDERLINE          0x0004
#define TLVIS_STRIKEOUT          0x0008

#define TLVS_EX_NOFOCUSRECT      0x00000001
#define TLVS_EX_SELTOHEADER      0x00000002

template<typename T, typename TBase = ATL::CWindow, typename TWinTraits = ATL::CControlWinTraits>
class ATL_NO_VTABLE CTreeListViewImpl :
	public CWindowImpl<T, TBase, TWinTraits>,
	public CCustomDraw<T> {
public:
	typedef CTreeListViewImpl<T, TBase, TWinTraits> thisClass;

	//DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName());

	CContainedWindowT<CTreeViewCtrl> m_ctrlTree;
	CContainedWindowT<CHeaderCtrl> m_ctrlHeader;
	//
	typedef CSimpleArray< TLVITEM*> tMapItem;
	CSimpleMap<HTREEITEM, tMapItem*> m_mapItems;   // Map of extended item info
	CSimpleArray<RECT> m_rcColumns;                // List of colunm header rects
	//
	CFont m_fontHeader;                              // Header font
	LONG m_cxHeader;                                 // Total header sizes
	LONG m_nOffset;                                  // Horiz. scrollbar offset
	//
	UINT m_iItemState;                               // Itemstate of item currently being painted
	RECT m_rcItem;                                   // Rect of item currently being painted
	HTREEITEM m_hSelItem;                            // Currently selected item (incl. dropitem)
	COLORREF m_clrSelection;                         // Background color for selected item
	COLORREF m_clrSelectionText;                     // Text color for selected item
	DWORD m_dwExStyle;                               // Extended TreeListView styles
	DWORD m_dwHeaderStyle;                           // Style for header at creation

	CTreeListViewImpl() :
		m_cxHeader(0),
		m_nOffset(0),
		m_dwExStyle(0),
		m_dwHeaderStyle(WS_CHILD | WS_VISIBLE | HDS_HORZ) {
	}

	// Operations

	BOOL SubclassWindow(HWND hWnd) {
		auto p = static_cast<T*>(this);
		ATLASSERT(p->m_hWnd == NULL);
		ATLASSERT(::IsWindow(hWnd));
		BOOL bRet = CWindowImpl<T, TBase, TWinTraits>::SubclassWindow(hWnd);
		if (bRet) _Init();
		return bRet;
	}

	BOOL SetSubItem(HTREEITEM hItem, const LPTLVITEM pItem) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		ATLASSERT(!::IsBadReadPtr(pItem, sizeof(TLVITEM)));
		if (pItem->iSubItem < 0 || pItem->iSubItem >= m_ctrlHeader.GetItemCount()) return FALSE;

		LPTLVITEM pItemT = _GetSubItem(hItem, pItem->iSubItem);
		ATLASSERT(pItemT);
		if (pItemT == NULL) return FALSE;

		// Copy attributes from caller's TLVITEM to internally
		// stored TLVITEM structure.
		if (pItem->mask & TLVIF_TEXT) {
			if (pItemT->mask & TLVIF_TEXT) ATLTRY(delete[] pItemT->pszText);
			int len;
			ATLTRY(pItemT->pszText = new TCHAR[(len = ::lstrlen(pItem->pszText)) + 1]);
			::StringCchCopy(pItemT->pszText, len + 1, pItem->pszText);
			pItemT->mask |= TLVIF_TEXT;
		}
		if (pItem->mask & TLVIF_IMAGE) {
			pItemT->iImage = pItem->iImage;
			pItemT->mask |= TLVIF_IMAGE;
		}
		if (pItem->mask & TLVIF_PARAM) {
			pItemT->lParam = pItem->lParam;
			pItemT->mask |= TLVIF_PARAM;
		}
		if (pItem->mask & TLVIF_FORMAT) {
			pItemT->format = pItem->format;
			pItemT->mask |= TLVIF_FORMAT;
		}
		if (pItem->mask & TLVIF_STATE) {
			pItemT->state &= ~pItem->stateMask;
			pItemT->state |= (pItem->state & pItem->stateMask);
			pItemT->mask |= TLVIF_STATE;
		}
		if (pItem->mask & TLVIF_COLOR) {
			pItemT->clrText = pItem->clrText;
			pItemT->clrBack = pItem->clrBack;
			pItemT->mask |= TLVIF_COLOR;
		}

		return TRUE;
	}
	BOOL GetSubItem(HTREEITEM hItem, LPTLVITEM pItem) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		ATLASSERT(!::IsBadWritePtr(pItem, sizeof(TLVITEM)));
		LPTLVITEM pItemT = _GetSubItem(hItem, pItem->iSubItem);
		if (pItemT == NULL) return FALSE;
		// Copy item data
		UINT mask = pItem->mask;
		if (mask & TLVIF_TEXT) {
			ATLASSERT(!::IsBadWritePtr(pItem->pszText, pItem->cchTextMax));
			::StringCchCopy(pItem->pszText, pItem->cchTextMax, pItemT->pszText == NULL ? _T("") : pItemT->pszText);
		}
		if (mask & TLVIF_IMAGE) pItem->iImage = pItemT->iImage;
		if (mask & TLVIF_FORMAT) pItem->format = pItemT->format;
		if (mask & TLVIF_STATE) {
			pItem->state &= ~pItem->stateMask;
			pItem->state |= pItemT->state & pItem->stateMask;
		}
		if (mask & TLVIF_COLOR) pItem->clrText = pItemT->clrText;
		if (mask & TLVIF_PARAM) pItem->lParam = pItemT->lParam;
		return TRUE;
	}
	BOOL SetSubItemText(HTREEITEM hItem, int nSubItem, LPCTSTR pstrString, DWORD format = TLVIFMT_LEFT) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		ATLASSERT(!::IsBadStringPtr(pstrString, (UINT)-1));
		TLVITEM itm = { 0 };
		itm.iSubItem = nSubItem;
		itm.mask = TLVIF_TEXT | TLVIF_FORMAT;
		itm.format = format;
		itm.pszText = const_cast<LPTSTR>(pstrString);
		return SetSubItem(hItem, &itm);
	}
	BOOL GetSubItemText(HTREEITEM hItem, int nSubItem, LPTSTR pstrString, UINT cchMax) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		ATLASSERT(!::IsBadWritePtr(pstrString, cchMax));
		LPTLVITEM pItem = _GetSubItem(hItem, nSubItem);
		if (pItem == NULL) return FALSE;
		::StringCchCopy(pstrString, cchMax, pItem->pszText);
		return TRUE;
	}
	COLORREF GetSubItemColor(HTREEITEM hItem, int nSubItem, COLORREF* pBackColor) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		LPTLVITEM pItem = _GetSubItem(hItem, nSubItem);
		if (pItem == NULL) return CLR_NONE;
		if (pBackColor) {
			*pBackColor = pItem->clrBack;
		}
		return pItem->clrText;
	}
	BOOL SetSubItemColor(HTREEITEM hItem, int nSubItem, COLORREF clrText, COLORREF clrBack = CLR_NONE) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		LPTLVITEM pItem = _GetSubItem(hItem, nSubItem);
		if (pItem == NULL) return FALSE;
		pItem->clrBack = clrBack;
		pItem->clrText = clrText;
		pItem->mask |= TLVIF_COLOR;
		return TRUE;
	}
	UINT GetSubItemState(HTREEITEM hItem, int nSubItem, UINT StateMask = (UINT)-1) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		LPTLVITEM pItem = _GetSubItem(hItem, nSubItem);
		if (pItem == NULL) return 0;
		return pItem->state & StateMask;
	}
	BOOL SetSubItemState(HTREEITEM hItem, int nSubItem, UINT State, UINT StateMask) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		ATLASSERT(hItem);
		LPTLVITEM pItem = _GetSubItem(hItem, nSubItem);
		if (pItem == NULL) return FALSE;
		pItem->state &= ~StateMask;
		pItem->state |= State;
		pItem->mask |= TLVIF_STATE;
		return TRUE;
	}
	void SetHeaderStyle(DWORD dwHeaderStyle) {
		auto p = static_cast<T*>(this);
		ATLASSERT(!::IsWindow(p->m_hWnd));  // Before control is created, please!
		m_dwHeaderStyle = dwHeaderStyle;
	}
	DWORD SetExtendedTreeListStyle(DWORD dwStyle) {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));
		DWORD dwOldStyle = m_dwExStyle;
		m_dwExStyle = dwStyle;
		p->Invalidate();
		return dwOldStyle;
	}

	CTreeViewCtrl GetTreeControl() const {
		ATLASSERT(::IsWindow(m_ctrlTree));
		return m_ctrlTree;
	}
	CHeaderCtrl GetHeaderControl() const {
		ATLASSERT(::IsWindow(m_ctrlHeader));
		return m_ctrlHeader;
	}

	// Implementation

	void _Init() {
		auto p = static_cast<T*>(this);
		ATLASSERT(::IsWindow(p->m_hWnd));

		// This is a Platform SDK define which we need
#ifndef TVS_NOHSCROLL
		const UINT TVS_NOHSCROLL = 0x8000;
#endif

		// Create the tree control
		// Thanks to Nicola Tufarelli for suggesting using the GetDlgCtrlID() to
		// preserve the original control ID...
		DWORD dwStyle = p->GetStyle();
		UINT nID = p->GetDlgCtrlID();
		m_ctrlTree.Create(this, 1, p->m_hWnd, &p->rcDefault, NULL, dwStyle, 0, nID);
		ATLASSERT(m_ctrlTree.IsWindow());
		m_ctrlTree.ModifyStyle(0, TVS_NOHSCROLL | TVS_FULLROWSELECT);  // we need these

		// Create the header control
		m_ctrlHeader.Create(this, 2, p->m_hWnd, &p->rcDefault, nullptr, m_dwHeaderStyle | HDS_FULLDRAG);
		ATLASSERT(m_ctrlHeader.IsWindow());
		::SetWindowTheme(m_ctrlHeader, L" ", L" ");
		p->SendMessage(WM_SETTINGCHANGE);

		p->UpdateLayout();

		// FIX: When used in a view...
		m_ctrlTree.ShowWindow(SW_SHOW);
		m_ctrlHeader.ShowWindow(SW_SHOW);
	}
	LPTLVITEM _GetSubItem(HTREEITEM hItem, int iSubItem) {
		ATLASSERT(hItem);
		if (iSubItem<0 || iSubItem>m_ctrlHeader.GetItemCount()) return NULL;
		tMapItem* pVal = m_mapItems.Lookup(hItem);
		ATLASSERT(pVal);
		ATLASSERT(iSubItem < pVal->GetSize());
		if (pVal == NULL) return NULL;
		if (iSubItem >= pVal->GetSize()) return NULL;
		return (*pVal)[iSubItem];
	}
	void _DeleteSubItem(const LPTLVITEM pItem) {
		ATLASSERT(!::IsBadReadPtr(pItem, sizeof(TLVITEM)));
		if (pItem->mask & TLVIF_TEXT) ATLTRY(delete[] pItem->pszText);
		ATLTRY(delete pItem);
	}
	void _CalcColumnSizes() {
		// Keep track of header sizes
		m_rcColumns.RemoveAll();
		m_cxHeader = 0;
		int nHeaders = m_ctrlHeader.GetItemCount();
		for (int i = 0; i < nHeaders; i++) {
			RECT rc;
			m_ctrlHeader.GetItemRect(i, &rc);
			m_rcColumns.Add(rc);
			m_cxHeader += rc.right - rc.left;
		}

		auto p = static_cast<T*>(this);

		// FIX: Nail Kaipov fixed the horizontal scrollbar code
		// If the width of all headers is bigger than the width of the client-area 
		// of the TreeView, then the Scrollbar is to be enabled
		RECT rcClient;

		p->GetClientRect(&rcClient);
		if (p->GetStyle() & WS_VSCROLL) rcClient.right += ::GetSystemMetrics(SM_CXHTHUMB);
		LONG cxClient = (rcClient.right - rcClient.left);
		if (m_cxHeader - cxClient >= 0) {
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			si.nMax = m_cxHeader;
			si.nMin = 0;
			si.nPage = cxClient;
			si.nPos = p->GetScrollPos(SB_HORZ);   // Preserve scroller position
			p->SetScrollInfo(SB_HORZ, &si, TRUE);  // Setup scrollbar params
			p->ModifyStyle(0, WS_HSCROLL, SWP_FRAMECHANGED);
			m_nOffset = -si.nPos;    // Save scrollbar position for correct drawing
			if (si.nPos) {          // Scroll position is non-zero. code is stolen from OnHScroll()
				RECT rcHeader, rcTree;
				m_ctrlHeader.GetClientRect(&rcHeader);
				m_ctrlTree.GetClientRect(&rcTree);
				m_ctrlHeader.SetWindowPos(NULL,
					-si.nPos,
					0,
					abs(si.nPos) + ::GetSystemMetrics(SM_CXVSCROLL) + 1 + rcTree.right - rcTree.left,
					rcHeader.bottom - rcHeader.top,
					SWP_SHOWWINDOW);
			}
		}
		else {
			p->SetScrollPos(SB_HORZ, 0, TRUE);
			p->ModifyStyle(WS_HSCROLL, 0, SWP_FRAMECHANGED);
			m_nOffset = 0;
			p->Invalidate();
		}
	}

	// Message map and handlers

	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkGnd)
		CHAIN_MSG_MAP(CCustomDraw< T >)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEMA, OnTreeItemDelete)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEMW, OnTreeItemDelete)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDEDA, OnTreeItemExpanded)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDEDW, OnTreeItemExpanded)
		NOTIFY_CODE_HANDLER(HDN_ITEMCHANGEDA, OnHeaderItemChanged)
		NOTIFY_CODE_HANDLER(HDN_ITEMCHANGEDW, OnHeaderItemChanged)
		NOTIFY_CODE_HANDLER(NM_RELEASEDCAPTURE, OnHeaderItemChanged)
		NOTIFY_CODE_HANDLER(HDN_BEGINDRAG, OnHeaderBeginDrag)
		NOTIFY_CODE_HANDLER(HDN_ENDDRAG, OnHeaderEndDrag)
		FORWARD_NOTIFICATIONS()
		ALT_MSG_MAP(1) // Tree
		MESSAGE_HANDLER(WM_KILLFOCUS, OnTreeKillFocus)
		MESSAGE_HANDLER(WM_NCDESTROY, OnTreeNcDestroy)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnTreeFixButtonHit)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnTreeFixButtonHit)
		MESSAGE_HANDLER(TVM_INSERTITEM, OnTreeItemInsert)
		MESSAGE_HANDLER(TVM_SETBKCOLOR, OnTreeSetColor)
		MESSAGE_HANDLER(TVM_SETTEXTCOLOR, OnTreeSetColor)
		ALT_MSG_MAP(2) // Header
		MESSAGE_HANDLER(HDM_INSERTITEM, OnHeaderItemInsert)
		MESSAGE_HANDLER(HDM_DELETEITEM, OnHeaderItemDelete)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		// Do not allow the TreeView control to initialize here! 
		// We are creating new child controls ourselves in the _Init() method.
		_Init();
		return 0;
	}

	bool AddColumn(PCWSTR text, int width, DWORD format = HDF_LEFT) {
		auto header = GetHeaderControl();
		HDITEM col;
		col.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
		col.fmt = format;
		col.cxy = width;
		col.pszText = (PWSTR)text;
		return header.InsertItem(header.GetItemCount(), &col);
	}

	LRESULT OnSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		if (!m_fontHeader.IsNull()) m_fontHeader.DeleteObject();
		NONCLIENTMETRICS ncm = { 0 };
		ncm.cbSize = sizeof(ncm);
		::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
		m_fontHeader.CreateFontIndirect(&ncm.lfMenuFont);
		m_ctrlHeader.SetFont(m_fontHeader);

		auto p = static_cast<T*>(this);
		p->Invalidate();

		return 0;
	}
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		m_ctrlTree.SetFocus();
		return 0;
	}

	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		T* pT = static_cast<T*>(this);
		pT->UpdateLayout();
		return 0;
	}

	LRESULT OnEraseBkGnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		return 1; // Children fill entire client area
	}

	LPARAM OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		// Thanks to Oleg Reabciuc for providing the horizontal scrolling
		// support for this control

		int nSBCode = (int)LOWORD(wParam);
		int nPos = (int)HIWORD(wParam);

		RECT rcClient;
		m_ctrlTree.GetClientRect(&rcClient);
		auto p = static_cast<T*>(this);

		int cxClient = abs(rcClient.right - rcClient.left);   // One Page
		const int nWidthLine = 6;               // Microsoft's line-step in a CListCtrl, got from MFC - sourcecode
		int nCurPos = p->GetScrollPos(SB_HORZ);    // Current scrollingposition
		int nPrevPos = nCurPos;                 // Save current scrolling position for calculating

		int nScrollMin;                         // Minimum scrolling value
		int nScrollMax;                         // Maximum scrolling value
		p->GetScrollRange(SB_HORZ, &nScrollMin, &nScrollMax);

		// Check which kind of scroll is wanted
		switch (nSBCode) {
			case SB_LEFT:                          // Scoll to left most position
				nCurPos = 0;
				break;
			case SB_RIGHT:                         // Scroll to right most position
				nCurPos = nScrollMax;
				break;
			case SB_LINELEFT:                      // Scroll left with the button
				nCurPos = std::max(nCurPos - nWidthLine, 0);
				break;
			case SB_LINERIGHT:                     // Scroll right with the button
				nCurPos = std::min(nCurPos + nWidthLine, nScrollMax);
				break;
			case SB_PAGELEFT:                      // Scroll left with a click to the background of the scrollbar
				nCurPos = std::max(nCurPos - cxClient, 0);
				break;
			case SB_PAGERIGHT:                     // Scroll left with a click to the background of the scrollbar
				nCurPos = std::min(nCurPos + cxClient, nScrollMax);
				break;
			case SB_THUMBPOSITION:                 // Scroll by moving the scrollbutton with the mouse
			case SB_THUMBTRACK:                    // Drop the scrollbarbutton
				// Check for illegal positions and correct them (out of the scrollbar?)
				if (nPos == 0) {
					nCurPos = 0;
				}
				else {
					nCurPos = std::min<int>(StretchWidth(nPos, nWidthLine), nScrollMax);
				}
		}

		// Move the scrollbarbutton to the position (graphically)
		p->SetScrollPos(SB_HORZ, nCurPos, TRUE);
		m_nOffset = -nCurPos;

		// Smoothly Scroll the Tree control
		RECT rcTree;
		m_ctrlTree.GetClientRect(&rcTree);
		m_ctrlTree.ScrollWindowEx(nPrevPos - nCurPos, 0, NULL, NULL, NULL, NULL, SW_INVALIDATE);

		RECT rcHeader;
		p->GetClientRect(&rcClient);
		m_ctrlHeader.GetClientRect(&rcHeader);

		if (rcTree.right - rcTree.left != 0) {
			int iVSWidth = ::GetSystemMetrics(SM_CXVSCROLL) + 1;
			m_ctrlHeader.SetWindowPos(NULL, m_nOffset, 0, abs(m_nOffset) + iVSWidth + rcTree.right - rcTree.left, rcHeader.bottom - rcHeader.top, SWP_SHOWWINDOW);
		}

		// Redraw the treecontrol so you can see the scrolling
		m_ctrlTree.Invalidate();
		return 0;
	}

	// Tree control

	LRESULT OnTreeNcDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
		// Clean up memory on destruction
		m_ctrlTree.DeleteAllItems();
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnTreeKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
		// HACK: Fix the TreeView focus paint problem
		m_ctrlTree.Invalidate();
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnTreeFixButtonHit(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		// If a click is to the right of the label then we're still clicking on the item...
		// NOTE: Handling TVM_HITTEST would be a lot nicer, but the TreeView control doesn't 
		//       actually use it internally.
		// FIX: On Windows XP the selection fails quickly if you move the mouse; so we
		//      just select it immediately.
		RECT rcClient;
		auto p = static_cast<T*>(this);
		p->GetClientRect(&rcClient);
		int x = GET_X_LPARAM(lParam) - m_nOffset;
		int y = GET_Y_LPARAM(lParam);
		if (x > rcClient.right - rcClient.left) x = rcClient.right - 40;
		TVHITTESTINFO hti;
		hti.pt.x = x;
		hti.pt.y = y;
		m_ctrlTree.HitTest(&hti);
		if (hti.flags == TVHT_ONITEMRIGHT) return m_ctrlTree.SelectItem(hti.hItem);
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnTreeItemInsert(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		HTREEITEM hItem = (HTREEITEM)m_ctrlTree.DefWindowProc(uMsg, wParam, lParam);
		if (hItem == NULL) return (LRESULT)hItem;

		// Create a new item
		tMapItem* pVal;
		ATLTRY(pVal = new tMapItem);
		ATLASSERT(pVal);
		LPTLVITEM pItem;
		int nHeaders = m_ctrlHeader.GetItemCount();
		for (int i = 0; i < nHeaders; i++) {
			ATLTRY(pItem = new TLVITEM);
			ATLASSERT(pItem);
			::ZeroMemory(pItem, sizeof(TLVITEM));
			pItem->clrText = CLR_NONE;
			pItem->clrBack = CLR_NONE;
			pVal->Add(pItem);
		}
		// Add the new item
		m_mapItems.Add(hItem, pVal);
		// Then add the item structure...
		if (nHeaders > 0) {
			// Convert TVITEM into a TLVITEM
			LPTVINSERTSTRUCT pTVIS = reinterpret_cast<LPTVINSERTSTRUCT>(lParam);
			ATLASSERT(pTVIS);
			TLVITEM itm = { 0 };
			itm.iSubItem = 0;
			if (pTVIS->item.mask & TVIF_TEXT) {
				ATLASSERT(pTVIS->item.pszText != LPSTR_TEXTCALLBACK); // Not supported
				itm.mask |= TLVIF_TEXT;
				itm.pszText = pTVIS->item.pszText;
			}
			SetSubItem(hItem, &itm);
		}

		return (LRESULT)hItem;
	}
	LRESULT OnTreeItemDelete(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled) {
		LPNMTREEVIEW pnmt = (LPNMTREEVIEW)pnmh;
		HTREEITEM hItem = pnmt->itemOld.hItem;
		tMapItem* pVal = m_mapItems.Lookup(hItem);
		ATLASSERT(pVal);
		if (pVal == NULL) return 0;
		int cnt = pVal->GetSize();
		for (int i = 0; i < cnt; i++) {
			LPTLVITEM pItem = (*pVal)[i];
			_DeleteSubItem(pItem);
		}
		ATLTRY(delete pVal);
		m_mapItems.Remove(hItem);
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnTreeItemExpanded(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled) {
		// Auto-scale the first column to the largest tree-item
		if (m_ctrlHeader.GetItemCount() > 0) {
			int cx = 0;
			HTREEITEM hItem = m_ctrlTree.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
			while (hItem != NULL) {
				RECT rc = { 0 };
				m_ctrlTree.GetItemRect(hItem, &rc, TRUE);
				if (rc.left > cx) cx = rc.left;
				hItem = m_ctrlTree.GetNextItem(hItem, TVGN_NEXTVISIBLE);
			}
			HDITEM hdr = { 0 };
			hdr.mask = HDI_WIDTH;
			m_ctrlHeader.GetItem(0, &hdr);
			cx = std::max(hdr.cxy, cx);
			if (hdr.cxy != cx) {
				hdr.cxy = cx;
				m_ctrlHeader.SetItem(0, &hdr);
			}
		}
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnTreeSetColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		LRESULT lRes = m_ctrlTree.DefWindowProc(uMsg, wParam, lParam);
		T* pT = static_cast<T*>(this);
		BOOL bDummy;
		pT->OnSettingChange(0, 0, 0, bDummy);
		return lRes;
	}

	// Header control

	LRESULT OnHeaderItemInsert(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		ATLASSERT((int)wParam == m_ctrlHeader.GetItemCount()); // Only supports appending right now!
		// When adding a header we need to place a TLVITEM in
		// each tree item as well...
		LRESULT lRes = m_ctrlHeader.DefWindowProc(uMsg, wParam, lParam);
		if (lRes != -1) {
			LPTLVITEM pItem;
			for (int i = 0; i < m_mapItems.GetSize(); i++) {
				tMapItem* pVal = m_mapItems.GetValueAt(i);
				ATLTRY(pItem = new TLVITEM);
				ATLASSERT(pItem);
				::ZeroMemory(pItem, sizeof(TLVITEM));
				pItem->clrText = CLR_NONE;
				pItem->clrBack = CLR_NONE;
				pVal->Add(pItem);
			}
			_CalcColumnSizes();
		}
		return lRes;
	}
	LRESULT OnHeaderItemDelete(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		// Deleting a column header also means cleaning up all
		// the TLVITEM structures in each of the column's sub-items.
		for (int i = 0; i < m_mapItems.GetSize(); i++) {
			tMapItem* pVal = m_mapItems.GetValueAt(i);
			ATLASSERT(pVal);
			LPTLVITEM pItem = (*pVal)[(int)wParam];
			ATLASSERT(pItem);
			pVal->RemoveAt((int)wParam);
			_DeleteSubItem(pItem);
		}
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnHeaderItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled) {
		// Reposition first header columns
		BOOL bDummy;
		OnTreeItemExpanded(0, NULL, bDummy);
		// Recalc column sizes
		_CalcColumnSizes();
		// Repaint, just in case...
		T* pT = static_cast<T*>(this);
		pT->UpdateLayout();

		bHandled = FALSE;
		return 0;
	}
	LRESULT OnHeaderBeginDrag(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled) {
		LPNMHEADER pnmhd = (LPNMHEADER)pnmh;
		if (pnmhd->iItem == 0) return TRUE; // Cannot drag first column!
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnHeaderEndDrag(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled) {
		LPNMHEADER pnmhd = (LPNMHEADER)pnmh;

		// Cannot drag first column, really!
		// Bug in MS control requires this extra check.
		if (pnmhd->iItem == 0) return TRUE;
		RECT rcItem;
		m_ctrlHeader.GetItemRect(0, &rcItem);
		DWORD dwPos = ::GetMessagePos();
		POINT pt = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };
		auto p = static_cast<T*>(this);
		p->ScreenToClient(&pt);
		if (pt.x <= rcItem.right) return TRUE; // Cannot re-order first column

		// Need to reposition the header
		T* pT = static_cast<T*>(this);
		pT->UpdateLayout();

		bHandled = FALSE;
		return 0;
	}

	// Helper methods

	static long StretchWidth(long nWidth, long nMeasure) {
		return ((nWidth / nMeasure) + 1) * nMeasure;
	}

	// Custom draw

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw) {
		if (lpNMCustomDraw->hdr.hwndFrom != m_ctrlTree) return CDRF_DODEFAULT;
		::SetViewportOrgEx(lpNMCustomDraw->hdc, m_nOffset, 0, NULL);

		// Sergey Solozhentsev provided better detection of selected/drop item
		// Among other things, we now support the TVS_SHOWSELALWAYS style properly.
		m_clrSelection = ::GetSysColor(COLOR_HIGHLIGHT);
		m_clrSelectionText = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		m_hSelItem = m_ctrlTree.GetDropHilightItem();
		if (m_hSelItem == NULL) {
			HTREEITEM hSelItem = m_ctrlTree.GetSelectedItem();
			bool bFocus = (::GetFocus() == m_ctrlTree.m_hWnd);
			if (bFocus || (m_ctrlTree.GetStyle() & TVS_SHOWSELALWAYS)) {
				if (!bFocus) {
					m_clrSelection = ::GetSysColor(COLOR_BTNFACE);
					m_clrSelectionText = ::GetSysColor(COLOR_BTNTEXT);
				}
				m_hSelItem = hSelItem;
			}
		}
		return CDRF_NOTIFYITEMDRAW;   // We need per-item notifications
	}
	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw) {
		if (lpNMCustomDraw->hdr.hwndFrom != m_ctrlTree) return CDRF_DODEFAULT;

		// Reset the focus because it will be drawn by us
		m_iItemState = lpNMCustomDraw->uItemState;
		lpNMCustomDraw->uItemState &= ~(CDIS_FOCUS | CDIS_SELECTED);

		// Remember the drawing rectangle of the item so we can draw it ourselves
		HTREEITEM hItem = (HTREEITEM)lpNMCustomDraw->dwItemSpec;
		m_ctrlTree.GetItemRect(hItem, &m_rcItem, TRUE);
		m_rcItem.right = std::max(lpNMCustomDraw->rc.right, m_cxHeader);
		if (m_dwExStyle & TLVS_EX_SELTOHEADER) m_rcItem.right = m_cxHeader;

		LPNMTVCUSTOMDRAW pTVCustomDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lpNMCustomDraw);
		if (hItem != m_hSelItem) {
			tMapItem* pVal = m_mapItems.Lookup(hItem);
			if (pVal && pVal->GetSize() > 0) {
				TLVITEM* pItem = (*pVal)[0];
				if (pItem->mask & TLVIF_COLOR) {
					if (pItem->clrText != CLR_NONE) pTVCustomDraw->clrText = pItem->clrText;
					if (pItem->clrBack != CLR_NONE) pTVCustomDraw->clrTextBk = pItem->clrBack;
				}
			}
		}

		return CDRF_NOTIFYPOSTPAINT;   // We need more notifications
	}
	DWORD OnItemPostPaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw) {
		if (lpNMCustomDraw->hdr.hwndFrom != m_ctrlTree) return CDRF_DODEFAULT;
		T* pT = static_cast<T*>(this);
		LPNMTVCUSTOMDRAW pCustomDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lpNMCustomDraw);
		pT->DrawTreeItem(pCustomDraw, m_iItemState, m_rcItem);
		return CDRF_DODEFAULT;
	}

	// Overridables

	void UpdateLayout() {
		ATLASSERT(::IsWindow(m_ctrlTree));
		ATLASSERT(::IsWindow(m_ctrlHeader));

		// FIX: Horizontal scrollbar fix by Nail Kaipov
		m_nOffset = 0;
		auto p = static_cast<T*>(this);
		if (p->GetStyle() & WS_HSCROLL) m_nOffset = -p->GetScrollPos(SB_HORZ); // read scrollbar position

		// Reposition the header and tree control
		RECT rcClient;
		p->GetClientRect(&rcClient);
		if (m_ctrlHeader.GetStyle() & WS_VISIBLE) {
			HDLAYOUT hdl = { 0 };
			WINDOWPOS wpos;
			hdl.prc = &rcClient;
			hdl.pwpos = &wpos;
			m_ctrlHeader.Layout(&hdl);
			ATLASSERT(wpos.y == 0);
			ATLASSERT(rcClient.top != 0); // We assume that HDLAYOUT is updated (MSDN docs doesn't mention it)
			m_ctrlTree.SetWindowPos(HWND_TOP, &rcClient, SWP_NOACTIVATE | SWP_NOZORDER);
			m_ctrlHeader.SetWindowPos(HWND_TOP, wpos.x, wpos.y, wpos.cx, wpos.cy, SWP_NOACTIVATE);
		}
		else {
			m_ctrlTree.SetWindowPos(HWND_TOP, &rcClient, SWP_NOACTIVATE | SWP_NOZORDER);
		}

		_CalcColumnSizes();

		// FIX: Repaint entire control.
		// NOTE: Cannot just use Invalidate() since WS_CLIPCHILDREN may not be there!!!
		m_ctrlHeader.Invalidate();
		m_ctrlTree.Invalidate();
	}

	void DrawTreeItem(LPNMTVCUSTOMDRAW lptvcd, UINT iState, const RECT& rcItem) {
		CDCHandle dc = lptvcd->nmcd.hdc;
		HTREEITEM hItem = (HTREEITEM)lptvcd->nmcd.dwItemSpec;
		bool bSelected = m_hSelItem == hItem;

		tMapItem* pVal = m_mapItems.Lookup(hItem);
		if (pVal == NULL) return;

		// NOTE: Having an ImageList attached to the TreeView control seems
		//       to produce some extra WM_ERASEBKGND msgs, which we can use to
		//       optimize the painting...
		CImageList il = m_ctrlTree.GetImageList(TVSIL_NORMAL);

		// If the item had focus then draw it
		// NOTE: Only when images are used (see note above)
		if ((m_dwExStyle & TLVS_EX_NOFOCUSRECT) == 0) {
			if ((iState & CDIS_FOCUS) != 0 && !il.IsNull()) {
				RECT rcFocus = rcItem;
				rcFocus.left = 1;
				dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
				dc.DrawFocusRect(&rcFocus);
			}
		}

		// If it's selected, paint the selection background
		if (bSelected) {
			RECT rcHigh = rcItem;
			dc.FillSolidRect(&rcHigh, m_clrSelection);
		}
		else if (il.IsNull()) {
			RECT rcHigh = rcItem;
			dc.FillSolidRect(&rcHigh, lptvcd->clrTextBk);
		}

		// Always write text with background
		dc.SetBkMode(OPAQUE);

		// Draw all columns of the item
		RECT rc = rcItem;
		int cnt = pVal->GetSize();
		for (int i = 0; i < cnt; i++) {
			LPTLVITEM pItem = (*pVal)[i];
			ATLASSERT(pItem);

			if (i != 0) rc.left = m_rcColumns[i].left;
			rc.right = m_rcColumns[i].right;

			if (pItem->mask & TLVIF_IMAGE) {
				ATLASSERT(!il.IsNull());
				int cx, cy;
				il.GetIconSize(cx, cy);
				il.DrawEx(
					pItem->iImage,
					dc,
					rc.left, rc.top,
					std::min<int>(cx, rc.right - rc.left), cy,
					CLR_NONE, CLR_NONE,
					ILD_TRANSPARENT);
				rc.left += cx;
			}

			// Sergey Solozhentsev kindly added coloring based on the dropitem as well.
			// We now support the control original text & background-color.
			if (pItem->mask & TLVIF_TEXT) {
				rc.left += 2;

				COLORREF clrText = lptvcd->clrText;
				COLORREF clrBack = lptvcd->clrTextBk;

				if (pItem->mask & TLVIF_COLOR) {
					if (bSelected) {
						dc.SetTextColor(m_clrSelectionText);
						dc.SetBkColor(m_clrSelection);
					}
					else {
						if (pItem->clrText != CLR_NONE) {
							dc.SetTextColor(pItem->clrText);
						}
						if (pItem->clrBack != CLR_NONE) {
							dc.SetBkColor(pItem->clrBack);
							if (i != 0) {
								// For first item we already set background color
								dc.FillSolidRect(&rc, pItem->clrBack);
							}
						}
						else {
							COLORREF clrBack = m_ctrlTree.GetBkColor();
							if (clrBack == CLR_NONE) clrBack = ::GetSysColor(COLOR_WINDOW);
							if (((*pVal)[0]->mask & TLVIF_COLOR) && ((*pVal)[0]->clrBack != CLR_NONE))clrBack = (*pVal)[0]->clrBack;
							dc.SetBkColor(clrBack);
						}
					}
				}
				else {
					COLORREF clrBack = m_ctrlTree.GetBkColor();
					if (clrBack == CLR_NONE) clrBack = ::GetSysColor(COLOR_WINDOW);
					dc.SetBkColor(bSelected ? m_clrSelection : clrBack);
					dc.SetTextColor(clrText);
				}

				CFont font;
				HFONT hOldFont = NULL;
				if (pItem->mask & TLVIF_STATE) {
					LOGFONT lf;
					::GetObject(m_ctrlTree.GetFont(), sizeof(LOGFONT), &lf);
					if (pItem->state & TLVIS_BOLD) lf.lfWeight += FW_BOLD - FW_NORMAL;
					if (pItem->state & TLVIS_ITALIC) lf.lfItalic = TRUE;
					if (pItem->state & TLVIS_UNDERLINE) lf.lfUnderline = TRUE;
					if (pItem->state & TLVIS_STRIKEOUT) lf.lfStrikeOut = TRUE;
					font.CreateFontIndirect(&lf);
					ATLASSERT(!font.IsNull());
					hOldFont = dc.SelectFont(font);
				}

				UINT format = pItem->mask & TLVIF_FORMAT ? pItem->format : 0;

				dc.DrawText(pItem->pszText,	-1,	&rc,
					DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | format);

				if (pItem->mask & TLVIF_STATE) 
					dc.SelectFont(hOldFont);
			}
		}
	}
};


class CTreeListView : public CTreeListViewImpl<CTreeListView> {
public:
	DECLARE_WND_CLASS(L"WTL_TreeListView")
};
