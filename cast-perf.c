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

int atoi(const char *s)
{
	int n;
	unsigned char sign = 0;

	if (*s == '-')
	{
		sign = 1;
		s++;
	}
	else if (*s == '+')
		s++;

	n = 0;
	while ('0' <= *s && *s <= '9')
		n = n * 10 + *s++ - '0';
	return sign ? -n : n;
}

int CAST_CC_AVG(struct cast_counter * cc)
{
	if( cc->num == 0 )
		return 0;
	return (int)(cc->unit/cc->num);
}

void CAST_CC_UNIT_RESET(struct cast_counter * cc)
{
	cc->unit = 0;
	cc->num = 0;
}

/* Creator for cast_counter */
struct cast_counter *new_cast_counter()
{
	struct cast_counter * cc;
	cc = (struct cast_counter *)kmalloc(sizeof(struct cast_counter), GFP_KERNEL);
    if(cc == NULL)
    	return cc;
	
	cc->unit 	= 0;
	cc->num 	= 0;
	cc->total	= 0;

	cc->avg 		= &CAST_CC_AVG;
	cc->reset_unit 	= &CAST_CC_UNIT_RESET;
	
	return cc;
}

/**************                  *************/

/* start measurment */
void CAST_PERF_INIT(void *private, int unit_time)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	c_perf->create_data_file(pblk);
	c_perf->reset_count(pblk);
	
	c_perf->mgr = get_cast_perf_mgr(pblk);

	//flush thread start
	printk(KERN_ALERT "[  CAST  ] - make cast_perf of %s \n", pblk->disk->disk_name);
	printk(KERN_ALERT "[  CAST  ] - Manager : %p\n", c_perf->mgr);
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
	c_perf->data_file = filp_open(filename, O_TRUNC | O_WRONLY | O_CREAT, 0);
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
	int rlat, wlat, num_dev_r, num_dev_w;
	int page_to_kbytes = 4096/1024;

	if( private == NULL )
		return -1;

	//from counter
	read_h		= c_perf->read_hit->unit*page_to_kbytes;
	read_m		= c_perf->read_miss->unit*page_to_kbytes;
	write_u		= c_perf->write_usr->unit*page_to_kbytes;
	write_g		= c_perf->write_gc->unit*page_to_kbytes;
	gc			= c_perf->gc->unit*page_to_kbytes;
	
	rlat 		= c_perf->rlat_dev->avg(c_perf->rlat_dev);
	wlat 		= c_perf->wlat_dev->avg(c_perf->wlat_dev);
	
	num_dev_r 	= c_perf->rlat_dev->num;
	num_dev_w 	= c_perf->wlat_dev->num;

	read 	= read_h + read_m;		// sum of read
	write 	= write_u + write_g;	// sum of write

	if( read != 0 )
		hit_rate	= read_h*100 / read;
	if( write_u != 0 )
		waf			= write*100 / write_u;

	//            [time  read write_u gc] [Detail r:hit/sum(rate) w:sum/usr(WAF)]
	sprintf(str, "[ %5d.%03ds %7d %7d %4d ]\t[ Detail r:%d/%d(%d) w:%d/%d(%d) ]\t[ Latency %d %d (%d %d)]\n",
		time/1000, time%1000, read, write_u, gc,
		read_h, read_m, hit_rate, write, write_u, waf,
		rlat, wlat, num_dev_r, num_dev_w
	);

	if (IS_ERR(pblk->c_perf->data_file))
	{
		printk(KERN_ALERT "[  CAST  ] - data file for %s is not opened\n", pblk->disk->disk_name);
		return -1;
	}
	vfs_write(pblk->c_perf->data_file, str, strlen(str), &pblk->c_perf->data_file->f_pos);

	return 0;
}

/* clear IOPS count, update the next time */
void CAST_INCREASE_COUNT(void *private, struct cast_counter *cc, long size)
{
	struct pblk *pblk = private;
	struct cast_perf *c_perf = pblk->c_perf;

	cc->unit += size;
	cc->total += size;
	cc->num++;
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
	
	c_perf->rlat_dev->	reset_unit(c_perf->rlat_dev);
	c_perf->wlat_dev->	reset_unit(c_perf->wlat_dev);
}

/* Creator for CAST_PERF */
struct cast_perf * new_cast_perf()
{
	struct cast_perf * perf;
	perf = (struct cast_perf *)kmalloc(sizeof(struct cast_perf), GFP_KERNEL);
    if(perf == NULL)
    	return perf;

