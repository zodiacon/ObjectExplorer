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
        { L"CoreMessageing", IDI_MESSAGE },
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
        }
    }
}
