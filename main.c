/* main.c
 *
 *
 * José Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>

#include <process.h>
#include <stdlib.h>

#include "getopt.h"
#include "debugger.h"
#include "dialog.h"
#include "log.h"
#include "misc.h"

#undef PACKAGE
#define PACKAGE "DrMingw"
#undef VERSION
#define VERSION "0.4.3"

int main (int argc, char **argv)
{
	int c;	/* Character of the parsed option.  */

	int process_id_given = 0;	/* Whether process-id was given.  */
	int install_given = 0;	/* Whether install was given.  */
	int auto_given = 0;	/* Whether auto was given.  */
	int uninstall_given = 0;	/* Whether uninstall was given.  */

	while (1)
	{
		int option_index = 0;
		static struct option long_options[] =
		{
			{ "help", 0, NULL, 'h'},
			{ "version", 0, NULL, 'V'},
			{ "install", 0, NULL, 'i'},
			{ "auto", 0, NULL, 'a'},
			{ "install", 0, NULL, 'i'},
			{ "uninstall", 0, NULL, 'u'},
			{ "process-id", 1, NULL, 'p'},
			{ "event", 1, NULL, 'e'},
			{ "verbose", 0, NULL, 'v'},
			{ NULL, 0, NULL, 0}
		};

		c = getopt_long (argc, argv, "hViaup:e:v", long_options, &option_index);

		if (c == -1)
			break;	/* Exit from `while (1)' loop.  */

		switch (c) {
			case 'h':	/* Print help and exit.  */
				MessageBox(
					NULL, 
					_T(
						"Usage: drmingw [-h|--help] [-V|--version] [-i|--install] [-a|--auto] [-u|--uninstall]\r\n"
						"[-pLONG|--process-id=LONG] [-eLONG|--event=LONG]\r\n"
						"[-v|--verbose]\r\n"
						"\r\n"
						"\t-h\t--help\t\tPrint help and exit\r\n"
						"\t-V\t--version\t\tPrint version and exit\r\n"
						"\t-i\t--install\t\tInstall as the default JIT debugger\r\n"
						"\t-a\t--auto\t\tAutomatically start. Used with --install' (`-i')\r\n"
						"\t-u\t--uninstall\t\tUninstall\r\n"
						"\t-pLONG\t--process-id=LONG\tAttach to the process with the given identifier\r\n"
						"\t-eLONG\t--event=LONG\tSignal an event after process is attached\r\n"
						"\t-v\t--verbose\t\tVerbose output\r\n"
					),
					_T(PACKAGE),
					MB_OK | MB_ICONINFORMATION
				);
				return 0;

			case 'V':	/* Print version and exit.  */
				MessageBox(
					NULL, 
					_T(PACKAGE " " VERSION),
					_T(PACKAGE),
					MB_OK | MB_ICONINFORMATION
				);
				return 0;

			case 'i':	/* Install as the default JIT debugger.  */
				if (install_given)
				{
					MessageBox(
						NULL,
						_T("`--install' (`-i') option redeclared"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				if (uninstall_given)
				{
					MessageBox(
						NULL,
						_T("conficting options `--uninstall' (`-u') and `--install' (`-i')"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				install_given = 1;
				break;

			case 'a':	/* Automatically start.  */
				if (auto_given)
				{
					MessageBox(
						NULL,
						_T("`--auto' (`-a') option redeclared"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				if (uninstall_given)
				{
					MessageBox(
						NULL,
						_T("conficting options `--uninstall' (`-u') and `--auto' (`-a')"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				auto_given = 1;
				break;

			case 'u':	/* Uninstall.  */
				if (uninstall_given)
				{
					MessageBox(
						NULL,
						_T("`--uninstall' (`-u') option redeclared"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				if (install_given)
				{
					MessageBox(
						NULL,
						_T("conficting options `--install' (`-i') and `--uninstall' (`-u')"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				if (auto_given)
				{
					MessageBox(
						NULL,
						_T("conficting options `--auto' (`-a') and `--uninstall' (`-u')"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				uninstall_given = 1;
				break;

			case 'p':	/* Attach to the process with the given identifier.  */
				if (process_id_given)
				{
					MessageBox(
						NULL,
						_T("`--process-id' (`-p') option redeclared"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				process_id_given = 1;
				dwProcessId = atol (optarg);
				break;

			case 'e':	/* Signal an event after process is attached.  */
				if (hEvent)
				{
					MessageBox(
						NULL,
						_T("`--event' (`-e') option redeclared"),
						_T(PACKAGE),
						MB_OK | MB_ICONSTOP
					);
					return 0;
				}
				hEvent = (HANDLE) atol (optarg);
				break;

			case 'v':	/* Verbose output.  */
				verbose_flag = 1;
				break;

			case '?':	/* Invalid option.  */
				MessageBox(
					NULL,
					_T("Invalid option"),
					_T(PACKAGE),
					MB_OK | MB_ICONSTOP
				);
				return 0;

			default:	/* bug: option not considered.  */
			{
				TCHAR szErrMsg[1024];
				
				wsprintf(szErrMsg, "'-%c' option unknown", c);

				MessageBox(
					NULL,
					szErrMsg,
					_T(PACKAGE),
					MB_OK | MB_ICONSTOP
				);
				return 0;
			}
		}
	}

	if(install_given)
	{
		TCHAR szFile[MAX_PATH];
		
		if(!GetModuleFileName(NULL, szFile, MAX_PATH))
			ErrorMessageBox(_T("GetModuleFileName: %s"), LastErrorMessage());
		else
		{
			TCHAR szFullCommand[MAX_PATH + 256];
			HKEY hKey;
			long lRet;
			DWORD dwDisposition;
			
			lstrcpy(szFullCommand, szFile);
			lstrcat(szFullCommand, _T (" -p %ld -e %ld"));			
			if(verbose_flag)
				lstrcat(szFullCommand, _T (" -v"));
		
			lRet = RegCreateKeyEx(
				HKEY_LOCAL_MACHINE,
				_T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),	// The AeDebug registry key.
				0,
				NULL,
				REG_OPTION_NON_VOLATILE,
				KEY_WRITE,
				NULL,
				&hKey,
				&dwDisposition
			);
			if(lRet == ERROR_ACCESS_DENIED)
				MessageBox(
					NULL, 
					_T("You must have administrator privileges to install Dr. Mingw as the default application debugger"),
					_T("DrMingw"),
					MB_OK | MB_ICONERROR
				);
			else if(lRet != ERROR_SUCCESS)
				ErrorMessageBox(_T("RegCreateKeyEx: %s"), FormatErrorMessage(lRet));
			else
			{
				// Write the Debugger value.
				lRet = RegSetValueEx(
					hKey,
					_T("Debugger"),	// The debugger value.
					0,
					REG_SZ,
					(CONST BYTE *) szFullCommand,
					lstrlen(szFullCommand)*sizeof(TCHAR)
				);
				
				if(lRet != ERROR_SUCCESS)
					ErrorMessageBox(_T("RegSetValueEx: %s"), FormatErrorMessage(lRet));
				else
				{
					// Write the Auto value.
					lRet = RegSetValueEx(
						hKey,
						_T("Auto"),	// The auto value.
						0,
						REG_SZ,
						(CONST BYTE *)  (auto_given ? _T("1") : _T("0")),
						sizeof(TCHAR)
					);
					
					if(lRet != ERROR_SUCCESS)
						ErrorMessageBox(_T("RegSetValueEx: %s"), FormatErrorMessage(lRet));
					else
						MessageBox(
							NULL, 
							_T("Dr. Mingw has been installed as the default application debugger"),
							_T("DrMingw"),
							MB_OK | MB_ICONINFORMATION
						);
				}

				// Close the key no matter what happened.
				RegCloseKey(hKey);
			}
		}
	}

	if(uninstall_given)
	{
		HKEY hKey;
		long lRet;
		
		lRet = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			_T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),	// The AeDebug registry key.
			0,
			KEY_ALL_ACCESS,
			&hKey
		);
		
		if(lRet == ERROR_ACCESS_DENIED)
			MessageBox(
				NULL, 
				_T("You must have administrator privileges to uninstall Dr. Mingw as the default application debugger"),
				_T("DrMingw"),
				MB_OK | MB_ICONERROR
			);
		else if(lRet != ERROR_SUCCESS)
			ErrorMessageBox(_T("RegOpenKeyEx: %s"), FormatErrorMessage(lRet));
		else
		{
			// Write the Debugger value.
			lRet = RegSetValueEx(
				hKey,
				_T("Debugger"),	// The debugger value.
				0,
				REG_SZ,
				(CONST BYTE *) _T(""),
				0
			);
			
			if(lRet != ERROR_SUCCESS)
				ErrorMessageBox(_T("RegSetValueEx: %s"), FormatErrorMessage(lRet));
			else
			{
				// Write the Auto value.
				lRet = RegSetValueEx(
					hKey,
					_T("Auto"),	// The auto value.
					0,
					REG_SZ,
					(CONST BYTE *)  _T("0"),
					sizeof(TCHAR)
				);
				
				if(lRet != ERROR_SUCCESS)
					ErrorMessageBox(_T("RegSetValueEx: %s"), FormatErrorMessage(lRet));
				else
					MessageBox(
						NULL, 
						_T("Dr. Mingw has been uninstalled"),
						_T("DrMingw"),
						MB_OK | MB_ICONINFORMATION
					);
			}

			// Close the key no matter what happened.
			RegCloseKey(hKey);
		}
	}

	if(process_id_given)
	{
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

		if(!ObtainSeDebugPrivilege())
			MessageBox(
				NULL, 
				_T("An error occured while obtaining debug privileges.\nDrMingw will not debug system processes."),
				_T("DrMingw"),
				MB_OK | MB_ICONERROR
			);

		_beginthread(DebugProcess, 0, NULL);

		Dialog();
	}

	return 0;
}

