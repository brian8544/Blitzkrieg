#ifndef BLITZKRIEG_DIAGNOSTICS_ASSERT_H
#define BLITZKRIEG_DIAGNOSTICS_ASSERT_H

#pragma once

#include <Windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>

#if !defined(_M_X64)
#error Blitzkrieg diagnostics support only the x64 build.
#endif
static_assert(sizeof(void*) == 8, "Blitzkrieg is x64-only");

#ifndef EXCEPTION_ASSERTION_FAILURE
#define EXCEPTION_ASSERTION_FAILURE ((DWORD)0xC0000420L)
#endif

namespace NDiagnostics
{
	enum class EAssertResult
	{
		Abort,
		Debug,
		Continue
	};

	inline EAssertResult ReportAssert(const char* condition, const char* description,
		const char* fileName, int lineNumber, bool /*forceMode*/)
	{
		char buffer[4096] = {};
		_snprintf_s(buffer, _countof(buffer), _TRUNCATE,
			"Assertion failed!\n\nCondition: %s\nDescription: %s\nFile: %s\nLine: %d\n\n"
			"Continue: execute the assertion recovery statement and continue.\n"
			"Try Again: break into the debugger.\n"
			"Cancel: generate a Wheaty crash report and terminate.",
			condition ? condition : "<none>",
			description ? description : "",
			fileName ? fileName : "<unknown>",
			lineNumber);

		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");

		const int result = MessageBoxA(nullptr, buffer, "Blitzkrieg assertion",
			MB_CANCELTRYCONTINUE | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
		if (result == IDTRYAGAIN)
			return EAssertResult::Debug;
		if (result == IDCONTINUE)
			return EAssertResult::Continue;
		return EAssertResult::Abort;
	}

	inline EAssertResult ReportAssertHR(HRESULT result, const char* description,
		const char* fileName, int lineNumber, bool forceMode)
	{
		char condition[64] = {};
		_snprintf_s(condition, _countof(condition), _TRUNCATE, "HRESULT 0x%08lX", static_cast<unsigned long>(result));
		return ReportAssert(condition, description, fileName, lineNumber, forceMode);
	}

	[[noreturn]] inline void RaiseAssertionCrash(const char* condition, const char* description,
		const char* fileName, int lineNumber)
	{
		static thread_local char assertionText[4096];
		_snprintf_s(assertionText, _countof(assertionText), _TRUNCATE,
			"Assertion failed: %s | %s | %s:%d",
			condition ? condition : "<none>",
			description ? description : "",
			fileName ? fileName : "<unknown>",
			lineNumber);

		ULONG_PTR args[2] = {
			reinterpret_cast<ULONG_PTR>(assertionText),
			reinterpret_cast<ULONG_PTR>(_ReturnAddress())
		};
		RaiseException(EXCEPTION_ASSERTION_FAILURE, EXCEPTION_NONCONTINUABLE, 2, args);
		std::abort();
	}
}

#if defined(_MSC_VER)
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK DebugBreak()
#endif

#if defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
#define NI_ASSERT_TF(x, user_text, statement)   NI_FORCE_ASSERT(x, user_text, statement, false)
#define NI_ASSERT_T(x, user_text)               NI_FORCE_ASSERT(x, user_text, ;, false)
#define NI_ASSERT(x)                            NI_FORCE_ASSERT(x, "", ;, false)
#define NI_ASSERTHR_TF(x, user_text, statement) NI_FORCE_ASSERT_HR(x, user_text, statement, false)
#define NI_ASSERTHR_T(x, user_text)             NI_FORCE_ASSERT_HR(x, user_text, ;, false)
#define NI_ASSERTHR(x)                          NI_FORCE_ASSERT_HR(x, "", ;, false)
#else
#define NI_ASSERT_TF(x, user_text, statement)   do { } while (false)
#define NI_ASSERT_T(x, user_text)               do { } while (false)
#define NI_ASSERT(x)                            do { } while (false)
#define NI_ASSERTHR_TF(x, user_text, statement) do { } while (false)
#define NI_ASSERTHR_T(x, user_text)             do { } while (false)
#define NI_ASSERTHR(x)                          do { } while (false)
#endif

#if defined(_DO_ASSERT_SLOW)
#define NI_ASSERT_SLOW_TF(x, user_text, statement)   NI_FORCE_ASSERT(x, user_text, statement, false)
#define NI_ASSERT_SLOW_T(x, user_text)               NI_FORCE_ASSERT(x, user_text, ;, false)
#define NI_ASSERT_SLOW(x)                            NI_FORCE_ASSERT(x, "", ;, false)
#define NI_ASSERTHR_SLOW_TF(x, user_text, statement) NI_FORCE_ASSERT_HR(x, user_text, statement, false)
#define NI_ASSERTHR_SLOW_T(x, user_text)             NI_FORCE_ASSERT_HR(x, user_text, ;, false)
#define NI_ASSERTHR_SLOW(x)                          NI_FORCE_ASSERT_HR(x, "", ;, false)
#else
#define NI_ASSERT_SLOW_TF(x, user_text, statement)   do { } while (false)
#define NI_ASSERT_SLOW_T(x, user_text)               do { } while (false)
#define NI_ASSERT_SLOW(x)                            do { } while (false)
#define NI_ASSERTHR_SLOW_TF(x, user_text, statement) do { } while (false)
#define NI_ASSERTHR_SLOW_T(x, user_text)             do { } while (false)
#define NI_ASSERTHR_SLOW(x)                          do { } while (false)
#endif

#if defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
#define NI_FORCE_ASSERT(x, user_text, statement, bForce)                                      \
	do {                                                                                        \
		if (!(x)) {                                                                              \
			const char* _ni_assert_text = (user_text);                                             \
			switch (NDiagnostics::ReportAssert(#x, _ni_assert_text, __FILE__, __LINE__, (bForce))) { \
			case NDiagnostics::EAssertResult::Continue: { statement; } break;                     \
			case NDiagnostics::EAssertResult::Debug: DEBUG_BREAK; break;                          \
			case NDiagnostics::EAssertResult::Abort:                                              \
				NDiagnostics::RaiseAssertionCrash(#x, _ni_assert_text, __FILE__, __LINE__);        \
			}                                                                                     \
		}                                                                                         \
	} while (false)

#define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce)                                    \
	do {                                                                                         \
		const HRESULT _ni_hr = (x);                                                               \
		if (FAILED(_ni_hr)) {                                                                      \
			const char* _ni_assert_text = (user_text);                                              \
			switch (NDiagnostics::ReportAssertHR(_ni_hr, _ni_assert_text, __FILE__, __LINE__, (bForce))) { \
			case NDiagnostics::EAssertResult::Continue: { statement; } break;                      \
			case NDiagnostics::EAssertResult::Debug: DEBUG_BREAK; break;                           \
			case NDiagnostics::EAssertResult::Abort:                                               \
				NDiagnostics::RaiseAssertionCrash(#x, _ni_assert_text, __FILE__, __LINE__);         \
			}                                                                                      \
		}                                                                                          \
	} while (false)
#else
#define NI_FORCE_ASSERT(x, user_text, statement, bForce)    do { } while (false)
#define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce) do { } while (false)
#endif

#endif
