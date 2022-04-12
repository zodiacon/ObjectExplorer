#include "pch.h"
#include "ProcessHandleTracker.h"
#include "NtDll.h"
#include <unordered_set>
#include <assert.h>
#include "ObjectManager.h"

ProcessHandlesTracker::ProcessHandlesTracker(DWORD pid) : m_pid(pid) {
}

ULONG ProcessHandlesTracker::EnumHandles(bool clearHostory) {
	m_mgr.EnumHandles(nullptr, m_pid);

	if (clearHostory)
		m_handles.clear();

	m_newHandles.clear();
	m_closedHandles.clear();
	if (m_handles.empty()) {
		m_newHandles.reserve(512);
		m_closedHandles.reserve(32);
	}

	if (m_handles.empty()) {
		auto& handles = m_mgr.GetHandles();
		m_handles.reserve(handles.size() + 32);
		for(auto& entry : handles) {
			HandleKey key = { entry->HandleValue, entry->ProcessId, (uint16_t)entry->ObjectTypeIndex };
			m_handles.insert({ key, entry });
		}
	}
	else {
		auto oldHandles = m_handles;
		for(auto& entry : m_mgr.GetHandles()) {
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

const std::vector<std::shared_ptr<HandleInfo>>& ProcessHandlesTracker::GetNewHandles() const {
	return m_newHandles;
}

const std::vector<std::shared_ptr<HandleInfo>>& ProcessHandlesTracker::GetClosedHandles() const {
	return m_closedHandles;
}

