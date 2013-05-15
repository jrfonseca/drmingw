/*
 * Copyright 2009 Jose Fonseca
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BFDHELP_H_
#define _BFDHELP_H_

#include <windows.h>
#include <dbghelp.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI BfdSymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);

BOOL WINAPI BfdSymCleanup(HANDLE hProcess);

DWORD WINAPI BfdSymSetOptions(DWORD SymOptions);

BOOL WINAPI BfdSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);

BOOL WINAPI BfdSymGetLineFromAddr64(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);

DWORD WINAPI BfdUnDecorateSymbolName(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags);

#ifdef __cplusplus
}
#endif

#endif
