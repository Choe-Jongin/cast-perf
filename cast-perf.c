/******************************************************************************/
/*                                                                            */
/*          Code for measuring the performance of open channel SSDs.          */
/*        This may slow down the program or require additional memory.        */
/*               For more information, please visit our github                */
/*                  https://github.com/Choe-Jongin/cast-perf                  */
/*              copyrightⓒ 2022 All rights reserved by CAST lab              */
/*                                                                            */
/******************************************************************************/

#include "cast-perf.h"

/**************** CAST ***************/

/* preparing measurment */
void CAST_PERF_INIT(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf.create_data_file(pblk);
	pblk->c_perf.next_time = get_jiffies_64();
	pblk->c_perf.reset_count(pblk);

	//start logging
	pblk->c_perf.active = 1;

	//flush thread start
	printk(KERN_ALERT "[  CAST  ] - flush thread of %s is run\n", pblk->disk->disk_name);
	pblk->c_perf.thead = (struct task_struct*)kthread_run((pblk->c_perf.flush_thread), 
		pblk, "flush_thread");
	printk(KERN_ALERT "[  CAST  ] - %s is start\n", pblk->disk->disk_name);
}

/* open data file */
int CAST_CREATE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	char filename[256];

	// set file name
	sprintf(filename,"/pblk-cast_perf/%s.data",pblk->disk->disk_name);

	/*open the file */
	pblk->c_perf.data_file = filp_open(filename, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(pblk->c_perf.data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", 
			filename, PTR_ERR(pblk->c_perf.data_file));
		return -1;
	}

	return 0;
}
/* close data file */
int CAST_CLOSE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	if (IS_ERR(pblk->c_perf.data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	filp_close(pblk->c_perf.data_file, NULL);
	printk(KERN_ALERT "[  CAST  ] - End measuring %s\n", pblk->disk->disk_name);

	//end logging
	pblk->c_perf.active = 0;
	kthread_stop(pblk->c_perf.thead);
	return 0;
}

/* memory -> data file */
int CAST_WRITE_TO_DATA_FILE(void *private, int time)
{
	struct pblk *pblk = private;
	char str[256];
	if (IS_ERR(pblk->c_perf.data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	sprintf(str,"%8d \t%8d \t%8d\n", time, pblk->c_perf.read, pblk->c_perf.write);
	vfs_write(pblk->c_perf.data_file, str, strlen(str), &pblk->c_perf.data_file->f_pos);
}

/* Call CAST_WRITE_TO_DATA_FILE() periodically */
int CAST_FLUSH_DATA_TO_FILE_THREAD(void *private)
{
	struct pblk *pblk = private;
	long now = get_jiffies_64();
	while (pblk->c_perf.active == 1)
	{
		if (now < pblk->c_perf.next_time)
		{
			pblk->c_perf.write_in_data_file(pblk, (int)(now / HZ));
		}
		msleep(pblk->c_perf.unit_time/10);
	}

	return 0;
}

/* increase IOPS by read */
void CAST_INCREASE_READ(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf.read++;
}
/* increase IOPS by write */
void CAST_INCREASE_WRIGHT(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf.write++;
}
/* clear IOPS count, update the next time */
void CAST_RESET_COUNT(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf.read	= 0;
	pblk->c_perf.write	= 0;
	pblk->c_perf.next_time = pblk->c_perf.next_time  + (HZ*pblk->c_perf.unit_time)/1000;
}

/* Creator for CAST_PERF */
void CAST_SET_PERF(void *private)
{
	struct pblk *pblk = private;
	// member variables
	pblk->c_perf.active		= 0;
	pblk->c_perf.unit_time	= 0;
	pblk->c_perf.next_time	= 0;
	pblk->c_perf.read		= 0;
	pblk->c_perf.write		= 0;

	// method
	pblk->c_perf.init 				= &CAST_PERF_INIT;
	pblk->c_perf.create_data_file	= &CAST_CREATE_DATA_FILE;
	pblk->c_perf.close_data_file	= &CAST_CLOSE_DATA_FILE;
	pblk->c_perf.write_in_data_file	= &CAST_WRITE_TO_DATA_FILE;
	pblk->c_perf.flush_thread		= &CAST_FLUSH_DATA_TO_FILE_THREAD;

	pblk->c_perf.increase_read		= &CAST_INCREASE_READ;
	pblk->c_perf.increase_write		= &CAST_INCREASE_WRIGHT;
	pblk->c_perf.reset_count		= &CAST_RESET_COUNT;
}
/**************** CAST END ***************/