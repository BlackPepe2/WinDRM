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

#include <process.h>
#include <windows.h>
#include <commctrl.h>           // prototypes and defs for common controls 
#include <direct.h>
#include "dialog.h"
#include "main.h"
#include "resource.h"
#include "common/DrmReceiver.h"
#include "common/DrmTransmitter.h"
#include "common/libs/ptt.h"
#include "common/settings.h"
#include "common/list.h"
#include "common/bsr.h"
#include "common/libs/callsign.h"
#include "common/libs/mixer.h"
#include "common/libs/graphwin.h"
#include "getfilenam.h"
#include "common/AudioFir.h"

CDRMReceiver	DRMReceiver;
CDRMTransmitter	DRMTransmitter;

CDataDecoder* DataDecoder;
CParameter*	TransmParam;
CAudioSourceEncoder* AudioSourceEncoder;

// Main dialog handler

#define starttx_time 12
#define starttx_time_long 24
#define stoptx_time 10

BOOL IsRX = TRUE;
BOOL RX_Running = FALSE;
BOOL TX_Running = FALSE;
BOOL UseTextMessage = FALSE;
BOOL RXTextMessageON = FALSE;
BOOL RXTextMessageWasClosed = FALSE;
BOOL AllowRXTextMessage = TRUE;
BOOL SaveRXFile = TRUE;
BOOL ShowRXFile = TRUE;
BOOL ShowOnlyFirst = TRUE;
BOOL rxaudfilt = FALSE;
BOOL fastreset = FALSE;
BOOL rtsonfac = FALSE;
BOOL dtronfac = FALSE;
BOOL dolog = FALSE;
int sensivity = 60;

char LeadIn = 1;
int TXpicpospt = 0;
FILE * logfile;
string rxcall     = "nocall";
string lastrxcall = "nocall";
string consrxcall = "nocall"; 

int numdev;
int disptype = 0; //0=spectr, 1=psd, 2=level
int newdata = 0;
int numbsrsegments = 0;
int acthash = 0;
int bsrposind;
int txbsrposind;
BOOL bCloseMe = FALSE;

HWND bsrhand;

string namebsrfile;
string filetosend;

float specbufarr[300];

char filetitle[8][80] = {" "};
char pictfile[8][300] = {" "};

char lastfilename[130] = { "none" };
char lastrxfilename[130] = { 0 };
int  lastrxfilesize = 0;
ERobMode lastrxrobmode = RM_ROBUSTNESS_MODE_B;
ESpecOcc  lastrxspecocc = SO_1;
CParameter::ECodScheme lastrxcodscheme = CParameter::CS_1_SM;
int lastrxprotlev = 0;

char robmode = ' ';
int specocc = 0;

/* Implementation of global functions *****************************************/

int messtate[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
HWND messhwnd;
HWND RXMessagehwnd;

void PostWinMessage(unsigned int MessID, int iMessageParam)
{
	HWND hdlg;
	if (MessID == MS_RESET_ALL)
	{
		int i;
		for (i=1;i<9;i++) messtate[i] = -1;
		hdlg = GetDlgItem (messhwnd, IDC_LED_IO);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		hdlg = GetDlgItem (messhwnd, IDC_LED_FREQ);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		hdlg = GetDlgItem (messhwnd, IDC_LED_FAC);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		hdlg = GetDlgItem (messhwnd, IDC_LED_MSC);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		hdlg = GetDlgItem (messhwnd, IDC_LED_FRAME);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		hdlg = GetDlgItem (messhwnd, IDC_LED_TIME);
		SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		if (rtsonfac) endrts();
		return;
	}
	if (messtate[MessID] != iMessageParam)
	{
		if (MessID == MS_IOINTERFACE)
			hdlg = GetDlgItem (messhwnd, IDC_LED_IO);
		else if (MessID == MS_FREQ_FOUND)
			hdlg = GetDlgItem (messhwnd, IDC_LED_FREQ);
		else if (MessID == MS_FAC_CRC)
		{
			hdlg = GetDlgItem (messhwnd, IDC_LED_FAC);
			if (rtsonfac) if (iMessageParam == 0) dorts();	else endrts();
			if (dtronfac) if (iMessageParam == 0) dodtr();	else enddtr();
		}
		else if (MessID == MS_MSC_CRC)
			hdlg = GetDlgItem (messhwnd, IDC_LED_MSC);
		else if (MessID == MS_FRAME_SYNC)
			hdlg = GetDlgItem (messhwnd, IDC_LED_FRAME);
		else if (MessID == MS_TIME_SYNC)
			hdlg = GetDlgItem (messhwnd, IDC_LED_TIME);
		else return;
		messtate[MessID] = iMessageParam;
		
		if (iMessageParam == 0)
			SendMessage (hdlg, BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (hdlg, BM_SETCHECK, (WPARAM)0, 0);
		
	}
}

/*--------------------------------------------------------------------
        PAINT WAVE DATA
        Repaint the dialog box's GraphClass display control
  --------------------------------------------------------------------*/

static void PaintWaveData ( HWND hDlg , BOOL erase )
{
    HWND hwndShowWave = GetDlgItem( hDlg, IDS_WAVE_PANE );

    InvalidateRect( hwndShowWave, NULL, erase );
    UpdateWindow( hwndShowWave );
    return;
}

/*--------------------------------------------------------------------
        Threads
  --------------------------------------------------------------------*/

void RxFunction(  void *dummy  )
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	try
	{
		DRMReceiver.Start();	
	}
	catch (CGenErr GenErr)
	{
		RX_Running = FALSE;
		MessageBox( messhwnd,"RX Audio Setup Wrong\nWinDRM needs 2 soundcards or WinXP\nTry -r, -t or -p startup options","RX Exception",0);	
	}
}

void TxFunction(  void *dummy  )
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	try
	{
		DRMTransmitter.Start();
	}
	catch (CGenErr GenErr)
	{
		TX_Running = FALSE;
		MessageBox( messhwnd,"TX Audio Setup Wrong\nWinDRM needs 2 soundcards or WinXP\n Try -r, -t or -p startup options","TX Exception",0);	
	}
}

int Playtype = 0;
BOOL isplaying = FALSE;

void PlaySound(  void *dummy  )
{
	isplaying = TRUE;
	try
	{
		if (Playtype == 1)
		{
			dotx();
			PlaySound("tune.wav",NULL,SND_FILENAME | SND_SYNC  | SND_NOSTOP | SND_NODEFAULT);
			endtx();
		}
		else if (Playtype == 2)
		{
			dotx();
			PlaySound("id.wav",NULL,SND_FILENAME | SND_SYNC | SND_NOSTOP | SND_NODEFAULT);
			endtx();
		}
		else if (Playtype == 3)
		{
			dotx();
			PlaySound("g.wav",NULL,SND_FILENAME | SND_SYNC | SND_NOSTOP | SND_NODEFAULT);
			endtx();
		}
		else if (Playtype == 4)
		{
			dotx();
			PlaySound("b.wav",NULL,SND_FILENAME | SND_SYNC | SND_NOSTOP | SND_NODEFAULT);
			endtx();
		}
	}
	catch (CGenErr GenErr)
	{
		MessageBox( messhwnd,"Works only with WinXP","Message",0);	
	}
	isplaying = FALSE;
}

int actdevinr;
int actdevint;
int actdevoutr;
int actdevoutt;

