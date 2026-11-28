/*
 * Copyright 2026 Jose Fonseca
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*
 * Print a minidump's exception stream, the program counter in the faulting
 * thread's context, and the comment stream, for test_catchsegv.py to check.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <dbghelp.h>


int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <minidump>\n", argv[0]);
        return EXIT_FAILURE;
    }

    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "error: failed to open %s (%lu)\n", argv[1], GetLastError());
        return EXIT_FAILURE;
    }

    DWORD dwFileSize = GetFileSize(hFile, NULL);

    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        fprintf(stderr, "error: failed to map %s (%lu)\n", argv[1], GetLastError());
        return EXIT_FAILURE;
    }

    const BYTE *pBase = static_cast<const BYTE *>(MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!pBase) {
        fprintf(stderr, "error: failed to map view of %s (%lu)\n", argv[1], GetLastError());
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;

    PMINIDUMP_DIRECTORY pDir = NULL;
    PVOID pStream = NULL;
    ULONG cbStream = 0;
    if (MiniDumpReadDumpStream((PVOID)pBase, ExceptionStream, &pDir, &pStream, &cbStream) &&
        cbStream >= sizeof(MINIDUMP_EXCEPTION_STREAM)) {
        auto pException = static_cast<const MINIDUMP_EXCEPTION_STREAM *>(pStream);
        const MINIDUMP_EXCEPTION &record = pException->ExceptionRecord;
        printf("ThreadId=%lu\n", (unsigned long)pException->ThreadId);
        printf("ExceptionCode=0x%08lx\n", (unsigned long)record.ExceptionCode);
        printf("ExceptionAddress=0x%llx\n", (unsigned long long)record.ExceptionAddress);

        const MINIDUMP_LOCATION_DESCRIPTOR &loc = pException->ThreadContext;
        if (loc.Rva && loc.DataSize >= sizeof(CONTEXT) &&
            (ULONG64)loc.Rva + loc.DataSize <= dwFileSize) {
            const CONTEXT *pContext = reinterpret_cast<const CONTEXT *>(pBase + loc.Rva);
#if defined(__x86_64__)
            unsigned long long pc = pContext->Rip;
#elif defined(__i386__)
            unsigned long long pc = pContext->Eip;
#elif defined(__aarch64__)
            unsigned long long pc = pContext->Pc;
#else
#error "unsupported architecture"
#endif
            printf("ContextPC=0x%llx\n", pc);
        } else {
            fprintf(stderr, "error: no thread context in exception stream\n");
            ret = EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "error: no exception stream\n");
        ret = EXIT_FAILURE;
    }

    if (MiniDumpReadDumpStream((PVOID)pBase, CommentStreamA, &pDir, &pStream, &cbStream)) {
        // The comment may span several lines, so only print the first.
        const char *pComment = static_cast<const char *>(pStream);
        size_t len = strnlen(pComment, cbStream);
        const char *pNewline = static_cast<const char *>(memchr(pComment, '\n', len));
        if (pNewline) {
            len = pNewline - pComment;
        }
        printf("Comment=%.*s\n", (int)len, pComment);
    } else {
        fprintf(stderr, "error: no comment stream\n");
        ret = EXIT_FAILURE;
    }

    fflush(stdout);

    UnmapViewOfFile(pBase);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    return ret;
}
