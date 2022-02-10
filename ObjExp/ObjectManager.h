#pragma once

struct ObjectTypeInfo;

enum class PoolType {
	PagedPool = 1,
	NonPagedPool = 0,
	NonPagedPoolNx = 0x200,
	NonPagedPoolSessionNx = NonPagedPoolNx + 32,
	PagedPoolSessionNx = NonPagedPoolNx + 33
};

struct ObjectInfo;

struct HandleInfo {
	PVOID Object;
	ULONG ProcessId;
	ULONG HandleValue;
	ULONG GrantedAccess;
	ULONG HandleAttributes;
	USHORT ObjectTypeIndex;
	CString Name;
	ObjectInfo* ObjectInfo;
};

struct ObjectInfo {
	PVOID Object;
	int HandleCount;
	int PointerCount;
	CString Name;
	USHORT TypeIndex;
	std::vector<std::shared_ptr<HandleInfo>> Handles;
	wil::unique_handle LocalHandle;
	PCWSTR TypeName;
};

struct ObjectTypeInfo {
	uint32_t TotalNumberOfObjects;
	uint32_t TotalNumberOfHandles;
	uint32_t TotalPagedPoolUsage;
	uint32_t TotalNonPagedPoolUsage;
	uint32_t TotalNamePoolUsage;
	uint32_t TotalHandleTableUsage;
	uint32_t HighWaterNumberOfObjects;
	uint32_t HighWaterNumberOfHandles;
	uint32_t HighWaterPagedPoolUsage;
	uint32_t HighWaterNonPagedPoolUsage;
	uint32_t HighWaterNamePoolUsage;
	uint32_t HighWaterHandleTableUsage;
	uint32_t InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	uint32_t ValidAccessMask;
	PoolType PoolType;
	uint32_t DefaultPagedPoolCharge;
	uint32_t DefaultNonPagedPoolCharge;
	CString TypeName;
	uint8_t TypeIndex;
	bool SecurityRequired;
	bool MaintainHandleCount;
	std::vector<std::shared_ptr<ObjectInfo>> Objects;
};

struct ObjectNameAndType {
	std::wstring Name;
	std::wstring TypeName;
};

enum class GdiObjectType {
	DeviceContext = 1,
	DirectDrawSurface = 3,
	Region = 4,
	Bitmap = 5,
	Path = 7,
	Palette = 8,
	LFont = 10,
	RFont = 11,
	Brush = 16,
	Metafile = 21,
};

struct GdiObject {
	ULONG Handle;
	ULONG ProcessId;
	PVOID Object;
	GdiObjectType Type;
	USHORT Count;
	USHORT Index;
};

struct ObjectAndHandleStats {
	int64_t TotalHandles;
	int64_t TotalObjects;
	int64_t PeakHandles;
	int64_t PeakObjects;
};

class ObjectManager {
public:
	using ObjectTypePtr = std::shared_ptr<ObjectTypeInfo>;

	bool EnumHandlesAndObjects(PCWSTR type = nullptr, DWORD pid = 0, PCWSTR prefix = nullptr, bool namedOnly = false);
	bool EnumHandles(PCWSTR type = nullptr, DWORD pid = 0, bool namedObjectsOnly = false);
	int EnumTypes();

	const std::vector<std::shared_ptr<ObjectInfo>>& GetObjects() const;

	static HANDLE DupHandle(ObjectInfo* pObject, ACCESS_MASK access = GENERIC_READ);
	static HANDLE DupHandle(HANDLE h, DWORD pid, USHORT type, ACCESS_MASK access = GENERIC_READ, DWORD flags = 0);
	static NTSTATUS OpenObject(PCWSTR path, PCWSTR type, HANDLE* pHandle, DWORD access = GENERIC_READ);
	static bool GetStats(ObjectAndHandleStats& stats);

	int64_t GetTotalHandles();
	int64_t GetTotalObjects();

	enum class ChangeType {
		NoChange,
		TotalHandles,
		TotalObjects,
		PeakHandles,
		PeakObjects,
	};

	using Change = std::tuple<std::shared_ptr<ObjectTypeInfo>, ChangeType, int32_t>;
	const std::vector<Change>& GetChanges() const;

	bool GetObjectInfo(ObjectInfo* p, HANDLE hObject, ULONG pid, USHORT type) const;
	CString GetObjectName(HANDLE hObject, ULONG pid, USHORT type) const;
	static CString GetObjectName(HANDLE hDup, USHORT type);

	std::shared_ptr<ObjectTypeInfo> GetType(USHORT index);
	std::shared_ptr<ObjectTypeInfo> GetType(PCWSTR name);
	const std::vector<ObjectTypePtr>& GetObjectTypes() const;
	const std::vector<std::shared_ptr<HandleInfo>>& GetHandles() const;

	static std::vector<ObjectNameAndType> EnumDirectoryObjects(PCWSTR path);
	static CString GetSymbolicLinkTarget(PCWSTR path);

private:
	std::vector<ObjectTypePtr> m_types;
	std::unordered_map<int16_t, ObjectTypePtr> m_typesMap;
	std::unordered_map<std::wstring, ObjectTypePtr> m_typesNameMap;

	std::vector<std::shared_ptr<ObjectInfo>> m_objects;
	std::unordered_map<PVOID, std::shared_ptr<ObjectInfo>> m_objectsByAddress;
	std::vector<std::shared_ptr<HandleInfo>> m_handles;
	std::vector<Change> m_changes;
	int64_t m_totalHandles, m_totalObjects;
	bool m_skipThisProcess = false;
};