BOOL CALLBACK DialogProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU   hMenu;
	int i;
    switch (message)
    {
    case WM_INITDIALOG:
		{
			HICON hIcon = LoadIcon (TheInstance, MAKEINTRESOURCE (IDI_ICON1));
			SendMessage (hwnd, WM_SETICON, WPARAM (ICON_SMALL), LPARAM (hIcon));
		}
		messhwnd = hwnd;
        SendMessage (GetDlgItem (hwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"TX Voice");
		SendMessage (GetDlgItem (hwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"TX Pic");

		getvar();
		comtx(gettxport());
		checkcomport(hwnd,gettxport());
		InitBsr();

		numdev = DRMReceiver.GetSoundInterface()->GetNumDev();
		SetMixerValues(numdev);
		hMenu = GetSubMenu(GetMenu(hwnd), 1);	
		if (numdev >= 3) numdev = 3;
		for (i=0;i<numdev;i++)
		{	
			string drivnam = DRMReceiver.GetSoundInterface()->GetDeviceName(i);
			AppendMenu(hMenu, MF_ENABLED, IDM_O_RX_I_DRIVERS0+i, drivnam.c_str());
		}
		AppendMenu(hMenu, MF_SEPARATOR, -1, NULL);
		AppendMenu(hMenu, MF_ENABLED, -1, "TX Output");
		for (i=0;i<numdev;i++)
		{	
			string drivnam = DRMReceiver.GetSoundInterface()->GetDeviceName(i);
			AppendMenu(hMenu, MF_ENABLED, IDM_O_TX_O_DRIVERS0+i, drivnam.c_str());
		}
		if (runmode != 'P')
		{
			AppendMenu(hMenu, MF_SEPARATOR, -1, NULL);
			AppendMenu(hMenu, MF_ENABLED, -1, "Voice Input");
			for (i=0;i<numdev;i++)
			{	
				string drivnam = DRMReceiver.GetSoundInterface()->GetDeviceName(i);
				AppendMenu(hMenu, MF_ENABLED, IDM_O_VO_I_DRIVERS0+i, drivnam.c_str());
			}
			AppendMenu(hMenu, MF_SEPARATOR, -1, NULL);
			AppendMenu(hMenu, MF_ENABLED, -1, "Voice Output");
			for (i=0;i<numdev;i++)
			{	
				string drivnam = DRMReceiver.GetSoundInterface()->GetDeviceName(i);
				AppendMenu(hMenu, MF_ENABLED, IDM_O_VO_O_DRIVERS0+i, drivnam.c_str());
			}
		}
		else	// Picture only mode, dont start up voice sound !
		{
			AllowRXTextMessage = FALSE;
			CheckMenuItem(GetMenu(hwnd), ID_CODEC_LPC10, MF_BYCOMMAND | MF_UNCHECKED) ;
			CheckMenuItem(GetMenu(hwnd), ID_CODEC_SPEEX, MF_BYCOMMAND | MF_UNCHECKED) ;
			EnableMenuItem(GetMenu(hwnd), ID_CODEC_SPEEX, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_CODEC_LPC10, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_TEXTMESSAGE, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_TEXTMESSAGE_OPENRXTEXTMESSAGE, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_SOUNDCARD_OPENMIXER_VOICEINPUT, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_SOUNDCARD_OPENMIXER_VOICEOUTPUT, MF_GRAYED);
			EnableWindow(GetDlgItem (hwnd, IDB_START),FALSE);
		}
  

		DrawMenuBar(hwnd);
		actdevinr = getsoundin('r');
		if (actdevinr >= numdev) actdevinr = numdev - 1;
		unchecksoundrx(hwnd,actdevinr);
		actdevoutt = getsoundout('t');
		if (actdevoutt >= numdev) actdevoutt = numdev - 1;
		unchecksoundtx(hwnd,actdevoutt);
		actdevint = getsoundin('t');
		if (actdevint >= numdev) actdevint = numdev - 1;
		unchecksoundvoI(hwnd,actdevint);
		actdevoutr = getsoundout('r');
		if (actdevoutr >= numdev) actdevoutr = numdev - 1;
		unchecksoundvoO(hwnd,actdevoutr);

		if (runmode != 'T')
		{
			try
			{
				DRMReceiver.GetParameters()->bOnlyPicture = (runmode == 'P');
				DRMReceiver.Init();
				DRMReceiver.GetSoundInterface()->SetInDev(actdevinr);
				DRMReceiver.GetSoundInterface()->SetOutDev(actdevoutr);
				DataDecoder = DRMReceiver.GetDataDecoder();
				DRMReceiver.SetFreqAcqSensivity(2.0 - (_REAL)sensivity/100.0);
	
				RX_Running = TRUE;
				_beginthread(RxFunction,0,NULL);
			}
			catch(CGenErr)
			{
				RX_Running = FALSE;
				MessageBox( hwnd,"Receiver will NOT work\nWinDRM needs 2 soundcards or WinXP\nTry -r, -t or -p startup options","RX Init Exception",0);	
			}
		}


		if (runmode == 'R')
		{
			EnableMenuItem(GetMenu(hwnd), ID_CODEC_SPEEX, MF_GRAYED);
			EnableMenuItem(GetMenu(hwnd), ID_CODEC_LPC10, MF_GRAYED);
		}

		if (runmode != 'R')
		{
			try
			{
				DRMTransmitter.GetParameters()->bOnlyPicture = (runmode == 'P');
				DRMTransmitter.Init();
				DRMTransmitter.GetSoundInterface()->SetInDev(actdevint);
				DRMTransmitter.GetSoundInterface()->SetOutDev(actdevoutt);
				TransmParam = DRMTransmitter.GetParameters();
				TransmParam->Service[0].strLabel = getcall();
				TransmParam->Service[0].AudioParam.eAudioCoding = CParameter::AC_LPC;
				AudioSourceEncoder = DRMTransmitter.GetAudSrcEnc();
				AudioSourceEncoder->ClearTextMessage();
				AudioSourceEncoder->ClearPicFileNames();
				DRMTransmitter.SetCarOffset(350.0);
				DRMTransmitter.GetAudSrcEnc()->SetStartDelay(starttx_time_long);
				
				TX_Running = TRUE;
				_beginthread(TxFunction,0,NULL);
			}
			catch(CGenErr)
			{
				TX_Running = FALSE;
				MessageBox( hwnd,"Transmitter will NOT work\nWinDRM needs 2 soundcards or WinXP\nTry -r, -t or -p startup options","TX Init Exception",0);	
			}
		}

		if (runmode == 'P')
		{
			TransmParam->Service[0].AudioParam.eAudioCoding = CParameter::AC_SSTV;
		}

		UseTextMessage = gettext();
		if ((UseTextMessage) && (TX_Running))
		{
			FILE * txtset = NULL;
			char textbuf[200];
			txtset = fopen("textmessage.txt","rt");
			if (txtset != NULL)
			{
				fscanf(txtset,"%[^\0]",&textbuf);
				fclose(txtset);
				DRMTransmitter.GetAudSrcEnc()->ClearTextMessage();
				DRMTransmitter.GetAudSrcEnc()->SetTextMessage(textbuf);
				DRMTransmitter.GetParameters()->Service[0].AudioParam.bTextflag = TRUE;
			}
		}

		uncheckdisp(hwnd);
		if (disptype == 0)
			CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_SPECTRUM, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 1)
			CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_PSD, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 2)
			CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_LEVEL, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 3)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_WATERFALL, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 4)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TRANSFERFUNCTION, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 5)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_IMPULSERESPONSE, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 6)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_FACPHASE, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 7)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_MSCPHASE, MF_BYCOMMAND | MF_CHECKED) ;
		else if (disptype == 8)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TEST, MF_BYCOMMAND | MF_CHECKED) ;


		for (i=0;i<250;i++)
			specbufarr[i] = 1.0;

		if (_chdir("Pictures"))
		{
			if( _mkdir( "Pictures" ) != 0 )
				MessageBox( hwnd,"Failed to create Picture Directory","ERROR",0);	
			else
				MessageBox( hwnd,"Picture Directory created","INFO",0);	
		}
		else
			_chdir("..");
		if (_chdir("Corrupt"))
		{
			if( _mkdir( "Corrupt" ) != 0 )
				MessageBox( hwnd,"Failed to create Corrupt Directory","ERROR",0);	
		}
		else
			_chdir("..");

		if (rtsonfac)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_RTSHIGHONFAC, MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_RTSHIGHONFAC, MF_BYCOMMAND | MF_UNCHECKED);
		if (dtronfac)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_DTRHIGHONFAC, MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_DTRHIGHONFAC, MF_BYCOMMAND | MF_UNCHECKED);

		DRMTransmitter.GetParameters()->eSymbolInterlMode = getinterleave();
		DRMTransmitter.GetParameters()->eMSCCodingScheme = getqam();
		DRMTransmitter.GetParameters()->SetSpectrumOccup(getspec());
		DRMTransmitter.GetParameters()->InitCellMapTable(getmode(),getspec());

		SetTimer(hwnd,1,100,TimerProc);

		return TRUE;   
		
    case WM_COMMAND:
		OnCommand(hwnd, LOWORD (wParam), HIWORD (wParam));
        return TRUE;

    case WM_CLOSE:
 		comtx('0');
		KillTimer(hwnd,1);
		savevar();
        DestroyWindow (hwnd);
 		DRMReceiver.Stop(); 
		DRMTransmitter.Stop();
		Sleep(1000);
	    PostQuitMessage (0);
        return TRUE;
    case WM_PAINT:
		PaintWaveData(hwnd,TRUE);
        return FALSE;
    }
    return FALSE;
}

int snddev;
char cmdstr[20];

