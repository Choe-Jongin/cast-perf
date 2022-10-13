/******************************************************************************/
/*                                                                            */
/*          Code for measuring the performance of open channel SSDs.          */
/*        This may slow down the program or require additional memory.        */
/*               For more information, please visit our github                */
/*                  https://github.com/Choe-Jongin/cast-perf                  */
/*              copyrightⓒ 2022 All rights reserved by CAST lab              */
/*                                                                            */
/******************************************************************************/

#include <linux/string.h>
#include <linux/jiffies.h>	//time

#include "cast-perf.h"

/* increase IOPS by read */
static inline void CAST_INCREASE_READ()
{
	++CAST_READ;
}
/* increase IOPS by write */
static inline void CAST_INCREASE_WRIGHT()
{
	++CAST_WRITE;
}
/* clear IOPS count*/
static inline void CAST_RESET_COUNT()
{
	CAST_READ = 0;
	CAST_WRITE = 0;
}

/* start time */
static inline void CAST_SET_START_TIME()
{
	CAST_RESET_COUNT();	
	CAST_UNIT_TIME = 100;
	CAST_START_TIME = get_jiffies_64();
	CAST_NEXT_TIME = CAST_START_TIME + HZ*(CAST_UNIT_TIME/100);

	//start logging
	CAST_LOGGING = 1;
}
static inline void CAST_WRITE_TO_LOG_FILE()
{

}

static inline int CAST_CREATE_LOG_FILE(char *device_name)
{
	char *log_filename[256];

	// set file name
	sprintf(log_filename,"/pblk-log/%s.log",device_name);

	/*open the file */
	CAST_LOG_FILE = filp_open(log_filename, O_WRONLY, 0);
	if (IS_ERR(fp))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", log_filename, PTR_ERR(fp));
		return -1;
	}

	return 0;
}

static inline int CAST_CLOSE_LOG_FILE(char *device_name)
{

	printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", log_filename, PTR_ERR(fp));
	filp_close(CAST_LOG_FILE, NULL);

	//end logging
	CAST_LOGGING = 0;
	return 0;
}

/* memory -> log file */
static inline CAST_FLUSH_TO_FILE_THREAD()
{

}
