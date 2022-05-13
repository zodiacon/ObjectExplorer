#pragma once

#include <shared_mutex>

struct ImageIconCache {
	static ImageIconCache& Get();

	void SetImageList(HIMAGELIST hil);
	HIMAGELIST GetImageList() const;
	int GetIcon(std::wstring const& path, HICON* phIcon = nullptr) const;

	using Map = std::unordered_map<std::wstring, int>;
	Map::const_iterator begin() const;
	Map::const_iterator end() const;

private:
	ImageIconCache();
	ImageIconCache(const ImageIconCache&) = delete;
	ImageIconCache& operator=(const ImageIconCache&) = delete;

private:
	mutable CImageList m_images;
	mutable Map m_icons;
};