void OnCommand ( HWND hwnd, int ctrlid, int code)
{
    switch (ctrlid)
    {
    case IDB_STARTPIC:
		if (IsRX)
		{
			if (callisok()) 
				DialogBox(TheInstance, MAKEINTRESOURCE (DLG_PICTURE_TX), hwnd, TXPictureDlgProc);
			else
				DialogBox(TheInstance, MAKEINTRESOURCE (DLG_CALLSIGN), hwnd, CallSignDlgProc);
		}
		else
		{
			if (TX_Running) DRMTransmitter.NotSend();
			IsRX = TRUE;
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_ENABLED);
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_DRMSETTINGS, MF_ENABLED);
			if (RX_Running) DRMReceiver.Rec();
			SendMessage (GetDlgItem (hwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"TX Voice");
			SendMessage (GetDlgItem (hwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"TX Pic");
			SetDlgItemText( hwnd, IDC_DCFREQ, " ");
			SetDlgItemText( hwnd, IDC_EDIT,   " ");
			SetDlgItemText( hwnd, IDC_EDIT2,  " ");
			SetDlgItemText( hwnd, IDC_EDIT4,  " ");
			SetDlgItemText( hwnd, IDC_EDIT3,  " ");
			if (strlen(rxdevice) >=1) SelectSrc(rxdevice);
		}
        break;
    case IDB_START:
		if (IsRX)
		{
			char dcbuf[20];
			if (runmode != 'P')
			{
				if (callisok()) 
				{
					SetTXmode(FALSE);
					DRMReceiver.NotRec();
					IsRX = FALSE;
					EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_GRAYED);
					EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_DRMSETTINGS, MF_GRAYED);
					if (TX_Running) 
					{	
						DRMTransmitter.Init();
						if (DRMTransmitter.GetParameters()->iNumDecodedBitsMSC <= 980)
							MessageBox( hwnd,"Mode does not allow Voice","Wrong Mode",0);	
						DRMTransmitter.Send();
						sprintf(dcbuf,"%d",(int)DRMTransmitter.GetCarOffset());
						SetDlgItemText( hwnd, IDC_DCFREQ, dcbuf);
					}
					SendMessage (GetDlgItem (hwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"RX");
					SendMessage (GetDlgItem (hwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"RX");
					if (strlen(txdevice) >=1) SelectSrc(txdevice);
				}
				else
				{
					DialogBox(TheInstance, MAKEINTRESOURCE (DLG_CALLSIGN), hwnd, CallSignDlgProc);
				}
			}
		}
		else
		{
			if (TX_Running) DRMTransmitter.NotSend();
			IsRX = TRUE;
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_ENABLED);
			EnableMenuItem(GetMenu(hwnd), ID_SETTINGS_DRMSETTINGS, MF_ENABLED);
			DRMReceiver.Rec();
			SendMessage (GetDlgItem (hwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"TX Voice");
			SendMessage (GetDlgItem (hwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"TX Pic");
			SetDlgItemText( hwnd, IDC_DCFREQ, " ");
			SetDlgItemText( hwnd, IDC_EDIT,   " ");
			SetDlgItemText( hwnd, IDC_EDIT2,  " ");
			SetDlgItemText( hwnd, IDC_EDIT4,  " ");
			SetDlgItemText( hwnd, IDC_EDIT3,  " ");
			if (strlen(rxdevice) >=1) SelectSrc(rxdevice);
		}
        break;
 
	case ID_PTTPORT_NONE:
		settxport('0');	comtx(gettxport());	checkcomport(hwnd,'0');	break;
	case ID_PTTPORT_COM1:
		settxport('1');	comtx(gettxport());	checkcomport(hwnd,'1');	break;
	case ID_PTTPORT_COM2:
		settxport('2');	comtx(gettxport());	checkcomport(hwnd,'2');	break;
	case ID_PTTPORT_COM3:
		settxport('3');	comtx(gettxport());	checkcomport(hwnd,'3');	break;
	case ID_PTTPORT_COM4:
		settxport('4');	comtx(gettxport());	checkcomport(hwnd,'4');	break;
	case ID_PTTPORT_COM5:
		settxport('5');	comtx(gettxport());	checkcomport(hwnd,'5');	break;
	case ID_PTTPORT_COM6:
		settxport('6');	comtx(gettxport());	checkcomport(hwnd,'6');	break;
	case ID_PTTPORT_COM7:
		settxport('7');	comtx(gettxport());	checkcomport(hwnd,'7');	break;
	case ID_PTTPORT_COM8:
		settxport('8');	comtx(gettxport());	checkcomport(hwnd,'8');	break;
	case ID_SETTINGS_PTTPORT_RTSHIGHONFAC:
		if (rtsonfac)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_RTSHIGHONFAC, MF_BYCOMMAND | MF_UNCHECKED);
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_RTSHIGHONFAC, MF_BYCOMMAND | MF_CHECKED);
		rtsonfac = ! rtsonfac;
		break;
	case ID_SETTINGS_PTTPORT_DTRHIGHONFAC:
		if (dtronfac)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_DTRHIGHONFAC, MF_BYCOMMAND | MF_UNCHECKED);
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_PTTPORT_DTRHIGHONFAC, MF_BYCOMMAND | MF_CHECKED);
		dtronfac = ! dtronfac;
		break;

	case IDM_O_RX_I_DRIVERS0:
		setsoundin(0,'r');
		unchecksoundrx(hwnd,0);
		break;
	case IDM_O_RX_I_DRIVERS1:
		setsoundin(1,'r');
		unchecksoundrx(hwnd,1);
		break;
	case IDM_O_RX_I_DRIVERS2:
		setsoundin(2,'r');
		unchecksoundrx(hwnd,2);
		break;
	case IDM_O_TX_O_DRIVERS0:
		setsoundout(0,'t');
		unchecksoundtx(hwnd,0);
		break;
	case IDM_O_TX_O_DRIVERS1:
		setsoundout(1,'t');
		unchecksoundtx(hwnd,1);
		break;
	case IDM_O_TX_O_DRIVERS2:
		setsoundout(2,'t');
		unchecksoundtx(hwnd,2);
		break;
	case IDM_O_VO_I_DRIVERS0:
		setsoundin(0,'t');
		unchecksoundvoI(hwnd,0);
		break;
	case IDM_O_VO_I_DRIVERS1:
		setsoundin(1,'t');
		unchecksoundvoI(hwnd,1);
		break;
	case IDM_O_VO_I_DRIVERS2:
		setsoundin(2,'t');
		unchecksoundvoI(hwnd,2);
		break;
	case IDM_O_VO_O_DRIVERS0:
		setsoundout(0,'r');
		unchecksoundvoO(hwnd,0);
		break;
	case IDM_O_VO_O_DRIVERS1:
		setsoundout(1,'r');
		unchecksoundvoO(hwnd,1);
		break;
	case IDM_O_VO_O_DRIVERS2:
		setsoundout(2,'r');
		unchecksoundvoO(hwnd,2);
		break;
	case ID_SOUNDCARD_OPENMIXER_RXINPUT:
		snddev = getsoundin('r');
		wsprintf(cmdstr,"-R -D %d",snddev);
		ShellExecute(NULL, "open", "sndvol32.exe", cmdstr, NULL, SW_SHOWNORMAL);
		break;
	case ID_SOUNDCARD_OPENMIXER_TXOUTPUT:
		snddev = getsoundout('t');
		wsprintf(cmdstr,"-D %d",snddev);
		ShellExecute(NULL, "open", "sndvol32.exe", cmdstr, NULL, SW_SHOWNORMAL);
		break;
	case ID_SOUNDCARD_OPENMIXER_VOICEINPUT:
		snddev = getsoundin('t');
		wsprintf(cmdstr,"-R -D %d",snddev);
		ShellExecute(NULL, "open", "sndvol32.exe", cmdstr, NULL, SW_SHOWNORMAL);
		break;
	case ID_SOUNDCARD_OPENMIXER_VOICEOUTPUT:
		snddev = getsoundout('r');
		wsprintf(cmdstr,"-D %d",snddev);
		ShellExecute(NULL, "open", "sndvol32.exe", cmdstr, NULL, SW_SHOWNORMAL);
		break;
	case ID_SOUNDCARD_AUTOMIXERSWITCH:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_MIXERSETTING), hwnd, MixerDlgProc);
		break;
	case ID_ABOUT_TEST:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_ABOUT), hwnd, AboutDlgProc);
		break;
	case ID_ABOUT_HELP:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_HELP), hwnd, AboutDlgProc);
		break;

	case ID_SETTINGS_CALLSIGN:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_CALLSIGN), hwnd, CallSignDlgProc);
		break;

	case ID_SETTINGS_DRMSETTINGS:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_DRMSETTINGS), hwnd, DRMSettingsDlgProc);
		break;

	case ID_SETTINGS_DRMSETTINGSRX:
		CreateDialog(TheInstance, MAKEINTRESOURCE (DLG_RXSETTINGS), hwnd, DRMRXSettingsDlgProc);
		break;

	case ID_SETTINGS_TEXTMESSAGE:
		CreateDialog(TheInstance, MAKEINTRESOURCE (DLG_TEXTMESSAGE), hwnd, TextMessageDlgProc);
		break;

	case ID_SETTINGS_TEXTMESSAGE_OPENRXTEXTMESSAGE:
		if (AllowRXTextMessage)
		{
			AllowRXTextMessage = FALSE;
			if (RXTextMessageON) SendMessage(RXMessagehwnd,WM_CLOSE,0,0);
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_TEXTMESSAGE_OPENRXTEXTMESSAGE, MF_BYCOMMAND | MF_UNCHECKED);
		}
		else
		{
			AllowRXTextMessage = TRUE;
			RXTextMessageWasClosed = FALSE;
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_TEXTMESSAGE_OPENRXTEXTMESSAGE, MF_BYCOMMAND | MF_CHECKED);
		}
		break;
	case ID_SETTINGS_FILETRANSFER_SAVERECEIVEDFILES:
		if (SaveRXFile)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SAVERECEIVEDFILES, MF_BYCOMMAND | MF_UNCHECKED);	
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SAVERECEIVEDFILES, MF_BYCOMMAND | MF_CHECKED);	
		SaveRXFile = !SaveRXFile;
	case ID_SETTINGS_FILETRANSFER_SHOWRECEIVEDFILES:
		if (ShowRXFile)
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SHOWRECEIVEDFILES, MF_BYCOMMAND | MF_UNCHECKED);	
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SHOWRECEIVEDFILES, MF_BYCOMMAND | MF_CHECKED);	
		ShowRXFile = !ShowRXFile;
		break;
	case ID_SETTINGS_FILETRANSFER_SHOWONLYONE:
		if (ShowOnlyFirst)
		{
			wsprintf(lastfilename,"none");
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SHOWONLYONE, MF_BYCOMMAND | MF_UNCHECKED);
		}
		else
			CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_FILETRANSFER_SHOWONLYONE, MF_BYCOMMAND | MF_CHECKED);	
		ShowOnlyFirst = !ShowOnlyFirst;
		break;
	case ID_SETTINGS_FILETRANSFER_SENDFILE:
		DialogBox(TheInstance, MAKEINTRESOURCE (DLG_PICTURE_TX), hwnd, TXPictureDlgProc);
		break;

	case ID_DISPLAY_SPECTRUM:
		disptype = 0;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_SPECTRUM, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_DISPLAY_PSD:
		disptype = 1;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_PSD, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_DISPLAY_LEVEL:
		disptype = 2;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_DISPLAY_LEVEL, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_WATERFALL:
		disptype = 3;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_WATERFALL, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_TRANSFERFUNCTION:
		disptype = 4;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TRANSFERFUNCTION, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_IMPULSERESPONSE:
		disptype = 5;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_IMPULSERESPONSE, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_FACPHASE:
		disptype = 6;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_FACPHASE, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_MSCPHASE:
		disptype = 7;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_MSCPHASE, MF_BYCOMMAND | MF_CHECKED) ;
		break;
	case ID_SETTINGS_DISPLAY_TEST:
		disptype = 8;
		uncheckdisp(hwnd);
		CheckMenuItem(GetMenu(hwnd), ID_SETTINGS_DISPLAY_TEST, MF_BYCOMMAND | MF_CHECKED) ;
		break;

	case ID_CODEC_SPEEX:
		TransmParam->Service[0].AudioParam.eAudioCoding = CParameter::AC_SPEEX;
		CheckMenuItem(GetMenu(hwnd), ID_CODEC_SPEEX, MF_BYCOMMAND | MF_CHECKED) ;
		CheckMenuItem(GetMenu(hwnd), ID_CODEC_LPC10, MF_BYCOMMAND | MF_UNCHECKED) ;
		break;
 	case ID_CODEC_LPC10:
		TransmParam->Service[0].AudioParam.eAudioCoding = CParameter::AC_LPC;
		CheckMenuItem(GetMenu(hwnd), ID_CODEC_LPC10, MF_BYCOMMAND | MF_CHECKED) ;
		CheckMenuItem(GetMenu(hwnd), ID_CODEC_SPEEX, MF_BYCOMMAND | MF_UNCHECKED) ;
		break;

 	case IDC_RESETACQ:
		if (IsRX) DRMReceiver.SetInStartMode();
		break;

	case IDC_GETPICANY:
		if (IsRX && RX_Running)
		{
			CMOTObject NewPic;
			int i;
			if (DRMReceiver.GetDataDecoder()->GetSlideShowPartPicture(NewPic))
			{
				char filename[130];
				int picsize;
				int filnamsize;
				FILE * set = NULL;
				picsize = NewPic.vecbRawData.Size();
				if (SaveRXFile)
				{
					wsprintf(filename,"Corrupt\\%s",NewPic.strName.c_str());
					set = fopen(filename,"wb");
					if (set != NULL)
					{
						for (i=0;i<picsize;i++) putc(NewPic.vecbRawData[i],set);
						fclose(set);
					}
					else
						MessageBox( hwnd,"Could not open File",filename,0);	

					filnamsize = strlen(filename);

					if (filnamsize >= 11)
					{
						if (checkfilenam(filename,filnamsize))
							ShellExecute(NULL, "open", filename, NULL, NULL, SW_SHOWNORMAL);
						else
							MessageBox( hwnd,"Executable File Saved",filename,0);	
					}
					else
						MessageBox( hwnd,"No Filename","Info",0);	
				}
			}
		}
		break;
	case IDC_SENDBSR:
		if (IsRX && RX_Running)
		{
			int myhashval;
			if (DRMReceiver.GetDataDecoder()->GetSlideShowBSR(&numbsrsegments,&namebsrfile,&myhashval))
			{
				if (namebsrfile == "bsr.bin") break;
				txbsrposind = -1;	// Search for old position
				for (int el=0;el<NO_BSR_WIND;el++)
					if (txbsr_onscreen_arr[el] == TRUE)
					{
						if (myhashval == txhasharr[el])
						{
							txbsrposind = el;
							el = NO_BSR_WIND;
						}
					}
				if (txbsrposind >= 0)	//Old pos Found
					PostMessage(txbsrhwnd[txbsrposind], WM_NOTIFY, 0, 0);
				else
				{
					txbsrposind = 0;	// Find new position
					for (int el=0;el<NO_BSR_WIND;el++)
						if (!txbsr_onscreen_arr[el])
						{
							txbsrposind = el;
							el = NO_BSR_WIND;
						}
					txbsr_onscreen_arr[txbsrposind] = TRUE;
					txhasharr[txbsrposind] = myhashval;
					txbsrhwnd[txbsrposind] = CreateDialog(TheInstance, MAKEINTRESOURCE (DLG_SENDBSR), hwnd, SendBSRDlgProc);
				}
			}
		}
		break;

	case IDC_TUNINGTONE:
		if ((TX_Running) && (!isplaying) && (messtate[MS_FAC_CRC] != 0))
		{
			Playtype = 1;
			if (IsRX) _beginthread(PlaySound,0,NULL);
		}
		break;
	case IDC_IDTONE:
		if ((TX_Running) && (!isplaying) && (messtate[MS_FAC_CRC] != 0))
		{
			Playtype = 2;
			if (IsRX) _beginthread(PlaySound,0,NULL);
		}
		break;
	case IDC_GTONE:
		if ((TX_Running) && (!isplaying) && (messtate[MS_FAC_CRC] != 0))
		{
			Playtype = 3;
			if (IsRX) _beginthread(PlaySound,0,NULL);
		}
		break;
	case IDC_BTONE:
		if ((TX_Running) && (!isplaying) && (messtate[MS_FAC_CRC] != 0))
		{
			Playtype = 4;
			if (IsRX) _beginthread(PlaySound,0,NULL);
		}
		break;
	case ID_SETTINGS_LOADLASTRXFILE:
		if (strlen(lastrxfilename) >= 4)
		{
			int i;
			int hashval;
			int iFileNameSize;
			unsigned char	xorfnam = 0;
			unsigned char	addfnam = 0;
			TXpicpospt = 1;
			wsprintf(filetitle[0],"%s",lastrxfilename);
			wsprintf(pictfile[0],"Pictures\\%s",lastrxfilename);
			file_size[0] = lastrxfilesize;
			iFileNameSize = strlen(filetitle[0]);
			if (iFileNameSize > 80)	iFileNameSize = 80; 
			for (i=0;i<iFileNameSize;i++)
			{
				xorfnam ^= filetitle[0][i];
				addfnam += filetitle[0][i];
				addfnam ^= (unsigned char)i;
			}
			hashval = 256*(int)addfnam + (int)xorfnam;
			if (hashval <= 2) hashval += iFileNameSize;
			storesentfilename(pictfile[0],filetitle[0],hashval);
			DRMTransmitter.GetParameters()->SetMSCProtLev(lastrxprotlev);
			DRMTransmitter.GetParameters()->SetSpectrumOccup(lastrxspecocc);
			DRMTransmitter.GetParameters()->eMSCCodingScheme = lastrxcodscheme;
			DRMTransmitter.GetParameters()->InitCellMapTable(lastrxrobmode,lastrxspecocc);
			DRMTransmitter.Init();
			DRMTransmitter.GetParameters()->Service[0].DataParam.iPacketLen = 
				calcpacklen(DRMTransmitter.GetParameters()->iNumDecodedBitsMSC); 
			if (IsRX)
			{
				DialogBox(TheInstance, MAKEINTRESOURCE (DLG_PICTURE_TX), hwnd, TXPictureDlgProc);
			}
		}
		break;
   }
}

