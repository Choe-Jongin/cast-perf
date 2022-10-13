/******************************************************************************/
/*                                                                            */
/*          Code for measuring the performance of open channel SSDs.          */
/*        This may slow down the program or require additional memory.        */
/*               For more information, please visit our github                */
/*              https://github.com/Choe-Jongin?tab=repositories               */
/*              copyrightⓒ 2022 All rights reserved by CAST lab              */
/*                                                                            */
/******************************************************************************/
#ifndef CAST_PERF_H
#define CAST_PERF_H

#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/string.h>

//file read
#include <linux/fs.h>      // Needed by filp
#include <linux/file.h>
#include <asm/uaccess.h>   // Needed by segment descriptors

static inline int CAST_CREATE_LOG_FILE(const char * tgtlistfile)
{
	// already
	if( CPS_TARGET_ONLY == 1)
	{
		CPS_TGT_FILE_NUM = 0;
	}

	char * start;
	int linelen;
	int hasnext;
	int level;
	int i;

	char buf[1024];
	struct file *fp;
	loff_t pos = 0;
	int offset = 0;
	
	for( i = 0 ; i < 1024 ; i++)
		buf[i] = '\0';

	/*open the file in read mode*/
	fp = filp_open(tgtlistfile, O_RDONLY, 0);
	if (IS_ERR(fp))
	{
		printk(KERN_ALERT "CPSERR - Cannot open the file %s %ld\n", tgtlistfile, PTR_ERR(fp));
		return -1;
	}

	/*Read the data to the end of the file*/
	buf[kernel_read(fp, buf, sizeof(buf), &pos)] = '\0';

	filp_close(fp, NULL);

	// parsing by line
	start = &buf[0];
	linelen = 0;
	hasnext = 1;
	level = 0;

	// parsing line and make CPS_tgt_file Object
	while (hasnext)
	{
		while (start[linelen] != '\n' && start[linelen] != '\0')
			linelen++;

		// Log Level setting
		if( strncmp(start,"LEVEL", 5) == 0 )
		{
			level = start[6] - '0'; 
			start = &start[8];
			linelen = 0;
			continue;
		}

		// check last char
		if (start[linelen] == '\0')
			hasnext = false;
		else
			start[linelen] = '\0';

		// regist
		if (strlen(start) > 0 && CPS_TGT_FILE_NUM < CPS_MAX_TGT_FILE - 1)
		{
			CPS_tgt_files[CPS_TGT_FILE_NUM].level = level;

			char * func = start;
			while(func[0] != ' ')
				func++;
			func[0] = '\0';
			func++;
			strcpy(CPS_tgt_files[CPS_TGT_FILE_NUM].name, start);
			strcpy(CPS_tgt_files[CPS_TGT_FILE_NUM].func, func);
			CPS_TGT_FILE_NUM++;
		}
		start = &start[++linelen];
		linelen = 0;
	}

	printk(KERN_ALERT "CPS - Opened the %s successfully(%d)\n", tgtlistfile, CPS_TGT_FILE_NUM);
	CPS_TARGET_ONLY = 1; 	// trace target only
	
	return 0;
}

#endif /* CAST_PERF_H */
