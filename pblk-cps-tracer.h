/*****************   CPS   ****************/

#ifndef CPS_TRACER_PBLK_H_
#define CPS_TRACER_PBLK_H_

#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/string.h>
//file read
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors
#include<linux/file.h>

// #pragma GCC optimize ("no-optimize-sibling-calls")
// #pragma GCC optimize ("O1")
// #pragma GCC optimize ("no-omit-frame-pointer")

// for TARGET 
#define CPS_MAX_TGT_FILE 20	// max target file number

typedef struct
{
	int level;
	char name[256];
	char func[256];
}CPS_tgt_file;

extern CPS_tgt_file CPS_tgt_files[CPS_MAX_TGT_FILE];
extern int CPS_TARGET_ONLY;
extern int CPS_TGT_FILE_NUM;

// for call stack
#define CPS_MAX_CALLSTACK 16
#define CPS_CALLER __builtin_frame_address(1) > (void*)0x1? __builtin_return_address(1) : __builtin_frame_address(1)
#define CPS_CALLEE __builtin_return_address(0)
extern int CPS_CALL_DEEP;
extern void *CPS_FUNC_ADDR[CPS_MAX_CALLSTACK];

// for print
extern int CPS_FUNC_COUNT;
extern char * current_filename;
extern char kstr[1024];

static inline int CPS_READ_TARGET_FILE(const char * tgtlistfile)
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

	char buf[512];
	struct file *fp;
	loff_t pos = 0;
	int offset = 0;
	
	for( i = 0 ; i < 512 ; i++)
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
static inline int CPS_GET_TARGET_LEVEL( const char * filename, const char * funcname )
{
	int tgt_i;

	if( CPS_TARGET_ONLY == 0 )
		return 2;					// if not target mode, return KERN_ALERT

	int filestart_i = strlen(filename);
	while(filestart_i > 0 && filename[filestart_i-1] != '/')
		filestart_i--;

	for(tgt_i = 0 ; tgt_i < CPS_TGT_FILE_NUM ; tgt_i++)
	{
		if( strcmp(&filename[filestart_i], CPS_tgt_files[tgt_i].name) == 0 )
		{
			// printk(KERN_ALERT "CPS - %s : %s \n", funcname, CPS_tgt_files[tgt_i].func);
			if( strcmp(CPS_tgt_files[tgt_i].func, "*") == 0 )
				return CPS_tgt_files[tgt_i].level;
			if( strstr(CPS_tgt_files[tgt_i].func, funcname) != NULL )
				return CPS_tgt_files[tgt_i].level;
		}
	}

	return 0;
}

static inline void CPS_OPEN_FUNC(void *caller, void *callee)
{
    while(CPS_CALL_DEEP >= 0 && CPS_FUNC_ADDR[CPS_CALL_DEEP] != caller)
        CPS_CALL_DEEP--;

    if (++CPS_CALL_DEEP < CPS_MAX_CALLSTACK)
        CPS_FUNC_ADDR[CPS_CALL_DEEP] = callee;
}

