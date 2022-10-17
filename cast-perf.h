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
#include <linux/jiffies.h>	//time
#include <linux/delay.h>
#include <linux/timer.h>	//msleep
#include <linux/kthread.h>	//thread
//file read
#include <linux/file.h>
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

typedef struct cast_perf
{
	struct file * data_file;	// log file
	struct task_struct *thead; 	// flush thread

	int 	active;			// 0:stop measuring, 1:measuring
	int	 	unit_time;		// unit time to log file write
	long 	init_time;		// initial time
	long 	next_time;		// next file write
	int 	read;			// read count per unit time
	int 	write;			// write count per unit time

	// like method
	void (*init)(void *private);
	int	 (*create_data_file)(void *private);
	int  (*close_data_file)(void *private);
	int  (*write_in_data_file)(void *private, int time);
	int (*flush_thread)(void *private);

	void (*increase_read)(void *private, int size);
	void (*increase_write)(void *private, int size);
	void (*reset_count)(void *private);

}CAST_PERF;

/* Creator for CAST_PERF */
void CAST_SET_PERF(void *private);



#endif /* CAST_PERF_H */
