// AMD64 adaptation of Matt Pietrek's WheatyExceptionReport concept.
// Writes a minidump and a human-readable symbolized report for unhandled crashes.
#include "WheatyExceptionReport.h"

#include <DbgHelp.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

#ifndef EXCEPTION_ASSERTION_FAILURE
#define EXCEPTION_ASSERTION_FAILURE ((DWORD)0xC0000420L)
#endif
#ifndef BLITZ_GIT_REVISION
#define BLITZ_GIT_REVISION "unknown"
#endif

namespace
{
	std::atomic<WheatyExceptionReport*> g_reporter = nullptr;
	std::atomic<WheatyExceptionReport::CrashCallback> g_crashCallback = nullptr;
	std::atomic_flag g_alreadyCrashed = ATOMIC_FLAG_INIT;

	struct FileCloser
	{
		FILE* file = nullptr;
		~FileCloser() { if (file) std::fclose(file); }
	};

	const char* ExceptionName(DWORD code)
	{
#define WHEATY_EXCEPTION(x) case EXCEPTION_##x: return #x
		switch (code)
		{
			WHEATY_EXCEPTION(ACCESS_VIOLATION);
			WHEATY_EXCEPTION(DATATYPE_MISALIGNMENT);
			WHEATY_EXCEPTION(BREAKPOINT);
			WHEATY_EXCEPTION(SINGLE_STEP);
			WHEATY_EXCEPTION(ARRAY_BOUNDS_EXCEEDED);
			WHEATY_EXCEPTION(FLT_DENORMAL_OPERAND);
			WHEATY_EXCEPTION(FLT_DIVIDE_BY_ZERO);
			WHEATY_EXCEPTION(FLT_INEXACT_RESULT);
			WHEATY_EXCEPTION(FLT_INVALID_OPERATION);
			WHEATY_EXCEPTION(FLT_OVERFLOW);
			WHEATY_EXCEPTION(FLT_STACK_CHECK);
			WHEATY_EXCEPTION(FLT_UNDERFLOW);
			WHEATY_EXCEPTION(INT_DIVIDE_BY_ZERO);
			WHEATY_EXCEPTION(INT_OVERFLOW);
			WHEATY_EXCEPTION(PRIV_INSTRUCTION);
			WHEATY_EXCEPTION(IN_PAGE_ERROR);
			WHEATY_EXCEPTION(ILLEGAL_INSTRUCTION);
			WHEATY_EXCEPTION(NONCONTINUABLE_EXCEPTION);
			WHEATY_EXCEPTION(STACK_OVERFLOW);
			WHEATY_EXCEPTION(INVALID_DISPOSITION);
			WHEATY_EXCEPTION(GUARD_PAGE);
			WHEATY_EXCEPTION(INVALID_HANDLE);
		case EXCEPTION_ASSERTION_FAILURE: return "ASSERTION_FAILURE";
		case 0xE06D7363: return "UNHANDLED_CPP_EXCEPTION";
		default: return "UNKNOWN_EXCEPTION";
		}
#undef WHEATY_EXCEPTION
	}

	std::string GetExecutablePath()
	{
		std::vector<char> buffer(MAX_PATH);
		for (;;)
		{
			DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (length == 0)
				return {};
			if (length < buffer.size())
				return std::string(buffer.data(), length);
			buffer.resize(buffer.size() * 2);
		}
	}

	std::string BaseName(const std::string& path)
	{
		const size_t slash = path.find_last_of("\\/");
		std::string result = slash == std::string::npos ? path : path.substr(slash + 1);
		const size_t dot = result.find_last_of('.');
		if (dot != std::string::npos)
			result.resize(dot);
		return result.empty() ? "Blitzkrieg" : result;
	}

