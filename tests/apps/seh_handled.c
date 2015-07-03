/**************************************************************************
 *
 * Copyright 2015 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OF OR CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/

//
// See also:
// - https://www.microsoft.com/msj/0197/exception/exception.aspx
// - https://msdn.microsoft.com/en-us/library/1eyas8tf.aspx
// - https://msdn.microsoft.com/en-us/library/swezty51.aspx
// - http://www.programmingunlimited.net/siteexec/content.cgi?page=mingw-seh
// - https://gist.github.com/rossy/7faf0ab90a54d6b5a46f
//


#include <stdio.h>

#include <windows.h>


#define EXCEPTION_CODE 0xE0000001


#ifdef _MSC_VER


static LONG WINAPI
exceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
    DWORD ExceptionCode = pExceptionRecord->ExceptionCode;
    fprintf(stderr, "exception 0x%lx\n", ExceptionCode);
    fflush(stderr);
    return ExceptionCode == EXCEPTION_CODE ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}


#else /* __MINGW32__ */


struct _EXCEPTION_REGISTRATION_RECORD
{
    struct _EXCEPTION_REGISTRATION_RECORD *Next;
    EXCEPTION_DISPOSITION (NTAPI *Handler)(struct _EXCEPTION_RECORD *, PVOID, struct _CONTEXT *, PVOID);
};


#ifdef _WIN64


static EXCEPTION_DISPOSITION NTAPI
__attribute__((used))
exceptionHandler(struct _EXCEPTION_RECORD *ExceptionRecord,
                 PVOID EstablisherFrame,
                 struct _CONTEXT *ContextRecord,
                 PVOID DispatcherContext)
{
    DWORD ExceptionCode = ExceptionRecord->ExceptionCode;
    fprintf(stderr, "exception 0x%lx\n", ExceptionCode);
    fflush(stderr);
    return ExceptionCode == EXCEPTION_CODE ? ExceptionExecuteHandler : ExceptionContinueSearch;
}


#else


static EXCEPTION_DISPOSITION NTAPI
ignore_handler(struct _EXCEPTION_RECORD *ExceptionRecord,
               void *EstablisherFrame,
               struct _CONTEXT *ContextRecord,
               void *DispatcherContext)
{
    DWORD ExceptionCode = ExceptionRecord->ExceptionCode;
    fprintf(stderr, "exception 0x%lx\n", ExceptionCode);
    fflush(stderr);
    return ExceptionCode == EXCEPTION_CODE ? ExceptionContinueExecution : ExceptionContinueSearch;
}

#endif


#endif /* __MINGW32__ */


int
main()
{
    setvbuf(stderr, NULL, _IONBF, 0);

    fprintf(stderr, "before\n");
    fflush(stderr);

#ifdef _MSC_VER

    __try {
        RaiseException(EXCEPTION_CODE, 0, 0, NULL);
        fprintf(stderr, "unreachable\n");
        fflush(stderr);
        _exit(1);
    }
    __except (exceptionFilter(GetExceptionInformation())) {
    }

#elif defined(_WIN64)

    __try1(exceptionHandler);

    RaiseException(EXCEPTION_CODE, 0, 0, NULL);
    fprintf(stderr, "unreachable\n");
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 1);

    __except1;

#else

    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    struct _EXCEPTION_REGISTRATION_RECORD Record;
    Record.Next = tib->ExceptionList;
    Record.Handler = ignore_handler;
    tib->ExceptionList = &Record;

    RaiseException(EXCEPTION_CODE, 0, 0, NULL);

    tib->ExceptionList = tib->ExceptionList->Next;

#endif

    fprintf(stderr, "after\n");
    fflush(stderr);

    return 0;
}


// CHECK_STDERR: /^before$/
// CHECK_STDERR: /^exception 0xe0000001$/
// CHECK_STDERR: /^after$/
// CHECK_EXIT_CODE: 0
