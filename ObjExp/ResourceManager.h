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
	HICON GetTypeIcon(PCWSTR typeName) const;
	HIMAGELIST GetTypesImageList() const;
	void Destroy();

private:
	ResourceManager();

	CFont m_monoFont;
	CFont m_defaultFont;
	CImageList m_typeImages;
	std::unordered_map<int, int> m_typeToImage;
	std::unordered_map<std::wstring, int> m_typeNameToImage;
};

