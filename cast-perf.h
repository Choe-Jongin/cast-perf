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

#include <linux/string.h>
#include <linux/kthread.h>	//thread
//file read
#include <linux/file.h>
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

typedef struct cast_counter
{
	int unit;
	long total;

	void (*reset_unit)(struct cast_counter * cc);
}Cast_counter;

typedef struct cast_perf
{
	struct file * data_file;	// data by file
	struct task_struct *thead; 	// flush thread

	int 	active;			// 0:stop measuring, 1:measuring
	int	 	unit_time;		// unit time to log file write
	long 	init_time;		// initial time
	long 	next_time;		// next file write

	struct cast_counter *read_hit;
	struct cast_counter *read_miss;
	struct cast_counter *write_usr;
	struct cast_counter *write_gc;
	struct cast_counter *gc;

	// like method
	void (*init)(void *private);
	int	 (*create_data_file)(void *private);
	int  (*close_data_file)(void *private);
	int  (*write_in_data_file)(void *private, int time);
	int  (*flush_thread)(void *private);

	void (*inc_count)(void *private, struct cast_counter *cc, int size);
	void (*reset_count)(void *private);
}Cast_perf;

/* Creator for Cast_perf */
struct cast_perf *new_cast_perf(void);

/* Creator for cast_counter */
struct cast_counter *new_cast_counter(void);

#endif /* CAST_PERF_H */
