/******************************************************************************\
 * Copyright (c) 2004
 *
 * Author(s):
 *	Francesco Lanza
 *
 * Description:
 *	
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later 
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined DIALOG_H
#define DIALOG_H

#include <windows.h>

/* Implementation of global functions *****************************************/
void PostWinMessage(unsigned int MessID, int iMessageParam);

// procedures called by Windows
// Main dialog handler

BOOL CALLBACK DialogProc
	(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK AboutDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK CallSignDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK DRMSettingsDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK DRMRXSettingsDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK TextMessageDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK RXTextMessageDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK TXPictureDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK SendBSRDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);
BOOL CALLBACK AnswerBSRDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam);

void CALLBACK TimerProc
   (HWND hwnd, UINT nMsg, UINT nIDEvent, DWORD dwTime);

void OnCommand (HWND hwnd, int id, int code);

#endif