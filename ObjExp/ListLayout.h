#pragma once

//
// column widths, order and sort of a list view, saved in the settings under a name
// (one per kind of view, e.g. "AllHandles"); a saved layout is used only if the column count matches
//
struct ListLayout abstract final {
	// hView gets the simulated column clicks that restore the sort (CVirtualListView handles them)
	static bool Restore(HWND hView, HWND hList, PCWSTR name);
	static void Save(HWND hList, PCWSTR name);
};
