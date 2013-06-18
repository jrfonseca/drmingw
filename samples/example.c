#include <assert.h>
#include <stdio.h>

#include <windows.h>
#include <dbghelp.h>

static int foo(int a, int b) {
    return a * b;
}

int main() {
    HMODULE hProcess = GetCurrentProcess();
    BOOL bRet;

    bRet = SymInitialize(hProcess, NULL, FALSE);
    assert(bRet);

#if 0
    DWORD Address = (DWORD64)(UINT_PTR)GetProcAddress(LoadLibraryA("user32.dll"), "MessageBoxA");
#else
    DWORD Address = (DWORD64)(UINT_PTR)&foo;
#endif

    DWORD64 Displacement = 0;
    struct {
        SYMBOL_INFO Symbol;
        CHAR Name[256];
    } s;
    memset(&s, 0, sizeof s);
    s.Symbol.SizeOfStruct = sizeof s.Symbol;
    s.Symbol.MaxNameLen = sizeof s.Symbol.Name + sizeof s.Name;
    bRet = SymFromAddr(hProcess, Address, &Displacement, &s.Symbol);
    printf("bRet = %i\n", bRet);
    if (bRet) {
        printf("Symbol.Name = %s\n", s.Symbol.Name);
    }

    SymCleanup(hProcess);

    return 0;
}
