#!/usr/bin/env python3
#
# Copyright (c) 2023 Jose Fonseca
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#


import argparse
import json
import os
import glob
import re
import subprocess
import sys


dbghelp_exports = {

	"SymInitialize",
	"SymInitializeW",
	"SymCleanup",
	"SymSetOptions",
	"SymFromAddr",
	"SymFromAddrW",
	"SymGetLineFromAddr64",
	"SymGetLineFromAddrW64",
    "SymLoadModuleEx",
    "SymLoadModuleExW",
    "UnDecorateSymbolName",

	"EnumDirTree",
	"EnumDirTreeW",
	"EnumerateLoadedModules",
	"EnumerateLoadedModules64",
	"EnumerateLoadedModulesW64",
	"ExtensionApiVersion",
	"FindDebugInfoFile",
	"FindDebugInfoFileEx",
	"FindExecutableImage",
	"FindExecutableImageEx",
	"FindExecutableImageExW",
	"GetTimestampForLoadedLibrary",
	"ImageDirectoryEntryToData",
	"ImageDirectoryEntryToDataEx",
	"ImageNtHeader",
	"ImageRvaToSection",
	"ImageRvaToVa",
	"ImagehlpApiVersion",
	"ImagehlpApiVersionEx",
	"MakeSureDirectoryPathExists",
	"MiniDumpReadDumpStream",
	"MiniDumpWriteDump",
	"SearchTreeForFile",
	"SearchTreeForFileW",
	"StackWalk",
	"StackWalk64",
	"SymAddSymbol",
	"SymAddSymbolW",
	"SymEnumLines",
	"SymEnumSourceFiles",
	"SymEnumSymbols",
	"SymEnumSymbolsW",
	"SymEnumTypes",
	"SymEnumTypesW",
	"SymEnumerateModules",
	"SymEnumerateModules64",
	"SymEnumerateModulesW64",
	"SymEnumerateSymbols",
	"SymEnumerateSymbols64",
	"SymFindFileInPath",
	"SymFindFileInPathW",
	"SymFromName",
	"SymFunctionTableAccess",
	"SymFunctionTableAccess64",
	"SymGetLineFromAddr",
	"SymGetLineNext",
	"SymGetLineNext64",
	"SymGetLinePrev",
	"SymGetLinePrev64",
	"SymGetModuleBase",
	"SymGetModuleBase64",
	"SymGetModuleInfo",
	"SymGetModuleInfo64",
	"SymGetModuleInfoW",
	"SymGetModuleInfoW64",
	"SymGetOptions",
	"SymGetSearchPath",
	"SymGetSearchPathW",
	"SymGetSourceFileToken",
	"SymGetSourceFileTokenW",
	"SymGetSymFromAddr",
	"SymGetSymFromAddr64",
	"SymGetSymFromName",
	"SymGetSymFromName64",
	"SymGetSymNext",
	"SymGetSymNext64",
	"SymGetSymPrev",
	"SymGetSymPrev64",
	"SymGetTypeFromName",
	"SymGetTypeInfo",
	"SymLoadModule",
	"SymLoadModule64",
	"SymMatchFileName",
	"SymMatchFileNameW",
	"SymMatchString",
	"SymRefreshModuleList",
	"SymRegisterCallback",
	"SymRegisterCallback64",
	"SymRegisterCallbackW64",
	"SymRegisterFunctionEntryCallback",
	"SymRegisterFunctionEntryCallback64",
	"SymSearch",
	"SymSearchW",
	"SymSetContext",
	"SymSetParentWindow",
	"SymSetScopeFromAddr",
	"SymSetSearchPath",
	"SymSetSearchPathW",
	"SymUnDName",
	"SymUnDName64",
	"SymUnloadModule",
	"SymUnloadModule64",
	"WinDbgExtensionDllInit",

    # TODO
    #"MapDebugInformation",
    #"UnmapDebugInformation",
}


