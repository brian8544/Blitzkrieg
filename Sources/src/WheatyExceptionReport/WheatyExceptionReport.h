#ifndef BLITZKRIEG_WHEATY_EXCEPTION_REPORT_H
#define BLITZKRIEG_WHEATY_EXCEPTION_REPORT_H

#pragma once

#include <Windows.h>
#include <crtdbg.h>
#include <stdlib.h>
#include <cstdint>

#if !defined(_M_X64)
#error WheatyExceptionReport in Blitzkrieg supports AMD64/x64 only.
#endif
static_assert(sizeof(void*) == 8, "Blitzkrieg is x64-only");

class WheatyExceptionReport
{
public:
	using CrashCallback = void (*)() noexcept;

	WheatyExceptionReport();
	WheatyExceptionReport(const WheatyExceptionReport&) = delete;
	WheatyExceptionReport& operator=(const WheatyExceptionReport&) = delete;
	~WheatyExceptionReport();

	static void SetCrashCallback(CrashCallback callback) noexcept;
	static LONG WINAPI WheatyUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) noexcept;

private:
	LONG HandleException(EXCEPTION_POINTERS* exceptionInfo) noexcept;
	static void __cdecl WheatyCrtHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t);

	LPTOP_LEVEL_EXCEPTION_FILTER previousFilter_ = nullptr;
	decltype(_set_invalid_parameter_handler(nullptr)) previousCrtHandler_ = nullptr;
};

#endif
