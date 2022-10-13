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

typedef struct cast_perf
{
	struct file * data_file;	// log file

	int 	active;			// 0:stop measuring, 1:measuring
	int	 	unit_time;		// unit time to log file write
	long 	next_time;		// next file write
	int 	read;			// read count per unit time
	int 	write;			// write count per unit time

	// like method
	void (*init)(struct pblk *pblk);
	int	 (*create_data_file)(struct pblk *pblk);
	int  (*close_data_file)(struct pblk *pblk);
	int  (*write_in_data_file)(struct pblk *pblk, int time);
	void (*flush_thread)(struct pblk *pblk);

	void (*increase_read)(struct pblk *pblk);
	void (*increase_write)(struct pblk *pblk);
	void (*reset_count)(struct pblk *pblk);

}CAST_PERF;


/* Creator for CAST_PERF */
void CAST_SET_PERF(struct pblk *pblk));


/*  TODO:remove this declarations  */

// /* preparing measurment */
// void CAST_PERF_INIT(struct pblk *pblk);
// /* start measuring */
// void CAST_MEASURING_START(struct pblk *pblk);
// /* memory -> data file */
// int CAST_WRITE_TO_DATA_FILE(struct pblk *pblk, int time);
// /* open data file */
// int CAST_CREATE_DATA_FILE(struct pblk *pblk);
// /* close data file */
// int CAST_CLOSE_DATA_FILE(struct pblk *pblk);
// /* Call CAST_WRITE_TO_DATA_FILE() periodically */
// void CAST_FLUSH_DATA_TO_FILE_THREAD(struct pblk *pblk);

// /* increase IOPS by read */
// void CAST_INCREASE_READ(struct pblk *pblk);
// /* increase IOPS by write */
// void CAST_INCREASE_WRIGHT(struct pblk *pblk);
// /* clear IOPS count*/
// void CAST_RESET_COUNT(struct pblk *pblk);

#endif /* CAST_PERF_H */
