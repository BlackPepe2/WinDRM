/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	DRM Parameters
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

#include "Parameter.h"


// To be replaced by something nicer!!! TODO
#include "DrmReceiver.h"
extern CDRMReceiver	DRMReceiver;


/* Implementation *************************************************************/
void CParameter::ResetServicesStreams()
{
	int i;

	/* Reset everything to possible start values */
	for (i = 0; i < MAX_NUM_SERVICES; i++)
	{
		Service[i].AudioParam.strTextMessage = "";
		Service[i].AudioParam.iStreamID = STREAM_ID_NOT_USED;
		Service[i].AudioParam.eAudioCoding = AC_LPC;
		Service[i].AudioParam.bTextflag = FALSE;

		Service[i].DataParam.iStreamID = STREAM_ID_NOT_USED;
		Service[i].DataParam.ePacketModInd = PM_PACKET_MODE;
		Service[i].DataParam.eDataUnitInd = DU_SINGLE_PACKETS;
		Service[i].DataParam.iPacketID = 0;
		Service[i].DataParam.iPacketLen = 0;
		Service[i].DataParam.eAppDomain = AD_DRM_SPEC_APP;

		Service[i].iServiceID = SERV_ID_NOT_USED;
		Service[i].iLanguage = 0;
		Service[i].eAudDataFlag = SF_AUDIO;
		Service[i].iServiceDescr = 0;
		Service[i].strLabel = "";
	}

	for (i = 0; i < MAX_NUM_STREAMS; i++)
	{
		Stream[i].iLenPartB = 0;
	}
}

int CParameter::GetNumActiveServices()
{
	int iNumAcServ = 0;

	for (int i = 0; i < MAX_NUM_SERVICES; i++)
		if (Service[i].IsActive())
			iNumAcServ++;

	return iNumAcServ;
}

void CParameter::GetActiveServices(CVector<int>& veciActServ)
{
	int				i;
	CVector<int>	vecbServices(MAX_NUM_SERVICES, 0);

	/* Init return vector */
	veciActServ.Init(0);

	/* Get active services */
	int iNumServices = 0;
	for (i = 0; i < MAX_NUM_SERVICES; i++)
	{
		if (Service[i].IsActive())
		{
			/* A service is active, enlarge return vector and store ID */
			veciActServ.Enlarge(1);
			veciActServ[iNumServices] = i;

			iNumServices++;
		}
	}
}

void CParameter::GetActiveStreams(CVector<int>& veciActStr)
{
	int					i;
	int					iNumStreams;
	CVector<int>		vecbStreams(MAX_NUM_STREAMS, 0);

	/* Determine which streams are active */
	for (i = 0; i < MAX_NUM_SERVICES; i++)
	{
		if (Service[i].IsActive())
		{
			/* Audio stream */
			if (Service[i].AudioParam.iStreamID != STREAM_ID_NOT_USED)
				vecbStreams[Service[i].AudioParam.iStreamID] = 1;

			/* Data stream */
			if (Service[i].DataParam.iStreamID != STREAM_ID_NOT_USED)
				vecbStreams[Service[i].DataParam.iStreamID] = 1;
		}
	}

	/* Now, count streams */
	iNumStreams = 0;
	for (i = 0; i < MAX_NUM_STREAMS; i++)
		if (vecbStreams[i] == 1)
			iNumStreams++;

	/* Now that we know how many streams are active, dimension vector */
	veciActStr.Init(iNumStreams);

	/* Store IDs of active streams */
	iNumStreams = 0;
	for (i = 0; i < MAX_NUM_STREAMS; i++)
	{
		if (vecbStreams[i] == 1)
		{
			veciActStr[iNumStreams] = i;
			iNumStreams++;
		}
	}
}

