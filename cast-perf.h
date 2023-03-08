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
#include <linux/ktime.h>

#include <linux/string.h>
#include <linux/kthread.h>	//thread
//file read
#include <linux/file.h>
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

typedef struct cast_counter
{
	long unit;
	int num;
	long total;

	int (*avg)(struct cast_counter * cc);			// get avg(unit/num)
	void (*reset_unit)(struct cast_counter * cc);	// clear unit, num
}CastCounter;

#define MAX_LATENCY_CHUNK 256
typedef struct cast_perf
{
	struct cast_perf_mgr * mgr;

	struct file * data_file;	// data by file
	struct task_struct *thead; 	// flush thread

	struct cast_counter *read_hit;
	struct cast_counter *read_miss;
	struct cast_counter *write_usr;
	struct cast_counter *write_gc;
	struct cast_counter *gc;

	int latency[MAX_LATENCY_CHUNK];

	// like method
	/* start measurment. unit_time(ms) */
	void (*init)(void *private);
	int	 (*create_data_file)(void *private);
	int  (*close_data_file)(void *private);
	int  (*write_in_data_file)(void *private, int time);

	void (*inc_count)(void *private, struct cast_counter *cc, long size);
	void (*add_latency)(struct cast_perf *this, long time);
	void (*reset_count)(void *private);
}CastPerf;

/* Creator for Cast_perf */
struct cast_perf *new_cast_perf(void);
/* Creator for cast_counter */
struct cast_counter *new_cast_counter(void);

/********   CPU   *******/
#define MAX_STAT_BUF 1024
typedef struct cast_cpu_stat
{
	int nproc;
	long user, nice, syst, idle, wait, irq, soft, steal, guest;	// /proc/stat values

	void (*get_current_stat)(struct cast_cpu_stat *this);
} CastCpuStat;
struct cast_cpu_stat * new_cast_cpu_stat(int core);

typedef struct cast_perf_mgr
{
	struct task_struct *thread; 	// flush thread
	struct file * data_file;	// data by file

	void *pblk_vec[32];		// pblk vector
	int nr_pblk;			// number of pblk
	int is_now_flushing;	// flushing

	struct cast_cpu_stat *elapsed_stat;
	struct cast_cpu_stat *current_stat;

	int 	active;			// 0:stop measuring, 1:measuring
	int	 	unit_time;		// unit time to log file write
	long 	init_time;		// initial time
	long 	next_time;		// next file write

	void (*set_active)		(struct cast_perf_mgr *this, int active);
	void (*reset)			(struct cast_perf_mgr *this);
	int  (*flush)			(struct cast_perf_mgr *this, long time);
	int  (*add_pblk)		(struct cast_perf_mgr *this, void *pblk);
	int  (*pop_pblk)		(struct cast_perf_mgr *this, void *pblk);
	int  (*flush_thread)	(void *data);
} CastPerfManager;
/* Manager implemented in a singleton pattern*/
extern int manager_allocated;
struct cast_perf_mgr *get_cast_perf_mgr(void*pblk);
/* 
Cpu user system nice idle wait hi si zero
CPU: CPU core의 번호.
User: user mode에서의 실행 시간
System: system mode에서의 실행 시간
Nice: 낮은 권한의 user mode에서의 실행 시간
Idle: I/O완료가 아닌 대기 시
Wait: I/O 완료 대기 시간
Hi: Hard Interrupt(IRQ)
Si: Soft Interrupt(SoftIRQ)
Zero: 끝
*/
#endif /* CAST_PERF_H */
