/*
 * sdcall.c
 *
 *  Created on: Nov 26, 2025
 *      Author: kantaridis
 */

/* Includes ------------------------------------------------------------------*/
/* BEGIN Includes */
#include "main.h"
#include "fatfs.h"
#include "stdio.h"
#include "sdmmc.h"
#include "sdcall.h"
/* END Includes */


/**
*******************************************************************************
* @brief WriteData to SDCard
*******************************************************************************
*/
void changeLine(FIL *_fil){

	f_puts("\n", _fil);
	return;
}
static char tmp[30];

void writeStrField(FIL *_fil, char *val){
//	static char tmp[20];
	sprintf(tmp, "%s,", val);
	f_puts(tmp, _fil);
//	vPortFree(&tmp);
	return;
}

void writeIntField(FIL *_fil, int val){
//	static char tmp[20];
	sprintf(tmp, "%d,", val);
	f_puts(tmp, _fil);
//	vPortFree(&tmp);
	return;
}

void writeFloatField(FIL *_fil, float val){
//	static char tmp[20];
	sprintf(tmp, "%.3f,", val);
	f_puts(tmp, _fil);
//	vPortFree(&tmp);
	return;
}

void writeLongFloatField(FIL *_fil, float val){
//	static char tmp[20];
	sprintf(tmp, "%.7f,", val);
	f_puts(tmp, _fil);
//	vPortFree(&tmp);
	return;
}
void writeLongField(FIL *_fil, long val){
//	static char tmp[20];
	sprintf(tmp, "%ld,", val);
	f_puts(tmp, _fil);
//	vPortFree(&tmp);
	return;
}
/* Files init function */
void filesInit(FIL *_fastWriteFile, FIL *_slowWriteFile){
	writeStrField(_fastWriteFile, "Timestamp");

	writeStrField(_fastWriteFile, "Error");
	writeStrField(_fastWriteFile, "OK");

	changeLine(_slowWriteFile);
	f_sync(_slowWriteFile);

}


/**
*******************************************************************************
* @brief  Fast Files update function.
* @retval none
*******************************************************************************
*/
void SDCardUpdateFastFile(FIL *_fastWriteFile, TrialData _data){

	//Timestamp
	writeIntField(_fastWriteFile, uwTick);

	//Trial data
	writeIntField(_fastWriteFile, data.errorFlag);
	writeIntField(_fastWriteFile, data.counterFlag);
	changeLine(_fastWriteFile);
}


/**
*******************************************************************************
* @brief  Slow Files update function.
* @retval none
*******************************************************************************
*/
//char DateAndTimeString[40];
void SDCardUpdateSlowFile(FIL *_slowWriteFile, TrialData _data){
	  //Timestamp
	  writeIntField(_slowWriteFile, uwTick);

//	  //GPS Timestamp
//	  sprintf(DateAndTimeString, "%2d:%2d:%4d %2d:%2d:%2d", _dashboardDataLogger.gnss.day,
//			  	  	  	  	  	  	  	  	  	  	  _dashboardDataLogger.gnss.month,
//													  _dashboardDataLogger.gnss.year,
//													  _dashboardDataLogger.gnss.hour,
//													  _dashboardDataLogger.gnss.min,
//			  	  	 	  	  	  	  	  	  	  	  _dashboardDataLogger.gnss.sec);
      //Trial Data
	  writeIntField(_slowWriteFile, data.errorFlag);
	  writeIntField(_slowWriteFile, data.counterFlag);
	  changeLine(_slowWriteFile);

}


