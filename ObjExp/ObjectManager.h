#pragma once

#include "NtDll.h"

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
	std::wstring Name;
};

struct ObjectInfo {
	PVOID Object;
	ULONG HandleCount;
	ULONG PointerCount;
	std::wstring Name;
	USHORT TypeIndex;
	HandleInfo FirstHandle;
	ULONG ManualHandleCount;
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

class ObjectManager {
public:
	using ObjectTypePtr = std::shared_ptr<ObjectTypeInfo>;

	template<typename T = ObjectInfo> requires std::is_base_of_v<ObjectInfo, T>
	static std::vector<std::shared_ptr<T>> EnumObjects(PCWSTR type = nullptr, bool namedOnly = false, bool skipNames = false) {
		if (s_types.empty())
			EnumTypes();

		auto p = EnumHandlesBuffer();
		auto filteredTypeIndex = type == nullptr || type[0] == 0 ? -1 : s_typesNameMap.at(type)->TypeIndex;
		auto count = p->NumberOfHandles;
		std::vector<std::shared_ptr<T>> objects;
		objects.reserve(filteredTypeIndex < 0 ? TotalObjects + 100 : 1024);
		std::unordered_map<PVOID, std::shared_ptr<T>> objectMap;

		for (decltype(count) i = 0; i < count; i++) {
			auto& handle = p->Handles[i];
			if (filteredTypeIndex >= 0 && handle.ObjectTypeIndex != filteredTypeIndex)
				continue;

			std::shared_ptr<T> object;
			if (auto it = objectMap.find(handle.Object); it != objectMap.end())
				object = it->second;
			else {
				object = std::make_shared<T>();
				object->ManualHandleCount = 0;
				if (!skipNames) {
					auto name = GetObjectName((HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId, handle.ObjectTypeIndex);
					if (namedOnly && name.IsEmpty())
						continue;
					object->Name = name;
				}
				object->TypeIndex = handle.ObjectTypeIndex;
				object->Object = handle.Object;
				objects.push_back(object);
				auto hDup = DupHandle((HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId);
				if (hDup) {
					NT::OBJECT_BASIC_INFORMATION info;
					if (NT_SUCCESS(NT::NtQueryObject(hDup, NT::ObjectBasicInformation, &info, sizeof(info)))) {
						object->HandleCount = info.HandleCount - 1;	// subtract our own handle
						object->PointerCount = info.PointerCount;
					}
					::CloseHandle(hDup);
				}
				objectMap.insert({ handle.Object, object });
			}
			if (object->ManualHandleCount == 0) {
				HandleInfo hi;
				hi.HandleValue = (ULONG)handle.HandleValue;
				hi.GrantedAccess = handle.GrantedAccess;
				hi.HandleAttributes = handle.HandleAttributes;
				hi.ProcessId = (ULONG)handle.UniqueProcessId;
				object->FirstHandle = std::move(hi);
			}
			object->ManualHandleCount++;
		}
		return objects;
	}

	std::vector<HandleInfo> FindHandlesForObject(std::string_view name);
	std::vector<HandleInfo> FindHandlesForObject(PVOID address);

	bool EnumHandles(PCWSTR type = nullptr, DWORD pid = 0, bool namedObjectsOnly = false);
	static int EnumTypes();

	template<typename T = HandleInfo> requires std::is_base_of_v<HandleInfo, T>
	static std::vector<std::shared_ptr<T>> EnumHandles2(PCWSTR type = nullptr, DWORD pid = 0, bool namedObjectsOnly = false, bool skipNames = false) {
		if(s_types.empty())
			EnumTypes();

		auto p = EnumHandlesBuffer();
		auto filteredTypeIndex = type == nullptr || type[0] == 0 ? -1 : s_typesNameMap.at(type)->TypeIndex;
		auto count = p->NumberOfHandles;
		std::vector<std::shared_ptr<T>> handles;
		handles.reserve(pid == 0 ? count : count / 16);
		for (decltype(count) i = 0; i < count; i++) {
			auto& handle = p->Handles[i];
			if (pid && handle.UniqueProcessId != pid)
				continue;

			if (filteredTypeIndex >= 0 && handle.ObjectTypeIndex != filteredTypeIndex)
				continue;

			CString name;
			if (!skipNames) {
				name = GetObjectName((HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId, handle.ObjectTypeIndex);
				if (namedObjectsOnly && name.IsEmpty())
					continue;
			}
			auto hi = std::make_shared<T>();
			hi->HandleValue = (ULONG)handle.HandleValue;
			hi->GrantedAccess = handle.GrantedAccess;
			hi->Object = handle.Object;
			hi->HandleAttributes = handle.HandleAttributes;
			hi->ProcessId = (ULONG)handle.UniqueProcessId;
			hi->ObjectTypeIndex = handle.ObjectTypeIndex;
			hi->Name = name;

			handles.emplace_back(std::move(hi));
		}
		return handles;
	}

	const std::vector<std::shared_ptr<ObjectInfo>>& GetObjects() const;

	static HANDLE DupHandle(HANDLE h, DWORD pid, ACCESS_MASK access = GENERIC_READ, DWORD flags = 0);
	static NTSTATUS OpenObject(PCWSTR path, PCWSTR type, HANDLE& handle, DWORD access = GENERIC_READ);
	static std::pair<HANDLE, DWORD> FindFirstHandle(PCWSTR name, USHORT index, DWORD pid = 0);

	enum class ChangeType {
		NoChange,
		TotalHandles,
		TotalObjects,
		PeakHandles,
		PeakObjects,
	};

	using Change = std::tuple<std::shared_ptr<ObjectTypeInfo>, ChangeType, int32_t>;
	static const std::vector<Change>& GetChanges();

	static CString GetObjectName(HANDLE hObject, ULONG pid, USHORT type);
	static CString GetObjectName(HANDLE hDup, USHORT type);

	static std::shared_ptr<ObjectTypeInfo> GetType(USHORT index);
	static std::shared_ptr<ObjectTypeInfo> GetType(PCWSTR name);
	static const std::vector<ObjectTypePtr>& GetObjectTypes();
	const std::vector<std::shared_ptr<HandleInfo>>& GetHandles() const;

	static std::vector<ObjectNameAndType> EnumDirectoryObjects(PCWSTR path);
	static CString GetSymbolicLinkTarget(PCWSTR path);

	inline static int64_t TotalHandles, TotalObjects, PeakHandles, PeakObjects;

private:
	static wil::unique_virtualalloc_ptr<NT::SYSTEM_HANDLE_INFORMATION_EX> EnumHandlesBuffer();

private:
	inline static std::vector<ObjectTypePtr> s_types;
	inline static std::unordered_map<int16_t, ObjectTypePtr> s_typesMap;
	inline static std::unordered_map<std::wstring, ObjectTypePtr> s_typesNameMap;
	inline static std::vector<Change> s_changes;

	std::vector<std::shared_ptr<ObjectInfo>> m_objects;
	std::unordered_map<PVOID, std::shared_ptr<ObjectInfo>> m_objectsByAddress;
	std::vector<std::shared_ptr<HandleInfo>> m_handles;
	bool m_skipThisProcess = false;
};

