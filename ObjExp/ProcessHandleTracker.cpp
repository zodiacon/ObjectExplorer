#include "pch.h"
#include "ProcessHandleTracker.h"
#include "NtDll.h"
#include <unordered_set>
#include <assert.h>

bool HandleEntryInfo::operator==(const HandleEntryInfo& other) const {
	return HandleValue == other.HandleValue && ObjectTypeIndex == other.ObjectTypeIndex;
}

template<>
struct std::hash<HandleEntryInfo> {
	size_t operator()(const HandleEntryInfo& key) const {
		return (size_t)key.HandleValue ^ ((uint32_t)key.ObjectTypeIndex << 16);
	}
};

struct ProcessHandlesTracker::Impl {
	Impl(uint32_t pid);
	Impl(HANDLE hProcess);

	ULONG EnumHandles(bool clearHistory);
	const std::vector<HandleEntryInfo>& GetNewHandles() const {
		return m_newHandles;
	}
	const std::vector<HandleEntryInfo>& GetClosedHandles() const {
		return m_closedHandles;
	}

	bool IsValid() const {
		return m_hProcess.is_valid();
	}

private:
	wil::unique_handle m_hProcess;
	std::vector<HandleEntryInfo> m_closedHandles, m_newHandles;
	std::unordered_set<HandleEntryInfo> m_handles;
};

ProcessHandlesTracker::Impl::Impl(uint32_t pid) :
	Impl::Impl(::OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid)) {
}

ProcessHandlesTracker::Impl::Impl(HANDLE hProcess) : m_hProcess(hProcess) {
	if (m_hProcess) {
		m_newHandles.reserve(16);
		m_closedHandles.reserve(16);
	}
}

ULONG ProcessHandlesTracker::Impl::EnumHandles(bool clearHostory) {
	if (!m_hProcess)
		return 0;

	auto rc = ::WaitForSingleObject(m_hProcess.get(), 0);
	assert(rc != WAIT_FAILED);

	if (rc == WAIT_OBJECT_0) {
		m_closedHandles.clear();
		m_closedHandles.insert(m_closedHandles.begin(), m_handles.begin(), m_handles.end());
		m_newHandles.clear();
		m_handles.clear();
		return 0;
	}
	auto size = 1 << 22;
	ULONG len;
	wil::unique_virtualalloc_ptr<> buffer;
	for (;;) {
		buffer.reset(::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		auto status = NT::NtQueryInformationProcess(m_hProcess.get(), ProcessHandleInformation, buffer.get(), size, &len);
		if (status == STATUS_SUCCESS)
			break;
		if (STATUS_BUFFER_TOO_SMALL == status) {
			size = len + (1 << 12);
			continue;
		}
		return 0;
	}

	auto info = reinterpret_cast<PROCESS_HANDLE_SNAPSHOT_INFORMATION*>(buffer.get());
	if (clearHostory)
		m_handles.clear();

	m_newHandles.clear();
	m_closedHandles.clear();
	if (m_handles.empty()) {
		m_newHandles.reserve(512);
		m_closedHandles.reserve(32);
	}

	if (m_handles.empty()) {
		m_handles.reserve(info->NumberOfHandles);
		for (ULONG i = 0; i < info->NumberOfHandles; i++) {
			const auto& entry = info->Handles[i];
			HandleEntryInfo key = { entry.HandleValue, (uint16_t)entry.ObjectTypeIndex };
			m_handles.insert(key);
		}
	}
	else {
		auto oldHandles = m_handles;
		for (ULONG i = 0; i < info->NumberOfHandles; i++) {
			const auto& entry = info->Handles[i];
			HandleEntryInfo key = { entry.HandleValue, (uint16_t)entry.ObjectTypeIndex };
			if (m_handles.find(key) == m_handles.end()) {
				// new handle
				m_newHandles.push_back(key);
				m_handles.insert(key);
			}
			else {
				// existing handle
				oldHandles.erase(key);
			}
		}
		for (auto& hi : oldHandles) {
			m_closedHandles.push_back(hi);
			m_handles.erase(hi);
		}
	}

	return static_cast<uint32_t>(m_handles.size());
}

ProcessHandlesTracker::ProcessHandlesTracker(uint32_t pid) : m_impl(new Impl(pid)) {
}

ProcessHandlesTracker::ProcessHandlesTracker(HANDLE hProcess) : m_impl(new Impl(hProcess)) {
}

ProcessHandlesTracker::~ProcessHandlesTracker() = default;

bool ProcessHandlesTracker::IsValid() const {
	return m_impl->IsValid();
}

ULONG ProcessHandlesTracker::EnumHandles(bool clearHistory) {
	return m_impl->EnumHandles(clearHistory);
}

const std::vector<HandleEntryInfo>& ProcessHandlesTracker::GetNewHandles() const {
	return m_impl->GetNewHandles();
}

const std::vector<HandleEntryInfo>& ProcessHandlesTracker::GetClosedHandles() const {
	return m_impl->GetClosedHandles();
}
