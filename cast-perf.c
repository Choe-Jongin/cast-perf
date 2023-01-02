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


void CAST_CC_UNIT_RESET(struct cast_counter * cc)
{
	cc->unit = 0;
}

/* Creator for cast_counter */
struct cast_counter *new_cast_counter()
{
	struct cast_counter * cc;
	cc = (struct cast_counter *)kmalloc(sizeof(struct cast_counter), GFP_KERNEL);
    if(cc == NULL)
    	return cc;
	
	cc->unit 	= 0;
	cc->total	= 0;

	cc->reset_unit = &CAST_CC_UNIT_RESET;
	
	return cc;
}

/**************                  *************/

/* preparing measurment */
void CAST_PERF_INIT(void *private)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	c_perf->create_data_file(pblk);
	c_perf->unit_time = 1000;
	c_perf->init_time = get_jiffies_64()*1000/HZ;
	c_perf->next_time = 0;
	c_perf->reset_count(pblk);

	//start logging
	c_perf->active = 1;

	//flush thread start
	c_perf->thead = (struct task_struct*)kthread_run((pblk->c_perf->flush_thread), 
		pblk, "flush_thread");
	printk(KERN_ALERT "[  CAST  ] - flushing thread of %s is run\n", pblk->disk->disk_name);
}

/* open data file */
int CAST_CREATE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;
	char filename[256];

	// set file name
	sprintf(filename,"/pblk-cast_perf/%s.data",pblk->disk->disk_name);

	/*open the file */
	c_perf->data_file = filp_open(filename, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", 
			filename, PTR_ERR(c_perf->data_file));
		return -1;
	}

	return 0;
}
/* close data file */
int CAST_CLOSE_DATA_FILE(void *private)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	if (IS_ERR(c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	filp_close(c_perf->data_file, NULL);
	printk(KERN_ALERT "[  CAST  ] - End measuring %s\n", pblk->disk->disk_name);

	//end logging
	c_perf->active = 0;
	// kthread_stop(pblk->c_perf->thead);
	printk(KERN_ALERT "[  CAST  ] - stop frushing thread %s\n", pblk->disk->disk_name);
	kfree(c_perf);
	printk(KERN_ALERT "[  CAST  ] - free perf pointer %s\n", pblk->disk->disk_name);
	return 0;
}

/* memory -> data file */
int CAST_WRITE_TO_DATA_FILE(void *private, int time)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	char str[256];
	int read_h, read_m, write_u, write_g;	// read_hit/miss, write_user/gc per unit
	int read, write, gc;					// read write gc per unit
	int hit_rate = 0, waf = 0;				// hit rate, waf per unit
	long total_read, total_write, total_gc, total_hit_rate = 0, total_waf = 0;	// total

	if (IS_ERR(pblk->c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}

	//from counter
	read_h		= c_perf->read_hit->unit;
	read_m		= c_perf->read_miss->unit;
	write_u		= c_perf->write_usr->unit;
	write_g		= c_perf->write_gc->unit;
	gc			= c_perf->gc->unit;

	read 	= read_h + read_m;		// sum of read
	write 	= write_u + write_g;	// sum of write

	if( read != 0 )
		hit_rate	= read_h*100 / read;
	if( write_u != 0 )
		waf			= write*100 / write_u;

	total_read 	= c_perf->read_hit->total + c_perf->read_miss->total;	// sum of total read
	total_write	= c_perf->write_usr->total + c_perf->write_gc->total;	// sum of total write
	total_gc	= c_perf->gc->total;

	if( total_read != 0 )
		total_hit_rate	= c_perf->read_hit->total*100 / total_read;
	if( c_perf->write_usr->total != 0 )
		total_waf		= total_write*100 / c_perf->write_usr->total;

	//            [time  read write gc] [Detail r:hit/sum(rate) w:sum/usr(WAF)] [TOTAL r(rate) w(WAF) gc]
	sprintf(str, "[ %5d.%03ds %7d %7d %4d ]\t[ Detail r:%d/%d(%d) w:%d/%d(%d) ]\t[ TOTAL %ld(%d) %ld(%d) %ld ]\n",
		time/1000, time%1000, read, write, gc,
		read_h, read_m, hit_rate, write_u, write_g, waf,
		total_read, total_hit_rate, total_write, total_waf, total_gc
	);
	vfs_write(pblk->c_perf->data_file, str, strlen(str), &pblk->c_perf->data_file->f_pos);

	return 0;
}

/* Call CAST_WRITE_TO_DATA_FILE() periodically */
int CAST_FLUSH_DATA_TO_FILE_THREAD(void *private)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	long now = 0;
	int fr_to_ms = 1000/HZ;
	while (c_perf->active == 1)
	{	
		now = get_jiffies_64()*fr_to_ms - c_perf->init_time;
		if (now >= c_perf->next_time)
		{
			if( c_perf->write_in_data_file(pblk, now) != 0 )
				return 1;
			c_perf->reset_count(pblk);
		}
		msleep(HZ/10);
	}
	return 0;
}

/* clear IOPS count, update the next time */
void CAST_INCREASE_COUNT(void *private, struct cast_counter *cc, int size)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	cc->unit += size;
	cc->total += size;
}

/* clear IOPS count, update the next time */
void CAST_RESET_COUNT(void *private)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	c_perf->read_hit->	reset_unit(c_perf->read_hit);
	c_perf->read_miss->	reset_unit(c_perf->read_miss);
	c_perf->write_usr->	reset_unit(c_perf->write_usr);
	c_perf->write_gc->	reset_unit(c_perf->write_gc);
	c_perf->gc->		reset_unit(c_perf->gc);

	c_perf->next_time	= c_perf->next_time + c_perf->unit_time;
}

/* Creator for CAST_PERF */
struct cast_perf * new_cast_perf()
{
	struct cast_perf * perf;
	perf = (struct cast_perf *)kmalloc(sizeof(struct cast_perf ), GFP_KERNEL);
    if(perf == NULL)
    	return perf;
	
	// member variables
	perf->active		= 0;
	perf->unit_time		= 0;
	perf->init_time		= 0;
	perf->next_time		= 0;

	//counters
	perf->read_hit	= new_cast_counter();
	perf->read_miss	= new_cast_counter();
	perf->write_usr	= new_cast_counter();
	perf->write_gc	= new_cast_counter();
	perf->gc	 	= new_cast_counter();

	// method
	perf->init 				= &CAST_PERF_INIT;
	perf->create_data_file	= &CAST_CREATE_DATA_FILE;
	perf->close_data_file	= &CAST_CLOSE_DATA_FILE;
	perf->write_in_data_file= &CAST_WRITE_TO_DATA_FILE;
	perf->flush_thread		= &CAST_FLUSH_DATA_TO_FILE_THREAD;

	perf->inc_count 		= &CAST_INCREASE_COUNT;
	perf->reset_count 		= &CAST_RESET_COUNT;

	return perf;
}