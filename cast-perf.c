/******************************************************************************/
/*                                                                            */
/*          Code for measuring the performance of open channel SSDs.          */
/*        This may slow down the program or require additional memory.        */
/*               For more information, please visit our github                */
/*                  https://github.com/Choe-Jongin/cast-perf                  */
/*              copyrightⓒ 2022 All rights reserved by CAST lab              */
/*                                                                            */
/******************************************************************************/

#include <linux/jiffies.h>	//time
#include <linux/delay.h>
#include <linux/timer.h>	//msleep

#include "cast-perf.h"
#include "pblk.h"

/* preparing measurment */
void CAST_PERF_INIT(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf->create_data_file(pblk);
	pblk->c_perf->unit_time = 1000;
	pblk->c_perf->init_time = get_jiffies_64()*1000/HZ;
	pblk->c_perf->next_time = 0;
	pblk->c_perf->reset_count(pblk);

	//start logging
	pblk->c_perf->active = 1;

	//flush thread start
	pblk->c_perf->thead = (struct task_struct*)kthread_run((pblk->c_perf->flush_thread), 
		pblk, "flush_thread");
	printk(KERN_ALERT "[  CAST  ] - flushing thread of %s is run\n", pblk->disk->disk_name);
}

/* open data file */
int CAST_CREATE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	char filename[256];

	// set file name
	sprintf(filename,"/pblk-cast_perf/%s.data",pblk->disk->disk_name);

	/*open the file */
	pblk->c_perf->data_file = filp_open(filename, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(pblk->c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", 
			filename, PTR_ERR(pblk->c_perf->data_file));
		return -1;
	}

	return 0;
}
/* close data file */
int CAST_CLOSE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	if (IS_ERR(pblk->c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	filp_close(pblk->c_perf->data_file, NULL);
	printk(KERN_ALERT "[  CAST  ] - End measuring %s\n", pblk->disk->disk_name);

	//end logging
	pblk->c_perf->active = 0;
	// kthread_stop(pblk->c_perf->thead);
	printk(KERN_ALERT "[  CAST  ] - stop frushing thread %s\n", pblk->disk->disk_name);
	kfree(pblk->c_perf);
	printk(KERN_ALERT "[  CAST  ] - free perf pointer %s\n", pblk->disk->disk_name);
	return 0;
}

/* memory -> data file */
int CAST_WRITE_TO_DATA_FILE(void *private, int time)
{
	struct pblk *pblk = private;
	char str[256];
	int read_io, write_io;						// IOPS
	int read_size, write_size;					// Bandwidth
	int read_size_per_io, write_size_per_io;	// IO size(maybe block size?)

	read_io 			= (int)(pblk->c_perf->read);
	read_size 			= (int)(pblk->c_perf->read_size>>13);
	read_size_per_io 	= (int)(pblk->c_perf->read/pblk->c_perf->read_size);

	write_io 			= (int)(pblk->c_perf->write);
	write_size 			= (int)(pblk->c_perf->write_size>>13);
	write_size_per_io 	= (int)(pblk->c_perf->write/pblk->c_perf->write_size);

	if (IS_ERR(pblk->c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	sprintf(str,"%5d.%03ds\t[r: %5d %7d KiB %5dByte/IO] [w: %5d %7d KiB %5dByte/IO]\n",
												time/1000, time%1000,
												read_io,  read_size,  read_size_per_io,
												write_io, write_size, write_size_per_io);
	vfs_write(pblk->c_perf->data_file, str, strlen(str), &pblk->c_perf->data_file->f_pos);

	// printk(KERN_ALERT "[  CAST  ] - %d : write to %s.data\n", time, pblk->disk->disk_name);
	return 0;
}

/* Call CAST_WRITE_TO_DATA_FILE() periodically */
int CAST_FLUSH_DATA_TO_FILE_THREAD(void *private)
{
	struct pblk *pblk = private;
	long now = 0;
	int fr_to_ms = 1000/HZ;
	while (pblk->c_perf->active == 1)
	{	
		now = get_jiffies_64()*fr_to_ms - pblk->c_perf->init_time;
		if (now >= pblk->c_perf->next_time)
		{
			if( pblk->c_perf->write_in_data_file(pblk, now) != 0 )
				return 1;
			pblk->c_perf->reset_count(pblk);
		}
		msleep(10);
	}
	return 0;
}

/* increase IOPS by read */
void CAST_INCREASE_READ(void *private, long size)
{
	struct pblk *pblk = private;
	pblk->c_perf->read++;
	pblk->c_perf->read_size	+= size;
}
/* increase IOPS by write */
void CAST_INCREASE_WRITE(void *private, long size)
{
	struct pblk *pblk = private;
	pblk->c_perf->write++;
	pblk->c_perf->write_size += size;
}
/* clear IOPS count, update the next time */
void CAST_RESET_COUNT(void *private)
{
	struct pblk *pblk = private;
	pblk->c_perf->read		= 0;
	pblk->c_perf->write		= 0;
	pblk->c_perf->read_size	= 0;
	pblk->c_perf->write_size	= 0;
	pblk->c_perf->next_time 	= pblk->c_perf->next_time + pblk->c_perf->unit_time;
}

/* Creator for CAST_PERF */
struct cast_perf * new_cast_perf()
{
	struct cast_perf * perf;
	perf = (struct cast_perf *)kmalloc(sizeof(struct cast_perf ), GFP_KERNEL);
    if(perf == NULL)
    	return perf;
	
	// member variables
	perf->active	= 0;
	perf->unit_time	= 0;
	perf->init_time	= 0;
	perf->next_time	= 0;
	perf->read		= 0;
	perf->write		= 0;
	perf->read_size	= 0;
	perf->write_size= 0;

	// method
	perf->init 				= &CAST_PERF_INIT;
	perf->create_data_file	= &CAST_CREATE_DATA_FILE;
	perf->close_data_file	= &CAST_CLOSE_DATA_FILE;
	perf->write_in_data_file= &CAST_WRITE_TO_DATA_FILE;
	perf->flush_thread		= &CAST_FLUSH_DATA_TO_FILE_THREAD;

	perf->increase_read		= &CAST_INCREASE_READ;
	perf->increase_write	= &CAST_INCREASE_WRITE;
	perf->reset_count		= &CAST_RESET_COUNT;

	return perf;
}
