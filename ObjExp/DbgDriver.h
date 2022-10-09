#pragma once

class DbgDriver {
public:
	static DbgDriver& Get();

	~DbgDriver();
	operator bool() const;
	bool Open();
	void Close();
	bool Install();
	ULONG ReadVirtual(PVOID address, ULONG size, PVOID buffer);
	ULONG WriteVirtual(PVOID address, ULONG size, PVOID buffer);
	static bool WriteFileFromResource(PCWSTR path, UINT id, PCWSTR type = L"BIN");

	template<typename T> requires std::is_trivially_constructible_v<T>
	T ReadVirtual(PVOID address) {
		T value;
		return ReadVirtual(address, sizeof(T), &value) ? value : (T)0;
	}

private:
	DbgDriver() {}
	DbgDriver(DbgDriver const&) = delete;
	DbgDriver& operator=(DbgDriver const&) = delete;

	HANDLE m_hDevice{ nullptr };
};

