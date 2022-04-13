#pragma once

template<typename T>
struct CTimerManager {
	BEGIN_MSG_MAP(CTimerManager)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_RUN, OnRun)
		COMMAND_ID_HANDLER(ID_PAUSERESUME, OnRun)
		COMMAND_RANGE_HANDLER(ID_UPDATEINTERVAL_HALFSEC, ID_UPDATEINTERVAL_10SECONDS, OnUpdateInterval)
	END_MSG_MAP()

	void ActivateTimer(bool active) {
		auto p = static_cast<T*>(this);
		if (active)
			p->SetTimer(1, m_Interval);
		else
			p->KillTimer(1);
	}

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM id, LPARAM lParam, BOOL& bHandled) {
		if (id == 1)
			static_cast<T*>(this)->DoTimerUpdate();
		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM id, LPARAM lParam, BOOL& bHandled) {
		bHandled = FALSE;
		Run(false);
		return 0;
	}

	inline static const int intervals[] {
		500, 1000, 2000, 5000, 10000
	};

	LRESULT OnRun(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
		Run(!m_Running);
		UpdateIntervalUI();
		return 0;
	}

	LRESULT OnUpdateInterval(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
		m_IntervalIndex = id - ID_UPDATEINTERVAL_HALFSEC;
		SetInterval(intervals[m_IntervalIndex]);
		return 0;
	}

	void SetInterval(int interval) {
		int i = 0;
		if (interval) {
			m_Interval = interval;
			for (i = 0; i < _countof(intervals); i++)
				if (intervals[i] == interval)
					break;
			ATLASSERT(i < _countof(intervals));
		}
		else {
			m_Running = !m_Running;
		}
		m_IntervalIndex = i;
		auto p = static_cast<T*>(this);
		p->Run(m_Running);
		UpdateIntervalUI();
	}

	bool IsRunning() const {
		return m_Running;
	}

	void UpdateIntervalUI() {
		auto& ui = static_cast<T*>(this)->GetFrame()->GetUI();
		ui.UISetRadioMenuItem(m_IntervalIndex + ID_UPDATEINTERVAL_HALFSEC,
			ID_UPDATEINTERVAL_HALFSEC, ID_UPDATEINTERVAL_10SECONDS);
		ui.UISetCheck(ID_RUN, m_Running);
		ui.UISetCheck(ID_PAUSERESUME, !m_Running);
	}

	// overridables

	void DoTimerUpdate() {}
	void Run(bool run, bool updateUI = true) {
		auto p = static_cast<T*>(this);
		m_Running = run;
		if (run) {
			p->SetTimer(1, m_Interval);
		}
		else {
			p->KillTimer(1);
		}
		if(updateUI)
			UpdateIntervalUI();
	}

protected:
	int m_Interval{ 1000 };
	UINT m_IntervalIndex{ 1 };
	bool m_Running{ false };
};
