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

	void Uninstall();

	HANDLE m_hDevice{ nullptr };
	//
	// what Install did, so a failed install or open undoes only that:
	// the driver may also be in use by others (e.g. a local kernel debugger)
	//
	bool m_Started{ false }, m_CreatedService{ false }, m_WroteFile{ false };
	std::wstring m_DriverPath;
};

