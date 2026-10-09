#pragma once

#include "ObjectManager.h"

struct StringHelper final {
	static PCWSTR PoolTypeToString(PoolType type);
	static CString SectionAttributesToString(DWORD value);
	static CString HandleAttributesToString(DWORD attributes);
	static CString TimeSpanToString(DWORD64 ts);
	static CString ObjectAttributesToString(DWORD attr);
	// domain\user, or empty if the SID can't be looked up
	static CString SidToName(PSID sid);
	// a mandatory label RID (SECURITY_MANDATORY_*_RID)
	static CString IntegrityLevelToString(DWORD rid);
	// an IMAGE_FILE_MACHINE_* value
	static CString MachineToString(USHORT machine);
};

