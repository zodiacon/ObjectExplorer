#include "pch.h"
#include "ImageIconCache.h"

int ImageIconCache::GetIcon(std::wstring const& path, HICON* phIcon) const {
	auto it = m_icons.find(path);
	if (it != m_icons.end()) {
		int index = it->second;
		if (phIcon)
			*phIcon = m_images.GetIcon(index);
		return index;
	}
	WORD index = 0;
	CString spath(path.c_str());
	auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), spath.GetBufferSetLength(MAX_PATH), &index);

	if (hIcon) {
		int index = m_images.AddIcon(hIcon);
		if (phIcon)
			*phIcon = hIcon;
		m_icons.insert({ path, index });
		return index;
	}
	return 0;
}

ImageIconCache::Map::const_iterator ImageIconCache::begin() const {
	return m_icons.begin();
}

ImageIconCache::Map::const_iterator ImageIconCache::end() const {
	return m_icons.end();
}

ImageIconCache::ImageIconCache() {
	m_images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 32, 32);
}

HIMAGELIST ImageIconCache::GetImageList() const {
	return m_images;
}

ImageIconCache& ImageIconCache::Get() {
	static ImageIconCache cache;
	return cache;
}

void ImageIconCache::SetImageList(HIMAGELIST hil) {
	m_images.Attach(hil);
	m_icons.clear();
}