class ObjParser:

    def __init__(self, objdump, filename):
        self.filename = filename
        self.p = subprocess.Popen([objdump, '-p', filename], stdout=subprocess.PIPE, text=True)
        self.lookahead = self._readline()
        self.eof = False
        self.imports = {}
        self.exports = []
        self.valid = True

    def _readline(self):
        line = self.p.stdout.readline()
        if not line:
            self.eof = True
        return line.rstrip('\r\n')

    def consume(self, value=None):
        if value is not None:
            assert self.lookahead == value
        line = self.lookahead
        self.lookahead = self._readline()
        return line

    def parse(self):
        while not self.eof:
            line = self.consume()
            if line.startswith('The Import Tables'):
                self.parse_imports()
            if line.startswith('Export Address Table'):
                self.parse_exports_binutils()
            if line.startswith('Export Table'):
                self.parse_exports_llvm()
        self.p.wait()
        return self.p.returncode == 0

    dll_name_re = re.compile(r'\s+DLL Name: (?P<name>.*)$')
    import_symbol_re = re.compile(r'\s+(?P<vma>[0-9a-f]+\s+)?(?P<ord>[0-9]+)\s+(?P<name>\S+)$')

    def parse_imports(self):
        while not self.eof:
            line = self.lookahead
            if line and not line[0].isspace():
                break
            line = self.consume(line)

            mo = self.dll_name_re.match(line)
            if mo:
                dll_name = mo.group('name')
                self.parse_imports_dll(dll_name)

    def parse_imports_dll(self, dll_name):
        symbols = []
        while self.lookahead:
            line = self.consume()
            mo = self.import_symbol_re.match(line)
            if mo:
                symbols.append(mo.group('name'))
        self.imports[dll_name.lower()] = symbols

    export_symbol_llvm_re = re.compile(r'\s+(?P<ord>[0-9]+)\s+(?P<rva>0x[0-9a-f]+)\s+(?P<name>\S+)$')
    export_symbol_llvm_fw_re = re.compile(r'\s+(?P<ord>[0-9]+)\s+(?P<name>\S+) \(forwarded to (?P<alias>\S+)\)$')

    def parse_exports_llvm(self):
        symbols = []
        while self.lookahead:
            line = self.consume()
            mo = self.export_symbol_llvm_re.match(line)
            if mo:
                symbol = mo.group('name')
                symbols.append(symbol)
                continue
            mo = self.export_symbol_llvm_fw_re.match(line)
            if mo:
                symbol = mo.group('name') + '=' + mo.group('alias')
                symbols.append(symbol)
                continue
        self.exports = symbols

    export_address_binutils_re = re.compile(r'^\s+\[\s*(?P<idx>[0-9]+)\] \+base\[\s*(?P<ord>[0-9]+)\]\s+[0-9a-f]+\s+(?:Forwarder RVA -- (?P<alias>\S+)|Export RVA)$')

    export_name_binutils_re = re.compile(r'^\s+\[\s*(?P<idx>[0-9]+)\] (?P<name>\S+)$')
    def parse_exports_binutils(self):

        addresses = {}
        while self.lookahead:
            line = self.consume()
            mo = self.export_address_binutils_re.match(line)
            if mo:
                alias = mo.group('alias')
                if alias is not None:
                    addresses[mo.group('idx')] = alias

        self.consume('')

        self.consume('[Ordinal/Name Pointer] Table')
        symbols = []
        while self.lookahead:
            line = self.consume()
            mo = self.export_name_binutils_re.match(line)
            assert mo

            name = mo.group('name')
            try:
                alias = addresses[mo.group('idx')]
            except KeyError:
                pass
            else:
                name += '=' + alias

            symbols.append(name)

        self.exports = symbols

    def validate(self):
        name = os.path.basename(self.filename).lower()
        if not self.imports:
            self.error('no imports')
        if name.endswith('.dll') and not self.exports:
            self.error('no exports')
        if name == 'mgwhelp.dll':
            self.validate_mgwhelp_exports()
        else:
            self.validate_mgwhelp_imports()
        if 'libwinpthread-1.dll' in self.imports:
            self.error('imports libwinpthread-1.dll')
        sys.stderr.write(f'info: {self.filename}: {"OK" if self.valid else "NOT OK"}\n')

    def validate_mgwhelp_exports(self):
        for symbol in self.exports:
            if '@' in symbol:
                self.error(f'exports mangled {symbol}')
        exports = set([symbol.split('=')[0] for symbol in self.exports])
        for symbol in dbghelp_exports - exports:
            self.error(f'export {symbol} missing')
        for symbol in exports - dbghelp_exports:
            if symbol.startswith('dwarf_'):
                continue
            if symbol in ("MapDebugInformation", "UnmapDebugInformation"):
                # XXX Eliminate this discrepancy between exports
                continue
            self.error(f'spurious export {symbol}')

    def validate_mgwhelp_imports(self):
        for symbol in self.imports.get('mgwhelp.dll', []):
            if '@' in symbol:
                self.error(f'imports mangled {symbol}')
        if 'dbghelp.dll' in self.imports:
            self.error('imports dbghelp.dll instead of mgwhelp.dll')

    def error(self, msg):
        sys.stderr.write(f'error: {self.filename}: {msg}\n')
        self.valid = False

    def warning(self, msg):
        sys.stderr.write(f'warning: {self.filename}: {msg}\n')


def main():
    argparser = argparse.ArgumentParser()
    argparser.add_argument('--objdump', default='llvm-objdump')
    argparser.add_argument('--validate', action='store_true')
    argparser.add_argument('filename',  nargs='+')
    args = argparser.parse_args()

    filenames = args.filename

    if sys.platform == 'win32':
        filenames = []
        for pattern in args.filename:
            filenames += glob.glob(pattern)
    else:
        filenames = args.filename

    status = 0
    for filename in filenames:
        objparser = ObjParser(args.objdump, filename)
        if objparser.parse():
            if args.validate:
                objparser.validate()
                if not objparser.valid:
                    status = 1
            else:
                # Dump as JSON
                state = {
                    'imports': objparser.imports,
                    'exports': objparser.exports,
                }
                json.dump(state, sys.stdout, sort_keys=False, indent=2)
                sys.stdout.write('\n')
        else:
            status = 1

    sys.exit(status)


if __name__ =='__main__':
    main()
