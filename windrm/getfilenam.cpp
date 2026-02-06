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

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>     
#include <stdlib.h>           
#include <Shlwapi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "resource.h"
#include "common/libs/callsign.h"
#include "getfilenam.h"
#include "common/DrmTransmitter.h"
#include "common/settings.h"


/*--------------------------------------------------------------------
        GET FILE NAME
        Invoke the File Open or File Save As common dialog box.
        If the bOpenName parameter is TRUE, the procedure runs
        the OpenFileName dialog box.

        RETURN
        TRUE if the dialog box closes without error.  If the dialog
        box returns TRUE, then lpszFile and lpszFileTitle point to
        the new file path and name, respectively.
  --------------------------------------------------------------------*/
static char szOFNDefExt[]     = "*.*";  // for file name dialog

BOOL GetFileName (HWND hDlg, LPSTR lpszFile, LPSTR lpszFileTitle, int iMaxFileNmLen, long * fsize)
{
    OPENFILENAME ofn;
	BOOL res;
	int reslen;
	struct _stat buf;
   
    /* initialize structure for the common dialog box */
    lpszFile[0] = '\0';
    ofn.lStructSize       = sizeof( OPENFILENAME );
    ofn.hwndOwner         = hDlg;
    ofn.hInstance         = NULL;
    ofn.lpstrFilter       = "*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter    = 0;
    ofn.nFilterIndex      = 0;
    ofn.lpstrFile         = lpszFile;
    ofn.nMaxFile          = iMaxFileNmLen;
    ofn.lpstrFileTitle    = lpszFileTitle;
    ofn.nMaxFileTitle     = 80;
    ofn.lpstrInitialDir   = NULL;
    ofn.lpstrTitle        = NULL;
    ofn.nFileOffset       = 0;
    ofn.nFileExtension    = 1;
    ofn.lpstrDefExt       = szOFNDefExt;
    ofn.lCustData         = 0;
    ofn.lpfnHook          = NULL;
    ofn.lpTemplateName    = NULL;

    /* invoke the common dialog box */
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST |
                OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	res = GetOpenFileName(&ofn);
	reslen = _stat( lpszFile, &buf );
	*fsize = buf.st_size;
    return res;
}


BOOL checkfilenam(LPSTR filename, int filnamsize)
{
	return ((stricmp(&filename[filnamsize-3],"exe") != 0) &&
			(stricmp(&filename[filnamsize-3],"bat") != 0) &&
			(stricmp(&filename[filnamsize-3],"bas") != 0) &&
			(stricmp(&filename[filnamsize-3],"chm") != 0) &&
			(stricmp(&filename[filnamsize-3],"cmd") != 0) &&
			(stricmp(&filename[filnamsize-3],"com") != 0) &&
			(stricmp(&filename[filnamsize-3],"cpl") != 0) &&
			(stricmp(&filename[filnamsize-3],"crt") != 0) &&
			(stricmp(&filename[filnamsize-3],"hlp") != 0) &&
			(stricmp(&filename[filnamsize-3],"hta") != 0) &&
			(stricmp(&filename[filnamsize-3],"inf") != 0) &&
			(stricmp(&filename[filnamsize-3],"ins") != 0) &&
			(stricmp(&filename[filnamsize-3],"isp") != 0) &&
			(stricmp(&filename[filnamsize-3],"jse") != 0) &&
			(stricmp(&filename[filnamsize-2],"js" ) != 0) &&
			(stricmp(&filename[filnamsize-3],"lnk") != 0) &&
			(stricmp(&filename[filnamsize-3],"msc") != 0) &&
			(stricmp(&filename[filnamsize-3],"msi") != 0) &&
			(stricmp(&filename[filnamsize-3],"msp") != 0) &&
			(stricmp(&filename[filnamsize-3],"mst") != 0) &&
			(stricmp(&filename[filnamsize-3],"pcd") != 0) &&
			(stricmp(&filename[filnamsize-3],"pif") != 0) &&
			(stricmp(&filename[filnamsize-3],"reg") != 0) &&
			(stricmp(&filename[filnamsize-3],"scr") != 0) &&
			(stricmp(&filename[filnamsize-3],"sct") != 0) &&
			(stricmp(&filename[filnamsize-3],"shs") != 0) &&
			(stricmp(&filename[filnamsize-2],"vb" ) != 0) &&
			(stricmp(&filename[filnamsize-3],"vbe") != 0) &&
			(stricmp(&filename[filnamsize-3],"vbs") != 0) &&
			(stricmp(&filename[filnamsize-3],"wsc") != 0) &&
			(stricmp(&filename[filnamsize-3],"wsf") != 0) &&
			(stricmp(&filename[filnamsize-3],"wsh") != 0) &&
			(stricmp(&filename[filnamsize-3],"url") != 0) &&
			(stricmp(&filename[filnamsize-3],"mdb") != 0) &&
			(stricmp(&filename[filnamsize-3],"adp") != 0) &&
			(stricmp(&filename[filnamsize-3],"ade") != 0) &&
			(stricmp(&filename[filnamsize-3],"mde") != 0) &&
			(stricmp(&filename[filnamsize-3],"scr") != 0));
}

