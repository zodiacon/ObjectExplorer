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
	static int EnumTypes();

	const std::vector<std::shared_ptr<ObjectInfo>>& GetObjects() const;

	static HANDLE DupHandle(ObjectInfo* pObject, ACCESS_MASK access = GENERIC_READ);
	static HANDLE DupHandle(HANDLE h, DWORD pid, USHORT type, ACCESS_MASK access = GENERIC_READ, DWORD flags = 0);
	static NTSTATUS OpenObject(PCWSTR path, PCWSTR type, HANDLE& handle, DWORD access = GENERIC_READ);
	static bool GetStats(ObjectAndHandleStats& stats);
	static std::pair<HANDLE, DWORD> FindFirstHandle(PCWSTR name, USHORT index, DWORD pid = 0);

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
	static const std::vector<Change>& GetChanges();

	bool GetObjectInfo(ObjectInfo* p, HANDLE hObject, ULONG pid, USHORT type) const;
	static CString GetObjectName(HANDLE hObject, ULONG pid, USHORT type);
	static CString GetObjectName(HANDLE hDup, USHORT type);

	static std::shared_ptr<ObjectTypeInfo> GetType(USHORT index);
	static std::shared_ptr<ObjectTypeInfo> GetType(PCWSTR name);
	static const std::vector<ObjectTypePtr>& GetObjectTypes();
	const std::vector<std::shared_ptr<HandleInfo>>& GetHandles() const;

	static std::vector<ObjectNameAndType> EnumDirectoryObjects(PCWSTR path);
	static CString GetSymbolicLinkTarget(PCWSTR path);

private:
	inline static std::vector<ObjectTypePtr> s_types;
	inline static std::unordered_map<int16_t, ObjectTypePtr> s_typesMap;
	inline static std::unordered_map<std::wstring, ObjectTypePtr> s_typesNameMap;
	inline static std::vector<Change> s_changes;
	inline static int64_t s_totalHandles, s_totalObjects;

	std::vector<std::shared_ptr<ObjectInfo>> m_objects;
	std::unordered_map<PVOID, std::shared_ptr<ObjectInfo>> m_objectsByAddress;
	std::vector<std::shared_ptr<HandleInfo>> m_handles;
	bool m_skipThisProcess = false;
};

