#include "pch.h"
#include "ResourceManager.h"
#include "resource.h"
#include "ObjectManager.h"

using namespace std;

ResourceManager& ResourceManager::Get() {
	static ResourceManager mgr;
	return mgr;
}

int ResourceManager::GetTypeImage(int typeIndex) const {
	if (auto it = m_typeToImage.find(typeIndex); it != m_typeToImage.end())
		return it->second;
	return 0;
}

int ResourceManager::GetTypeImage(PCWSTR typeName) const {
	if (auto it = m_typeNameToImage.find(typeName); it != m_typeNameToImage.end())
		return it->second;
	return 0;
}

HICON ResourceManager::GetTypeIcon(PCWSTR typeName) const {
	int index = 0;
	if (auto it = m_typeNameToImage.find(typeName); it != m_typeNameToImage.end())
		index = it->second;
	return m_typeImages.GetIcon(index);
}

HIMAGELIST ResourceManager::GetTypesImageList() const {
	return m_typeImages.m_hImageList;
}

void ResourceManager::Destroy() {
	if (m_monoFont)
		m_monoFont.DeleteObject();
	if (m_defaultFont)
		m_defaultFont.DeleteObject();
	if(m_typeImages)
		m_typeImages.Destroy();
}

ResourceManager::ResourceManager() {
	m_typeImages.Create(16, 16, ILC_COLOR32 | ILC_MASK, 32, 8);
	
	//
	// add default object icon
	//
	m_typeImages.AddIcon(AtlLoadIconImage(IDI_OBJECT, 0, 16, 16));

	std::unordered_map<std::wstring, UINT> icons = { 
		{ L"Process", IDI_PROCESS },
		{ L"Thread", IDI_THREAD },
		{ L"Key", IDI_KEY },
		{ L"Job", IDI_JOB },
		{ L"Desktop", IDI_DESKTOP },
		{ L"ALPC Port", IDI_ALPC },
		{ L"Mutant", IDI_MUTEX },
		{ L"Event", IDI_EVENT },
		{ L"Semaphore", IDI_SEMAPHORE },
		{ L"PowerRequest", IDI_ATOM },
		{ L"Driver", IDI_CAR },
		{ L"Device", IDI_DEVICE },
		{ L"File", IDI_FILE },
		{ L"Callback", IDI_CALLBACK },
		{ L"Section", IDI_MEMORY },
		{ L"Type", IDI_STRUCT },
		{ L"WindowStation", IDI_WINSTATION },
		{ L"SymbolicLink", IDI_LINK },
		{ L"Directory", IDI_DIRECTORY },
		{ L"Timer", IDI_TIMER },
		{ L"IRTimer", IDI_TIMER },
		{ L"Token", IDI_SHIELD2 },
		{ L"Session", IDI_USER },
		{ L"DebugObject", IDI_DEBUG },
		{ L"Profile", IDI_PROFILE },
		{ L"CoreMessaging", IDI_MESSAGE },
		{ L"Partition", IDI_PARTITION },
		{ L"CrossVmEvent", IDI_EVENTVM },
		{ L"FilterCommunicationPort", IDI_COMMPORT },
		{ L"CrossVmMutant", IDI_MUTEXVM },
		{ L"KeyedEvent", IDI_EVENT_KEY },
		{ L"PsSiloContextPaged", IDI_SILO },
		{ L"PsSiloContextNonPaged", IDI_SILO },
		{ L"DxgkCompositionObject", IDI_DIRECTX },
		{ L"DxgkSharedResource", IDI_DIRECTX },
		{ L"DxgkSharedSyncObject", IDI_DIRECTX },
		{ L"DxgkDisplayManagerObject", IDI_DIRECTX },
		{ L"DxgkSharedProtectedSessionObject", IDI_DIRECTX },
		{ L"DxgkSharedKeyedMutexObject", IDI_DIRECTX },
		{ L"DxgkSharedSwapChainObject", IDI_DIRECTX },
		{ L"DxgkSharedBundleObject", IDI_DIRECTX },
		{ L"VRegConfigurationContext", IDI_REGISTRY },
		{ L"EtwRegistration", IDI_ETWREG },
		{ L"EtwConsumer", IDI_ETW },
		{ L"FilterConnectionPort", IDI_PLUG },
	};

	ObjectManager mgr;
	auto count = mgr.EnumTypes();
	m_typeToImage.reserve(count);
	for (auto& info : mgr.GetObjectTypes()) {
		auto it = icons.find((PCWSTR)info->TypeName);
		if (it != end(icons)) {
			auto image = m_typeImages.AddIcon(AtlLoadIconImage(it->second, 0, 16, 16));
			ATLASSERT(image >= 0);
			m_typeToImage.insert({ info->TypeIndex, image });
			m_typeNameToImage.insert({ (PCWSTR)info->TypeName, image });
		}
	}
}