CVector<_REAL>		vecrData;
CVector<_REAL>		vecrScale;
BOOL firstnorx = TRUE;
int isspdisp = 0;
int stoptx = -1;	
float specagc = 1.0;

void CALLBACK TimerProc
   (HWND hwnd, UINT nMsg, UINT nIDEvent, DWORD dwTime)
{
  int i;
  char tempstr[20];
  char interl = ' ';
  int qam = 0;
  int prot = 0;
  float snr;

  try
  {

	isspdisp++;
	if (isspdisp >= 4) isspdisp = 0;

	if (stoptx == 0) OnCommand(messhwnd,IDB_START,0);
	if (stoptx >= 0) stoptx--;

	if (IsRX && RX_Running)
	{
		newdata++;
		level = (int)(170.0 * DRMReceiver.GetReceiver()->GetLevelMeter());
		if (level * specagc >= 120)
			specagc -= (float)0.1;
		else if (level * specagc <= 80)
			specagc += (float)0.1;
		if (specagc >= 5.0) specagc = 5.0;

		if (disptype == 0)	//spectrum
		{
			DCFreq = (int)DRMReceiver.GetParameters()->GetDCFrequency();
			DRMReceiver.GetReceiver()->GetInputSpec(vecrData);
			if (vecrData.Size() >= 500)
			{
				for (i=0;i<250;i++)
					specbufarr[i] = (3.0 * specbufarr[i] + vecrData[2*i] + vecrData[2*i+1]) * 0.2;
				for (i=0;i<250;i++)
				{
					specarr[i] = (int)(specbufarr[i] * 40.0 * specagc);
				}
			}
			if (isspdisp != 0) PaintWaveData(hwnd,TRUE);
		}
		if (disptype == 3)	//waterfall
		{
			DRMReceiver.GetReceiver()->GetInputSpec(vecrData);
			if (vecrData.Size() >= 500)
			{
				for (i=0;i<250;i++)
					specarr[i] = 25.0 * vecrData[i] * specagc;
			}
			if (isspdisp != 0) PaintWaveData(hwnd,FALSE);
		}
		if (disptype == 8)	//Moving Waterfall
		{
			DCFreq = (int)DRMReceiver.GetParameters()->GetDCFrequency();
			DRMReceiver.GetReceiver()->GetInputSpec(vecrData);
			if (vecrData.Size() >= 500)
			{
				for (i=0;i<500;i++)
					specarr[i] = 40.0 * vecrData[i] * specagc;
			}
			if (isspdisp != 0) PaintWaveData(hwnd,FALSE);
		}
	}
	if (isspdisp != 0) return;

	if (IsRX && RX_Running)
	{
		if (disptype == 1)	//shifted PSD
		{
			int tmp;
			DRMReceiver.GetOFDMDemod()->GetPowDenSpec(vecrData);
			if (vecrData.Size() >= 256)  // size = 512
				for (i=0;i<250;i++)
				{
					tmp = (int)(-3.0 * vecrData[i+85]) + 45;
					specarr[i] = tmp;
				}
		}
		if (disptype == 4)	//Transfer Funct.
		{
			int tmp;
			DRMReceiver.GetChanEst()->GetTransferFunction(vecrData, vecrScale);
			specarrlen = vecrData.Size();
			for (i=0;i<specarrlen;i++)
			{
				tmp = (int)(-0.5 * vecrData[i]) + 45;
				specarr[i] = tmp;
				tmp = (int)(-0.5 * vecrScale[i]) + 85;
				specarrim[i] = tmp;
			}
		}
		if (disptype == 5)	//Impulse Response
		{
			int tmp;
			_REAL				rLowerBound, rHigherBound;
			_REAL				rStartGuard, rEndGuard;
			_REAL				rPDSBegin, rPDSEnd;
			DRMReceiver.GetChanEst()->
				GetAvPoDeSp(vecrData, vecrScale, rLowerBound, rHigherBound,
				rStartGuard, rEndGuard, rPDSBegin, rPDSEnd);
			specarrlen = vecrData.Size();
			for (i=0;i<specarrlen;i++)
			{
				tmp = (int)(-1.5 * vecrData[i]) + 100;
				specarr[i] = tmp;
			}
		}
		if (disptype == 6)	//FAC constellation
		{
			int tmp;
			CVector<_COMPLEX>	veccData;
			DRMReceiver.GetFACMLC()->GetVectorSpace(veccData);
			specarrlen = veccData.Size();
			for (i=0;i<specarrlen;i++)
			{
				tmp = (int)(50.0*veccData[i].real());
				specarr[i] = tmp;
				tmp = (int)(50.0*veccData[i].imag());
				specarrim[i] = tmp;
			}
		}
		if (disptype == 7)	//MSC constellation
		{
			int tmp;
			CVector<_COMPLEX>	veccData;
			DRMReceiver.GetMSCMLC()->GetVectorSpace(veccData);
			specarrlen = veccData.Size();
			if(specarrlen >= 530) specarrlen = 530;
			for (i=0;i<specarrlen;i++)
			{
				tmp = (int)(50.0*veccData[i].real());
				specarr[i] = tmp;
				tmp = (int)(50.0*veccData[i].imag());
				specarrim[i] = tmp;
			}
		}

		if (DRMReceiver.GetParameters()->Service[0].IsActive())	//flags, etc.
		{
			CParameter::EAudCod ecodec;
			firstnorx = TRUE;
			rxcall = DRMReceiver.GetParameters()->Service[0].strLabel;
			if (rxcall == lastrxcall) consrxcall = rxcall;
			else if (rxcall.length() >= 3) lastrxcall = rxcall;
			SendMessage (GetDlgItem (hwnd, IDC_EDIT), WM_SETTEXT, 0,(LPARAM)rxcall.c_str());
			snr = (float)DRMReceiver.GetChanEst()->GetSNREstdB();
			sprintf(tempstr,"%.1f",snr);
			SendMessage (GetDlgItem (hwnd, IDC_EDIT4), WM_SETTEXT, 0, (LPARAM)tempstr);
			snr = DRMReceiver.GetParameters()->GetDCFrequency();
			sprintf(tempstr,"%d",(int)snr);
			SendMessage (GetDlgItem (hwnd, IDC_DCFREQ), WM_SETTEXT, 0, (LPARAM)tempstr);

			if (DRMReceiver.GetParameters()->Service[0].eAudDataFlag == CParameter::SF_AUDIO)
			{
				sprintf(tempstr,"%d ",DRMReceiver.GetAudSrcDec()->getdecodperc());
				SendMessage (GetDlgItem (hwnd, IDC_EDIT5), WM_SETTEXT, 0, (LPARAM)tempstr);
				ecodec = DRMReceiver.GetParameters()->Service[0].AudioParam.eAudioCoding;
				if (ecodec == CParameter::AC_LPC)
					SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"LPC");
				else if (ecodec == CParameter::AC_SPEEX)
					SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"SPEEX");
				else if (ecodec == CParameter::AC_SSTV)
					SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"SSTV");
				else
					SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)" ");
			}
			else
			{
				CMOTObject NewPic;
				SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"Data");
				sprintf(tempstr,"%d / %d / %d"	,DRMReceiver.GetDataDecoder()->GetTotSize()
												,DRMReceiver.GetDataDecoder()->GetActSize()
												,DRMReceiver.GetDataDecoder()->GetActPos());
				SendMessage (GetDlgItem (hwnd, IDC_EDIT5), WM_SETTEXT, 0, (LPARAM)tempstr);
				if (DRMReceiver.GetDataDecoder()->GetSlideShowPicture(NewPic))
				{
					char filename[130];
					int picsize;
					int filnamsize;
					FILE * set = NULL;
					picsize = NewPic.vecbRawData.Size();
					if (strcmp(NewPic.strName.c_str(),"bsr.bin" ) == 0)
					{
						set = fopen("bsrreq.bin","wt");
						if (set != NULL)
						{
							for (i=0;i<picsize;i++) putc(NewPic.vecbRawData[i],set);
							fclose(set);

							filetosend = readbsrfile();
						}
						else
							filetosend = 'x';

						if (filetosend.size() >= 3)
						{
							acthash = gethash();

							bsrposind = -1;	// Search for old position
							for (int el=0;el<NO_BSR_WIND;el++)
							{
								if (bsr_onscreen_arr[el] == TRUE)
								{
									if (acthash == hasharr[el]) 
									{
										if (bsrcall[el] == consrxcall)
										{
											bsrposind = el;
											el = NO_BSR_WIND;
										}
									}
								}
							}
							if (bsrposind >= 0)	//Old pos Found
							{
								PostMessage(bsrhwnd[bsrposind], WM_NOTIFY, 0, 0);
							}
							else				//Find new pos
							{
								bsrposind = 0;	// Find new position
								for (int el=0;el<NO_BSR_WIND;el++)
								{
									if (bsr_onscreen_arr[el] == FALSE)
									{
										bsrposind = el;
										el = NO_BSR_WIND;
									}
								}
								bsr_onscreen_arr[bsrposind] = TRUE;
								hasharr[bsrposind] = acthash;
								bsrcall[bsrposind] = consrxcall;
								bsrhwnd[bsrposind] = 
									CreateDialog(TheInstance, MAKEINTRESOURCE (DLG_ANSWERBSR), hwnd, AnswerBSRDlgProc);
							}
						}
					}
					else if (SaveRXFile)
					{
						int iFileNameSize;
						int tmphashval;
						unsigned char	xorfnam = 0;
						unsigned char	addfnam = 0;
						wsprintf(filename,"%s",NewPic.strName.c_str());
						iFileNameSize = strlen(filename);
						if (iFileNameSize > 80)	iFileNameSize = 80; 
						for (i=0;i<iFileNameSize;i++)
						{
							xorfnam ^= filename[i];
							addfnam += filename[i];
							addfnam ^= (unsigned char)i;
						}
						tmphashval = 256*(int)addfnam + (int)xorfnam;
						if (tmphashval <= 2) tmphashval += iFileNameSize;

						for (int el=0;el<NO_BSR_WIND;el++)
						{
							if (tmphashval == txhasharr[el])
							{
								if (txbsr_onscreen_arr[el] == TRUE)
								{
								    bCloseMe = TRUE;
									SendMessage(txbsrhwnd[el], WM_NOTIFY, 0, 0);
								}
							}
						}
						
						wsprintf(lastrxfilename,"%s",NewPic.strName.c_str());
						lastrxfilesize = picsize;
						lastrxrobmode = DRMReceiver.GetParameters()->GetWaveMode();
						lastrxspecocc = DRMReceiver.GetParameters()->GetSpectrumOccup();
						lastrxcodscheme = DRMReceiver.GetParameters()->eMSCCodingScheme;
						lastrxprotlev = DRMReceiver.GetParameters()->MSCPrLe.iPartB;
						wsprintf(filename,"Pictures\\%s",NewPic.strName.c_str());
						set = fopen(filename,"wb");
						if (set != NULL)
						{
							for (i=0;i<picsize;i++) putc(NewPic.vecbRawData[i],set);
							fclose(set);
						}
						if (ShowRXFile)
						{
							filnamsize = strlen(filename);
							if (filnamsize >= 11)
							{
								if (checkfilenam(filename,filnamsize))
								{
									if (stricmp(lastfilename,filename) != 0)
										ShellExecute(NULL, "open", filename, NULL, NULL, SW_SHOWNORMAL);
								}
								else 
								{
									if (stricmp(lastfilename,filename) != 0)
										MessageBox( hwnd,filename,"Executable File Saved",0);
								}
							}
							if (ShowOnlyFirst) wsprintf(lastfilename,"%s",filename);
						}
					}
				}
			}
			if (DRMReceiver.GetParameters()->GetWaveMode() == RM_ROBUSTNESS_MODE_A)
				robmode = 'A';
			else if (DRMReceiver.GetParameters()->GetWaveMode() == RM_ROBUSTNESS_MODE_B)
				robmode = 'B';
			if ((DRMReceiver.GetParameters()->GetSpectrumOccup()) == SO_0)
				specocc = 3;
			else if ((DRMReceiver.GetParameters()->GetSpectrumOccup()) == SO_1)
				specocc = 5;
			if (DRMReceiver.GetParameters()->eSymbolInterlMode == CParameter::SI_LONG)
				interl = 'L';
			else if (DRMReceiver.GetParameters()->eSymbolInterlMode == CParameter::SI_SHORT)
				interl = 'S';
			if (DRMReceiver.GetParameters()->eMSCCodingScheme == CParameter::CS_2_SM)
				qam = 16;
			else if (DRMReceiver.GetParameters()->eMSCCodingScheme == CParameter::CS_3_SM)
				qam = 64;
			else 
				qam = 4;
			prot = DRMReceiver.GetParameters()->MSCPrLe.iPartB;
			wsprintf(tempstr,"%c/%c/%d/%d/2.%d",robmode,interl,qam,prot,specocc);	
			SendMessage (GetDlgItem (hwnd, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)tempstr);	
			if ((DRMReceiver.GetParameters()->Service[0].eAudDataFlag == CParameter::SF_AUDIO) &&
				(DRMReceiver.GetParameters()->Service[0].AudioParam.bTextflag == TRUE))
			{
				if ((AllowRXTextMessage) && (runmode != 'P'))
				{
					if ((!RXTextMessageON) && (!RXTextMessageWasClosed))
						RXMessagehwnd = CreateDialog (TheInstance, 
							MAKEINTRESOURCE (DLG_RXTEXTMESSAGE), hwnd, RXTextMessageDlgProc);
					else if (RXTextMessageON)
						SendMessage(RXMessagehwnd,WM_NOTIFY,0,0);
				}
			}
		}
		else if (firstnorx)
		{
			//if (RXTextMessageON) SendMessage(RXMessagehwnd,WM_CLOSE,0,0);
			wsprintf(lastfilename,"%s","none");
			SendMessage (GetDlgItem (hwnd, IDC_EDIT), WM_SETTEXT, 0, (LPARAM)"No Service");
			SendMessage (GetDlgItem (hwnd, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)" ");
			SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)" ");
			SendMessage (GetDlgItem (hwnd, IDC_EDIT4), WM_SETTEXT, 0, (LPARAM)"0");
			SendMessage (GetDlgItem (hwnd, IDC_EDIT5), WM_SETTEXT, 0, (LPARAM)" ");
			SendMessage (GetDlgItem (hwnd, IDC_DCFREQ), WM_SETTEXT, 0, (LPARAM)" ");
			firstnorx = FALSE;
		}
		else
		{
			RXTextMessageWasClosed = FALSE;
		}
	}
	else if (!IsRX && TX_Running)
	{
		int numbits,percent;
		firstnorx = TRUE;
		level = (int)(170.0 * DRMTransmitter.GetReadData()->GetLevelMeter());
		if (TransmParam->Service[0].eAudDataFlag == CParameter::SF_AUDIO)
		{
			if (TransmParam->Service[0].AudioParam.eAudioCoding == CParameter::AC_LPC)
				SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"LPC");
			if (TransmParam->Service[0].AudioParam.eAudioCoding == CParameter::AC_SPEEX)
				SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"SPEEX");
			if (TransmParam->Service[0].AudioParam.eAudioCoding == CParameter::AC_SSTV)
				SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"SSTV");
		}
		else
		{
			SendMessage (GetDlgItem (hwnd, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"Data");
			numbits = DRMTransmitter.GetAudSrcEnc()->GetPicCnt();
			percent = DRMTransmitter.GetAudSrcEnc()->GetPicPerc();
			wsprintf(tempstr,"%d / %d",numbits,percent);
			SendMessage (GetDlgItem (hwnd, IDC_EDIT5), WM_SETTEXT, 0, (LPARAM)tempstr);
			percent = DRMTransmitter.GetAudSrcEnc()->GetNoOfPic();
			if ((numbits+1 > percent) && (stoptx == -1)) stoptx = stoptx_time;
		}
		numbits = DRMTransmitter.GetParameters()->iNumDecodedBitsMSC;
		wsprintf(tempstr,"%d",numbits);
		SendMessage (GetDlgItem (hwnd, IDC_EDIT4), WM_SETTEXT, 0, (LPARAM)tempstr);
		SendMessage (GetDlgItem (hwnd, IDC_EDIT), WM_SETTEXT, 0, (LPARAM)getcall());
		if (DRMTransmitter.GetParameters()->GetWaveMode() == RM_ROBUSTNESS_MODE_A)
			robmode = 'A';
		else if (DRMTransmitter.GetParameters()->GetWaveMode() == RM_ROBUSTNESS_MODE_B)
				robmode = 'B';
		if ((DRMTransmitter.GetParameters()->GetSpectrumOccup()) == SO_0)
			specocc = 3;
		else if ((DRMTransmitter.GetParameters()->GetSpectrumOccup()) == SO_1)
			specocc = 5;
		if (DRMTransmitter.GetParameters()->eSymbolInterlMode == CParameter::SI_LONG)
			interl = 'L';
		else if (DRMTransmitter.GetParameters()->eSymbolInterlMode == CParameter::SI_SHORT)
			interl = 'S';
		if (DRMTransmitter.GetParameters()->eMSCCodingScheme == CParameter::CS_2_SM)
			qam = 16;
		else if (DRMTransmitter.GetParameters()->eMSCCodingScheme == CParameter::CS_3_SM)
			qam = 64;
		else
			qam = 4;
		prot = DRMTransmitter.GetParameters()->MSCPrLe.iPartB;
		wsprintf(tempstr,"%c/%c/%d/%d/2.%d",robmode,interl,qam,prot,specocc);	
		SendMessage (GetDlgItem (hwnd, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)tempstr);	
	}


	if (IsRX)
	{
		if ((disptype == 3) || (disptype == 8))
			PaintWaveData(hwnd,FALSE);
		else
			PaintWaveData(hwnd,TRUE);
	}
	else
		PaintWaveData(hwnd,TRUE);
	
  }
  catch (CGenErr GenErr)
  {
  	return;
  }

}


