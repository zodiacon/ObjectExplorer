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
	HIMAGELIST GetTypesImageList() const;
	void Destroy();

private:
	ResourceManager();

	CFont m_monoFont;
	CFont m_defaultFont;
	CImageList m_typeImages;
	std::unordered_map<int, int> m_typeToImage;
};

