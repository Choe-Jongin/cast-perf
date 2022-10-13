/******************************************************************************/
/*                                                                            */
/*          Code for measuring the performance of open channel SSDs.          */
/*        This may slow down the program or require additional memory.        */
/*               For more information, please visit our github                */
/*                  https://github.com/Choe-Jongin/cast-perf                  */
/*              copyrightⓒ 2022 All rights reserved by CAST lab              */
/*                                                                            */
/******************************************************************************/
#ifndef CAST_PERF_H
#define CAST_PERF_H

#include <linux/module.h> 
#include <linux/kernel.h>

//file read
#include <linux/file.h>
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

#define CAST_MAX_FILE_NUM 16

typedef struct
{
	int logging = 0;		// 0:stop logging, 1:logging

	struct file *CAST_LOG_FILE;	// log file

	int	 	unit_time;		// unit time to log file write
	int 	read;			// read count per unit time
	int 	write;			// write count per unit time
	long 	start_time;	// time margin
	long 	next_time;		// next file write
}CAST_LOG_FILE;
CAST_LOG_FILE CAST_LOG_FILE[CAST_MAX_FILE_NUM];

/* increase IOPS by read */
static void CAST_INCREASE_READ();
/* increase IOPS by write */
static void CAST_INCREASE_WRIGHT();
/* clear IOPS count*/
static void CAST_RESET_COUNT();
/* start measuring */
static void CAST_MEASURING_START();
/* memory -> log file */
static void CAST_WRITE_TO_LOG_FILE();
/* make log file */
static inline int CAST_CREATE_LOG_FILE(char *device_name);
/* close log file */
static int CAST_CLOSE_LOG_FILE(char *device_name);
/* Call CAST_WRITE_TO_LOG_FILE() periodically */
static CAST_FLUSH_TO_FILE_THREAD();

#endif /* CAST_PERF_H */
