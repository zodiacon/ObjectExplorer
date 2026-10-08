#include "pch.h"
#include "FileQuery.h"
#include <unordered_set>

namespace {
	//
	// A worker that times out is abandoned, not terminated; it exits by itself once the query completes.
	// While it's stuck, its key is remembered so the same object isn't queried again.
	//
	class QueryRunner {
	public:
		static bool Run(HANDLE hFile, ULONG64 key, FileQuery::Function query, BYTE* result);

	private:
		enum class State : LONG { Busy, Completed, Abandoned };

		struct Worker {
			wil::unique_event_nothrow Start, Done;
			wil::unique_handle hFile;
			FileQuery::Function Query{ nullptr };
			ULONG64 Key{ 0 };
			bool Success{ false };
			LONG State{ (LONG)State::Completed };
			BYTE Buffer[FileQuery::BufferSize];
		};

		struct Shared {
			wil::srwlock Lock;
			std::vector<Worker*> Idle;
			std::unordered_set<ULONG64> StuckKeys;
			int StuckCount{ 0 };
		};

		static Shared& GetShared() {
			// intentionally leaked so abandoned workers can still use it during process shutdown
			static auto shared = new Shared;
			return *shared;
		}

		static Worker* CreateWorker();
		static void ReturnToPool(Worker* worker);
		static DWORD WINAPI WorkerThread(PVOID p);

		static constexpr DWORD QueryTimeout = 100;
		static constexpr int MaxStuckWorkers = 16;
	};

	QueryRunner::Worker* QueryRunner::CreateWorker() {
		auto worker = new (std::nothrow) Worker;
		if (!worker)
			return nullptr;

		if (FAILED(worker->Start.create(wil::EventOptions::None)) || FAILED(worker->Done.create(wil::EventOptions::None))) {
			delete worker;
			return nullptr;
		}

		wil::unique_handle hThread(::CreateThread(nullptr, 1 << 16, WorkerThread, worker, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
		if (!hThread) {
			delete worker;
			return nullptr;
		}
		return worker;
	}

	void QueryRunner::ReturnToPool(Worker* worker) {
		auto& shared = GetShared();
		auto lock = shared.Lock.lock_exclusive();
		shared.Idle.push_back(worker);
	}

	DWORD WINAPI QueryRunner::WorkerThread(PVOID p) {
		auto worker = (Worker*)p;
		for (;;) {
			worker->Start.wait();
			worker->Success = worker->Query(worker->hFile.get(), worker->Buffer);
			worker->hFile.reset();
			if (::InterlockedCompareExchange(&worker->State, (LONG)State::Completed, (LONG)State::Busy) == (LONG)State::Busy) {
				worker->Done.SetEvent();
				continue;
			}

			//
			// the caller gave up on this worker
			//
			auto& shared = GetShared();
			{
				auto lock = shared.Lock.lock_exclusive();
				shared.StuckCount--;
				if (worker->Key)
					shared.StuckKeys.erase(worker->Key);
			}
			delete worker;
			return 0;
		}
	}

	bool QueryRunner::Run(HANDLE hFile, ULONG64 key, FileQuery::Function query, BYTE* result) {
		auto& shared = GetShared();
		Worker* worker = nullptr;
		{
			auto lock = shared.Lock.lock_exclusive();
			if (key && shared.StuckKeys.contains(key))
				return false;
			if (!shared.Idle.empty()) {
				worker = shared.Idle.back();
				shared.Idle.pop_back();
			}
			else if (shared.StuckCount >= MaxStuckWorkers) {
				return false;
			}
		}
		if (!worker) {
			worker = CreateWorker();
			if (!worker)
				return false;
		}

		//
		// the worker gets its own handle, since the caller closes its handle when we return
		//
		HANDLE hDup;
		if (!::DuplicateHandle(::GetCurrentProcess(), hFile, ::GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			ReturnToPool(worker);
			return false;
		}
		worker->hFile.reset(hDup);
		worker->Query = query;
		worker->Key = key;
		worker->State = (LONG)State::Busy;
		worker->Start.SetEvent();

		if (!worker->Done.wait(QueryTimeout)) {
			{
				auto lock = shared.Lock.lock_exclusive();
				if (::InterlockedCompareExchange(&worker->State, (LONG)State::Abandoned, (LONG)State::Busy) == (LONG)State::Busy) {
					shared.StuckCount++;
					if (key)
						shared.StuckKeys.insert(key);
					return false;
				}
			}
			// completed just as the timeout expired
			worker->Done.wait();
		}

		bool success = worker->Success;
		if (success)
			memcpy(result, worker->Buffer, sizeof(worker->Buffer));
		ReturnToPool(worker);
		return success;
	}
}

bool FileQuery::Run(HANDLE hFile, ULONG64 key, Function query, BYTE* result) {
	return QueryRunner::Run(hFile, key, query, result);
}
