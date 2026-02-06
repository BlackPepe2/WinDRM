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
#include <conio.h>
#include "../getfilenam.h"
#include "libs/callsign.h"
#include "DrmTransmitter.h"
#include "bsr.h"

extern CDRMTransmitter	DRMTransmitter;

HWND bsrhwnd[NO_BSR_WIND];
BOOL bsr_onscreen_arr[NO_BSR_WIND];
int hasharr[NO_BSR_WIND];
string bsrcall[NO_BSR_WIND];

HWND txbsrhwnd[NO_BSR_WIND];
BOOL txbsr_onscreen_arr[NO_BSR_WIND];
int  txhasharr[NO_BSR_WIND];

string sentfilenamearr[numstredele];
string sentfilenamenodirarr[numstredele];
int sentfilehasharr[numstredele];


//CVector<short>  vecsToSendarr[numstredele];
int sentfilept = 0;

void storesentfilename(string the_filename, string the_filenamenodir, int hashval)
{
	if (the_filename.size() == 0) return;
	if (sentfilehasharr[sentfilept] != hashval)
	{
		sentfilept++;
		if (sentfilept >= numstredele) sentfilept = 0;
	}
	sentfilenamearr[sentfilept] = the_filename;
	sentfilenamenodirarr[sentfilept] = the_filenamenodir;
	sentfilehasharr[sentfilept] = hashval;
}

string idfilename;
string idfilenamenodir;
int hashval;

CVector<short>  vecsToSend;

void getsentfilename(int thehashval)
{
	int i;
	for (i=0;i<numstredele;i++)
	{
		if (sentfilehasharr[i] == thehashval)
		{
			idfilename = sentfilenamearr[i];
			idfilenamenodir = sentfilenamenodirarr[i];
			return;
		}
	}
	idfilename = ' ';
	idfilenamenodir = ' ';
}

int segsize;

string readbsrfile()
{
	FILE * bsr;
	int trid;
	char rubbish[10];
	int segno;
	int vct = 0;
	bsr = fopen("bsrreq.bin","rt");
	if (bsr != NULL)
	{
		vecsToSend.Init(0);
		fscanf(bsr,"%d",&trid);
		fscanf(bsr,"%s",&rubbish);
		fscanf(bsr,"%d",&segsize);
		while (!feof(bsr))
		{
			fscanf(bsr,"%d",&segno);
			vecsToSend.Enlarge(1);
			vecsToSend[vct] = segno;
			vct++;
		}
		fclose(bsr);
		hashval = trid;
		getsentfilename(trid);
		return idfilenamenodir;
	}
	return "";
}

int gethash()
{
	return hashval;
}

int vecsize;

int segnobsrfile()
{
	vecsize = vecsToSend.Size();
	if (vecsize >= 2) vecsize--;
	return vecsize;
}

void writeselsegments(int no)
{
		int noseg;
		int i;
		noseg = vecsToSend.Size();
		DRMTransmitter.GetParameters()->iNumAudioService = 0;
		DRMTransmitter.GetParameters()->iNumDataService = 1;
		DRMTransmitter.GetParameters()->Service[0].eAudDataFlag = CParameter::SF_DATA;
		DRMTransmitter.GetParameters()->Service[0].DataParam.iStreamID = 0;
		DRMTransmitter.GetParameters()->Service[0].DataParam.eDataUnitInd = CParameter::DU_DATA_UNITS;
		DRMTransmitter.GetParameters()->Service[0].DataParam.eAppDomain = CParameter::AD_DAB_SPEC_APP;
		DRMTransmitter.GetParameters()->Service[0].iServiceDescr = 0;
		DRMTransmitter.Init();
		DRMTransmitter.GetParameters()->Service[0].DataParam.iPacketLen = 
			calcpacklen(DRMTransmitter.GetParameters()->iNumDecodedBitsMSC); 
		DRMTransmitter.GetAudSrcEnc()->ClearPicFileNames();

		for (i=0;i<no;i++)
			DRMTransmitter.GetAudSrcEnc()->SetPicFileName(idfilename,idfilenamenodir,vecsToSend);
		if (noseg <= 40)	// two instances if few segments
			DRMTransmitter.GetAudSrcEnc()->SetPicFileName(idfilename,idfilenamenodir,vecsToSend);
		if (noseg <= 10)	// three instances if few segments
			DRMTransmitter.GetAudSrcEnc()->SetPicFileName(idfilename,idfilenamenodir,vecsToSend);
		if (noseg <=  5)	// four instances if few segments
			DRMTransmitter.GetAudSrcEnc()->SetPicFileName(idfilename,idfilenamenodir,vecsToSend);
}

void InitBsr(void)
{
	int i;
	for (i=0;i<numstredele;i++)
	{
		sentfilenamearr[i] = "";
		sentfilenamenodirarr[i] = "";
		sentfilehasharr[i] = 0;
	}
	for (i=0;i<NO_BSR_WIND;i++)
	{
		bsrhwnd[i] = NULL;
		txbsrhwnd[i] = NULL;
		bsr_onscreen_arr[i] = FALSE;
		txbsr_onscreen_arr[i] = FALSE;
		hasharr[i] = -1;
		txhasharr[i] = -1;
		bsrcall[i] = "";
	}
}

