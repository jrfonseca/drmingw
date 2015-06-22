/*
 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
 */


#include <windows.h>


static const DWORD MS_VC_EXCEPTION = 0x406D1388;


#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO {
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)


static void
SetThreadName(DWORD dwThreadID,
              LPCSTR szThreadName)
{
    THREADNAME_INFO ti;
    ti.dwType = 0x1000;
    ti.szName = szThreadName;
    ti.dwThreadID = dwThreadID;
    ti.dwFlags = 0;

    RaiseException(MS_VC_EXCEPTION, 0, sizeof ti / sizeof(ULONG_PTR), (ULONG_PTR *) &ti);
}


int
main(int argc, char *argv[])
{
    SetThreadName(-1, "main");
    return 0;
}


// CHECK_EXIT_CODE: 0
