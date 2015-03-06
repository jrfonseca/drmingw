#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include <windows.h>
#include <dbghelp.h>


static int
status = EXIT_SUCCESS;

static unsigned
test_no = 0;

// https://testanything.org/tap-specification.html
__attribute__ ((format (printf, 2, 3)))
static void
test_line(bool ok, const char *format, ...)
{
    va_list ap;

    if (!ok) {
        status = EXIT_FAILURE;
    }

    fprintf(stdout, "%s %u",
            ok ? "ok" : "not ok",
            ++test_no);
    if (format) {
        fputs(" - ", stdout);
        va_start(ap, format);
        vfprintf(stdout, format, ap);
        va_end(ap);
    }
    fputs("\n", stdout);
    fflush(stdout);
}

__attribute__ ((format (printf, 1, 2)))
static void
test_diagnostic(const char *format, ...)
{
    va_list ap;

    fputs("# ", stdout);
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fputs("\n", stdout);
    fflush(stdout);
}

static void
test_plan(void)
{
    fprintf(stdout, "1..%u\n", test_no);
    fflush(stdout);
}


static void
checkSym(HANDLE hProcess,
         const void *symbol,
         const char *szSymbolName,
         const char *szFileName,
         DWORD dwLineNumber)
{
    bool ok;
    BOOL bRet;

    DWORD64 dwAddr = (DWORD64)(UINT_PTR)symbol;

    // Test SymFromAddr
    DWORD64 Displacement = 0;
    struct {
        SYMBOL_INFO Symbol;
        CHAR Name[256];
    } s;
    memset(&s, 0, sizeof s);
    s.Symbol.SizeOfStruct = sizeof s.Symbol;
    s.Symbol.MaxNameLen = sizeof s.Symbol.Name + sizeof s.Name;
    bRet = SymFromAddr(hProcess, dwAddr, &Displacement, &s.Symbol);
    test_line(bRet, "SymFromAddr(&%s)", szSymbolName);
    if (bRet) {
        ok = strcmp(s.Symbol.Name, szSymbolName) == 0;
        test_line(ok, "SymFromAddr(&%s).Name", szSymbolName);
        if (!ok) {
            test_diagnostic("Name = \"%s\" != \"%s\"",
                            s.Symbol.Name, szSymbolName);
        }
    }

    // Test SymGetLineFromAddr64
    DWORD dwDisplacement;
    IMAGEHLP_LINE64 Line;
    ZeroMemory(&Line, sizeof Line);
    Line.SizeOfStruct = sizeof Line;
    bRet = SymGetLineFromAddr64(hProcess, dwAddr, &dwDisplacement, &Line);
    test_line(bRet, "SymGetLineFromAddr64(&%s)", szSymbolName);
    if (bRet) {
        ok = strcmp(Line.FileName, szFileName) == 0;
        test_line(ok, "SymGetLineFromAddr64(&%s).FileName", szSymbolName);
        if (!ok) {
            test_diagnostic("FileName = \"%s\" != \"%s\"",
                            Line.FileName, szFileName);
        }
        ok = Line.LineNumber == dwLineNumber;
        test_line(ok, "SymGetLineFromAddr64(&%s).LineNumber", szSymbolName);
        if (Line.LineNumber != dwLineNumber) {
            test_diagnostic("LineNumber = %lu != %lu",
                            Line.LineNumber, dwLineNumber);
        }
    }
}


static void
checkCaller(HANDLE hProcess,
            const char *szSymbolName,
            const char *szFileName,
            DWORD dwLineNumber)
{
    void *addr = __builtin_return_address(0);
    checkSym(hProcess, addr, szSymbolName, szFileName, dwLineNumber);
}


static const DWORD foo_line = __LINE__; static int foo(int a, int b) {
    return a * b;
}


static void dummy(void)
{
    getenv("HOME");
}

int
main()
{
    HMODULE hProcess = GetCurrentProcess();
    BOOL bRet;

    bRet = SymInitialize(hProcess, NULL, FALSE);
    test_line(bRet, "SymInitialize()");
    if (bRet) {
        checkSym(hProcess, &foo, "foo", __FILE__, foo_line);

        checkCaller(hProcess, "main", __FILE__, __LINE__); dummy();

        bRet = SymCleanup(hProcess);
        test_line(bRet, "SymCleanup()");
    }

    test_plan();

    return status;
}
