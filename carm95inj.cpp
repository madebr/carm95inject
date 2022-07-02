#include <windows.h>
#include <detours.h>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ExportContext
{
    BOOL    fHasOrdinal1;
    ULONG   nExports;
};

static BOOL CALLBACK ExportCallback(_In_opt_ PVOID pContext,
                                    _In_ ULONG nOrdinal,
                                    _In_opt_ LPCSTR pszSymbol,
                                    _In_opt_ PVOID pbTarget)
{
    (void)pContext;
    (void)pbTarget;
    (void)pszSymbol;

    ExportContext *pec = (ExportContext *)pContext;

    if (nOrdinal == 1) {
        pec->fHasOrdinal1 = TRUE;
    }
    pec->nExports++;

    return TRUE;
}

static bool ValidatePlugin(const std::string &pluginPath) {
    HMODULE hDll = LoadLibraryExA(pluginPath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (hDll == nullptr) {
        printf("Error: %s failed to load (error %ld).\n", pluginPath.c_str(), GetLastError());
        return false;
    }

    ExportContext ec;
    ec.fHasOrdinal1 = FALSE;
    ec.nExports = 0;
    DetourEnumerateExports(hDll, &ec, ExportCallback);
    FreeLibrary(hDll);

    if (!ec.fHasOrdinal1) {
        printf("Error: %s does not export ordinal #1.\n", pluginPath.c_str());
        return false;
    }
    return true;
}

int CDECL main(int argc, char **argv)
{
#if 0
    auto carmaPathDir = fs::current_path();
#else
    char exePath[512];
    DWORD nLen = GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    if (nLen == sizeof(exePath)) {
        return 1;
    }
    exePath[nLen] = '\0';
    auto carmaPathDir = fs::path(exePath).parent_path();
#endif
    const auto carmaPathDirStr = carmaPathDir.generic_string();
    const auto pluginsDirPath = carmaPathDir / "plugins";

    const auto carm95ExePath = (carmaPathDir / "CARM95.EXE").generic_string();
    if (!fs::is_regular_file(carm95ExePath)) {
        printf("%s does not exist.\n", carm95ExePath.c_str());
        return 1;
    }

    std::vector<std::string> pluginPaths;

    printf("plugins:\n");
    for (auto pluginItem : fs::directory_iterator(pluginsDirPath)) {
        if (!pluginItem.exists() || !pluginItem.is_regular_file()) {
            continue;
        }
        auto pluginPath = pluginItem.path();
        if (pluginPath.extension() != ".dll") {
            continue;
        }
        auto pluginPathStr = pluginPath.generic_string();
        if (!ValidatePlugin(pluginPathStr)) {
            continue;
        }
        pluginPaths.push_back(pluginPathStr);
        printf("- %s\n", pluginPaths.back().c_str());
    }

    //////////////////////////////////////////////////////////////////////////
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    std::string command;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    command = carm95ExePath;
    char* commandBuffer = new char[command.size() + 1];
    command.copy(commandBuffer, command.size());
    commandBuffer[command.size()] = '\0';

    std::vector<const char*> pluginPathsCStrs;
    pluginPathsCStrs.resize(pluginPaths.size());
    std::transform(pluginPaths.begin(), pluginPaths.end(), pluginPathsCStrs.begin(), [](const auto &str) { return str.data(); });

    printf("carm95inj.exe: Starting: `%s'\n", command.c_str());
    for (const auto& pluginPath : pluginPaths) {
        printf("carm95inj.exe:   with `%s'\n", pluginPath.c_str());
    }
    fflush(stdout);

    DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

    SetLastError(0);
    if (!DetourCreateProcessWithDllsA(
            carm95ExePath.c_str(),
            commandBuffer,
            nullptr,
            nullptr,
            TRUE,
            dwFlags,
            nullptr,
            carmaPathDirStr.c_str(),
            &si,
            &pi,
            pluginPathsCStrs.size(),
            pluginPathsCStrs.data(),
            nullptr)) {
        DWORD dwError = GetLastError();
        printf("carm95inj.exe: DetourCreateProcessWithDllEx failed: %ld\n", dwError);
        if (dwError == ERROR_INVALID_HANDLE) {
#if DETOURS_64BIT
            printf("carm95inj.exe: Can't detour a 32-bit target process from a 64-bit parent process.\n");
#else
            printf("carm95inj"
                   ".exe: Can't detour a 64-bit target process from a 32-bit parent process.\n");
#endif
        }
        printf("ERROR");
        ExitProcess(9009);
    }

    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD dwResult = 0;
    if (!GetExitCodeProcess(pi.hProcess, &dwResult)) {
        printf("withdll.exe: GetExitCodeProcess failed: %ld\n", GetLastError());
        return 9010;
    }

    return dwResult;
}
