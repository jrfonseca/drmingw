"""Launch a process on a background desktop so created windows are invisible.

A ctypes rewrite of https://gist.github.com/jl2/1083319
"""

__all__ = ["spawndesk"]


import ctypes
import random
import sys

from subprocess import list2cmdline  # undocumented

from ctypes import POINTER
from ctypes.wintypes import HANDLE, HDESK, LPVOID, WORD, DWORD, LPDWORD, BOOL, LPCWSTR


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("nLength", DWORD),
        ("lpSecurityDescriptor", LPVOID),
        ("bInheritHandle", BOOL),
    ]


LPSECURITY_ATTRIBUTES = POINTER(SECURITY_ATTRIBUTES)


class STARTUPINFO(ctypes.Structure):
    _fields_ = [
        ("cb", DWORD),
        ("lpReserved", LPCWSTR),
        ("lpDesktop", LPCWSTR),
        ("lpTitle", LPCWSTR),
        ("dwX", DWORD),
        ("dwY", DWORD),
        ("dwXSize", DWORD),
        ("dwYSize", DWORD),
        ("dwXCountChars", DWORD),
        ("dwYCountChars", DWORD),
        ("dwFillAttribute", DWORD),
        ("dwFlags", DWORD),
        ("wShowWindow", WORD),
        ("cbReserved2", WORD),
        ("lpReserved2", LPVOID),
        ("hStdInput", HANDLE),
        ("hStdOutput", HANDLE),
        ("hStdError", HANDLE),
    ]


LPSTARTUPINFO = POINTER(STARTUPINFO)


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("hProcess", HANDLE),
        ("hThread", HANDLE),
        ("dwProcessId", DWORD),
        ("dwThreadId", DWORD),
    ]


LPPROCESS_INFORMATION = POINTER(PROCESS_INFORMATION)


INFINITE = 0xFFFFFFFF
MAXIMUM_ALLOWED = 0x02000000
STILL_ACTIVE = 259
WAIT_FAILED = 0xFFFFFFFF
WAIT_TIMEOUT = 0x0102


CreateProcess = ctypes.windll.kernel32.CreateProcessW
CreateProcess.argtypes = [
    LPCWSTR,
    LPCWSTR,
    LPSECURITY_ATTRIBUTES,
    LPSECURITY_ATTRIBUTES,
    BOOL,
    DWORD,
    LPVOID,
    LPCWSTR,
    LPSTARTUPINFO,
    LPPROCESS_INFORMATION,
]
CreateProcess.restype = BOOL

GetExitCodeProcess = ctypes.windll.kernel32.GetExitCodeProcess
GetExitCodeProcess.argtypes = [HANDLE, LPDWORD]
GetExitCodeProcess.restype = BOOL

WaitForSingleObject = ctypes.windll.kernel32.WaitForSingleObject
WaitForSingleObject.argtypes = [HANDLE, DWORD]
WaitForSingleObject.restype = DWORD

CloseHandle = ctypes.windll.kernel32.CloseHandle
CloseHandle.argtypes = [HANDLE]
CloseHandle.restype = BOOL

CreateDesktop = ctypes.windll.user32.CreateDesktopW
CreateDesktop.argstypes = [
    LPCWSTR,
    LPCWSTR,
    LPVOID,  # DEVMODEW
    DWORD,
    DWORD,  # ACCESS_MASK
    LPSECURITY_ATTRIBUTES,
]
CreateDesktop.restype = HDESK

CloseDesktop = ctypes.windll.user32.CloseDesktop
CloseDesktop.argstypes = [HDESK]
CloseDesktop.restype = BOOL


def spawndesk(args):

    saAttr = SECURITY_ATTRIBUTES()
    saAttr.nLength = ctypes.sizeof(saAttr)
    saAttr.bInheritHandle = True
    saAttr.lpSecurityDescriptor = None

    lpDesktop = "".join(random.sample("abcdefghijklmnopqrstuvwxyz", 8))

    hDesktop = CreateDesktop(
        lpDesktop, None, None, 0, MAXIMUM_ALLOWED, ctypes.byref(saAttr)
    )
    if hDesktop is None:
        raise ctypes.WinError()

    try:

        cmdLine = list2cmdline(args)

        siStartInfo = STARTUPINFO()
        siStartInfo.cb = ctypes.sizeof(siStartInfo)
        siStartInfo.lpDesktop = lpDesktop
        siStartInfo.dwFlags = 0

        piProcInfo = PROCESS_INFORMATION()

        if not CreateProcess(
            None,
            cmdLine,
            None,
            None,
            True,
            0,  # dwCreationFlags
            None,
            None,
            ctypes.byref(siStartInfo),
            ctypes.byref(piProcInfo),
        ):
            raise ctypes.WinError()

        try:
            if WaitForSingleObject(piProcInfo.hProcess, INFINITE) == WAIT_FAILED:
                raise ctypes.WinError()

            dwExitCode = DWORD(STILL_ACTIVE)
            GetExitCodeProcess(piProcInfo.hProcess, ctypes.byref(dwExitCode))

        finally:
            CloseHandle(piProcInfo.hProcess)
            CloseHandle(piProcInfo.hThread)

    finally:
        CloseDesktop(hDesktop)

    exit(dwExitCode.value)


if __name__ == "__main__":
    spawndesk(sys.argv[1:])
