#pragma once

//
// matches names against the text of a Find: a substring, ignoring case unless asked not to
//
class SearchMatcher {
public:
	SearchMatcher(PCWSTR text, bool matchCase) : m_Text(text), m_MatchCase(matchCase) {
		if (!matchCase)
			m_Text.MakeLower();
	}

	bool Matches(CString const& name) const {
		if (m_MatchCase)
			return name.Find(m_Text) >= 0;
		CString lower(name);
		return lower.MakeLower().Find(m_Text) >= 0;
	}

private:
	CString m_Text;
	bool m_MatchCase;
};
