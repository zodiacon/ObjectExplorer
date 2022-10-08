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

private:
	DbgDriver() {}
	DbgDriver(DbgDriver const&) = delete;
	DbgDriver& operator=(DbgDriver const&) = delete;

	HANDLE m_hDevice{ nullptr };
};