BOOL CALLBACK AboutDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog (hwnd, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}


BOOL CALLBACK SendBSRDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	char tmpbsrname[20];
	int tmpno;
	BOOL send3 = FALSE;
    switch (message)
    {
    case WM_INITDIALOG:
    case WM_NOTIFY:
		if (bCloseMe)
		{
			bCloseMe = FALSE;
			tmpno = GetDlgItemInt( hwnd, IDC_BSRINST, NULL, FALSE );
			txbsr_onscreen_arr[tmpno] = FALSE;
			EndDialog (hwnd, 0);
		}
		else
		{
			wsprintf(tmpbsrname,"bsr%d.bin",txbsrposind);
			SetDlgItemInt( hwnd, IDC_SENDBSR_NOSEG, numbsrsegments, FALSE );
			SetDlgItemText( hwnd, IDC_SENDBSR_FNAME, namebsrfile.c_str() );
			SetDlgItemInt( hwnd, IDC_BSRINST, txbsrposind, FALSE );
			SetDlgItemText( hwnd, IDC_SENDBSR_FROMCALL, consrxcall.c_str());
			CopyFile("bsr.bin",tmpbsrname,FALSE);
		}
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK3:
			send3 = TRUE;
        case IDOK:
			if (!IsRX) return TRUE;
			tmpno = GetDlgItemInt( hwnd, IDC_BSRINST, NULL, FALSE );
			wsprintf(tmpbsrname,"bsr%d.bin",tmpno);
			CopyFile(tmpbsrname,"bsr.bin",FALSE);
  
			strcpy(pictfile[0],"bsr.bin");	strcpy(filetitle[0],"bsr.bin");
			strcpy(pictfile[1],"bsr.bin");	strcpy(filetitle[1],"bsr.bin");
			TXpicpospt = 2;
			if (send3)
			{
				strcpy(pictfile[2],"bsr.bin");	strcpy(filetitle[2],"bsr.bin");
				strcpy(pictfile[3],"bsr.bin");	strcpy(filetitle[3],"bsr.bin");
				TXpicpospt = 4;
			}
			SetTXmode(TRUE);
			if (RX_Running) DRMReceiver.NotRec();
			IsRX = FALSE;
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_GRAYED);
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_DRMSETTINGS, MF_GRAYED);
			if (TX_Running)
			{
				char dcbuf[20];
				DRMTransmitter.Init();
				DRMTransmitter.Send();
				sprintf(dcbuf,"%d",(int)DRMTransmitter.GetCarOffset());
				SetDlgItemText( messhwnd, IDC_DCFREQ, dcbuf);
			}
			SendMessage (GetDlgItem (messhwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"RX");
			SendMessage (GetDlgItem (messhwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"RX");
			TXpicpospt = 0;
            return TRUE;
        case IDCANCEL:
			tmpno = GetDlgItemInt( hwnd, IDC_BSRINST, NULL, FALSE );
			wsprintf(tmpbsrname,"bsr%d.bin",tmpno);
			remove(tmpbsrname);
			txbsr_onscreen_arr[tmpno] = FALSE;
            EndDialog (hwnd, 0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
		tmpno = GetDlgItemInt( hwnd, IDC_BSRINST, NULL, FALSE );
		wsprintf(tmpbsrname,"bsr%d.bin",tmpno);
		remove(tmpbsrname);
		txbsr_onscreen_arr[tmpno] = FALSE;
 		EndDialog (hwnd, 0);
		return TRUE;
     }
    return FALSE;
}


BOOL CALLBACK AnswerBSRDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	char tmpbsrname[20];
	int tmpno;
	BOOL send3 = FALSE;
    switch (message)
    {
    case WM_NOTIFY:
    case WM_INITDIALOG:
		SetDlgItemInt( hwnd, ID_ABSRINST, bsrposind, FALSE );
		SetDlgItemInt( hwnd, IDC_SENDNOBSR, segnobsrfile(), FALSE );
		SetDlgItemText( hwnd, IDC_SENDNAMEBSR, filetosend.c_str());
		SetDlgItemText( hwnd, IDC_SENDCALLBSR, consrxcall.c_str());
		wsprintf(tmpbsrname,"bsrreq%d.bin",bsrposind);
		CopyFile("bsrreq.bin",tmpbsrname,FALSE);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK3:
			send3 = TRUE;
        case IDOK:
			if (!IsRX) return TRUE;
			tmpno = GetDlgItemInt( hwnd, ID_ABSRINST, NULL, FALSE );
			wsprintf(tmpbsrname,"bsrreq%d.bin",tmpno);
			CopyFile(tmpbsrname,"bsrreq.bin",FALSE);
			readbsrfile();
			if (send3)
				writeselsegments(3);
			else
				writeselsegments(1);
			if (RX_Running) DRMReceiver.NotRec();
			IsRX = FALSE;
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_GRAYED);
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_DRMSETTINGS, MF_GRAYED);
			if (TX_Running)
			{
				char dcbuf[20];
				DRMTransmitter.Init();
				DRMTransmitter.Send();
				sprintf(dcbuf,"%d",(int)DRMTransmitter.GetCarOffset());
				SetDlgItemText( messhwnd, IDC_DCFREQ, dcbuf);
			}
			SendMessage (GetDlgItem (messhwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"RX");
			SendMessage (GetDlgItem (messhwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"RX");  
            return TRUE;
		case IDCANCEL:
			tmpno = GetDlgItemInt( hwnd, ID_ABSRINST, NULL, FALSE );
			wsprintf(tmpbsrname,"bsrreq%d.bin",tmpno);
			remove(tmpbsrname);
			bsr_onscreen_arr[tmpno] = FALSE;
            EndDialog (hwnd, 0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
		tmpno = GetDlgItemInt( hwnd, ID_ABSRINST, NULL, FALSE );
		wsprintf(tmpbsrname,"bsrreq%d.bin",tmpno);
		remove(tmpbsrname);
		bsr_onscreen_arr[tmpno] = FALSE;
 		EndDialog (hwnd, 0);
		return TRUE;
     }
    return FALSE;
}

BOOL CALLBACK CallSignDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	char callbuf[20];
    switch (message)
    {
    case WM_INITDIALOG:
		SetDlgItemText( hwnd, IDC_EDIT1, getcall() );
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK:
			GetDlgItemText( hwnd, IDC_EDIT1, callbuf, 20);
			setcall(callbuf);
			savevar();
			if (runmode != 'R') TransmParam->Service[0].strLabel = getcall();
            EndDialog (hwnd, 0);
            return TRUE;
        case IDCANCEL:
            EndDialog (hwnd, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}


BOOL CALLBACK DRMSettingsDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	char dcbuf[20];
	float TxDCFreq = 0.0;
    switch (message)
    {
    case WM_INITDIALOG:
		if (DRMTransmitter.GetParameters()->GetSpectrumOccup() == SO_1)
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_25), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_23), BM_SETCHECK, (WPARAM)1, 0);
		if (DRMTransmitter.GetParameters()->eSymbolInterlMode == CParameter::SI_SHORT)
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_SHORT400MS), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_LONG2S), BM_SETCHECK, (WPARAM)1, 0);
		if (DRMTransmitter.GetParameters()->GetWaveMode() == RM_ROBUSTNESS_MODE_A)
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)1, 0);
		if (DRMTransmitter.GetParameters()->eMSCCodingScheme == CParameter::CS_1_SM)
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)1, 0);
		else if (DRMTransmitter.GetParameters()->eMSCCodingScheme == CParameter::CS_2_SM)
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)1, 0);
		if (DRMTransmitter.GetParameters()->MSCPrLe.iPartB == 0)
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_NORMAL), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_LOW), BM_SETCHECK, (WPARAM)1, 0);
		TxDCFreq = DRMTransmitter.GetCarOffset();
		sprintf(dcbuf,"%.1f",TxDCFreq);
		SetDlgItemText( hwnd, IDC_DCOFFS, dcbuf);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDCANCEL:
            EndDialog (hwnd, 0);
            return TRUE;

        case IDOK:
 			GetDlgItemText( hwnd, IDC_DCOFFS, dcbuf, 20);
			if (sscanf(dcbuf,"%f",&TxDCFreq) == 1)
				DRMTransmitter.SetCarOffset(TxDCFreq);
			DRMTransmitter.Init();
			DRMTransmitter.GetParameters()->Service[0].DataParam.iPacketLen = 
				calcpacklen(DRMTransmitter.GetParameters()->iNumDecodedBitsMSC); 
			setmodeqam(DRMTransmitter.GetParameters()->GetWaveMode(),
				       DRMTransmitter.GetParameters()->eMSCCodingScheme,
					   DRMTransmitter.GetParameters()->eSymbolInterlMode,
					   DRMTransmitter.GetParameters()->GetSpectrumOccup());
            EndDialog (hwnd, 0);
            return TRUE;

		case ID_INTERLEAVE_SHORT400MS:
			DRMTransmitter.GetParameters()->eSymbolInterlMode = CParameter::SI_SHORT;
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_SHORT400MS), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_LONG2S), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_INTERLEAVE_LONG2S:
			DRMTransmitter.GetParameters()->eSymbolInterlMode = CParameter::SI_LONG;
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_SHORT400MS), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_LONG2S), BM_SETCHECK, (WPARAM)1, 0);
			break;
		case ID_SETTINGS_MSCPROTECTION_NORMAL:
			DRMTransmitter.GetParameters()->SetMSCProtLev(0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_NORMAL), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_LOW), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_SETTINGS_MSCPROTECTION_LOW:
			DRMTransmitter.GetParameters()->SetMSCProtLev(1);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_NORMAL), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_LOW), BM_SETCHECK, (WPARAM)1, 0);
			break;
		case ID_MSCCODING_4QAM:
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_1_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_MSCCODING_16QAM:
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_2_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_MSCCODING_64QAM:
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_3_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)1, 0);
			break;
		case ID_MODE_ROBMODEA:
			DRMTransmitter.GetParameters()->InitCellMapTable(RM_ROBUSTNESS_MODE_A,
				DRMTransmitter.GetParameters()->GetSpectrumOccup());
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)1, 0);
			break;
		case ID_MODE_ROBMODEB:
			DRMTransmitter.GetParameters()->InitCellMapTable(RM_ROBUSTNESS_MODE_B,
				DRMTransmitter.GetParameters()->GetSpectrumOccup());
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_SETTINGS_BW_25:
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_25), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_23), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->SetSpectrumOccup(SO_1);
			break;
		case ID_SETTINGS_BW_23:
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_23), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_BW_25), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->SetSpectrumOccup(SO_0);
			break;
		case ID_SET_A_MAXSPEED:
			DRMTransmitter.GetParameters()->eSymbolInterlMode = CParameter::SI_LONG;
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_SHORT400MS), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_LONG2S), BM_SETCHECK, (WPARAM)1, 0);
			DRMTransmitter.GetParameters()->SetMSCProtLev(1);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_NORMAL), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_LOW), BM_SETCHECK, (WPARAM)1, 0);
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_2_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->InitCellMapTable(RM_ROBUSTNESS_MODE_A,
				DRMTransmitter.GetParameters()->GetSpectrumOccup());
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)1, 0);
			break;
		case ID_SET_B_DEFAULT:
			DRMTransmitter.GetParameters()->eSymbolInterlMode = CParameter::SI_SHORT;
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_SHORT400MS), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_INTERLEAVE_LONG2S), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->SetMSCProtLev(0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_NORMAL), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_MSCPROTECTION_LOW), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_2_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)0, 0);
			DRMTransmitter.GetParameters()->InitCellMapTable(RM_ROBUSTNESS_MODE_B,
				DRMTransmitter.GetParameters()->GetSpectrumOccup());
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)0, 0);
			break;
		case ID_SET_B_ROBUST:
			DRMTransmitter.GetParameters()->eMSCCodingScheme = CParameter::CS_1_SM;
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_64QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_16QAM), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, ID_MSCCODING_4QAM ), BM_SETCHECK, (WPARAM)1, 0);
			DRMTransmitter.GetParameters()->InitCellMapTable(RM_ROBUSTNESS_MODE_B,
				DRMTransmitter.GetParameters()->GetSpectrumOccup());
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEB), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, ID_MODE_ROBMODEA), BM_SETCHECK, (WPARAM)0, 0);
			break;
        }
        break;
    }
    return FALSE;
}

