#pragma once

#pragma once

#include "ObjectManager.h"
#include "NtDll.h"
#include <unordered_set>
#include <assert.h>

template<typename TInfo = ObjectInfo>
	requires std::is_base_of_v<ObjectInfo, TInfo>
class ProcessObjectsTracker final {
public:
	explicit ProcessObjectsTracker(PCWSTR typeName = nullptr) : m_type(typeName ? typeName : L"") {}

	ULONG EnumObjects(bool clearHistory = false) {
		if (clearHistory)
			m_objects.clear();

		m_newObjects.clear();
		m_deletedObjects.clear();
		if (m_objects.empty()) {
			m_newObjects.reserve(512);
			m_deletedObjects.reserve(32);
		}

		auto objects = ObjectManager::EnumObjects<TInfo>(m_type.c_str(), false, true);
		if (m_objects.empty()) {
			m_objects.reserve(objects.size());
			m_newObjects = std::move(objects);
			for (auto& entry : m_newObjects) {
				m_objects.insert({ entry->Object, entry });
			}
		}
		else {
			auto oldObjects = m_objects;
			for (auto& entry : objects) {
				if (m_objects.find(entry->Object) == m_objects.end()) {
					// new object
					m_newObjects.push_back(entry);
				}
				else {
					// existing handle
					oldObjects.erase(entry->Object);
				}
				m_objects.insert_or_assign(entry->Object, entry);
			}
			for (auto const& [key, entry] : oldObjects) {
				m_deletedObjects.push_back(entry);
				m_objects.erase(key);
			}
		}

		return static_cast<uint32_t>(m_objects.size());
	}
	const std::vector<std::shared_ptr<TInfo>>& GetNewObjects() const {
		return m_newObjects;
	}
	const std::vector<std::shared_ptr<TInfo>>& GetDeletedObjects() const {
		return m_deletedObjects;
	}

private:
	ObjectManager m_mgr;
	DWORD m_pid;
	std::wstring m_type;
	wil::unique_handle m_hProcess;
	std::vector<std::shared_ptr<TInfo>> m_deletedObjects, m_newObjects;
	std::unordered_map<PVOID, std::shared_ptr<TInfo>> m_objects;
};