	bool PrepareCrashPaths(std::string& dumpPath, std::string& textPath)
	{
		const std::string executable = GetExecutablePath();
		if (executable.empty())
			return false;
		const size_t slash = executable.find_last_of("\\/");
		const std::string directory = slash == std::string::npos ? "." : executable.substr(0, slash);
		const std::string crashDirectory = directory + "\\Crashes";
		if (!CreateDirectoryA(crashDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
			return false;

		SYSTEMTIME now{};
		GetLocalTime(&now);
		char stamp[128] = {};
		_snprintf_s(stamp, _countof(stamp), _TRUNCATE,
			"%04u-%02u-%02u_%02u-%02u-%02u_%03u",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);

		const std::string prefix = crashDirectory + "\\" + BaseName(executable) + "_" + stamp;
		dumpPath = prefix + ".dmp";
		textPath = prefix + ".txt";
		return true;
	}

	void PrintSystemInfo(FILE* out)
	{
		SYSTEM_INFO systemInfo{};
		GetNativeSystemInfo(&systemInfo);
		MEMORYSTATUSEX memory{};
		memory.dwLength = sizeof(memory);
		GlobalMemoryStatusEx(&memory);

		using RtlGetVersionFn = NTSTATUS (NTAPI*)(PRTL_OSVERSIONINFOW);
		auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
		RTL_OSVERSIONINFOW version{};
		version.dwOSVersionInfoSize = sizeof(version);
		if (rtlGetVersion)
			rtlGetVersion(&version);

		std::fprintf(out, "\n=== System ===\n");
		std::fprintf(out, "Architecture: x86-64 (AMD64)\n");
		std::fprintf(out, "Processors: %lu\n", static_cast<unsigned long>(systemInfo.dwNumberOfProcessors));
		std::fprintf(out, "Physical memory: %.2f GiB total, %.2f GiB available\n",
			static_cast<double>(memory.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0),
			static_cast<double>(memory.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0));
		if (version.dwMajorVersion)
			std::fprintf(out, "Windows: %lu.%lu build %lu\n",
				static_cast<unsigned long>(version.dwMajorVersion),
				static_cast<unsigned long>(version.dwMinorVersion),
				static_cast<unsigned long>(version.dwBuildNumber));
	}

	void PrintRegisters(FILE* out, const CONTEXT& c)
	{
		std::fprintf(out, "\n=== Registers ===\n");
		std::fprintf(out, "RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX\n",
			c.Rax, c.Rbx, c.Rcx, c.Rdx);
		std::fprintf(out, "RSI=%016llX RDI=%016llX RBP=%016llX RSP=%016llX\n",
			c.Rsi, c.Rdi, c.Rbp, c.Rsp);
		std::fprintf(out, " R8=%016llX  R9=%016llX R10=%016llX R11=%016llX\n",
			c.R8, c.R9, c.R10, c.R11);
		std::fprintf(out, "R12=%016llX R13=%016llX R14=%016llX R15=%016llX\n",
			c.R12, c.R13, c.R14, c.R15);
		std::fprintf(out, "RIP=%016llX EFLAGS=%08lX\n", c.Rip, static_cast<unsigned long>(c.EFlags));
	}

	std::string ModuleForAddress(void* address)
	{
		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQuery(address, &mbi, sizeof(mbi)) || !mbi.AllocationBase)
			return {};
		char path[MAX_PATH] = {};
		if (!GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), path, MAX_PATH))
			return {};
		return path;
	}

	void PrintStack(FILE* out, HANDLE process, HANDLE thread, CONTEXT context, const char* heading, bool symbolsReady)
	{
		std::fprintf(out, "\n=== %s ===\n", heading);
		if (!symbolsReady)
		{
			std::fprintf(out, "#000 %016llX <symbols unavailable>\n", context.Rip);
			return;
		}

		STACKFRAME64 frame{};
		frame.AddrPC.Offset = context.Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rbp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode = AddrModeFlat;

		for (unsigned depth = 0; depth < 256; ++depth)
		{
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
				SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
				break;
			if (!frame.AddrPC.Offset)
				break;

			const DWORD64 address = frame.AddrPC.Offset;
			alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
			auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;
			DWORD64 displacement = 0;

			std::fprintf(out, "#%03u %016llX ", depth, address);
			if (SymFromAddr(process, address, &displacement, symbol))
				std::fprintf(out, "%s+0x%llX", symbol->Name, displacement);
			else
				std::fprintf(out, "<unknown>");

			IMAGEHLP_LINE64 line{};
			line.SizeOfStruct = sizeof(line);
			DWORD lineDisplacement = 0;
			if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
				std::fprintf(out, "  %s:%lu", line.FileName, static_cast<unsigned long>(line.LineNumber));
			std::fputc('\n', out);
		}
	}

	void PrintOtherThreads(FILE* out, HANDLE process, DWORD processId, DWORD crashingThreadId, bool symbolsReady)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return;

		THREADENTRY32 entry{};
		entry.dwSize = sizeof(entry);
		if (Thread32First(snapshot, &entry))
		{
			do
			{
				if (entry.th32OwnerProcessID != processId || entry.th32ThreadID == crashingThreadId)
					continue;
				HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME,
					FALSE, entry.th32ThreadID);
				if (!thread)
					continue;

				const DWORD suspendResult = SuspendThread(thread);
				if (suspendResult != static_cast<DWORD>(-1))
				{
					CONTEXT context{};
					context.ContextFlags = CONTEXT_FULL;
					if (GetThreadContext(thread, &context))
					{
						char heading[64] = {};
						_snprintf_s(heading, _countof(heading), _TRUNCATE, "Thread %lu call stack", static_cast<unsigned long>(entry.th32ThreadID));
						PrintStack(out, process, thread, context, heading, symbolsReady);
					}
					ResumeThread(thread);
				}
				CloseHandle(thread);
			} while (Thread32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);
	}

	void InvokeCrashCallbackSafely(WheatyExceptionReport::CrashCallback callback) noexcept
	{
		if (!callback)
			return;

		__try
		{
			callback();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void WriteMiniDump(const std::string& path, EXCEPTION_POINTERS* exceptionInfo)
	{
		HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		MINIDUMP_EXCEPTION_INFORMATION exception{};
		exception.ThreadId = GetCurrentThreadId();
		exception.ExceptionPointers = exceptionInfo;
		exception.ClientPointers = FALSE;

		MINIDUMP_USER_STREAM assertionStream{};
		MINIDUMP_USER_STREAM_INFORMATION streamInfo{};
		MINIDUMP_USER_STREAM_INFORMATION* streamInfoPtr = nullptr;
		if (exceptionInfo && exceptionInfo->ExceptionRecord &&
			exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ASSERTION_FAILURE &&
			exceptionInfo->ExceptionRecord->NumberParameters >= 1 &&
			exceptionInfo->ExceptionRecord->ExceptionInformation[0])
		{
			const char* text = reinterpret_cast<const char*>(exceptionInfo->ExceptionRecord->ExceptionInformation[0]);
			assertionStream.Type = CommentStreamA;
			assertionStream.Buffer = const_cast<char*>(text);
			assertionStream.BufferSize = static_cast<ULONG>(std::min<size_t>(std::strlen(text) + 1, 4096));
			streamInfo.UserStreamCount = 1;
			streamInfo.UserStreamArray = &assertionStream;
			streamInfoPtr = &streamInfo;
		}

		const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
			MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpScanMemory);
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dumpType,
			exceptionInfo ? &exception : nullptr, streamInfoPtr, nullptr);
		CloseHandle(file);
	}

	void WriteTextReport(const std::string& path, EXCEPTION_POINTERS* exceptionInfo)
	{
		FILE* raw = nullptr;
		if (fopen_s(&raw, path.c_str(), "wb") != 0 || !raw)
			return;
		FileCloser closer{raw};
		FILE* out = raw;

		SYSTEMTIME now{};
		GetLocalTime(&now);
		std::fprintf(out, "Blitzkrieg x64 crash report\n");
		std::fprintf(out, "Revision: %s\n", BLITZ_GIT_REVISION);
		std::fprintf(out, "Date: %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);

		if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord)
		{
			std::fprintf(out, "No exception context is available.\n");
			return;
		}

		const EXCEPTION_RECORD& record = *exceptionInfo->ExceptionRecord;
		std::fprintf(out, "\n=== Exception ===\n");
		std::fprintf(out, "Code: 0x%08lX (%s)\n", static_cast<unsigned long>(record.ExceptionCode), ExceptionName(record.ExceptionCode));
		std::fprintf(out, "Address: %p\n", record.ExceptionAddress);
		const std::string module = ModuleForAddress(record.ExceptionAddress);
		if (!module.empty())
			std::fprintf(out, "Module: %s\n", module.c_str());

		if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
		{
			const char* operation = record.ExceptionInformation[0] == 0 ? "read" :
				record.ExceptionInformation[0] == 1 ? "write" : "execute";
			std::fprintf(out, "Access violation: %s address %p\n", operation,
				reinterpret_cast<void*>(record.ExceptionInformation[1]));
		}
		if (record.ExceptionCode == EXCEPTION_ASSERTION_FAILURE && record.NumberParameters >= 1 && record.ExceptionInformation[0])
		{
			const char* text = reinterpret_cast<const char*>(record.ExceptionInformation[0]);
			std::fprintf(out, "Assertion: %.4095s\n", text);
			if (record.NumberParameters >= 2)
				std::fprintf(out, "Assertion origin: %p\n", reinterpret_cast<void*>(record.ExceptionInformation[1]));
		}

		PrintSystemInfo(out);
		PrintRegisters(out, *exceptionInfo->ContextRecord);

		HANDLE process = GetCurrentProcess();
		SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
		const BOOL symbolsReady = SymInitialize(process, nullptr, TRUE);
		if (!symbolsReady)
			std::fprintf(out, "\nDbgHelp: SymInitialize failed with %lu; stack addresses may lack symbols.\n", static_cast<unsigned long>(GetLastError()));

		PrintStack(out, process, GetCurrentThread(), *exceptionInfo->ContextRecord, "Crashing thread call stack", symbolsReady != FALSE);
		PrintOtherThreads(out, process, GetCurrentProcessId(), GetCurrentThreadId(), symbolsReady != FALSE);

		if (symbolsReady)
			SymCleanup(process);
	}
}

