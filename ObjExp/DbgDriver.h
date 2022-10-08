#pragma once

class DbgDriver {
public:
	static DbgDriver& Get();
	~DbgDriver();
	bool Open();
	void Close();
	bool Install();
	ULONG ReadVirtual(PVOID address, ULONG size, PVOID buffer);
	ULONG WriteVirtual(PVOID address, ULONG size, PVOID buffer);
	static bool WriteFileFromResource(PCWSTR path, UINT id, PCWSTR type = L"BIN");

private:
	DbgDriver() {}
	HANDLE m_hDevice{ nullptr };
};