_REAL CParameter::GetBitRate(int iServiceID)
{
	int iNoBitsPerFrame;
	int iLenPartA;
	int iLenPartB;

	/* First, check if audio or data service and get lengths */
	if (Service[iServiceID].eAudDataFlag == SF_AUDIO)
	{
		if (Service[iServiceID].AudioParam.iStreamID != STREAM_ID_NOT_USED)
		{
			iLenPartA = 0;

			iLenPartB =
				Stream[Service[iServiceID].AudioParam.iStreamID].iLenPartB;
		}
		else
		{
			/* Stream is not yet assigned, set lengths to zero */
			iLenPartA = 0;
			iLenPartB = 0;
		}
	}
	else
	{
		if (Service[iServiceID].DataParam.iStreamID != STREAM_ID_NOT_USED)
		{
			iLenPartA = 0;

			iLenPartB =
				Stream[Service[iServiceID].DataParam.iStreamID].iLenPartB;
		}
		else
		{
			/* Stream is not yet assigned, set lengths to zero */
			iLenPartA = 0;
			iLenPartB = 0;
		}
	}

	/* Total length in bits */
	iNoBitsPerFrame = (iLenPartA + iLenPartB) * SIZEOF__BYTE;

	/* We have 3 frames with time duration of 1.2 seconds. Bit rate should be
	   returned in kbps (/ 1000) */
	return (_REAL) iNoBitsPerFrame * 3 / 1.2 / 1000;
}

_BOOLEAN CParameter::IsEEP(int iServiceID)
{
	return FALSE;
}

void CParameter::InitCellMapTable(const ERobMode eNewWaveMode,
								  const ESpecOcc eNewSpecOcc)
{
	/* Set new values and make table */
	eRobustnessMode = eNewWaveMode;
	eSpectOccup = eNewSpecOcc;
	MakeTable(eRobustnessMode, eSpectOccup);


// Should be done but is no good for simulation, TODO
///* Set init flags */
//DRMReceiver.InitsForAllModules();
}

_BOOLEAN CParameter::SetWaveMode(const ERobMode eNewWaveMode)
{
	/* Apply changes only if new paramter differs from old one */
	if (eRobustnessMode != eNewWaveMode)
	{
		/* Set new value */
		eRobustnessMode = eNewWaveMode;

		/* This parameter change provokes update of table */
		MakeTable(eRobustnessMode, eSpectOccup);

		/* Set init flags */
		DRMReceiver.InitsForWaveMode();

		/* Signal that parameter has changed */
		return TRUE;
	}
	else
		return FALSE;
}

void CParameter::SetSpectrumOccup(ESpecOcc eNewSpecOcc)
{
	/* Apply changes only if new paramter differs from old one */
	if (eSpectOccup != eNewSpecOcc)
	{
		/* Set new value */
		eSpectOccup = eNewSpecOcc;

		/* This parameter change provokes update of table */
		MakeTable(eRobustnessMode, eSpectOccup);

		/* Set init flags */
		DRMReceiver.InitsForSpectrumOccup();
	}
}

void CParameter::SetStreamLen(const int iStreamID, const int iNewLenPartB)
{
	/* Apply changes only if parameters have changed */
	if (Stream[iStreamID].iLenPartB != iNewLenPartB)
	{
		/* Use new parameters */
		Stream[iStreamID].iLenPartB = iNewLenPartB;

		/* Set init flags */
		DRMReceiver.InitsForMSC();
	}
}