WheatyExceptionReport::WheatyExceptionReport()
{
	WheatyExceptionReport* expected = nullptr;
	if (!g_reporter.compare_exchange_strong(expected, this))
		return;

	previousFilter_ = SetUnhandledExceptionFilter(WheatyUnhandledExceptionFilter);
	previousCrtHandler_ = _set_invalid_parameter_handler(WheatyCrtHandler);
}

WheatyExceptionReport::~WheatyExceptionReport()
{
	if (g_reporter.load() != this)
		return;
	_set_invalid_parameter_handler(previousCrtHandler_);
	SetUnhandledExceptionFilter(previousFilter_);
	g_reporter.store(nullptr);
}

void WheatyExceptionReport::SetCrashCallback(CrashCallback callback) noexcept
{
	g_crashCallback.store(callback, std::memory_order_release);
}

LONG WINAPI WheatyExceptionReport::WheatyUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) noexcept
{
	WheatyExceptionReport* reporter = g_reporter.load(std::memory_order_acquire);
	return reporter ? reporter->HandleException(exceptionInfo) : EXCEPTION_CONTINUE_SEARCH;
}

LONG WheatyExceptionReport::HandleException(EXCEPTION_POINTERS* exceptionInfo) noexcept
{
	if (g_alreadyCrashed.test_and_set(std::memory_order_acq_rel))
		return EXCEPTION_EXECUTE_HANDLER;

	InvokeCrashCallbackSafely(g_crashCallback.load(std::memory_order_acquire));

	std::string dumpPath;
	std::string textPath;
	if (PrepareCrashPaths(dumpPath, textPath))
	{
		WriteMiniDump(dumpPath, exceptionInfo);
		WriteTextReport(textPath, exceptionInfo);
	}

	if (previousFilter_ && previousFilter_ != WheatyUnhandledExceptionFilter)
		return previousFilter_(exceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER;
}

void __cdecl WheatyExceptionReport::WheatyCrtHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	RaiseException(0xC000000DL, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}