int centerfreq = 350;
int windowsize = 200;

BOOL CALLBACK DRMRXSettingsDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	BOOL sucess;
    switch (message)
    {
    case WM_INITDIALOG:
		SetDlgItemInt(hwnd, IDC_FREQACC, sensivity, FALSE);
		SetDlgItemInt(hwnd, IDC_SEARCHWINLOWER, centerfreq, FALSE);
		SetDlgItemInt(hwnd, IDC_SEARCHWINUPPER, windowsize, FALSE);
		if (rxaudfilt)
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_AUDIOFILTER_RXFILTER), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, ID_SETTINGS_AUDIOFILTER_RXFILTER), BM_SETCHECK, (WPARAM)0, 0);
		setRXfilt((rxaudfilt==TRUE));
		if (fastreset)
			SendMessage (GetDlgItem (hwnd, IDC_SETTINGS_FASTRESET), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, IDC_SETTINGS_FASTRESET), BM_SETCHECK, (WPARAM)0, 0);
		DRMReceiver.SetFastReset(fastreset==TRUE);
		break;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK:
			sensivity = GetDlgItemInt(hwnd, IDC_FREQACC, &sucess, FALSE);
			if (sucess) DRMReceiver.SetFreqAcqSensivity(2.0 - (_REAL)sensivity/100.0);
			centerfreq = GetDlgItemInt(hwnd, IDC_SEARCHWINLOWER, &sucess, FALSE);
			windowsize = GetDlgItemInt(hwnd, IDC_SEARCHWINUPPER, &sucess, FALSE);
			if (sucess)
			{
				windowsize = GetDlgItemInt(hwnd, IDC_SEARCHWINUPPER, &sucess, FALSE);
				if (sucess) DRMReceiver.SetFreqAcqWinSize((_REAL)centerfreq,(_REAL)windowsize);
			}
            EndDialog (hwnd, 0);
            return TRUE;
        case IDAPP:
			sensivity = GetDlgItemInt(hwnd, IDC_FREQACC, &sucess, FALSE);
			if (sucess) DRMReceiver.SetFreqAcqSensivity(2.0 - (_REAL)sensivity/100.0);
			centerfreq = GetDlgItemInt(hwnd, IDC_SEARCHWINLOWER, &sucess, FALSE);
			if (sucess)
			{
				windowsize = GetDlgItemInt(hwnd, IDC_SEARCHWINUPPER, &sucess, FALSE);
				if (sucess) DRMReceiver.SetFreqAcqWinSize((_REAL)centerfreq,(_REAL)windowsize);
			}
            return TRUE;
		case ID_SETTINGS_AUDIOFILTER_RXFILTER:
			if (rxaudfilt)
			{
				rxaudfilt = FALSE;
				SendMessage (GetDlgItem (hwnd, ID_SETTINGS_AUDIOFILTER_RXFILTER), BM_SETCHECK, (WPARAM)0, 0);
				setRXfilt(false);
			}
			else
			{
				rxaudfilt = TRUE;
				SendMessage (GetDlgItem (hwnd, ID_SETTINGS_AUDIOFILTER_RXFILTER), BM_SETCHECK, (WPARAM)1, 0);
				setRXfilt(true);
			}
			break;
		case IDC_SETTINGS_FASTRESET:
			if (fastreset)
				SendMessage (GetDlgItem (hwnd, IDC_SETTINGS_FASTRESET), BM_SETCHECK, (WPARAM)0, 0);
			else
				SendMessage (GetDlgItem (hwnd, IDC_SETTINGS_FASTRESET), BM_SETCHECK, (WPARAM)1, 0);
			fastreset = !fastreset;
			DRMReceiver.SetFastReset(fastreset==TRUE);
			break;
		}
	break;
	}
    return FALSE;
}

