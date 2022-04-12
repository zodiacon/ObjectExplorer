#include "pch.h"
#include "HandlesView.h"

CHandlesView::CHandlesView(IMainFrame* frame, DWORD pid, PCWSTR name) 
	: CViewBase(frame), m_Tracker(m_Pid = pid), m_ProcessName(name) {
}

void CHandlesView::OnFinalMessage(HWND) {
	delete this;
}

CString CHandlesView::GetTitle() const {
	if (m_Pid == 0)
		return L"All Handles";

	CString title;
	title.Format(L"%s (PID: %u)", (PCWSTR)m_ProcessName, m_Pid);
	return title;
}

LRESULT CHandlesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	return 0;
}

LRESULT CHandlesView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	return LRESULT();
}

LRESULT CHandlesView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	return LRESULT();
}

LRESULT CHandlesView::OnPauseResume(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}
