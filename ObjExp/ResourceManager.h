#pragma once

class ResourceManager final {
public:
	//
	// prevent copying and moving
	//
	ResourceManager(ResourceManager const&) = delete;
	ResourceManager& operator=(ResourceManager const&) = delete;
	ResourceManager(ResourceManager&&) = delete;
	ResourceManager& operator=(ResourceManager&&) = delete;

	static ResourceManager& Get();

	int GetTypeImage(int typeIndex) const;
	int GetTypeImage(PCWSTR typeName) const;
	// the icon is owned by the resource manager; the caller must not destroy it
	HICON GetTypeIcon(PCWSTR typeName) const;
	HIMAGELIST GetTypesImageList() const;
	void Destroy();

	// adds a 16x16 icon resource to an image list (which keeps its own copy)
	static int AddIcon(HIMAGELIST images, UINT id);

private:
	ResourceManager();

	CFont m_monoFont;
	CFont m_defaultFont;
	CImageList m_typeImages;
	// icons extracted from m_typeImages by image index
	mutable std::unordered_map<int, HICON> m_typeIcons;
	std::unordered_map<int, int> m_typeToImage;
	std::unordered_map<std::wstring, int> m_typeNameToImage;
};

