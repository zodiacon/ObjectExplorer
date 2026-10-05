#include "pch.h"
#include "ListLayout.h"
#include "AppSettings.h"
#include <sstream>

//
// format: version|count|sort column|ascending|order,...|width,...
//
static const int LayoutVersion = 1;

static std::wstring SettingName(PCWSTR name) {
	return std::wstring(L"Layout.") + name;
}

void ListLayout::Save(HWND hList, PCWSTR name) {
	CListViewCtrl list(hList);
	auto header = list.GetHeader();
	int count = header.GetItemCount();
	if (count <= 0)
		return;

	std::vector<int> order(count);
	list.GetColumnOrderArray(count, order.data());

	// the sorted column is the one showing the sort arrow
	int sortColumn = -1;
	bool ascending = true;
	for (int i = 0; i < count; i++) {
		HDITEM hdi{ HDI_FORMAT };
		header.GetItem(i, &hdi);
		if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) {
			sortColumn = i;
			ascending = (hdi.fmt & HDF_SORTUP) != 0;
			break;
		}
	}

	std::wostringstream text;
	text << LayoutVersion << L'|' << count << L'|' << sortColumn << L'|' << (ascending ? 1 : 0) << L'|';
	for (int i = 0; i < count; i++)
		text << (i ? L"," : L"") << order[i];
	text << L'|';
	for (int i = 0; i < count; i++)
		text << (i ? L"," : L"") << list.GetColumnWidth(i);

	AppSettings::Get().SetString(SettingName(name).c_str(), text.str().c_str());
}

bool ListLayout::Restore(HWND hView, HWND hList, PCWSTR name) {
	auto text = AppSettings::Get().GetString(SettingName(name).c_str());
	if (text.empty())
		return false;

	CListViewCtrl list(hList);
	int count = list.GetHeader().GetItemCount();

	std::wistringstream in(text);
	wchar_t sep;
	int version, savedCount, sortColumn, ascending;
	if (!(in >> version >> sep >> savedCount >> sep >> sortColumn >> sep >> ascending >> sep) || version != LayoutVersion || savedCount != count)
		return false;

	std::vector<int> order(count), widths(count);
	for (int i = 0; i < count; i++)
		if (!(in >> order[i]) || (i < count - 1 && !(in >> sep)))
			return false;
	in >> sep;
	for (int i = 0; i < count; i++)
		if (!(in >> widths[i]) || (i < count - 1 && !(in >> sep)))
			return false;

	// the order must be a permutation of the columns
	auto sorted = order;
	std::sort(sorted.begin(), sorted.end());
	for (int i = 0; i < count; i++)
		if (sorted[i] != i)
			return false;

	for (int i = 0; i < count; i++)
		list.SetColumnWidth(i, widths[i]);
	list.SetColumnOrderArray(count, order.data());

	if (sortColumn >= 0 && sortColumn < count) {
		//
		// a click on the column header sorts (ascending) and shows the arrow; a second one sorts descending
		//
		NMLISTVIEW nm{};
		nm.hdr.hwndFrom = hList;
		nm.hdr.idFrom = list.GetDlgCtrlID();
		nm.hdr.code = LVN_COLUMNCLICK;
		nm.iItem = -1;
		nm.iSubItem = sortColumn;
		::SendMessage(hView, WM_NOTIFY, nm.hdr.idFrom, reinterpret_cast<LPARAM>(&nm));
		if (!ascending)
			::SendMessage(hView, WM_NOTIFY, nm.hdr.idFrom, reinterpret_cast<LPARAM>(&nm));
	}
	return true;
}
