#pragma once

//
// Querying a file object opened for synchronous I/O can block indefinitely
// (e.g. a pipe with a pending read), so such queries run on worker threads with a timeout.
//
struct FileQuery abstract final {
	static constexpr ULONG BufferSize = 2048;

	// runs on a worker thread; writes its result to buffer (BufferSize bytes); returns false on failure
	using Function = bool(*)(HANDLE hFile, BYTE* buffer);

	// the result (BufferSize bytes) is copied to result on success;
	// key identifies the object (0 if unknown): while a query of an object is stuck, it isn't queried again
	static bool Run(HANDLE hFile, ULONG64 key, Function query, BYTE* result);
};
