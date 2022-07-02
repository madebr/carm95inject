#include <windows.h>
#include <detours.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

class StaticPatcher
{
private:
    using Patcher = void(*)();

    Patcher m_func;
    StaticPatcher   *m_next;
    static StaticPatcher    *ms_head;

    void Run() { m_func(); }
public:
    StaticPatcher(Patcher func);
    static void Apply();
};

StaticPatcher* StaticPatcher::ms_head;

StaticPatcher::StaticPatcher(Patcher func)
        : m_func(func)
{
    m_next = ms_head;
    ms_head = this;
}

void
StaticPatcher::Apply()
{
    StaticPatcher *current = ms_head;
    while(current){
        current->Run();
        current = current->m_next;
    }
    ms_head = nullptr;
}

#define STARTPATCHES_INNER2(VAL) static StaticPatcher Patcher##VAL([](){
#define STARTPATCHES_INNER(VAL) STARTPATCHES_INNER2(VAL)
#define STARTPATCHES STARTPATCHES_INNER(__COUNTER__)
#define ENDPATCHES });


enum class Carm95Type {
    CARM95_UNKNOWN,
    CARM95_C1,
    CARM95_SPLATPACK,
};

//__COUNTER__

static Carm95Type DetectCarm95Type() {
    if (*(uint32_t*)0x004b2600 == 0x8b575653) {
        return Carm95Type::CARM95_SPLATPACK;
    }
    if (*(uint32_t*)0x004985ec == 0xfdf4858b) {
        return Carm95Type::CARM95_C1;
    }
    return Carm95Type::CARM95_UNKNOWN;
}

#ifdef INJECT_DR_DPRINTF
typedef void (__cdecl * tdr_dprintf)(const char* fmt, ...);
static  tdr_dprintf  dr_dprintf;
static void dr_dprintf_hook(const char* fmt, ...) {
    va_list ap;

    if (strcmp(fmt, "ACTIVE but couldn't get keyboard device state on 1st attempt") == 0
        || strcmp(fmt, "Keyboard input lost; reacquiring...") == 0
        || strcmp(fmt, "Couldn't get keyboard device state on 2nd attempt") == 0
        || strcmp(fmt, "Zeroing pKeys") == 0
        || strcmp(fmt, "Keyboard reacquired OK.") == 0) {
        // Avoid noisy prints when Carmageddon loses focus
        return;
    }

    printf("dr_dprintf: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

STARTPATCHES
    switch (DetectCarm95Type()) {
    case Carm95Type::CARM95_SPLATPACK:
        dr_dprintf = (tdr_dprintf)0x00461645;
        break;
    case Carm95Type::CARM95_C1:
        dr_dprintf = (tdr_dprintf)0x004616d5;
        break;
    }
    if (dr_dprintf) {
        DetourAttach(&dr_dprintf, dr_dprintf_hook);
    }
ENDPATCHES
#endif

#ifdef INJECT_DRFOPEN
typedef void * (__cdecl * tDRfopen)(const char*, const char*);
static tDRfopen DRfopen = (tDRfopen)0x00426583;
static void *__cdecl DRfopen_hook(const char* path, const char* mode) {
    printf("DRfopen(\"%s\", \"%s\")\n", path, mode);
    return DRfopen(path, mode);
}
STARTPATCHES
    printf("DRfopen\n");
    switch (DetectCarm95Type()) {
    case Carm95Type::CARM95_SPLATPACK:
    case Carm95Type::CARM95_C1:
        DRfopen = (tDRfopen)0x00426583;
        break;
    }
    if (DRfopen) {
        DetourAttach(&DRfopen, DRfopen_hook);
    }
ENDPATCHES
#endif

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        BOOL bSuccess = AllocConsole();
        SetConsoleTitle(TEXT("CARM95 Console"));
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        switch (DetectCarm95Type()) {
        case Carm95Type::CARM95_SPLATPACK:
            printf("Detected Carmageddon Splat Pack CARM95.EXE executable\n");
            break;
        case Carm95Type::CARM95_C1:
            printf("Detected Carmageddon1 CARM95.EXE executable\n");
            break;
        case Carm95Type::CARM95_UNKNOWN:
            printf("UNKNOWN executable\n");
            break;
        }
        StaticPatcher::Apply();

        DetourTransactionCommit();
    } else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourTransactionCommit();
    }
    return TRUE;
}