void SetTXmode(BOOL ispic)
{
	if (ispic)
	{
		int k,i;
		CVector<short>  dummy;
		dummy.Init(0);
		DRMTransmitter.GetParameters()->iNumAudioService = 0;
		DRMTransmitter.GetParameters()->iNumDataService = 1;
		DRMTransmitter.GetParameters()->Service[0].eAudDataFlag = CParameter::SF_DATA;
		DRMTransmitter.GetParameters()->Service[0].DataParam.iStreamID = 0;
		DRMTransmitter.GetParameters()->Service[0].DataParam.eDataUnitInd = CParameter::DU_DATA_UNITS;
		DRMTransmitter.GetParameters()->Service[0].DataParam.eAppDomain = CParameter::AD_DAB_SPEC_APP;
		DRMTransmitter.GetParameters()->Service[0].iServiceDescr = 0;
		DRMTransmitter.GetAudSrcEnc()->ClearPicFileNames();
		DRMTransmitter.Init();
		DRMTransmitter.GetParameters()->Service[0].DataParam.iPacketLen = 
			calcpacklen(DRMTransmitter.GetParameters()->iNumDecodedBitsMSC); 
		for (k=0;k<TXpicpospt;k++)
		{
			for (i=0;i<LeadIn;i++)
				DRMTransmitter.GetAudSrcEnc()->SetPicFileName(pictfile[k],filetitle[k],dummy);
		}
	}
	else
	{
		DRMTransmitter.GetParameters()->iNumAudioService = 1;
		DRMTransmitter.GetParameters()->iNumDataService = 0;
		DRMTransmitter.GetParameters()->Service[0].eAudDataFlag = CParameter::SF_AUDIO;
		DRMTransmitter.GetParameters()->Service[0].DataParam.iStreamID = 0;
		DRMTransmitter.GetParameters()->Service[0].iServiceDescr = 0;
	}
}

void putfiles(HWND hwnd)
{
	char acttxt[300];
	int i;
	int strpt = 0;
	if (TXpicpospt == 0) { SetDlgItemText( hwnd, IDC_PICFILE_TX, "  "); return; }
	for (i=0;i<TXpicpospt;i++) 
	{
		wsprintf(&acttxt[strpt],"%s   %d",filetitle[i],file_size[i]);
		strpt = strlen(acttxt);
		acttxt[strpt++] = 13;
		acttxt[strpt++] = 10;
	}
	acttxt[strpt++] = 0x00;
	SetDlgItemText( hwnd, IDC_PICFILE_TX, acttxt);
}

void unchecksoundrx(HWND hDlg, int num)
{
	int i;
	for (i=0;i<3;i++)
	if (num == i) CheckMenuItem(GetMenu(hDlg), IDM_O_RX_I_DRIVERS0+i, MF_BYCOMMAND | MF_CHECKED) ;
	else CheckMenuItem(GetMenu(hDlg), IDM_O_RX_I_DRIVERS0+i, MF_BYCOMMAND | MF_UNCHECKED) ;
	DRMReceiver.GetSoundInterface()->SetInDev(getsoundin('r'));
}
void unchecksoundtx(HWND hDlg, int num)
{
	int i;
	for (i=0;i<3;i++)
	if (num == i) CheckMenuItem(GetMenu(hDlg), IDM_O_TX_O_DRIVERS0+i, MF_BYCOMMAND | MF_CHECKED) ;
	else CheckMenuItem(GetMenu(hDlg), IDM_O_TX_O_DRIVERS0+i, MF_BYCOMMAND | MF_UNCHECKED) ;
	DRMTransmitter.GetSoundInterface()->SetOutDev(getsoundout('t'));
}
void unchecksoundvoI(HWND hDlg, int num)
{
	int i;
	for (i=0;i<3;i++)
	if (num == i) CheckMenuItem(GetMenu(hDlg), IDM_O_VO_I_DRIVERS0+i, MF_BYCOMMAND | MF_CHECKED) ;
	else CheckMenuItem(GetMenu(hDlg), IDM_O_VO_I_DRIVERS0+i, MF_BYCOMMAND | MF_UNCHECKED) ;
	DRMTransmitter.GetSoundInterface()->SetInDev(getsoundin('t'));
}
void unchecksoundvoO(HWND hDlg, int num)
{
	int i;
	for (i=0;i<3;i++)
	if (num == i) CheckMenuItem(GetMenu(hDlg), IDM_O_VO_O_DRIVERS0+i, MF_BYCOMMAND | MF_CHECKED) ;
	else CheckMenuItem(GetMenu(hDlg), IDM_O_VO_O_DRIVERS0+i, MF_BYCOMMAND | MF_UNCHECKED) ;
	DRMReceiver.GetSoundInterface()->SetOutDev(getsoundout('r'));
}

void uncheckdisp(HWND hwnd)
{
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_WATERFALL, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_LEVEL, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_SPECTRUM, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_PSD, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TRANSFERFUNCTION, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_IMPULSERESPONSE, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_FACPHASE, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_MSCPHASE, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TEST, MF_BYCOMMAND | MF_UNCHECKED) ;
}

void checkcomport(HWND hDlg, int num)
{
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_NONE, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM1, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM2, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM3, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM4, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM5, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM6, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM7, MF_BYCOMMAND | MF_UNCHECKED) ;
	CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM8, MF_BYCOMMAND | MF_UNCHECKED) ;
	if      (num == '1')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM1, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '2')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM2, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '3')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM3, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '4')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM4, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '5')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM5, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '6')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM6, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '7')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM7, MF_BYCOMMAND | MF_CHECKED) ;
	else if (num == '8')
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_COM8, MF_BYCOMMAND | MF_CHECKED) ;
	else 
		CheckMenuItem(GetMenu(hDlg), ID_PTTPORT_NONE, MF_BYCOMMAND | MF_CHECKED) ;
}