void CParameter::SetNumDecodedBitsMSC(const int iNewNumDecodedBitsMSC)
{
	/* Apply changes only if parameters have changed */
	if (iNewNumDecodedBitsMSC != iNumDecodedBitsMSC)
	{
		iNumDecodedBitsMSC = iNewNumDecodedBitsMSC;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::SetNumBitsHieraFrTot(const int iNewNumBitsHieraFrTot)
{
	/* Apply changes only if parameters have changed */
	if (iNewNumBitsHieraFrTot != iNumBitsHierarchFrameTotal)
	{
		iNumBitsHierarchFrameTotal = iNewNumBitsHieraFrTot;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::SetNumAudioDecoderBits(const int iNewNumAudioDecoderBits)
{
	/* Apply changes only if parameters have changed */
	if (iNewNumAudioDecoderBits != iNumAudioDecoderBits)
	{
		iNumAudioDecoderBits = iNewNumAudioDecoderBits;

		/* Set init flags */
		DRMReceiver.InitsForAudParam();
	}
}

void CParameter::SetNumDataDecoderBits(const int iNewNumDataDecoderBits)
{
	/* Apply changes only if parameters have changed */
	if (iNewNumDataDecoderBits != iNumDataDecoderBits)
	{
		iNumDataDecoderBits = iNewNumDataDecoderBits;

		/* Set init flags */
		DRMReceiver.InitsForDataParam();
	}
}

void CParameter::SetMSCProtLev(const int NewMSCPrLe)
{
	_BOOLEAN bParamersHaveChanged = FALSE;

	if (NewMSCPrLe != MSCPrLe.iPartB)
	{
		MSCPrLe.iPartB = NewMSCPrLe;

		bParamersHaveChanged = TRUE;
	}

	/* In case parameters have changed, set init flags */
	if (bParamersHaveChanged == TRUE)
		DRMReceiver.InitsForMSC();
}

void CParameter::SetAudioParam(const int iShortID,
							   const CAudioParam NewAudParam)
{
	/* Apply changes only if parameters have changed */
	if (Service[iShortID].AudioParam != NewAudParam)
	{
		Service[iShortID].AudioParam = NewAudParam;

		/* Set init flags */
		DRMReceiver.InitsForAudParam();
	}
}

void CParameter::SetDataParam(const int iShortID, const CDataParam NewDataParam)
{
	/* Apply changes only if parameters have changed */
	if (Service[iShortID].DataParam != NewDataParam)
	{
		Service[iShortID].DataParam = NewDataParam;

		/* Set init flags */
		DRMReceiver.InitsForDataParam();
	}
}

void CParameter::SetInterleaverDepth(const ESymIntMod eNewDepth)
{
	if (eSymbolInterlMode != eNewDepth)
	{
		eSymbolInterlMode = eNewDepth;

		/* Set init flags */
		DRMReceiver.InitsForInterlDepth();
	}

}

void CParameter::SetMSCCodingScheme(const ECodScheme eNewScheme)
{
	if (eMSCCodingScheme != eNewScheme)
	{
		eMSCCodingScheme = eNewScheme;

		/* Set init flags */
		DRMReceiver.InitsForMSCCodSche();
	}
}

void CParameter::SetCurSelAudioService(const int iNewService)
{
	if (iCurSelAudioService != iNewService)
	{
		iCurSelAudioService = iNewService;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::SetCurSelDataService(const int iNewService)
{
	if (iCurSelDataService != iNewService)
	{
		iCurSelDataService = iNewService;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::EnableMultimedia(const _BOOLEAN bFlag)
{
	if (bUsingMultimedia != bFlag)
	{
		bUsingMultimedia = bFlag;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::SetNumOfServices(const int iNNumAuSe, const int iNNumDaSe)
{
	/* Check whether number of activated services is not greater than the
	   number of services signalled by the FAC because it can happen that
	   a false CRC check (it is only a 8 bit CRC) of the FAC block
	   initializes a wrong service */
	if (GetNumActiveServices() > iNNumAuSe + iNNumDaSe)
	{
		/* Reset services and streams and set flag for init modules */
		ResetServicesStreams();
		DRMReceiver.InitsForMSCDemux();
	}

	if ((iNumAudioService != iNNumAuSe) || (iNumDataService != iNNumDaSe))
	{
		iNumAudioService = iNNumAuSe;
		iNumDataService = iNNumDaSe;

		/* Set init flags */
		DRMReceiver.InitsForMSCDemux();
	}
}

void CParameter::SetAudDataFlag(const int iServID, const ETyOServ iNewADaFl)
{
	if (Service[iServID].eAudDataFlag != iNewADaFl)
	{
		Service[iServID].eAudDataFlag = iNewADaFl;

		/* Set init flags */
		DRMReceiver.InitsForMSC();
	}
}

void CParameter::SetServID(const int iServID, const _UINT32BIT iNewServID)
{
	if (Service[iServID].iServiceID != iNewServID)
	{
		Service[iServID].iServiceID = iNewServID;

		/* Set init flags */
		DRMReceiver.InitsForMSC();
	}
}



/* Reception log implementation --------------------------------------------- */
CParameter::CReceptLog::CReceptLog() : iNumAACFrames(10), pFileLong(NULL),
	pFileShort(NULL), iFrequency(0), strAdditText(""), bDelayedLogStart(FALSE)
{
	ResetLog(TRUE);
	ResetLog(FALSE);
}

void CParameter::CReceptLog::ResetLog(const _BOOLEAN bIsLong)
{
	if (bIsLong == TRUE)
	{
		bSyncOK = TRUE;
		bFACOk = TRUE;
		bMSCOk = TRUE;

		rCurSNR = (_REAL) 0.0;
	}
	else
	{
		iNumCRCOkFAC = 0;
		iNumCRCOkMSC = 0;
		iNumSNR = 0;
		rAvSNR = (_REAL) 0.0;
	}
}

void CParameter::CReceptLog::SetSync(const _BOOLEAN bCRCOk)
{
	if (bLogActivated == TRUE)
	{
		Mutex.Lock();

		/* If one of the syncs were wrong in one second, set to false */
		if (bCRCOk == FALSE)
			bSyncOK = FALSE;

		Mutex.Unlock();
	}
}

void CParameter::CReceptLog::SetFAC(const _BOOLEAN bCRCOk)
{
	if (bLogActivated == TRUE)
	{
		Mutex.Lock();

		if (bCRCOk == TRUE)
			iNumCRCOkFAC++;
		else
			bFACOk = FALSE;

		Mutex.Unlock();
	}
}

void CParameter::CReceptLog::SetMSC(const _BOOLEAN bCRCOk)
{
	if (bLogActivated == TRUE)
	{
		Mutex.Lock();

		if (bCRCOk == TRUE)
			iNumCRCOkMSC++;
		else
			bMSCOk = FALSE;

		Mutex.Unlock();
	}
}

void CParameter::CReceptLog::SetSNR(const _REAL rNewCurSNR)
{
	if (bLogActivated == TRUE)
	{
		Mutex.Lock();

		/* Set parameter for long log file version */
		rCurSNR = rNewCurSNR;

		iNumSNR++;

		/* Average SNR values */
		rAvSNR += rNewCurSNR;

		Mutex.Unlock();
	}
}

void CParameter::CReceptLog::SetNumAAC(const int iNewNum)
{
	/* Set the number of AAC frames in one block */
	iNumAACFrames = iNewNum;

	ResetLog(TRUE);
	ResetLog(FALSE);
}

void CParameter::CReceptLog::SetLog(const _BOOLEAN bLog)
{
	bLogActivated = bLog;

	/* Open or close the file */
	if (bLogActivated == TRUE)
	{
		Mutex.Lock();

		/* Init long and short version of log file. Open output file, write
		   header and reset log file parameters */
		/* Short */
		pFileShort = fopen("DreamLog.txt", "a");
		SetLogHeader(pFileShort, FALSE);
		ResetLog(FALSE);
		iTimeCntShort = 0;

		/* Long */
		pFileLong = fopen("DreamLogLong.csv", "a");
		SetLogHeader(pFileLong, TRUE);
		ResetLog(TRUE);

		/* Init time with current time. The time function returns the number of
		   seconds elapsed since midnight (00:00:00), January 1, 1970,
		   coordinated universal time, according to the system clock */
		time(&TimeCntLong);

		Mutex.Unlock();
	}
	else
	{
		/* Close both types of log files */
		CloseFile(pFileLong, TRUE);
		CloseFile(pFileShort, FALSE);
	}
}

void CParameter::CReceptLog::SetLogHeader(FILE* pFile, const _BOOLEAN bIsLong)
{
	time_t		ltime;
	struct tm*	today;

	/* Get time and date */
	time(&ltime);
	today = gmtime(&ltime); /* Should be UTC time */

	if (pFile != NULL)
	{
		/* Beginning of new table (similar to standard DRM log file) */
		fprintf(pFile, "\n>>>>\nDream\nSoftware \n");

		fprintf(pFile, "Starttime (UTC)  %d-%02d-%02d %02d:%02d:%02d\n",
			today->tm_year + 1900, today->tm_mon + 1, today->tm_mday,
			today->tm_hour, today->tm_min, today->tm_sec);

		fprintf(pFile, "Frequency        ");
		if (iFrequency != 0)
			fprintf(pFile, "%d kHz", iFrequency);
		
		/* Write additional text */
		if (strAdditText != "")
			fprintf(pFile, "\n%s\n\n", strAdditText.c_str());
		else
			fprintf(pFile, "\n\n");

		/* The long version of log file has different header */
		if (bIsLong == TRUE)
			fprintf(pFile, "DATE,       TIME,     SNR, SYNC, FAC, MSC\n");
		else
			fprintf(pFile, "MINUTE  SNR     SYNC    AUDIO     TYPE\n");

		fflush(pFile);
	}
}

void CParameter::CReceptLog::CloseFile(FILE* pFile, const _BOOLEAN bIsLong)
{
	if (pFile != NULL)
	{
		if (bIsLong == FALSE)
			fprintf(pFile, "\nCRC: \n");

		fprintf(pFile, "<<<<\n\n");
		fclose(pFile);

		pFile = NULL;
	}
}

void CParameter::CReceptLog::WriteParameters(const _BOOLEAN bIsLong)
{
	try
	{
		if (bLogActivated == TRUE)
		{
			Mutex.Lock();

			if (bIsLong == TRUE)
			{
				int			iSyncInd, iFACInd, iMSCInd;
				struct tm*	TimeNow;

				if (bSyncOK == TRUE)
					iSyncInd = 1;
				else
					iSyncInd = 0;

				if (bFACOk == TRUE)
					iFACInd = 1;
				else
					iFACInd = 0;

				if (bMSCOk == TRUE)
					iMSCInd = 1;
				else
					iMSCInd = 0;

				TimeNow = gmtime(&TimeCntLong); /* Should be UTC time */

				/* This data can be read by Microsoft Excel */
				fprintf(pFileLong,
					"%d-%02d-%02d, %02d:%02d:%02d, %3d,    %1d,   %1d,   %1d\n",
					TimeNow->tm_year + 1900, TimeNow->tm_mon + 1,
					TimeNow->tm_mday, TimeNow->tm_hour, TimeNow->tm_min,
					TimeNow->tm_sec, (int) rCurSNR, iSyncInd, iFACInd, iMSCInd);
			}
			else
			{
				int iAverageSNR, iTmpNumAAC;

				/* Avoid division by zero */
				if (iNumSNR == 0)
					iAverageSNR = 0;
				else
					iAverageSNR = (int) Round(rAvSNR / iNumSNR);

				/* If no sync, do not print number of AAC frames. If the number
				   of correct FAC CRCs is lower than 10%, we assume that
				   receiver is not synchronized */
				if (iNumCRCOkFAC < 15)
					iTmpNumAAC = 0;
				else
					iTmpNumAAC = iNumAACFrames;

				fprintf(pFileShort, "  %04d   %2d      %3d  %4d/%02d        0",
					iTimeCntShort, iAverageSNR, iNumCRCOkFAC,
					iNumCRCOkMSC, iTmpNumAAC);

#ifdef _DEBUG_
				/* Save additional system parameter for debugging performance */
				fprintf(pFileShort,
					"   Dopp: %.2f Hz   FreOff  %.2f Hz   SamOff %.2f Hz",
					DRMReceiver.GetChanEst()->GetSigma(),
					DRMReceiver.GetParameters()->GetDCFrequency(),
					DRMReceiver.GetParameters()->GetSampFreqEst());
#endif
				fprintf(pFileShort, "\n"); /* New line */
			}

			fflush(pFileLong);
			fflush(pFileShort);

			ResetLog(bIsLong);

			if (bIsLong == TRUE)
			{
				/* This is a time_t type variable. It contains the number of
				   seconds from a certain defined date. We simply increment
				   this number for the next second instance */
				TimeCntLong++;
			}
			else
				iTimeCntShort++;

			Mutex.Unlock();
		}
	}

	catch (...)
	{
		/* To prevent errors if user views the file during reception */
	}
}