	//counters
	perf->read_hit	= new_cast_counter();
	perf->read_miss	= new_cast_counter();
	perf->write_usr	= new_cast_counter();
	perf->write_gc	= new_cast_counter();
	perf->gc	 	= new_cast_counter();

	perf->rlat_dev	= new_cast_counter();
	perf->wlat_dev	= new_cast_counter();

	// method
	perf->init 				= &CAST_PERF_INIT;
	perf->create_data_file	= &CAST_CREATE_DATA_FILE;
	perf->close_data_file	= &CAST_CLOSE_DATA_FILE;
	perf->write_in_data_file= &CAST_WRITE_TO_DATA_FILE;

	perf->inc_count 		= &CAST_INCREASE_COUNT;
	perf->reset_count 		= &CAST_RESET_COUNT;

	return perf;
}

/*** CPU ***/

// read data from /proc/stat
void GetCurrentStat(struct cast_cpu_stat *this)
{
	// init
	char filename[32] = "/proc/stat";
	static char buf[MAX_STAT_BUF];				// file contents
	loff_t pos = 0;								// end of file
	char token, *line, *words[MAX_STAT_BUF/2];	// parser
	int nr_line, length, nr_word;			// parser
	struct file *stat_file;

	// open the file
	stat_file = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(stat_file))
	{
		printk(KERN_ALERT "[  CAST  ] - Cannot open the file %s %ld\n", 
			filename, PTR_ERR(stat_file));
		return ;
	}

	// Read the data to the end of the file
	memset(buf , '\0', MAX_STAT_BUF * sizeof(char));	
	buf[kernel_read(stat_file, buf, sizeof(buf), &pos)] = '\0';
	filp_close(stat_file, NULL);		//file pointer closed
	
	// parse buf
	line 	= &buf[0];
	nr_line = 1;
	nr_word = 0;
	length 	= 0;
	token 	= ' ';

	while (line[length] != '\0')
	{
		if (line[length] == token || line[length] == '\n')
		{
			if (length > 0)
				words[nr_word++] = line;
			if (line[length] == '\n')
				nr_line++;
			line[length] = '\0';
			line = &line[length + 1];
			length = 0;
		}else
			length++;
	}
	this->user = atoi(words[1]); 
	this->nice = atoi(words[2]); 
	this->syst = atoi(words[3]); 
	this->idle = atoi(words[4]); 
	this->wait = atoi(words[5]); 
	this->irq  = atoi(words[6]); 
	this->soft = atoi(words[7]); 
	this->steal = atoi(words[8]);
	this->guest = atoi(words[9]);
	this->nproc = nr_line -2;
}
// constructor cast_cpu_stat
struct cast_cpu_stat * new_cast_cpu_stat(int core)
{
	struct cast_cpu_stat * new = (struct cast_cpu_stat *)kmalloc(sizeof(struct cast_cpu_stat), GFP_KERNEL);
	new->nproc = core;
	new->get_current_stat = &GetCurrentStat;
	return new;
}

// destroyer cast_perf_mgr
void delete_cast_perf_mgr(struct cast_perf_mgr *this)
{
	this->active = 0;
	kthread_stop(this->thread);
	filp_close(this->data_file, 0);
	kfree(this->elapsed_stat);
	kfree(this->current_stat);
	kfree(this);
}

/* memory -> data file */
void Reset(struct cast_perf_mgr *this)
{
	struct cast_cpu_stat * temp;
	temp = this->elapsed_stat;
	this->elapsed_stat = this->current_stat;
	this->current_stat = temp;

	this->current_stat->get_current_stat(this->current_stat);
	this->next_time += this->unit_time;
}

