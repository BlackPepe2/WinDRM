/******************************************************************************\
 * Copyright (c) 2004
 *
 * Author(s):
 *	Francesco Lanza
 *
 * Description:
 *	bsr.h
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

#if !defined(BSR_H__INCLUDED_)
#define BSR_H__INCLUDED_


#define NO_BSR_WIND 6
#define numstredele 8

extern BOOL bsr_onscreen_arr[];
extern HWND bsrhwnd[];
extern int hasharr[];
extern string bsrcall[];

extern BOOL txbsr_onscreen_arr[];
extern HWND txbsrhwnd[];
extern int  txhasharr[];

void InitBsr(void);

void storesentfilename(string the_filename, 
					   string the_filenamenodir, 
					   int hashval);

string readbsrfile();				// reads file bsrreq.bin (RX of bsr request), returns filename
int gethash();
	
int segnobsrfile();					// returns number of segments in bsrreq.bin

void writeselsegments(int no);			// writes selected segments to tx buffer

#endif