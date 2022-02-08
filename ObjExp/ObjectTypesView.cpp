// View.cpp : implementation of the CObjectTypesView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "ObjectTypesView.h"

BOOL CObjectTypesView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CObjectTypesView::OnFinalMessage(HWND /*hWnd*/) {
	delete this;
}

CString CObjectTypesView::GetTitle() const {
	return L"Object Types";
}

LRESULT CObjectTypesView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CPaintDC dc(m_hWnd);

	//TODO: Add your drawing code here

	return 0;
}
