#include "pch.h"
#include "ObjectManager.h"
#include "NtDll.h"

int ObjectManager::EnumTypes() {
	const ULONG len = 1 << 14;
	wil::unique_virtualalloc_ptr<> buffer(::VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!NT_SUCCESS(NT::NtQueryObject(nullptr, NT::ObjectTypesInformation, buffer.get(), len, nullptr)))
		return 0;

	auto p = static_cast<NT::OBJECT_TYPES_INFORMATION*>(buffer.get());
	bool empty = m_types.empty();

	auto count = p->NumberOfTypes;
	if (empty) {
		m_types.reserve(count);
		m_typesMap.reserve(count);
		m_changes.reserve(32);
	}
	else {
		m_changes.clear();
		ATLASSERT(count == m_types.size());
	}
	auto raw = &p->TypeInformation[0];
	m_totalHandles = m_totalObjects = 0;

	for (ULONG i = 0; i < count; i++) {
		if (!::IsWindows8OrGreater()) {
			// TypeIndex is only supported since Win8. Uses the fake index for previous OS.
			raw->TypeIndex = static_cast<decltype(raw->TypeIndex)>(i);
		}
		auto type = empty ? std::make_shared<ObjectTypeInfo>() : m_typesMap[raw->TypeIndex];
		if (empty) {
			type->GenericMapping = raw->GenericMapping;
			type->TypeIndex = raw->TypeIndex;
			type->DefaultNonPagedPoolCharge = raw->DefaultNonPagedPoolCharge;
			type->DefaultPagedPoolCharge = raw->DefaultPagedPoolCharge;
			type->TypeName = CString(raw->TypeName.Buffer, raw->TypeName.Length / sizeof(WCHAR));
			type->PoolType = (PoolType)raw->PoolType;
			type->DefaultNonPagedPoolCharge = raw->DefaultNonPagedPoolCharge;
			type->DefaultPagedPoolCharge = raw->DefaultPagedPoolCharge;
			type->ValidAccessMask = raw->ValidAccessMask;
			type->InvalidAttributes = raw->InvalidAttributes;
		}
		else {
			if (type->TotalNumberOfHandles != raw->TotalNumberOfHandles)
				m_changes.push_back({ type, ChangeType::TotalHandles, (int32_t)raw->TotalNumberOfHandles - (int32_t)type->TotalNumberOfHandles });
			if (type->TotalNumberOfObjects != raw->TotalNumberOfObjects)
				m_changes.push_back({ type, ChangeType::TotalObjects, (int32_t)raw->TotalNumberOfObjects - (int32_t)type->TotalNumberOfObjects });
			if (type->HighWaterNumberOfHandles != raw->HighWaterNumberOfHandles)
				m_changes.push_back({ type, ChangeType::PeakHandles, (int32_t)raw->HighWaterNumberOfHandles - (int32_t)type->HighWaterNumberOfHandles });
			if (type->HighWaterNumberOfObjects != raw->HighWaterNumberOfObjects)
				m_changes.push_back({ type, ChangeType::PeakObjects, (int32_t)raw->HighWaterNumberOfObjects - (int32_t)type->HighWaterNumberOfObjects });
		}

		type->TotalNumberOfHandles = raw->TotalNumberOfHandles;
		type->TotalNumberOfObjects = raw->TotalNumberOfObjects;
		type->TotalNonPagedPoolUsage = raw->TotalNonPagedPoolUsage;
		type->TotalPagedPoolUsage = raw->TotalPagedPoolUsage;
		type->HighWaterNumberOfObjects = raw->HighWaterNumberOfObjects;
		type->HighWaterNumberOfHandles = raw->HighWaterNumberOfHandles;
		type->TotalNamePoolUsage = raw->TotalNamePoolUsage;

		m_totalObjects += raw->TotalNumberOfObjects;
		m_totalHandles += raw->TotalNumberOfHandles;

		if (empty) {
			m_types.emplace_back(type);
			m_typesMap.insert({ type->TypeIndex, type });
			m_typesNameMap.insert({ std::wstring(type->TypeName), type });
		}

		auto temp = (BYTE*)raw + sizeof(NT::OBJECT_TYPE_INFORMATION) + raw->TypeName.MaximumLength;
		temp += sizeof(PVOID) - 1;
		raw = reinterpret_cast<NT::OBJECT_TYPE_INFORMATION*>((ULONG_PTR)temp / sizeof(PVOID) * sizeof(PVOID));
	}
	return static_cast<int>(m_types.size());
}

const std::vector<ObjectManager::Change>& ObjectManager::GetChanges() const {
	return m_changes;
}

const std::vector<std::shared_ptr<ObjectTypeInfo>>& ObjectManager::GetObjectTypes() const {
	return m_types;
}

std::vector<ObjectNameAndType> ObjectManager::EnumDirectoryObjects(PCWSTR path) {
	std::vector<ObjectNameAndType> objects;
	wil::unique_handle hDirectory;
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, path);
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (!NT_SUCCESS(NT::NtOpenDirectoryObject(hDirectory.addressof(), DIRECTORY_QUERY, &attr)))
		return objects;

	objects.reserve(128);
	BYTE buffer[1 << 12];
	auto info = reinterpret_cast<NT::OBJECT_DIRECTORY_INFORMATION*>(buffer);
	bool first = true;
	ULONG size, index = 0;
	for (;;) {
		auto start = index;
		if (!NT_SUCCESS(NT::NtQueryDirectoryObject(hDirectory.get(), info, sizeof(buffer), FALSE, first, &index, &size)))
			break;
		first = false;
		for (ULONG i = 0; i < index - start; i++) {
			ObjectNameAndType data;
			auto& p = info[i];
			data.Name = std::wstring(p.Name.Buffer, p.Name.Length / sizeof(WCHAR));
			data.TypeName = std::wstring(p.TypeName.Buffer, p.TypeName.Length / sizeof(WCHAR));

			objects.push_back(std::move(data));
		}
	}
	return objects;

}

CString ObjectManager::GetSymbolicLinkTarget(PCWSTR path) {
	wil::unique_handle hLink;
	OBJECT_ATTRIBUTES attr;
	CString target;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, path);
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (NT_SUCCESS(NT::NtOpenSymbolicLinkObject(hLink.addressof(), GENERIC_READ, &attr))) {
		WCHAR buffer[1 << 10];
		UNICODE_STRING result;
		result.Buffer = buffer;
		result.MaximumLength = sizeof(buffer);
		if (NT_SUCCESS(NT::NtQuerySymbolicLinkObject(hLink.get(), &result, nullptr)))
			target.SetString(result.Buffer, result.Length / sizeof(WCHAR));
	}
	return target;
}
