#include "iris/crash.hpp"

#include "iris/crash_report.hpp"
#include "iris/log.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace iris {
namespace {

std::filesystem::path crashPath;

#if defined(_WIN32)
wchar_t crashFile[MAX_PATH] = {};

void appendWide(const std::string& text) {
    if (crashFile[0] == L'\0') {
        return;
    }
    HANDLE file = CreateFileW(
        crashFile, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
}

LONG WINAPI onUnhandled(EXCEPTION_POINTERS* info) {
    if (IsDebuggerPresent() != FALSE) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    unsigned long code = 0;
    std::uint64_t address = 0;
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        code = info->ExceptionRecord->ExceptionCode;
        address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress));
    }
    const std::string summary = formatCrashSummary(code, address) + "\r\n";
    appendWide(summary);
    MessageBoxA(nullptr, summary.c_str(), "Iris Emulator", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void appendCrashFile(std::string_view message) {
    if (crashPath.empty()) {
        return;
    }
    std::ofstream output(crashPath, std::ios::binary | std::ios::app);
    output << message << '\n';
}

}  // namespace

void installCrashHandler() {
#if defined(_WIN32)
    SetUnhandledExceptionFilter(onUnhandled);
#endif
}

void setCrashLogFile(const std::filesystem::path& path) {
    crashPath = path;
#if defined(_WIN32)
    const std::wstring text = path.wstring();
    if (text.size() >= MAX_PATH) {
        crashFile[0] = L'\0';
        return;
    }
    wcscpy_s(crashFile, text.c_str());
#endif
}

void reportFatalError(std::string_view message) {
    std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()), message.data());
    log::critical(message);
    appendCrashFile(message);
#if defined(_WIN32)
    MessageBoxA(nullptr, std::string(message).c_str(), "Iris Emulator", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
}

}  // namespace iris
