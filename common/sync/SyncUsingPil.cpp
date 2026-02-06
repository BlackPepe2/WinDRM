/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Frame synchronization (Using time-pilots), frequency sync tracking
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

#include "SyncUsingPil.h"


/* Implementation *************************************************************/
void CSyncUsingPil::ProcessDataInternal(CParameter& ReceiverParam)
{
	int			i;
	int			iMinIndex;
	int			iCurIndex;
	CReal		rResultIREst;
	CReal		rResultPilPairEst;
	CReal		rMinValue;
	CReal		rFreqOffsetEst;
	CComplex	cOldFreqPilCorr;
	CComplex	cFreqOffEstVecSym;
	CComplex	cCurPilMult;
	CComplex	cErrVec;


	/**************************************************************************\
	* Frame synchronization detection										   *
	\**************************************************************************/
	if ((bSyncInput == FALSE) && (bAquisition == TRUE))
	{
		/* DRM frame synchronization using impulse response ----------------- */
		/* This algorithm uses all scattered pilots in a symbol. We extract the
		   signal at the pilot positions (the positions of pilots in the first
		   OFDM symbol in a DRM frame) and calculate a channel estimate.
		   Afterwards we go into the time domain and check, if this is really
		   a possible impulse response by calculating the peak-to-average ratio
		   of the given transformed signal */

		/* Pick pilot positions and calculate "test" channel estimation */
		iCurIndex = 0;
		for (i = 0; i < iNumCarrier; i++)
		{
			if (_IsScatPil(ReceiverParam.matiMapTab[0][i]))
			{
				/* Get channel estimate */
				veccChan[iCurIndex] =
					(*pvecInputData)[i] / ReceiverParam.matcPilotCells[0][i];

				/* We have to introduce a new index because not on all carriers
				   is a pilot */
				iCurIndex++;
			}
		}

		/* Calculate abs(IFFT) for getting estimate of impulse response */
		vecrTestImpResp = Abs(Ifft(veccChan, FftPlan));

		/* Calculate peak to average, we need to add a minus since the following
		   algorithm searches for a minimum */
		rResultIREst = - Max(vecrTestImpResp) / Sum(vecrTestImpResp);


		/* DRM frame synchronization based on time pilots ------------------- */
		/* We use a differential demodulation of the time-pilots, because there
		   is no channel-estimation done, yet. The differential factors are pre-
		   calculated in the Init-routine. Additionally, the index of the first
		   pilot in the pair is stored in ".iNumCarrier". We calculate and
		   averaging the Euclidean-norm of the resulting complex values */
		rResultPilPairEst = (CReal) 0.0;
		for (i = 0; i < iNumDiffFact; i++)
		{
			cErrVec = ((*pvecInputData)[vecDiffFact[i].iNumCarrier] *
				vecDiffFact[i].cDiff -
				(*pvecInputData)[vecDiffFact[i].iNumCarrier + 1]);

			/* Add squard magnitude of error vector */
			rResultPilPairEst += SqMag(cErrVec);
		}


		/* Finding beginning of DRM frame in results ------------------------ */
		/* Store correlation results in a shift register for finding the peak.
		   Method of IR should not be used with robustness mode A because the
		   guard-interval is too short in this mode */

		// @@ change volker @@
		//if (eCurRobMode == RM_ROBUSTNESS_MODE_A)
			vecrCorrHistory.AddEnd(rResultPilPairEst);
		//else
		//	vecrCorrHistory.AddEnd(rResultIREst);

		/* Search for minimum distance. Init value with a high number */
		iMinIndex = 0;
		rMinValue = _MAXREAL;
		for (i = 0; i < iNumSymPerFrame; i++)
		{
			if (vecrCorrHistory[i] < rMinValue)
			{
				rMinValue = vecrCorrHistory[i];
				iMinIndex = i;
			}
		}

		/* If minimum is in the middle of the interval -> check frame sync */
		if (iMinIndex == iMiddleOfInterval)
		{
			if (iSymbCntFraSy == iNumSymPerFrame - iMiddleOfInterval - 1)
			{
				/* Reset flag */
				bBadFrameSync = FALSE;

				/* Post Message for GUI (Good frame sync) */
				PostWinMessage(MS_FRAME_SYNC, 0);
			}
			else
			{
				if (bBadFrameSync == TRUE)
				{
					/* Reset symbol counter according to received data */
					iSymbCntFraSy = iNumSymPerFrame - iMiddleOfInterval - 1;

					/* Reset flag */
					bBadFrameSync = FALSE;

					/* Post Message for GUI for bad frame sync (red light) */
					PostWinMessage(MS_FRAME_SYNC, 2);
				}
				else
				{
					/* One false detected frame sync should not reset the actual
					   frame sync because the measurement could be wrong.
					   Sometimes the frame sync detection gets false results. If
					   the next time the frame sync is still unequal to the
					   measurement, then correct it */
					bBadFrameSync = TRUE;

					/* Post Message that frame sync was wrong but was not yet
					   corrected (yellow light) */
					PostWinMessage(MS_FRAME_SYNC, 1);
				}
			}
		}
	}
	else
	{
		/* Frame synchronization has successfully finished, show always green
		   light */
		PostWinMessage(MS_FRAME_SYNC, 0);
	}

	/* Set current symbol ID in extended data of output vector */
	(*pvecOutputData).GetExData().iSymbolID = iSymbCntFraSy;

	/* Increase symbol counter and take care of wrap around */
	iSymbCntFraSy++;
	if (iSymbCntFraSy >= iNumSymPerFrame)
		iSymbCntFraSy = 0;


	/**************************************************************************\
	* Using Frequency pilot information										   *
	\**************************************************************************/
	if ((bSyncInput == FALSE) && (bTrackPil == TRUE))
	{
		cFreqOffEstVecSym = CComplex((CReal) 0.0, (CReal) 0.0);

		for (i = 0; i < NUM_FREQ_PILOTS; i++)
		{
			/* The old pilots must be rotated due to timing corrections */
			cOldFreqPilCorr = Rotate(cOldFreqPil[i], iPosFreqPil[i],
				(*pvecInputData).GetExData().iCurTimeCorr);

			/* Calculate the inner product of the sum */
			cCurPilMult =
				(*pvecInputData)[iPosFreqPil[i]] * Conj(cOldFreqPilCorr);

			/* Save "old" frequency pilots for next symbol. Special treatment
			   for robustness mode D (carriers 7 and 21) necessary 
			   (See 8.4.2.2) */
			cOldFreqPil[i] = (*pvecInputData)[iPosFreqPil[i]];

#ifdef USE_SAMOFFS_TRACK_FRE_PIL
			/* Get phase difference for sample rate offset estimation. Average
			   the vector, real and imaginary part separately */
			IIR1(cFreqPilotPhDiff[i], cCurPilMult, rLamSamRaOff);
#endif

			/* Calculate estimation of frequency offset */
			cFreqOffEstVecSym += cCurPilMult;
		}


		/* Frequency offset ------------------------------------------------- */
		/* Correct frequency offset estimation for resample offset corrections.
		   When a sample rate offset correction was applied, the frequency
		   offset is shifted proportional to this correction. The correction
		   is mandatory if large sample rate offsets occur */

		/* Get sample rate offset change */
		CReal rDiffSamOffset =
			rPrevSamRateOffset - ReceiverParam.rResampleOffset;

		/* Save current resample offset for next symbol */
		rPrevSamRateOffset = ReceiverParam.rResampleOffset;

		/* Correct sample-rate offset correction according to the proportional
		   rule. Use relative DC frequency offset plus relative average offset
		   of frequency pilots to the DC frequency. Normalize this offset so
		   that it can be used as a phase correction for frequency offset
		   estimation  */
		CReal rPhaseCorr = (ReceiverParam.rFreqOffsetAcqui +
			ReceiverParam.rFreqOffsetTrack + rAvFreqPilDistToDC) *
			rDiffSamOffset / SOUNDCRD_SAMPLE_RATE / rNormConstFOE;

		/* Actual correction (rotate vector) */
		cFreqOffVec *= CComplex(Cos(rPhaseCorr), Sin(rPhaseCorr));


		/* Average vector, real and imaginary part separately */
		IIR1(cFreqOffVec, cFreqOffEstVecSym, rLamFreqOff);

		/* Calculate argument */
		rFreqOffsetEst = Angle(cFreqOffVec);

		/* Correct measurement average for actually applied frequency
		   correction */
		cFreqOffVec *= CComplex(Cos(-rFreqOffsetEst), Sin(-rFreqOffsetEst));

#ifndef USE_FRQOFFS_TRACK_GUARDCORR
		/* Integrate the result for controling the frequency offset, normalize
		   estimate */
		ReceiverParam.rFreqOffsetTrack += rFreqOffsetEst * rNormConstFOE;
#endif


#ifdef USE_SAMOFFS_TRACK_FRE_PIL
		/* Sample rate offset ----------------------------------------------- */
		/* Calculate estimation of sample frequency offset. We use the different
		   frequency offset estimations of the frequency pilots. We normalize
		   them with the distance between them and average the result (/ 2.0) */
		CReal rSampFreqOffsetEst =
			((Angle(cFreqPilotPhDiff[1]) - Angle(cFreqPilotPhDiff[0])) /
			(iPosFreqPil[1] - iPosFreqPil[0]) +
			(Angle(cFreqPilotPhDiff[2]) - Angle(cFreqPilotPhDiff[0])) /
			(iPosFreqPil[2] - iPosFreqPil[0])) / (CReal) 2.0;

		/* Integrate the result for controling the resampling */
		ReceiverParam.rResampleOffset +=
			CONTR_SAMP_OFF_INTEGRATION * rSampFreqOffsetEst;
#endif

#ifdef _DEBUG_
/* Save frequency and sample rate tracking */
static FILE* pFile = fopen("test/freqtrack.dat", "w");
fprintf(pFile, "%e %e\n", SOUNDCRD_SAMPLE_RATE * ReceiverParam.rFreqOffsetTrack,
	ReceiverParam.rResampleOffset);
fflush(pFile);
#endif
	}


	/* If synchronized DRM input stream is used, overwrite the detected
	   frequency offest estimate by "0", because we know this value */
	if (bSyncInput == TRUE)
		ReceiverParam.rFreqOffsetTrack = (CReal) 0.0;


	/* Copy data from input to the output. Data is not modified in this
	   module */
	for (i = 0; i < iOutputBlockSize; i++)
		(*pvecOutputData)[i] = (*pvecInputData)[i];
}

