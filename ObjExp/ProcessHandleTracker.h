#pragma once

#include "ObjectManager.h"

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

class ProcessHandlesTracker {
public:
	ProcessHandlesTracker(DWORD pid);

	bool IsValid() const;
	operator bool() const {
		return IsValid();
	}

	ULONG EnumHandles(bool clearHistory = false);
	const std::vector<std::shared_ptr<HandleInfo>>& GetNewHandles() const;
	const std::vector<std::shared_ptr<HandleInfo>>& GetClosedHandles() const;

private:
	ObjectManager m_mgr;
	DWORD m_pid;
	wil::unique_handle m_hProcess;
	std::vector<std::shared_ptr<HandleInfo>> m_closedHandles, m_newHandles;
	std::unordered_map<HandleKey, std::shared_ptr<HandleInfo>> m_handles;
};

