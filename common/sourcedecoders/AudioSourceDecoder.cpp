/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *  Francesco Lanza
 *
 * Description:
 *	Audio source decoder
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
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 1111
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "AudioSourceDecoder.h"
#include "../AudioFir.h"
#include "../libs/lpc10.h"
#include "../speex/speex.h"



/* Implementation *************************************************************/
/******************************************************************************\
*/

//LPC_10
struct lpc10_e_state *es;
struct lpc10_d_state *ds;

short wavdata[LPC10_SAMPLES_PER_FRAME];
unsigned char encbytes[LPC10_BITS_IN_COMPRESSED_FRAME+2];
unsigned char chksum[30];

//SPEEX
SpeexBits encbits;
SpeexBits decbits;
void *enc_state;
void *dec_state;
int speex_frame_size;
float spinp[160];
char spoutp[20];
char spoutpb[60];
int nbBytes;

/*
* Encoder                                                                      *
\******************************************************************************/
void CAudioSourceEncoder::ProcessDataInternal(CParameter& TransmParam)
{
	int i,k;
	//short val;
	int ival;
	

	/* Here, the AAC encoder must be put in --------------------------------- */
	/* Here is the recorded data. Now, the DRM AAC encoder must be put in to
	   encode this material and generate the _BINARY output data */

	if (bIsDataService == FALSE)
	{
		// avail space:
		// 2.25khz, msc=0 1920 bits 
		// 2.25khz, msc=1 2400 bits 
		// 2.50khz, msc=0 2208 bits
		// 2.50khz, msc=1 2760 bits

		if (TransmParam.Service[0].AudioParam.eAudioCoding == CParameter::AC_LPC)
		{

			// source:
			// 19200 at 48000 sample/sec = 3200 at 8000samples/sec
			// 3200 / 180 = 18 audio blocks, 140 samples lost
			// destination:
			// 54 bits * 17 = 918 bits audio.
	

			for (i = 0; i < 17; i++) //17 blocks a 54 bits = 918 bits
			{
				int sct = i*1080;
				for (k = 0; k < LPC10_SAMPLES_PER_FRAME; k++)
				{
					ival  = (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					wavdata[k] = (float)ival / 6.0;
					//val  = (*pvecInputData)[i*1080+k*6];
					//wavdata[k] = val;
				}
				lpc10_bit_encode(wavdata,encbytes,es);
				for (k = 0; k < LPC10_BITS_IN_COMPRESSED_FRAME; k++) 
					(*pvecOutputData)[i*54+k] = encbytes[k];
			}
			for (k = 918; k < 930; k++)
				(*pvecOutputData)[k] = 0;

			//Make 30 bit Checksum, 31 iterations
			for (i = 0; i < 30; i++)
				chksum[i] = 0;
			for (k = 0; k < 31; k++)
			{
				for (i = 0; i < 30; i++)
					chksum[i] ^= (*pvecOutputData)[k*30+i]; 
			}
			for (i = 0; i < 30; i++)
				(*pvecOutputData)[930+i] = chksum[i];

			// useful size is 1920 bits
			for (i = 960; i < iOutputBlockSize; i++) //pad 960 to full
				(*pvecOutputData)[i] = 0;

		}
		if (TransmParam.Service[0].AudioParam.eAudioCoding == CParameter::AC_SPEEX)
		{

			// source:
			// 19200 at 48000 sample/sec = 3200 at 8000samples/sec
			// 3200 / 160 = 20 audio blocks, 0 byte spare
			// destination
			// 20 audio blocks * 6 bytes = 120 bytes = 960 bits


			for (i = 0; i < 20; i++) //20 blocks 
			{
				speex_bits_reset(&encbits);
				int sct = i*960;
				for (k = 0; k < 160; k++)
				{
					ival  = (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					ival += (*pvecInputData)[sct++];
					spinp[k]  = (float)ival / 6.0;
					//val  = (*pvecInputData)[i*960+k*6];
					//spinp[k] = (float)val;
				}
				speex_encode(enc_state, spinp, &encbits);
				nbBytes = speex_bits_write(&encbits, spoutp, 20);

				// 20 audio blocks, each 48 bits long = 6 bytes
				for (k = 0; k < 8; k++) 
				{
					char ak = (1 << k);
					spoutpb[k   ] = ((spoutp[0] & ak) != 0);
					spoutpb[k+ 8] = ((spoutp[1] & ak) != 0);
					spoutpb[k+16] = ((spoutp[2] & ak) != 0);
					spoutpb[k+24] = ((spoutp[3] & ak) != 0);
					spoutpb[k+32] = ((spoutp[4] & ak) != 0);
					spoutpb[k+40] = ((spoutp[5] & ak) != 0);
				}
				for (k = 0; k < 48; k++)  
					(*pvecOutputData)[i*48+k] = spoutpb[k];
			}

			//Make 15 bit Checksum, 64 iterations
			for (i = 0; i < 15; i++)
				chksum[i] = 0;
			for (k = 0; k < 64; k++)
			{
				for (i = 0; i < 15; i++)
					chksum[i] ^= (*pvecOutputData)[k*15+i]; 
			}
			for (i = 0; i < 15; i++)
				(*pvecOutputData)[960+i] = chksum[i];
					
			for (i = 975; i < iOutputBlockSize; i++) //pad 975 to full
				(*pvecOutputData)[i] = 0;
			
		}
		if (TransmParam.Service[0].AudioParam.eAudioCoding == CParameter::AC_CELP)
		{
			for (i = 0; i < iOutputBlockSize; i++) //pad 0 to full
			{
				(*pvecOutputData)[i] = 0;
			}
		}
		if (TransmParam.Service[0].AudioParam.eAudioCoding == CParameter::AC_SSTV)
		{
			int len = iOutputBlockSize - 24;
			if (bUsingTextMessage) len -= 32;

			//getbuf(len,pvecOutputData);

			//Make 24 bit Checksum
			for (i = 0; i < 24; i++) chksum[i] = 0;
			k = 0;
			while (k < len)
			{
				if (len - k <= 24)
					for (i = 0; i < len - k; i++) chksum[i] ^= (*pvecOutputData)[k+i]; 
				else
					for (i = 0; i < 24; i++) chksum[i] ^= (*pvecOutputData)[k+i]; 
				k += 24;
			}

			//Write checksum
			for (i = 0; i < 24; i++)
				(*pvecOutputData)[len+i] = chksum[i];
		}

		/* Text message application. Last four bytes in stream are written */
		if ((bUsingTextMessage == TRUE) && (iOutputBlockSize > 975+32))
		{
			/* Always four bytes for text message "piece" */
			CVector<_BINARY> vecbiTextMessBuf(
			SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR);
			
			/* Get a "piece" */
			TextMessage.Encode(vecbiTextMessBuf);

			/* Total number of bytes which are actually used. The number is
			   specified by iLenPartA + iLenPartB which is set in
			   "SDCTransmit.cpp". There is currently no "nice" solution for
			   setting these values. TODO: better solution */
			/* Padding to byte as done in SDCTransmit.cpp line 138ff */
			const int iTotByt =
				(iOutputBlockSize / SIZEOF__BYTE) * SIZEOF__BYTE;
	
			for (i = iTotByt - SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR;
				 i < iTotByt; i++)
			{
				(*pvecOutputData)[i] = vecbiTextMessBuf[i -
					(iTotByt - SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR)];
			}
		}

	}
	/* ---------------------------------------------------------------------- */




	if (bIsDataService == TRUE)
	{
// TODO: make a separate modul for data encoding
		/* Write data packets in stream */
		CVector<_BINARY> vecbiData;
		const int iNumPack = iOutputBlockSize / iTotPacketSize;
		int iPos = 0;

		for (int j = 0; j < iNumPack; j++)
		{
			/* Get new packet */
			DataEncoder.GeneratePacket(vecbiData);

			/* Put it on stream */
			for (i = 0; i < iTotPacketSize; i++)
			{
				(*pvecOutputData)[iPos] = vecbiData[i];
				iPos++;
			}
		}
	}
}

void CAudioSourceEncoder::InitInternal(CParameter& TransmParam)
{
	if (TransmParam.iNumDataService == 1)
	{
		bIsDataService = TRUE;
		iTotPacketSize = DataEncoder.Init(TransmParam);
	}
	else
	{
		bIsDataService = FALSE;
		init_lpc10_encoder_state(es);
	}
	/* Define input and output block size */
	iOutputBlockSize = TransmParam.iNumDecodedBitsMSC;
	iInputBlockSize = 38400; //37800;
}

void CAudioSourceEncoder::SetTextMessage(const string& strText)
{
	/* Set text message in text message object */
	TextMessage.SetMessage(strText);

	/* Set text message flag */
	bUsingTextMessage = TRUE;
}

void CAudioSourceEncoder::ClearTextMessage()
{
	/* Clear all text segments */
	TextMessage.ClearAllText();

	/* Clear text message flag */
	bUsingTextMessage = FALSE;
}

CAudioSourceEncoder::CAudioSourceEncoder()
{
	int tmp;
	// LPC_10
	es = create_lpc10_encoder_state();
	if (es == NULL) { printf("Couldn't allocate  encoder state.\n"); }
	init_lpc10_encoder_state(es);
	// SPEEX
	speex_bits_init(&encbits);
	enc_state = speex_encoder_init(&speex_nb_mode);
	speex_encoder_ctl(enc_state,SPEEX_GET_FRAME_SIZE,&speex_frame_size);
	tmp = 10; speex_encoder_ctl(enc_state, SPEEX_SET_COMPLEXITY, &tmp);
	tmp = 2400; speex_encoder_ctl(enc_state, SPEEX_SET_BITRATE, &tmp);
}

CAudioSourceEncoder::~CAudioSourceEncoder()
{
	// LPC_10
	destroy_lpc10_encoder_state (es);
	// SPEEX
	speex_bits_destroy(&encbits);
	speex_encoder_destroy(enc_state);
}

/******************************************************************************\
* Decoder                                                                      *
\******************************************************************************/

float oldwavsample = 0.0;
int succdecod = 0;
int errdecod = 0;
int percentage = 0;

int CAudioSourceDecoder::getdecodperc(void)
{
	double tot;
	double fact;
	tot = succdecod + errdecod;
	if (tot == 0.0) return 0;
	fact = 100.0/tot;
	percentage = (int)(succdecod*fact);
	return percentage;
}

void CAudioSourceDecoder::ProcessDataInternal(CParameter& ReceiverParam)
{
	int i,k;

	/* Check if something went wrong in the initialization routine */
	if (DoNotProcessData == TRUE)
	{
		errdecod++;
		return;
	}

	/* Text Message ***********************************************************/
	/* Total frame size depends on whether text message is used or not */
	if ((bTextMessageUsed == TRUE) && (iTotalFrameSize > 975+32))
	{
		/* Decode last for bytes of input block for text message */
		for (i = 0; i < SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR; i++)
			vecbiTextMessBuf[i] = (*pvecInputData)[iTotalFrameSize + i]; 

		TextMessage.Decode(vecbiTextMessBuf);
	}

	/* Check if LPC should not be decoded */
	if (IsLPCAudio == TRUE)
	{
		bool lpccrc = TRUE;
		iOutputBlockSize = 0;

		//Make 30 bit Checksum, 31 iterations
		for (i = 0; i < 30; i++)
			chksum[i] = 0;
		for (k = 0; k < 31; k++)
		{
			for (i = 0; i < 30; i++)
				chksum[i] ^= (*pvecInputData)[k*30+i]; 
		}
		for (i = 0; i < 30; i++)
			lpccrc &= ((*pvecInputData)[930+i] == chksum[i]);

		if (lpccrc)
		{  // 17 blocks a 54 bites encbytes 
			int j;
			float tmp;
			for (i = 0; i < 17; i++)
			{
				for (j = 0; j < 54; j++)
					encbytes[j] = (*pvecInputData)[i*54+j];
				lpc10_bit_decode(encbytes,wavdata,ds);
				for (j = 0; j < 180; j++)
				{
					short interp;
					tmp = AudFirRX(wavdata[j]);
					if (tmp >=  32700) tmp =  32700;
					if (tmp <= -32700) tmp = -32700;

					interp = (short)(0.1666*tmp+0.8334*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.3333*tmp+0.6667*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.5000*tmp+0.5000*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.6667*tmp+0.3333*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.8334*tmp+0.1666*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)tmp;
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 

					oldwavsample = tmp;
				}
				/* Add new block to output block size ("* 2" for stereo output block) */
				//iOutputBlockSize += 180 * 6;
			}
			tmp = 0;
			for (j = 0; j < 140; j++)
			{
				int j2;
				for (j2 = 0;j2 < 6; j2++)
				{
					(*pvecOutputData)[iOutputBlockSize++] = tmp; 
					(*pvecOutputData)[iOutputBlockSize++] = tmp; 
				}
			}
			
			bAudioIsOK = TRUE;
			succdecod++;
		}
		else 
		{
			bAudioIsOK = FALSE;
			iOutputBlockSize = 0;
			errdecod++;
		}
	}
	else if (IsSPEEXAudio == TRUE)
	{
		bool spxcrc = TRUE;
		int j;
		short tmp;
		int ind;
		iOutputBlockSize = 0;

		if (iTotalFrameSize >= 990)
		{
			//Make 15 bit Checksum, 64 iterations
			for (i = 0; i < 15; i++)
				chksum[i] = 0;
			for (k = 0; k < 64; k++)
			{
				for (i = 0; i < 15; i++)
					chksum[i] ^= (*pvecInputData)[k*15+i]; 
			}
			for (i = 0; i < 15; i++)
				spxcrc &= ((*pvecInputData)[960+i] == chksum[i]);
		}

		/* Reset bit extraction access */
		//(*pvecInputData).ResetBitAccess();

		// 20 audio blocks, each 48 bits long = 6 bytes
		if (spxcrc)
		{

			for (i = 0; i < 20; i++)
			{
				
				ind = i*48;
				for (j = 0; j < 48; j++)
					spoutpb[j] = (*pvecInputData)[ind+j];
				for (j = 0; j < 6; j++)
				{
					ind = j*8;
					spoutp[j]  = spoutpb[ind++];
					spoutp[j] += spoutpb[ind++] << 1;
					spoutp[j] += spoutpb[ind++] << 2;
					spoutp[j] += spoutpb[ind++] << 3;
					spoutp[j] += spoutpb[ind++] << 4;
					spoutp[j] += spoutpb[ind++] << 5;
					spoutp[j] += spoutpb[ind++] << 6;
					spoutp[j] += spoutpb[ind++] << 7;
				}
				
				//for (j = 0; j < 6; j++)
				//	spoutp[j] = (*pvecInputData).Separate(8); 
				
				try
				{
					speex_bits_read_from(&decbits, spoutp, 6);
					speex_decode(dec_state, &decbits, spinp);
				}
				catch(void)
				{
					bAudioIsOK = FALSE;
					iOutputBlockSize = 0;
					return;
				}
				for (j = 0; j < 160; j++)
				{
					short interp;
					tmp = AudFirRX(spinp[j]);
					if (tmp >=  32700) tmp =  32700;
					if (tmp <= -32700) tmp = -32700;

					interp = (short)(0.1666*tmp+0.8334*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.3333*tmp+0.6667*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.5000*tmp+0.5000*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.6667*tmp+0.3333*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)(0.8334*tmp+0.1666*oldwavsample);
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					interp = (short)tmp;
					(*pvecOutputData)[iOutputBlockSize++] = interp; 
					(*pvecOutputData)[iOutputBlockSize++] = interp; 

					oldwavsample = tmp;
				}
				/* Add new block to output block size ("* 2" for stereo output block) */
				//iOutputBlockSize += 160 * 12;
			}
			bAudioIsOK = TRUE;
			succdecod++;
		}
		else 
		{
			bAudioIsOK = FALSE;
			iOutputBlockSize = 0;
			errdecod++;
		}
	}
	else if (IsSSTVData == TRUE)
	{
		bool sstvcrc = TRUE;
		int len = iTotalFrameSize - 24;
		if (bTextMessageUsed) len -= 32;


		//Make 24 bit Checksum
		for (i = 0; i < 24; i++) chksum[i] = 0;
		k = 0;
		while (k < len)
		{
			if (len - k <= 24)
				for (i = 0; i < len - k; i++) chksum[i] ^= (*pvecOutputData)[k+i]; 
			else
				for (i = 0; i < 24; i++) chksum[i] ^= (*pvecOutputData)[k+i]; 
			k += 24;
		}
		for (i = 0; i < 24; i++)
			sstvcrc &= ((*pvecInputData)[len+i] == chksum[i]);

		if (sstvcrc)
		{
			//putbuf(len,pvecOutputData);
			bAudioIsOK = TRUE;
		}
		else
			bAudioIsOK = FALSE;
		iOutputBlockSize = 0;
	}
	else if (IsCELPAudio == TRUE)
	{
		bAudioIsOK = FALSE;
		iOutputBlockSize = 0;
	}
	else
	{
		bAudioIsOK = FALSE;
		iOutputBlockSize = 0;
	}

	if (bAudioIsOK) PostWinMessage(MS_MSC_CRC, 0);
	else if (bAudioWasOK) PostWinMessage(MS_MSC_CRC, 1);
	else PostWinMessage(MS_MSC_CRC, 2);
	bAudioWasOK = bAudioIsOK;

}

void CAudioSourceDecoder::InitInternal(CParameter& ReceiverParam)
{
/*
	Since we use the exception mechanism in this init routine, the sequence of
	the individual initializations is very important!
	Requirement for text message is "stream is used" and "audio service".
	Requirement for AAC decoding are the requirements above plus "audio coding
	is AAC"
*/
	int iCurAudioStreamID;
	int iCurSelServ;

	succdecod = 0;
	errdecod = 0;

	try
	{
		/* Init error flags and output block size parameter. The output block
		   size is set in the processing routine. We must set it here in case
		   of an error in the initialization, this part in the processing
		   routine is not being called */
		IsLPCAudio = FALSE;
		IsSPEEXAudio = FALSE;
		IsCELPAudio = FALSE;
		IsSSTVData = FALSE;
		DoNotProcessData = FALSE;
		iOutputBlockSize = 2*38400; //2*37800;

		/* Get number of total input bits for this module */
		iInputBlockSize = ReceiverParam.iNumAudioDecoderBits;

		/* Get current selected audio service */
		iCurSelServ = ReceiverParam.GetCurSelAudioService();

		/* Current audio stream ID */
		iCurAudioStreamID =
			ReceiverParam.Service[iCurSelServ].AudioParam.iStreamID;

		/* The requirement for this module is that the stream is used and the
		   service is an audio service. Check it here */
		if ((ReceiverParam.Service[iCurSelServ].
			eAudDataFlag != CParameter::SF_AUDIO) ||
			(iCurAudioStreamID == STREAM_ID_NOT_USED))
		{
			throw CInitErr(ET_ALL);
		}

		/* Init text message application ------------------------------------ */
		switch (ReceiverParam.Service[iCurSelServ].AudioParam.bTextflag)
		{
		case TRUE:
			bTextMessageUsed = TRUE;

			/* Get a pointer to the string */
			TextMessage.Init(&ReceiverParam.Service[iCurSelServ].AudioParam.
				strTextMessage);

			/* Total frame size is input block size minus the bytes for the text
			   message */
			iTotalFrameSize = iInputBlockSize -
				SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR;

			/* Init vector for text message bytes */
			vecbiTextMessBuf.Init(SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR);
			break;

		case FALSE:
			bTextMessageUsed = FALSE;

			/* All bytes are used for AAC data, no text message present */
			iTotalFrameSize = iInputBlockSize;
			break;
		}


		/* Init for decoding -------------------------------------------- */
		if (ReceiverParam.Service[iCurSelServ].AudioParam.
			eAudioCoding == CParameter::AC_LPC)
			IsLPCAudio = TRUE;
		if (ReceiverParam.Service[iCurSelServ].AudioParam.
			eAudioCoding == CParameter::AC_SPEEX)
			IsSPEEXAudio = TRUE;
		if (ReceiverParam.Service[iCurSelServ].AudioParam.
			eAudioCoding == CParameter::AC_CELP)
			IsCELPAudio = TRUE;
		if (ReceiverParam.Service[iCurSelServ].AudioParam.
			eAudioCoding == CParameter::AC_SSTV)
			IsSSTVData = TRUE;


	}

	catch (CInitErr CurErr)
	{
		switch (CurErr.eErrType)
		{
		case ET_ALL:
			/* An init error occurred, do not process data in this module */
			DoNotProcessData = TRUE;
			break;

		case ET_AAC:
			/* AAC part should not be decdoded, set flag */
			IsCELPAudio = FALSE;
			IsSPEEXAudio = FALSE;
			IsLPCAudio = FALSE;
			IsSSTVData = FALSE;
			break;

		default:
			DoNotProcessData = TRUE;
		}
	}
}

CAudioSourceDecoder::CAudioSourceDecoder()
{
	int enhon = 1;
	// LPC_10
	ds = create_lpc10_decoder_state();
	if (ds == NULL) { printf("Couldn't allocate  decoder state.\n"); }
	init_lpc10_decoder_state(ds);
	// SPEEX
	speex_bits_init(&decbits);
	dec_state = speex_decoder_init(&speex_nb_mode);
	speex_decoder_ctl(dec_state, SPEEX_SET_ENH, &enhon);
}

CAudioSourceDecoder::~CAudioSourceDecoder()
{
	// LPC_10
	destroy_lpc10_decoder_state (ds);
	// SPEEX
	speex_bits_destroy(&decbits);
	speex_decoder_destroy(dec_state);
}
