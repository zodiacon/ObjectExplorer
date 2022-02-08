#pragma once

struct HandleEntryInfo {
	HANDLE HandleValue;
	USHORT ObjectTypeIndex;

	bool operator==(const HandleEntryInfo& other) const;
};

class ProcessHandlesTracker {
public:
	ProcessHandlesTracker(uint32_t pid);
	ProcessHandlesTracker(HANDLE hProcess);
	~ProcessHandlesTracker();

	bool IsValid() const;
	operator bool() const {
		return IsValid();
	}

	ULONG EnumHandles(bool clearHistory = false);
	const std::vector<HandleEntryInfo>& GetNewHandles() const;
	const std::vector<HandleEntryInfo>& GetClosedHandles() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};


