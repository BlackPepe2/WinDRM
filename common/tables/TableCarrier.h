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


/* Definitions ****************************************************************/
/* FAC ************************************************************************/
#define NUM_FAC_CELLS			45 //65

/* FAC positions. The two numbers are {symbol no, carrier no} */


const int iTableFACRobModA[NUM_FAC_CELLS][2] = {  
											{ 1, 10},	{ 1, 22}, { 1, 30}, { 1, 50},	 
							 				{ 2, 14},	{ 2, 26}, { 2, 34},	  
											{ 3, 18},	{ 3, 30}, { 3, 38},   
											{ 4, 22},	{ 4, 34}, { 4, 42}, 
								{ 5, 18},	{ 5, 26},	{ 5, 38}, { 5, 46},
								{ 6, 22},	{ 6, 30},	{ 6, 42}, { 6, 50},
								{ 7, 26},	{ 7, 34},	{ 7, 46},  
					{ 8, 10},	{ 8, 30},	{ 8, 38},	{ 8, 50},  
					{ 9, 14},	{ 9, 34},	{ 9, 42},	
					{10, 18},	{10, 38},	{10, 46},	 
		{11, 10},	{11, 22},	{11, 42},	{11, 50}, 
		{12, 14},	{12, 26},	{12, 46},	 
		{13, 18},	{13, 30},			 
		{14, 22},	{14, 34},		   
};

const int iTableFACRobModB[NUM_FAC_CELLS][2] = {  
											{ 0, 21},	
								{ 1, 11},	{ 1, 23}, { 1, 35}, 
							 	{ 2, 13},	{ 2, 25}, { 2, 37},  
								{ 3, 15},	{ 3, 27}, { 3, 39},  
					{ 4,  5},	{ 4, 17},	{ 4, 29}, { 4, 41},
					{ 5,  7},	{ 5, 19},	{ 5, 31},
					{ 6,  9},	{ 6, 21},	{ 6, 33},
					{ 7, 11},	{ 7, 23},	{ 7, 35},  
					{ 8, 13},	{ 8, 25},	{ 8, 37},  
					{ 9, 15},	{ 9, 27},	{ 9, 39}, 
		{10,  5},	{10, 17},	{10, 29},	{10, 41}, 
		{11,  7},	{11, 19},	{11, 31}, 
		{12,  9},	{12, 21},	{12, 33}, 
		{13, 11},	{13, 23},	{13, 35}, 
		{14, 13},	{14, 25},	{14, 37},   
};


/* Frequency pilots ***********************************************************/
#define NUM_FREQ_PILOTS			3
const int iTableFreqPilRobModA[NUM_FREQ_PILOTS][2] = {
	{ 9, 205},
	{27, 836},
	{36, 215}
};

const int iTableFreqPilRobModB[NUM_FREQ_PILOTS][2] = {
	{ 8, 331},
	{24, 651},
	{32, 555}
};

/* Spectrum occupancy, carrier numbers for each mode **************************/
const int iTableCarrierKmin[2][2] = {
	{2, 1},
	{2, 1}
};

const int iTableCarrierKmax[2][2] = {
	{54, 45}, //{102, 91},
	{58, 51}  //{114, 51}
};

