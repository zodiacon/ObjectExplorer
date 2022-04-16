#pragma once

#include "ObjectManager.h"
#include "NtDll.h"
#include <unordered_set>
#include <assert.h>

struct HandleKey {
	ULONG HandleValue;
	ULONG Pid;
	USHORT TypeIndex;

	bool operator==(HandleKey const& other) const {
		return HandleValue == other.HandleValue && Pid == other.Pid && TypeIndex == other.TypeIndex;
	}
};

template<>
struct std::hash<HandleKey> {
	size_t operator()(const HandleKey& key) const {
		return (size_t)key.HandleValue ^ ((uint32_t)key.TypeIndex << 16) ^ ((size_t)key.Pid << 32);
	}
};

template<typename TInfo = HandleInfo>
	requires std::is_base_of_v<HandleInfo, TInfo>
class ProcessHandlesTracker final {
public:
	explicit ProcessHandlesTracker(DWORD pid) : m_pid(pid) {}
	explicit ProcessHandlesTracker(PCWSTR typeName, DWORD pid = 0) : m_type(typeName ? typeName : L""), m_pid(pid) {}

	ULONG EnumHandles(bool clearHistory = false) {
		if (clearHistory)
			m_handles.clear();

		m_newHandles.clear();
		m_closedHandles.clear();
		if (m_handles.empty()) {
			m_newHandles.reserve(512);
			m_closedHandles.reserve(32);
		}

		auto handles = ObjectManager::EnumHandles2<TInfo>(m_type.c_str(), m_pid, false, true);
		if (m_handles.empty()) {
			m_handles.reserve(handles.size());
			m_newHandles = std::move(handles);
			for (auto& entry : m_newHandles) {
				HandleKey key = { entry->HandleValue, entry->ProcessId, (uint16_t)entry->ObjectTypeIndex };
				m_handles.insert({ key, entry });
			}
		}
		else {
			auto oldHandles = m_handles;
			for (auto& entry : handles) {
				HandleKey key = { entry->HandleValue, entry->ProcessId, (uint16_t)entry->ObjectTypeIndex };
				if (m_handles.find(key) == m_handles.end()) {
					// new handle
					m_newHandles.push_back(entry);
					m_handles.insert({ key, entry });
				}
				else {
					// existing handle
					oldHandles.erase(key);
				}
			}
			for (auto const& [key, entry] : oldHandles) {
				m_closedHandles.push_back(entry);
				m_handles.erase(key);
			}
		}

		return static_cast<uint32_t>(m_handles.size());
	}
	const std::vector<std::shared_ptr<TInfo>>& GetNewHandles() const {
		return m_newHandles;
	}
	const std::vector<std::shared_ptr<TInfo>>& GetClosedHandles() const {
		return m_closedHandles;
	}

private:
	ObjectManager m_mgr;
	DWORD m_pid;
	std::wstring m_type;
	std::vector<std::shared_ptr<TInfo>> m_closedHandles, m_newHandles;
	std::unordered_map<HandleKey, std::shared_ptr<TInfo>> m_handles;
};