void CSyncUsingPil::InitInternal(CParameter& ReceiverParam)
{
	int			i;
	_COMPLEX	cPhaseCorTermDivi;
	_REAL		rArgumentTemp;

	/* Init base class for modifying the pilots (rotation) */
	CPilotModiClass::InitRot(ReceiverParam);

	/* Init internal parameters from global struct */
	iNumCarrier = ReceiverParam.iNumCarrier;
	eCurRobMode = ReceiverParam.GetWaveMode();

	/* Check if symbol number per frame has changed. If yes, reset the
	   symbol counter */
	if (iNumSymPerFrame != ReceiverParam.iNumSymPerFrame)
	{
		/* Init internal counter for symbol number */
		iSymbCntFraSy = 0;

		/* Refresh parameter */
		iNumSymPerFrame = ReceiverParam.iNumSymPerFrame;
	}

	/* After an initialization the frame sync must be adjusted */
	bBadFrameSync = TRUE;

	/* Allocate memory for histories. Init history with large values, because
	   we search for minimum! */
	vecrCorrHistory.Init(iNumSymPerFrame, _MAXREAL);

	/* Set middle of observation interval */
	iMiddleOfInterval = iNumSymPerFrame / 2;


	/* DRM frame synchronization using impulse response, inits--------------- */
	/* Get number of pilots in first symbol of a DRM frame */
	iNumPilInFirstSym = 0;
	for (i = 0; i < iNumCarrier; i++)
		if (_IsScatPil(ReceiverParam.matiMapTab[0][i]))
			iNumPilInFirstSym++;

	/* Init vector for "test" channel estimation result */
	veccChan.Init(iNumPilInFirstSym);
	vecrTestImpResp.Init(iNumPilInFirstSym);

	/* Init plans for FFT (faster processing of Fft and Ifft commands) */
	FftPlan.Init(iNumPilInFirstSym);

	
	/* DRM frame synchronization based on time pilots, inits ---------------- */
	/* Allocate memory for storing differential complex factors. Since we do
	   not know the resulting "iNumDiffFact" we allocate memory for the
	   worst case, i.e. "iNumCarrier" */
	vecDiffFact.Init(iNumCarrier);

	/* Calculate differential complex factors for time-synchronization pilots
	   Use only first symbol of "matcPilotCells", because there are the pilots
	   for Frame-synchronization */
	iNumDiffFact = 0;
	for (i = 0; i < iNumCarrier - 1; i++)
	{
		/* Only successive pilots (in frequency direction) are used */
		if (_IsPilot(ReceiverParam.matiMapTab[0][i]) &&
			_IsPilot(ReceiverParam.matiMapTab[0][i + 1]))
		{
			/* Store index of first pilot of the couple */
			vecDiffFact[iNumDiffFact].iNumCarrier = i;

			/* Calculate phase correction term. This term is needed, because the
			   desired position of the main peak (line of sight) is the middle
			   of the guard-interval */
			rArgumentTemp = (CReal) 2.0 * crPi / ReceiverParam.iFFTSizeN *
				ReceiverParam.iGuardSize / 2;
			cPhaseCorTermDivi =
				CComplex(Cos(rArgumentTemp), -Sin(rArgumentTemp));

			/* Calculate differential factor */
			vecDiffFact[iNumDiffFact].cDiff = 
				ReceiverParam.matcPilotCells[0][i + 1] /
				ReceiverParam.matcPilotCells[0][i] * cPhaseCorTermDivi;

			iNumDiffFact++;
		}
	}


	/* Frequency offset estimation ------------------------------------------ */
	/* Get position of frequency pilots */
	int iFreqPilCount = 0;
	int iAvPilPos = 0;
	for (i = 0; i < iNumCarrier - 1; i++)
	{
		if (_IsFreqPil(ReceiverParam.matiMapTab[0][i]))
		{
			/* For average frequency pilot position to DC carrier */
			iAvPilPos += i + ReceiverParam.iCarrierKmin;
			
			iPosFreqPil[iFreqPilCount] = i;
			iFreqPilCount++;
		}
	}

	/* Average distance of the frequency pilots from the DC carrier. Needed for
	   corrections for sample rate offset changes. Normalized to sample rate! */
	rAvFreqPilDistToDC =
		(CReal) iAvPilPos / NUM_FREQ_PILOTS / ReceiverParam.iFFTSizeN;

	/* Init memory for "old" frequency pilots */
	for (i = 0; i < NUM_FREQ_PILOTS; i++)
		cOldFreqPil[i] = CComplex((CReal) 0.0, (CReal) 0.0);
	
	/* Nomalization constant for frequency offset estimation */
	rNormConstFOE =
		(CReal) 1.0 / ((CReal) 2.0 * crPi * ReceiverParam.iSymbolBlockSize);

	/* Init time constant for IIR filter for frequency offset estimation */
	rLamFreqOff = IIR1Lam(TICONST_FREQ_OFF_EST, (CReal) SOUNDCRD_SAMPLE_RATE /
		ReceiverParam.iSymbolBlockSize);

	/* Init vector for averaging the frequency offset estimation */
	cFreqOffVec = CComplex((CReal) 0.0, (CReal) 0.0);

	/* Init value for previous estimated sample rate offset with the current
	   setting. This can be non-zero if, e.g., an initial sample rate offset
	   was set by command line arguments */
	rPrevSamRateOffset = ReceiverParam.rResampleOffset;


#ifdef USE_SAMOFFS_TRACK_FRE_PIL
	/* Inits for sample rate offset estimation algorithm -------------------- */
	/* Init memory for actual phase differences */
	for (i = 0; i < NUM_FREQ_PILOTS; i++)
		cFreqPilotPhDiff[i] = CComplex((CReal) 0.0, (CReal) 0.0);

	/* Init time constant for IIR filter for sample rate offset estimation */
	rLamSamRaOff = IIR1Lam(TICONST_SAMRATE_OFF_EST,
		(CReal) SOUNDCRD_SAMPLE_RATE / ReceiverParam.iSymbolBlockSize);
#endif


	/* Define block-sizes for input and output */
	iInputBlockSize = iNumCarrier;
	iOutputBlockSize = iNumCarrier;
}

void CSyncUsingPil::StartAcquisition()
{
	/* Init internal counter for symbol number */
	iSymbCntFraSy = 0;

	/* Reset correlation history */
	vecrCorrHistory.Reset(_MAXREAL);

	bAquisition = TRUE;
}