static inline void CPS_FUNCTION(void *caller, void *callee, const char * filename, const char * funcname, int argc, ...)
{
	int cps_level = CPS_GET_TARGET_LEVEL(filename, funcname);
	char * log_level = KERN_ALERT;
	if( cps_level == 0 )
		return ;
	else if (cps_level == 1)
		log_level = KERN_INFO;
	else if (cps_level == 2)
		log_level = KERN_ALERT;

	char tab_str[CPS_MAX_CALLSTACK*2] = "";
	int filestart_i = strlen(filename);
	int cps_i;
	int tab_i;
	va_list ap;
	void * p_primi;
	
	++CPS_FUNC_COUNT;

	//filename without path
	while(filestart_i > 0 && filename[filestart_i-1] != '/')
		filestart_i--;

	CPS_OPEN_FUNC(caller, callee);
	for (tab_i= 0; tab_i < CPS_CALL_DEEP*2; tab_i++)
	{
		if( tab_i%2 == 0 )
        	tab_str[tab_i] = '|';
		else if( tab_i%2 == 1 )
        	tab_str[tab_i] = ' ';
	}
	
	//_builtin_return_address(0);
	sprintf(kstr,"%s\033[0;31mCPS[%d] - %s\e[1m%s->%s\e[m\033[0m", log_level, CPS_FUNC_COUNT, tab_str, &filename[filestart_i], funcname);
	
	//print args
	va_start(ap, argc);
	for( cps_i = 0 ; cps_i < argc ; cps_i++ )
	{
		int currentlen = strlen(kstr);
		char * typestr  = va_arg(ap, char*);
		char * varname  = va_arg(ap, char*);
		p_primi = NULL;

		//type and name
		sprintf(&(kstr[currentlen]), " [%s %s", (const char *)typestr, (const char *)varname);
		//value by type
		currentlen = strlen(kstr);
		if(		 strcmp(typestr,"char *")==0)		sprintf(&(kstr[currentlen]), ":%s]",va_arg(ap, char*));
		else if( strcmp(typestr,"const char *")==0)	sprintf(&(kstr[currentlen]), ":%s]", va_arg(ap, char*));
		else if( strcmp(typestr,"int *")==0)		sprintf(&(kstr[currentlen]), ":%p(%d)]",
															 (int*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, int*))), 
															*(int*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, int*))));
		else if( strcmp(typestr,"unsigned int *")==0)		sprintf(&(kstr[currentlen]), ":%p(%u)]",
															 (unsigned int*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, int*))), 
															*(unsigned int*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, int*))));
		else if( strcmp(typestr,"long *")==0)		sprintf(&(kstr[currentlen]), ":%p(%ld)]",
															 (long*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, long*))), 
															*(long*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, long*))));
		else if( strcmp(typestr,"unsigned long *")==0)		sprintf(&(kstr[currentlen]), ":%p(%lu)]",
															 (unsigned long*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, long*))), 
															*(unsigned long*)(p_primi!=NULL?p_primi:(p_primi=va_arg(ap, long*))));
		else if( typestr[strlen(typestr)-1] == '*')	sprintf(&(kstr[currentlen]), ":%ps]", va_arg(ap, void*));
		else if( strcmp(typestr,"int")==0)			sprintf(&(kstr[currentlen]), ":%d]", va_arg(ap, int));
		else if( strcmp(typestr,"unsigned int")==0)	sprintf(&(kstr[currentlen]), ":%u]", va_arg(ap, int));
		else if( strcmp(typestr,"long")==0)			sprintf(&(kstr[currentlen]), ":%ld]", va_arg(ap, long));
		else if( strcmp(typestr,"unsigned long")==0)sprintf(&(kstr[currentlen]), ":%lu]", va_arg(ap, long));
		else if( strcmp(typestr,"float")==0)		sprintf(&(kstr[currentlen]), ":%f]", va_arg(ap, float));
		else if( strcmp(typestr,"double")==0)		sprintf(&(kstr[currentlen]), ":%f]", va_arg(ap, float));
		else if( strcmp(typestr,"char")==0)			sprintf(&(kstr[currentlen]), ":%c]", va_arg(ap, char));
		else if( strcmp(typestr,"bool")==0)			sprintf(&(kstr[currentlen]), ":%d]", va_arg(ap, int));
		else										sprintf(&(kstr[currentlen]), ":etc]", va_arg(ap, int));
	}

	printk(kstr);
	va_end(ap);

	current_filename = (char *)filename;
}
static inline void CPS_MSG(const char *str)
{
	sprintf(kstr,"%s\033[0;31mCPSMSG -\e[1m %s\e[m\033[0m",
		KERN_ALERT , str);
	printk(kstr);
}

#endif /* CPS_TRACER_PBLK_H_ */