BOOL CALLBACK TextMessageDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	FILE * txtset = NULL;
	char textbuf[200];
    switch (message)
    {
    case WM_INITDIALOG:
		txtset = fopen("textmessage.txt","rt");
		if (txtset != NULL)
		{
			fscanf(txtset,"%[^\0]",&textbuf);
			fclose(txtset);
		}
		else 
			strcpy(textbuf," \x0d\x0a This is a Test ");
		SetDlgItemText( hwnd, IDC_TEXTMESSAGE, textbuf);		
		if (UseTextMessage)
			SendMessage (GetDlgItem (hwnd, IDC_USETEXTMESSAGE), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, IDC_USETEXTMESSAGE), BM_SETCHECK, (WPARAM)0, 0);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK:
			GetDlgItemText( hwnd, IDC_TEXTMESSAGE, textbuf, 200);
			txtset = fopen("textmessage.txt","wt");
			if (txtset != NULL)
			{
				fprintf(txtset,"%s",&textbuf);
				fclose(txtset);
			}
			DRMTransmitter.GetAudSrcEnc()->ClearTextMessage();
			if (UseTextMessage)
			{
				DRMTransmitter.GetAudSrcEnc()->SetTextMessage(textbuf);
				DRMTransmitter.GetParameters()->Service[0].AudioParam.bTextflag = TRUE;
			}
			else
				DRMTransmitter.GetParameters()->Service[0].AudioParam.bTextflag = FALSE;
            EndDialog (hwnd, 0);
            return TRUE;
        case IDCANCEL:
            EndDialog (hwnd, 0);
            return TRUE;
		case IDC_USETEXTMESSAGE:
			if (UseTextMessage)
			{
				UseTextMessage = FALSE;
				settext(FALSE);
				SendMessage (GetDlgItem (hwnd, IDC_USETEXTMESSAGE), BM_SETCHECK, (WPARAM)0, 0);
				DRMTransmitter.GetAudSrcEnc()->ClearTextMessage();
			}
			else
			{
				UseTextMessage = TRUE;
				settext(TRUE);
				SendMessage (GetDlgItem (hwnd, IDC_USETEXTMESSAGE), BM_SETCHECK, (WPARAM)1, 0);
			}
			return TRUE;
        }
        break;
    case WM_CLOSE:
 		EndDialog (hwnd, 0);
		return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK RXTextMessageDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	char textbuf[200];
    switch (message)
    {
    case WM_INITDIALOG:
		RXTextMessageON = TRUE;
		strcpy(textbuf,"Waiting");
		SetDlgItemText( hwnd, IDC_RXTEXTMESSAGE, textbuf);
		return TRUE;
    case WM_NOTIFY:
		strcpy(textbuf,DRMReceiver.GetParameters()->Service[0].AudioParam.strTextMessage.c_str());
		if (strlen(textbuf) >= 1)
			SetDlgItemText( hwnd, IDC_RXTEXTMESSAGE, textbuf);
		return TRUE;
    case WM_COMMAND:
		RXTextMessageWasClosed = TRUE;
		RXTextMessageON = FALSE;
		EndDialog (hwnd, 0);
		return TRUE;
    case WM_CLOSE:
		RXTextMessageON = FALSE;
 		EndDialog (hwnd, 0);
		return TRUE;
    }
    return FALSE;
}