/* memory -> data file */
int ManagerFlush(struct cast_perf_mgr *this, long time)
{
	char str[256];
	long total, used, user, nice, syst, idle, wait, irq, soft, steal, guest;
	int i;

	if (IS_ERR(this->data_file) || this->active != 1)
	{
		printk(KERN_ALERT "[  CAST  ] - data file for CPU is not opened\n");
		return -1;
	}

	user = this->current_stat->user	- this->elapsed_stat->user;
	nice = this->current_stat->nice	- this->elapsed_stat->nice;
	syst = this->current_stat->syst	- this->elapsed_stat->syst;
	idle = this->current_stat->idle	- this->elapsed_stat->idle;
	wait = this->current_stat->wait	- this->elapsed_stat->wait;
	irq	 = this->current_stat->irq	- this->elapsed_stat->irq;
	soft = this->current_stat->soft	- this->elapsed_stat->soft;
	steal = this->current_stat->steal - this->elapsed_stat->steal;
	guest = this->current_stat->guest - this->elapsed_stat->guest;

	total = user + syst + nice + idle + wait + irq + soft + steal + guest;
	used  = total - idle;

	sprintf(str, "[%5ld.%03lds] CPU TOTAL %3ld [ %ld %ld %ld %ld %ld %ld %ld %ld %ld ]\n",
	time/1000, time%1000, used*100/total, user, nice, syst, idle, wait, irq, soft, steal, guest);
	vfs_write(this->data_file, str, strlen(str), &this->data_file->f_pos);

	// flust each pblk
	for( i = 0; i < this->nr_pblk ; i++)
	{
		int ret = ((struct pblk*)(this->pblk_vec[i]))->c_perf->write_in_data_file(this->pblk_vec[i], time);
		if( ret < 0)
			return ret;
		((struct pblk*)(this->pblk_vec[i]))->c_perf->reset_count(this->pblk_vec[i]);
	}
	return 0;
}

/* Call CAST_WRITE_TO_DATA_FILE() periodically */
int ManagerFlushThread(void *data)
{
	struct cast_perf_mgr *this = (struct cast_perf_mgr*)data;
	long now = 0;
	int fr_to_ms = 1000/HZ;
	while (this->active == 1)
	{	
		now = get_jiffies_64()*fr_to_ms - this->init_time;
		if (now >= this->next_time)
		{
			if( this->flush(this, now) != 0 )
				return 1;
			this->reset(this);
		}
		msleep(HZ/10);
	}
	return 0;
}

int AddPblk(struct cast_perf_mgr *this, void *pblk)
{
	if( this->nr_pblk >= 32 )
		return -1;	//FULL
	this->pblk_vec[this->nr_pblk++] = pblk;
	return 0;
}
int PopPblk(struct cast_perf_mgr *this, void *pblk)
{
	int i, j;
	if( this->nr_pblk <= 0 )
		return -1;	//EMPTY
	
	for( i = 0; i < this->nr_pblk ; i++ )
	{	
		if( this->pblk_vec[i] == pblk )
		{
			this->nr_pblk--;
			for( j = i ; j < this->nr_pblk ; j++ )
				this->pblk_vec[j] = this->pblk_vec[j+1];
			this->pblk_vec[this->nr_pblk] = NULL;
			if( this->nr_pblk == 0 )
				delete_cast_perf_mgr(this);
			return 0;
		}
	}
	return -2; // Not found
}

struct cast_perf_mgr *get_cast_perf_mgr(void*pblk)
{
	static struct cast_perf_mgr *instance = NULL;

	//initialize
	if (instance == NULL)
	{
		printk(KERN_ALERT "[  CAST  ] - Make new manager : %p\n", instance);
		instance = (struct cast_perf_mgr *)kmalloc(sizeof(struct cast_perf_mgr), GFP_KERNEL);
		
		// file open
		instance->data_file = filp_open("/pblk-cast_perf/cpu.data", O_TRUNC | O_WRONLY | O_CREAT, 0);
		if (IS_ERR(instance->data_file))
		{
			printk(KERN_ALERT "[  CAST  ] - Cannot open the file /pblk-cast_perf/cpu.data %ld\n",
			PTR_ERR(instance->data_file));
			return NULL;
		}

		instance->elapsed_stat = new_cast_cpu_stat(0);
		instance->current_stat = new_cast_cpu_stat(0);

		instance->nr_pblk = 0;

		instance->active 	= 1;
		instance->unit_time = 1000;
		instance->init_time = get_jiffies_64()*1000/HZ;
		instance->next_time = 0;

		// functions
		instance->reset			= &Reset;
		instance->flush 		= &ManagerFlush;
		instance->flush_thread 	= &ManagerFlushThread;
		instance->add_pblk 		= &AddPblk;
		instance->pop_pblk 		= &PopPblk;

		// run flushing Thread
		instance->reset(instance);
		instance->thread = (struct task_struct*)kthread_run(instance->flush_thread, 
		instance, "flush_thread");
		printk(KERN_ALERT "[  CAST  ] - Start flushing thread of Manager(%p)\n", instance);
	}
	instance->add_pblk(instance, pblk);
	return instance;
}