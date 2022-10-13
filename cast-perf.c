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


/* Creator for CAST_PERF */
void CAST_SET_PERF(struct pblk *pblk)
{
	// member variables
	pblk->c_perf.active		= 0;
	pblk->c_perf.unit_time	= 0;
	pblk->c_perf.next_time	= 0;
	pblk->c_perf.read		= 0;
	pblk->c_perf.write		= 0;

	// method
	pblk->c_perf.init 				= &CAST_PERF_INIT
	pblk->c_perf.create_data_file	= &CAST_CREATE_DATA_FILE
	pblk->c_perf.close_data_file	= &CAST_CLOSE_DATA_FILE
	pblk->c_perf.write_in_data_file	= &CAST_WRITE_TO_DATA_FILE
	pblk->c_perf.flush_thread		= &CAST_FLUSH_DATA_TO_FILE_THREAD

	pblk->c_perf.increase_read		= &CAST_INCREASE_READ
	pblk->c_perf.increase_write		= &CAST_INCREASE_WRIGHT
	pblk->c_perf.reset_count		= &CAST_RESET_COUNT
}

/* preparing measurment */
void CAST_PERF_INIT(struct pblk *pblk)
{
	pblk->c_perf.create_data_file(pblk);
	pblk->c_perf.next_time = get_jiffies_64();
	pblk->c_perf.reset_count(pblk);

	//start logging
	pblk->c_perf.active = 1;

	//flush thread start
	pblk->c_perf.flush_thread(pblk);
}

/* open data file */
int CAST_CREATE_DATA_FILE(struct pblk *pblk)
{
	char filename[256];

	// set file name
	sprintf(filename,"/pblk-cast_perf/%s.data",pblk->disk->name);

	/*open the file */
	pblk->c_perf.data_file = filp_open(filename, O_WRONLY, 0);
	if (IS_ERR(fp))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", filename, PTR_ERR(fp));
		return -1;
	}

	return 0;
}
/* close data file */
int CAST_CLOSE_DATA_FILE(struct pblk *pblk)
{
	if (IS_ERR(fp))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->name);
		return -1;
	}

	filp_close(pblk->c_perf.data_file, NULL);
	printk(KERN_ALERT "[  CAST  ] - end measuring %s\n", pblk->disk->name);

	//end logging
	pblk->c_perf.active = 0;
	return 0;
}

/* memory -> data file */
int CAST_WRITE_TO_DATA_FILE(struct pblk *pblk, int time)
{
	char str[256];
	if (IS_ERR(fp))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->name);
		return -1;
	}

	sprintf(str,"%8d \t%8d \t%8d\n", time, pblk->c_perf.read, pblk->c_perf.write);
	vfs_write(pblk->c_perf.data_file, str, strlen(str), &pblk->c_perf.data_file->f_pos);
}

/* Call CAST_WRITE_TO_DATA_FILE() periodically */
void CAST_FLUSH_DATA_TO_FILE_THREAD(struct pblk *pblk)
{
	long now = get_jiffies_64();
	while (pblk->c_perf.active == 1)
	{
		if (now < pblk->c_perf.next_time)
		{
			pblk->c_perf.write_in_data_file(pblk, (int)(now / HZ))
		}
		msleep(pblk->c_perf.unit_time/10);
	}
}

/* increase IOPS by read */
void CAST_INCREASE_READ(struct pblk *pblk)
{
	pblk->c_perf.read++;
}
/* increase IOPS by write */
void CAST_INCREASE_WRIGHT(struct pblk *pblk)
{
	pblk->c_perf.write++;
}
/* clear IOPS count, update the next time */
void CAST_RESET_COUNT(struct pblk *pblk)
{
	pblk->c_perf.read	= 0;
	pblk->c_perf.write	= 0;
	pblk->c_perf.next_time = pblk->c_perf.next_time  + HZ*(pblk->c_perf.uint_time/100);
}