long file_size[8];
BOOL longleadin = TRUE;

BOOL CALLBACK TXPictureDlgProc
   (HWND hwnd, UINT message, UINT wParam, LPARAM lParam)
{
	BOOL res;
    switch (message)
    {
    case WM_INITDIALOG:
		putfiles(hwnd);
		if (LeadIn == 2) {
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)0, 0);
		}
		else if (LeadIn == 3) {
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)1, 0);
		}
		else {
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)0, 0);
		}
		if (longleadin)
			SendMessage (GetDlgItem (hwnd, IDC_LEADINLONG ), BM_SETCHECK, (WPARAM)1, 0);
		else
			SendMessage (GetDlgItem (hwnd, IDC_LEADINLONG ), BM_SETCHECK, (WPARAM)0, 0);
		return TRUE;
		break;
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDOK:
			if (TXpicpospt == 0)
			{
				EndDialog (hwnd, 0);
				return TRUE;
			}
			SetTXmode(TRUE);
			if (RX_Running) DRMReceiver.NotRec();
			IsRX = FALSE;
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_FILETRANSFER_SENDFILE, MF_GRAYED);
			EnableMenuItem(GetMenu(messhwnd), ID_SETTINGS_DRMSETTINGS, MF_GRAYED);
			if (TX_Running)
			{
				char dcbuf[20];
				DRMTransmitter.Init();
				if (callisok()) DRMTransmitter.Send();
				sprintf(dcbuf,"%d",(int)DRMTransmitter.GetCarOffset());
				SetDlgItemText( messhwnd, IDC_DCFREQ, dcbuf);
			}
			SendMessage (GetDlgItem (messhwnd, IDB_START), WM_SETTEXT, 0, (LPARAM)"RX");
			SendMessage (GetDlgItem (messhwnd, IDB_STARTPIC), WM_SETTEXT, 0, (LPARAM)"RX");
			EndDialog (hwnd, 0);
			return TRUE;
		case ID_CHOOSEFILE:
			if (TXpicpospt > 7) 
			{
				MessageBox(hwnd,"Too many Files in Buffer","Buffer Full",MB_OK);
				return TRUE; 
			}
			res = GetFileName(hwnd,pictfile[TXpicpospt],filetitle[TXpicpospt],
				sizeof(pictfile[TXpicpospt]),&file_size[TXpicpospt]);
			if (res == 1) TXpicpospt++;
			putfiles(hwnd);
			return TRUE;
		case ID_DELALLFILES:
			TXpicpospt = 0;
			putfiles(hwnd);
			return TRUE;
		case ID_CANCELPICTX:
			EndDialog (hwnd, 0);
			return TRUE;
		case IDC_SENDONCE:
			LeadIn = 1;
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)0, 0);
			return TRUE;
		case IDC_SENDTWICE:
			LeadIn = 2;
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)1, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)0, 0);
			return TRUE;
		case IDC_SENDTHREE:
			LeadIn = 3;
			SendMessage (GetDlgItem (hwnd, IDC_SENDONCE ), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTWICE), BM_SETCHECK, (WPARAM)0, 0);
			SendMessage (GetDlgItem (hwnd, IDC_SENDTHREE), BM_SETCHECK, (WPARAM)1, 0);
			return TRUE;
		case IDC_LEADINLONG:
			if (longleadin)
			{
				longleadin = FALSE;
				SendMessage (GetDlgItem (hwnd, IDC_LEADINLONG ), BM_SETCHECK, (WPARAM)0, 0);
				DRMTransmitter.GetAudSrcEnc()->SetStartDelay(starttx_time);
			}
			else
			{
				longleadin = TRUE;
				SendMessage (GetDlgItem (hwnd, IDC_LEADINLONG ), BM_SETCHECK, (WPARAM)1, 0);
				DRMTransmitter.GetAudSrcEnc()->SetStartDelay(starttx_time_long);
			}
			return TRUE;
        }
        break;
    case WM_CLOSE:
 		EndDialog (hwnd, 0);
		return TRUE;
   }
    return FALSE;
}

