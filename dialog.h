/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i dialog.c' */
#ifndef CFH_DIALOG_H
#define CFH_DIALOG_H

/* From `dialog.c': */
BOOL CALLBACK AboutDlgProc (HWND hwnd , UINT Message , WPARAM wParam , LPARAM lParam );
void UpdateText (LPCTSTR lpszBuffer );
LRESULT CALLBACK WndProc (HWND hwnd , UINT Message , WPARAM wParam , LPARAM lParam );
int Dialog (void);

#endif /* CFH_DIALOG_H */
