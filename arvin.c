/*
	By Sirus Shahini
	~cyn
*/

#include "head.h"
#include "hash.h"
#include "arvin.h"


/*******************************************************************************/

void terminate_units(){

	/*
		Back to text mode
	*/
	use_term_gui = 0;

	if (target_id>0){
		kill(target_id,SIGKILL);
	}
	if (starter_id){
		kill(starter_id,SIGKILL);
	}


	if (shm_adr){
		/*
			Release shared memory to OS
		*/
		if (shmctl(shm_id,IPC_RMID,0)==-1){
			printf(CRED"Warning: Couldn't release shared memory (main).\n"CNORM);
		}else{
			printf("Released memory (Main SHM)\n");
		}
		if (shmctl(comm_id,IPC_RMID,0)==-1){
			printf(CRED"Warning: Couldn't release shared memory (comm).\n"CNORM);
		}else{
			printf("Released memory (COMM)\n");
		}
	}
    /*
    	else we're exiting at initialization
    	phase and memory has not been allocated.
    */

    /*
    	All file cleanups here
    */
	if (!file_feed)
		unlink(cinp_file_path);
	else
		unlink(feed_file_path);
	if (!parallel_mode) unlink(master_instance_p);
	printf(SC);
}
char *convert_time(char *ts){
	time_t cur_time;
	long h,m,s;

	time(&cur_time);
    cur_time-=start_time;

    h=cur_time/3600;
    cur_time -= 3600*h;

    m=cur_time/60;
    s=cur_time-60*m;

	sprintf(ts,"%02ld:%02ld:%02ld",h,m,s);

	return ts;
}


void dump_hex(u8 *buf,u8 size,int start,int end){
    int i;
    int last=0;
    char tmp[16];
    char sect[16];
    char *outbuf= malloc(2048);

    outbuf[0] = 0;
    for (i=0;i<size;i++){
    	if (i==start){
    		strcpy(sect,CRED);

    	}else if (i==end){
    		strcpy(sect,CNORM);
    	}else{
    		strcpy(sect,"");
    	}
        sprintf(tmp,"%02X ",(u8)*((u8*)(buf)+i));
        strcat(sect,tmp);
        strcat(outbuf,sect);
        last+=3;
        if (i%8==7) strcat(outbuf,"    ");
        if (i%16==15 || i==size-1) {
            printf("%s\n",outbuf);
            outbuf[0] = 0;
        }
    }
    free(outbuf);
    printf("\n");
}
void rep_use_time(){
	char ts[128];

	convert_time(ts);
	printf("Total time:  %s\n",ts);
}

void print_queue(){
	int i;

	for (i=0;i<queue_ind;i++){
		if (_st_indp)
			printf ("Q%d: \n\tdepth:%d %s prio=%d coverage=%lf nodes:%lu hits:%lu\n",i,
									input_queue[i].depth,input_queue[i].i_path,input_queue[i].prio,
									(double)input_queue[i].total_blocks/_st_indp,input_queue[i].total_blocks,
									input_queue[i].total_hits);
		else{
			printf ("Q%d: \n\tdepth:%d %s prio=%d coverage=%s nodes:%lu hits:%lu\n",i,
									input_queue[i].depth,input_queue[i].i_path,input_queue[i].prio,
									"N/A",input_queue[i].total_blocks,
									input_queue[i].total_hits);
			printf("\t Path: %s size:%lu \n",input_queue[i].i_path,input_queue[i].size);
		}
	}
}



void aexit(char *fmt, ...){
	char err_format[2048];

	strcpy(err_format,CRED "[!] Arvin: " CNORM );
	strcat(err_format,fmt);
	strcat(err_format,"\n");
	va_list argp;
	va_start(argp,err_format);
	vprintf(err_format,argp);
	va_end(argp);

	terminate_units();
	printf(DStop);

	if (!arvin_init_state)
		rep_use_time();

	exit(-1);
}

void rep(u8 warn, char *fmt, ...){
	char msg_format[2048];
	u8 should_print=0;

	if (arvin_init_state || !use_term_gui){
		should_print = 1;
	}
	if (!warn){
		strcpy(msg_format,CGREEN "[-] " CNORM  );
	}
	else{
		strcpy(msg_format,CYELLOW "[W] " CNORM );

	}
	strcat(msg_format,fmt);
	va_list argp;

	if (should_print)
		strcat(msg_format,"\n");

	va_start(argp,msg_format);


	if (!should_print){
    	/*
		Only write to stat line if this is arep
	*/
	if (!warn)
    		vsprintf(stat_line,msg_format,argp);
	}
	else{
		vprintf(msg_format,argp);
	}
	if (warn){
		/*
    		In gui mode this is the only place that we
    		want to see fuzzer warnings.
    		Reserve stat line for rep.
		*/
		vsprintf(current_stat,msg_format,argp);
	}
	va_end(argp);

}

/*
 https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
*/
#define arep(fmt,...) rep(0,fmt __VA_OPT__(,) __VA_ARGS__)
#define awarn(fmt,...) rep(1,fmt __VA_OPT__(,) __VA_ARGS__)

#define SPACE(n) do{\
					int i;\
					for (i=0;i<n;i++) printf(" ");\
				 }while(0);


void clear_warn(){
	if (net_mode)
		strcpy(current_stat,CGREEN"NORMAL [Network Mode]"CNORM);
	else if (parallel_mode)
		strcpy(current_stat,CGREEN"NORMAL [Parallel Mode]"CNORM);
	else
		strcpy(current_stat,CGREEN"NORMAL"CNORM);
}
char *exec_speed(char *cs){
	struct timespec ctime;

	u64 cur_timeus;
	u64 espeed;
	double distance_s;

	clock_gettime(CLOCK_REALTIME,&ctime);
	cur_timeus= ctime.tv_sec*1000000 + ctime.tv_nsec/1000;
	distance_s = (cur_timeus-last_etime_inq)/1000000.0;


    if (distance_s > 0.0){
		espeed = (u64)((total_exec-last_exec_inq)/distance_s);

		sprintf(cs,"%lu/sec",espeed);
		/*
			10 refresh pre sec seems reasonable to have a fancy hex view
			However 10 seems to be too fast to keep its performance cost low.
			Number between 2 and 5 are better choices.
			It seems that 2 gives the upper bound speed.
		*/
		brefr_freq = espeed/4;
		brefr_freq=brefr_freq>MAX_BREFR_FREQ?MAX_BREFR_FREQ:brefr_freq;
		if (espeed>0){
			/*
				First call espeed gets zero
				because last_exec_inq is zero.
			*/
			if ( espeed<20){
				awarn("Target is running too slow");
				state_slow_warn=1;
			}
			else if (state_slow_warn){ //don't clear other wanrning messages
				clear_warn();
				state_slow_warn=0;
			}
		}
    }else{

    	sprintf(cs,">%lu/sec",total_exec);
    }

    last_exec_inq = total_exec;
    last_etime_inq = cur_timeus;

    return cs;

}
char *get_sig_name(int signum){
	int i;
	for (i=0;i<SIG_SET_SIZE;i++){
		if (signum==sig_set[i].id)
			return sig_set[i].name;
	}
	awarn("Unknown crash signal from target: %d",signum);
	return "UNKNOWN";
}


char * psect(char *buf,int rlen,char *fmt, ...){
	char sbuf[255];
	int len;
	int i;

	buf[0]=0;
	sbuf[0] = 0;
	va_list argp;
    va_start(argp,fmt);
    vsprintf(buf,fmt,argp);
    va_end(argp);

    len = strlen(buf);
    if (len<rlen){
    	for (i=0;i<(rlen-len);i++)
    		strcat(sbuf," ");
    	strcat(buf,sbuf);
    }else if(len>rlen){
    	buf[len] = 0; //trim
    }
    return buf;

}

char * show_data(char *buf,void *orig_data,size_t len,size_t start,size_t end){
	size_t i;
	char line[255];
	char tmp[255];
	char *s;
	int finished =0 ;
	u8 extra;
	int len_limit = 128;
	u8 pad = 0;
	int page ;
	void *data ;
	u8 mut_area=0;
	size_t orig_start,orig_end,orig_len;


	orig_start=start;
	orig_end=end;
	orig_len=len;

	/*
		mut function has returned -1 ?
		This function shouldn't be called
		in that case, but set a default for safety
	*/
	if (start==-1){
		aexit("Borad: Wrong arg START <%s> ",cmt);
		//start=0;
	}

	/*
		Prepare the approximate view while checking
		the sanity of the values
	*/

	if (end>=len){
		end=len-1;
	}

	if (len<=start || len<=end || end < start || (start==end && start==0)){
		aexit("Board wrong intpus: %d,%d,%d <%s> ",start,end,len,cmt);
		//start=0;
		//end=1;
	}

	if (start==end){
		start--;
	}

	page=(start/len_limit); //Go to the corresponding page
	data = orig_data + (128*page);
	/*
		Remove the first "page" pages.
		Go to page+1
	*/
	len = len - 128*page;


	if (len>len_limit) //len is in next pages
		len = len_limit;

	//Normalize start and end
	if (start >= len_limit) start = start % len_limit;
	if (end >= len_limit) end = end % len_limit;
	/*
		After normalization end might get smaller than
		start if they fall into different pages.
		In which case we make all bytes from start
		up to the end of the page red.
	*/

	/*
		Pad?
	*/
	if (len<=(len_limit-16)) pad=1;

	/*
		Final check
	*/
	if (start >= len || end>=len){
		aexit("Board: args overflow page offset %d,%d,%d %d,%d,%d <%s> ",start,end,len,
								orig_start,orig_end,orig_len,cmt);
	}
	/*
		For mut functions that set:
			end=start+1
		while end=len-1
		however we set coloring to norm when
		i==len-1 anyways.
		So this is not necessary:
	*/
	/*
	if (end==len){
		end=len-1;
	}
	*/
#define APPEND 	psect(tmp,78+extra,line);\
				strcpy(line,DStart VR DStop);\
				strcat(line,tmp);\
				strcat(line,DStart VR DStop);\
				strcat(buf,line);\
				if (finished) break;\
				strcat(buf,"\n");\

	strcpy(line,_15SP);
	s=&line[15];
	extra = 0;

	buf[0]=0;
	for (i=0;i<len_limit;i++){
		if (i==start && i%16!=15){
			strcpy(s,CRED);
			s+=7;
			extra+=7;
			mut_area=1;
		}
		if (i==end || i==len_limit-1){
			strcpy(s,CNORM);
			s+=4;
			extra+=4;
			mut_area=0;
		}

		if (i%16==0 && mut_area){
			strcpy(s,CRED);
			s+=7;
			extra+=7;
		}
		sprintf(s,"%02x",((unsigned char*)data)[i]);
		if (i==len-1){
			finished = 1;
			strcpy(s+2,CNORM);
			extra+=4;
			APPEND
		}

		if (i%16==15){
			if (i+1==len) finished=1;
			if (mut_area){
				/*
					Reset colors for vertical table lines
				*/
				strcpy(s+2,CNORM);
				extra+=4;
			}
			APPEND
			//re-init
			strcpy(line,_15SP);
			s=&line[15];
			extra = 0;
		}else{
			strcat(s," ");
			s+=3;
		}
	}

	if (pad){
		line[0]=0;
		extra=0;
		finished=0;
		strcat(buf,"\n");
		i = ((i/16)+1)*16;
		for (;i<len_limit;i++){
			if (i%16==15){
				if (i+1==len_limit) finished=1;
				APPEND
				line[0]=0;
			}
		}

	}
	return buf;
}

/*TODO: PAPER STUFF*/
u32 _all_found_paths=0;
void refresh_board(void *data,size_t size,size_t start,size_t end){
	char buf[1024];
	char line[1024];
	char tmp[255];
	char line2[1024];
	char tmp2[255];
	char *sleft,*sright;
	int i,_is,_ie;
	static u8 cleared_once = 0;

/*time_t tt;
time(&tt);
if (tt-start_time >= 1*3600){aexit("TEST FINISHED, ALL PATHS: %d",_all_found_paths);}	*/

	if (!use_term_gui) return;

	if (!cleared_once){
		printf(DC);
		cleared_once = 1;
	}


	printf(DH);
	printf("\n");
	SPACE(35) printf(CLCYAN"ARVIN (1.0)\n");
	SPACE(32) printf("By Sirus Shahini\n"CNORM);




	//Design the frame to be 80 chars wide
	printf(DStart LCD _32HO _6HO BHD BHD _32HO _6HO RCD DStop "\n" );

	//line1
	printf(DStart VR DStop);

	i=strlen(target_path);
	if (strlen(target_path)<18)
		printf("%s",psect(buf,38+22,CDBlue"Target: "CNORM CYELLOW"%s"CNORM,target_path));
	else
		printf("%s",psect(buf,38+22,CDBlue"Target: "CNORM CYELLOW"%.8s...%s"CNORM,target_path,&target_path[i-10])); //%*s can be used to right align
	printf(DStart VR DStop);

	printf(DStart VR DStop);

	printf("%s",psect(buf,38+22,CDBlue"Last Independet: "CNORM CGRAY"%lu"CNORM,indp_counter));
	printf(DStart VR DStop);

	printf("\n");

	//line2
	printf(DStart VR DStop);
	if (_st_bl)
		printf("%s",psect(buf,38+22,CDBlue"Blocks: "CNORM CGRAY"%lu"CNORM,_st_bl));
	else
		printf("%s",psect(buf,38+22,CDBlue"Blocks: "CNORM CGRAY"N/A"CNORM));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+22,CDBlue"Last Nested: "CNORM CGRAY"%lu"CNORM,nested_counter));
	printf(DStart VR DStop);

	printf("\n");

	//line3
	printf(DStart VR DStop);
	if (_st_indp)
		printf("%s",psect(buf,38+22,CDBlue"Independent: "CNORM CGRAY"%lu"CNORM,_st_indp));
	else
		printf("%s",psect(buf,38+22,CDBlue"Independent: "CNORM CGRAY"N/A"CNORM));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	if (_st_indp)
		printf("%s",psect(buf,38+22,CDBlue"Coverage: "CNORM CGRAY"%lf"CNORM,(double)total_covered/_st_indp));
	else
		printf("%s",psect(buf,38+22,CDBlue"Coverage: "CNORM CGRAY"N/A"CNORM));
	printf(DStart VR DStop);

	printf("\n");

	//line4
	printf(DStart VR DStop);
	if (_st_nes)
		printf("%s",psect(buf,38+22,CDBlue"Nested: "CNORM CGRAY"%lu"CNORM,_st_nes));
	else
		printf("%s",psect(buf,38+22,CDBlue"Nested: "CNORM CGRAY"N/A"CNORM));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+22,CDBlue"Time: "CNORM CGRAY"%s"CNORM,convert_time(tmp)));
	printf(DStart VR DStop);

	printf("\n");

	//********************next sect
	printf(DStart BVR _32HO _6HO DP DP _32HO _6HO BVL DStop "\n" );


	//line1
	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Inputs: "CNORM"%d",input_count));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"TM OUT: "CNORM"%02d",(int)(active_timeout/1000)));
	printf(DStart VR DStop);

	printf("\n");

	//line2
	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Queue Elms: "CNORM"%d",queue_ind));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38 + 22,CORANGE"Execs: "CNORM CWHITE"%lu"CNORM,total_exec));
	printf(DStart VR DStop);

	printf("\n");

	//line3
	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"QI: "CNORM"%d",queue_use_ind));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Last Nodes: "CNORM"%lu",last_trace_nodes));
	printf(DStart VR DStop);

	printf("\n");

	//line4
	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CLGREEN"Depth: "CNORM"%lu",max_depth));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Output: "CNORM"%s",output_dir));
	printf(DStart VR DStop);

	printf("\n");

	//line5
	printf(DStart VR DStop);
	if (parallel_mode)
		printf("%s",psect(buf,38+11,CDBlue"PModel: N/A"CNORM));
	else
		printf("%s",psect(buf,38+11,CDBlue"PModel: "CNORM"%s",pm_str));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Core: "CNORM"%d",attached_core));
	printf(DStart VR DStop);

	printf("\n");

	//line6
	printf(DStart VR DStop);
	//printf("%s",psect(buf,38+11,CDBlue"Q coverage: "CNORM"%lf",((double)queue_use_ind)/queue_ind));
	printf("%s",psect(buf,38+11,CDBlue"Q cycles: "CNORM"%u",qc));
	printf(DStart VR DStop);

	printf(DStart VR DStop);
	printf("%s",psect(buf,38+11,CDBlue"Exec speed: "CNORM"%s",exec_speed(tmp)));
	printf(DStart VR DStop);

	printf("\n");

	//********************next sect
	printf(DStart BVR _32HO _6HO BHU BHU _32HO _6HO BVL DStop "\n" );

	printf(DStart VR DStop);
	printf("%s",psect(buf,78 + 18,CORANGE"Crashes: "CRED"%lu"CNORM,total_crashes));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart VR DStop);
	printf("%s",psect(buf,78 + 18,CORANGE"Unique Crashes: "CRED"%lu"CNORM,unique_crashes));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart VR DStop);
	if (save_soft_crash)
		printf("%s",psect(buf,78 + 18,CWHITE"Soft Crashes: "CORANGE"%lu "CNORM,soft_crashes));
	else
		printf("%s",psect(buf,78 + 18,CWHITE"Soft Crashes: "CORANGE"[DISABLED] "CNORM));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart VR DStop);
	if (save_soft_crash)
		printf("%s",psect(buf,78+18,CWHITE"Unique Soft Crashes: "CORANGE"%lu"CNORM,unique_soft_crashes));
	else
		printf("%s",psect(buf,78+18,CWHITE"Unique Soft Crashes: "CORANGE"[DISABLED]"CNORM));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart VR DStop);
	/*
		11 extra spaces because of color codes of current_stat
	*/

	printf("%s",psect(buf,78 + 22,CWHITE"Fuzzer Status: "CNORM"%s",current_stat));

	printf(DStart VR DStop);

	printf("\n");


	//********************next sect
	printf(DStart BVR _64HO _14HO BVL DStop "\n" );

	printf(DStart VR DStop);
	strcpy(line,"Queue: ");
	strcpy(line2,"       ");
	if (queue_ind<10){
		_is = 0;
		_ie = 10;
	}else {
		_is = queue_use_ind-5;
		_ie = queue_use_ind+5;
	}
	if (_is<0)
		_is=0;
	if (_ie>queue_ind)
		_ie=queue_ind;

	for (i=_is;i<_ie;i++){
		if (i == queue_use_ind){
			if (input_queue[queue_use_ind].passed)
				sleft=CGRAY;
			else
				sleft=CYELLOW;
			sright=CNORM;
		}else{
			sleft=0;
			sright=0;
		}
		if (input_queue[i].total_blocks<1000){
			sprintf(tmp2,"%s%03lu%s",(sleft?sleft:""),input_queue[i].total_blocks,(sright?sright:""));
			sprintf(tmp,"%s%03d%s",(sleft?sleft:""),input_queue[i].depth,(sright?sright:""));
		}else{
			sprintf(tmp2,"%s%lu%s",(sleft?sleft:""),input_queue[i].total_blocks,(sright?sright:""));
			sprintf(tmp,"%s%d%s",(sleft?sleft:""),input_queue[i].depth,(sright?sright:""));
		}
		if (input_queue[i].prio==0){
			strcat(tmp,"L,");
			strcat(tmp2,"L,");
		}
		else{
			strcat(tmp,",");
			strcat(tmp2,",");
		}

		strcat(line,tmp);
		strcat(line2,tmp2);
	}
	strcat(line," ...");

	/*
		11: sleft + sright color codes
	*/
	printf("%s",psect(buf,78+11,line));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart VR DStop);
	printf("%s",psect(buf,78+11,line2));
	printf(DStart VR DStop);
	printf("\n");

	//********************next sect
	printf(DStart BVR _64HO _14HO BVL DStop "\n" );


	printf("%s",show_data(buf,data,size,start,end));
	printf("\n");

	/*u8 e=0;
	for (i=0;i<=1024;i++){
		if (buf[i]=='\n'){
			//printf(">>> %c %d\n",buf[i],buf[i]);
			e++;
		}else {
			//printf("%c %d\n",buf[i],buf[i]);
		}
	}
	if (e!=7) aexit("%d",e);*/


	//********************next sect
	printf(DStart BVR _64HO _14HO BVL DStop "\n" );

	printf(DStart VR DStop);
	printf("%s",psect(buf,78 + 11,stat_line));
	printf(DStart VR DStop);
	printf("\n");

	printf(DStart LCU _64HO _14HO RCU DStop "\n" );
	printf(DStop);

	//printf("\n\n");


}

void print_usage(){
	printf("Usage: arvin -i <input_directory> -o <output_directory> <target_program>\n");
	printf("Arguments:\n"
			"\t i <path> : Directory of input seeds (Required)\n"
			"\t o <path> : Output directory (Required)\n"
			"\t f <path> : Input file to target\n"
			"\t n <port> : TCP mode\n"
			"\t m : Multithreaded target\n"
			"\t c : Custom cleanup\n"
			"\t p: Priority model (TNF, TDF, TNS)\n"
			"\t g: Limited; initial coverage only\n"
			"\t e: Limited; show initial basic block count\n"
			"\t a: Add coverage increasing inputs to the input directory\n"
			"\t k <n>: Performance check mode (0,1,2)\n"
			"\t r: Store graph (developer only)\n"
			"\t y <path>: Dictionary to use\n"
			"\t d: Debug mode (developer only)\n"
			"\t T <n>: Number of basic blocks (Get from zcc)\n"
			"\t N <n>: Number of nested blocks (Get from zcc)\n"
			"\t B: Number of independent basic blocks (Get from zcc)\n"
			"\t j: Don't mutate the first 16 bytes (Parallel mode only)\n");

	exit(-1);
}

/*
	There's no other choice to catch
	a corrupted tree!
*/
u8 check_adr(void *adr){
	return (adr >= shm_adr && adr <= shm_end);
}

int ulock(void * uadr){
    u64 volatile r =0 ;
    asm volatile(
        "xor %%rax,%%rax\n"
        "mov $1,%%rbx\n"
        "lock cmpxchg %%ebx,(%1)\n"
        "sete (%0)\n"
        : : "r"(&r),"r" (uadr)
        : "%rax","%rbx"
    );
    return (r) ? 1 : 0;
}

#define MULT_THREAD_SUPPORT

#ifdef MULT_THREAD_SUPPORT

#define ACQUIRE_LOCK while(!ulock((void *)lib_lock));
#define RELEASE_LOCK *lib_lock = 0;
#define ACQUIRE_LOCK_CONDITIONED	if (target_id) ACQUIRE_LOCK

#else

#define ACQUIRE_LOCK ;
#define RELEASE_LOCK ;
#define ACQUIRE_LOCK_CONDITIONED ;

#endif



void queue_add(char *file_path,char *m_path,u8 init,size_t size){

	strcpy(input_queue[queue_ind].i_path,file_path);
	if (m_path) strcpy(input_queue[queue_ind].m_path,m_path);
	input_queue[queue_ind].initial = init;
	input_queue[queue_ind].prio = 1; //init to high priority
	input_queue[queue_ind].fixed_loc=0;
	input_queue[queue_ind].marked = 0;
	input_queue[queue_ind].passed = 0;
	input_queue[queue_ind].leaves = 0;
	input_queue[queue_ind].invalidated_i = 0;
	input_queue[queue_ind].size=size;
	//printf("added %s to %d\n",file_path,queue_ind);
	queue_ind++;
	if (queue_ind == INPUT_MAX){
		/*
			We start to overwrite the queue from
			beginning
		*/
		queue_ind=0;
	}

}

/*
	Return value indicates whether the candidate has
	been added or not.
*/
u8 queue_add_traced(struct input_val *iv, int *add_indx){
	struct input_val *cur;
	struct input_val temp;
	u8 swap;


#define CHECK_QUEUE_CAP		if (queue_ind == queue_use_ind){\
								awarn("Hit queue slider. Discarding tail.");\
								queue_ind = 0;\
								queue_hit_sl = 1;\
							}

/*
	Sort low prio inputs
	based on a probability for
	lpq balance mode 0
*/
#define LP_PROB_FIX	(!RU8(5))


	queue_hit_sl = 0;

	switch (pm_mode){
		case 0:
			/*
				This is the default mode.
				sort based on:
					1-depth (except when it's multithreaded)
					2-block count
					3-hits
			*/

			/*
				Should we add it?
			*/
			if (iv->prio == 0){
				if (iv->depth < LPSD_MAX_DEPTH){
					if (LPSD_queue[iv->depth] > MLPSD_ALLOWED){
						/*
							We have enough inputs of this
							depth and low priority (MLPSD_ALLOWED+1)
							Discard this.
						*/
						return 0;
					}
					LPSD_queue[iv->depth]++;
				}
				/*
					Else this is an unsually deep tree!
					Store it.
				*/
			}

			CHECK_QUEUE_CAP

			cur=&input_queue[queue_ind];
			*cur=*iv;

			if (cur->prio == 1 || (cur->prio ==0 && sort_low_prio)){




				/*
					the swap variable is not needed since if
					it doesn't need swap it breaks out of loop
					but I leave it there to show when explicitely
					a swap should happen.
				*/


				while (cur != input_queue){
					/*
						If we are here cur is poiting to somewhere
						after input_queue[0]
					*/
					swap=0;
					/*
						Keep in mind cur always points to our new
						element sliding in the queue

						We don't want to go beyond the initials
						the initial inputs stay where they are

						Also, don't go beyond queue_use_ind
						We don't want to postpone processing
						a good input to the next cycle after we start
						from the beginning of the queue
					*/


					if ((cur-1)->initial){
						//printf("Staying at %d \n",(int)(cur-input_queue));
						break;
					}
					if (queue_use_ind + 1 == (u64)(cur-input_queue) )
					{
						//printf("> Staying at %d \n",(int)(cur-input_queue));
						break;
					}

					if (cur->prio==0 && (cur-1)->fixed_loc){
						break;
					}


					if (cur->prio < (cur-1)->prio){
						/*

							low priority inputs will be kept sorted at the end of
							the queue because there's already another candidate
							with the same nodes in the queue
							We do this to avoid starvation of less deep inputs

							Even if the new input is marked but is low priority,
							we still don't move it to higher priority classes as
							this most probably starve many useful inputs.

						*/
						break;
					}else if (cur->prio > (cur-1)->prio){

						/*
							Any higher priority input is preferred
							over all lower priority ones including
							deeper inputs.
							Again, doing otherwise will starve
							many good inputs
						*/
						swap=1;

					}else{
						/*
							Ok we've reached the same priority class.
							Slide it until it finds its correct place.

							Sort descending
							deeper trees close to the queue head
								1.depth
								2.blocks
								3.hits


						*/
						if (!cur->marked && (cur-1)->marked){
							/*
								Don't pass a marked input when cur
								is unmarked.
							*/
							break;
						}else if (cur->marked && !(cur-1)->marked){
							/*
								move marked nodes up
							*/
							swap=1;
						}else if (!target_mult_threaded && cur->depth>(cur-1)->depth){
							swap=1;
						}else if (cur->depth == (cur-1)->depth){
							if(cur->total_blocks > (cur-1)->total_blocks){
								swap=1;
							}else if(cur->total_blocks == (cur-1)->total_blocks){
								if(cur->total_hits > (cur-1)->total_hits){
									swap=1;
								}else{
									break;
								}
							}
							else{
								break;
							}
						}else{
							break;
						}
					}
					if (swap){
						temp=*cur;
						*cur=*(cur-1);
						*(cur -1)=temp;
						cur--;
					}

				} //end while
			} //end if
			/*
				else this is a low priority input and
				we don't want to sort these inputs
				New input will be kept in tail of the queue
			*/
			break;

		case 1:
			/*
				1- nodes count
				2- depth
				3- hit
			*/

			/*
				Should we add it?
			*/
			if (iv->prio == 0){
				if (iv->total_blocks < LPSC_MAX_NODES){
					if (LPSC_queue[iv->total_blocks] > MLPSC_ALLOWED){
						/*
							We have enough inputs of this
							set of nodes and low priority (MLPSC_ALLOWED+1)
							Discard this.
						*/

						return 0;
					}
					LPSC_queue[iv->total_blocks]++;
				}
				/*
					Else this is an unsually big tree!
					Don't waste memory for restricting
					these cases.
					Just store it like pm0.
				*/
			}


			CHECK_QUEUE_CAP

			cur=&input_queue[queue_ind];
			*cur=*iv;

			if (cur->prio == 1 || (cur->prio ==0 && sort_low_prio)){

				while (cur != input_queue){
					swap=0;
					if ((cur-1)->initial){
						break;
					}
					if (queue_use_ind + 1 == (u64)(cur-input_queue) )
					{
						break;
					}

					if (cur->prio==0 && (cur-1)->fixed_loc){
						break;
					}

					if (cur->prio < (cur-1)->prio){
						break;
					}else if (cur->prio > (cur-1)->prio){
						swap=1;

					}else{
						/*
							Same priority class
						*/

						if (!cur->marked && (cur-1)->marked){
							break;
						}else if (cur->marked && !(cur-1)->marked){
							swap=1;
						}else if (cur->total_blocks>(cur-1)->total_blocks){
							swap=1;
						}else if (cur->total_blocks == (cur-1)->total_blocks){
							if(cur->depth > (cur-1)->depth){
								swap=1;
							}else if(cur->depth == (cur-1)->depth){
								if(cur->total_hits > (cur-1)->total_hits){
									swap=1;
								}else{
									break;
								}
							}
							else{
								break;
							}
						}else{
							break;
						}
					}
					if (swap){
						temp=*cur;
						*cur=*(cur-1);
						*(cur -1)=temp;
						cur--;
					}

				} //end while
			} //end if
			break;
		case 2:
			/*
				This is linear mode
				No sorting is done.
				The queue is built similar to something
				like AFL
			*/
			cur=&input_queue[queue_ind];
			*cur=*iv;
			break;
		default:
			aexit("Undefined mode");

	} //end switch




	cur->initial = 0;

	/*
		Balance low priority inputs in lpq_balance=0 mode
	*/
	if (cur->prio ==0 && lpq_balance==0){
		if (LP_PROB_FIX){
			cur->fixed_loc=1;
		}
	}
	//printf("added %s to %lu total %d\n",cur->i_path,(u64)(cur-input_queue),queue_ind);
	queue_ind++;
	if (queue_ind == INPUT_MAX){
		/*
			We start to overwrite the queue from
			beginning
		*/
		queue_ind=0;
	}

	/*
		Input added
		Let the caller know where it was added
		for statistics and sanity check
	*/
	*add_indx = (int)(cur-input_queue);

	return 1;
}
/*
	Aside from adding, we count total
	covered blocks here
*/
void block_add_traced(struct block_info_local *b){
	struct block_info_local *cur;
	struct block_info_local temp;

	/*
		If we are here we are sure that
		this is a new block
	*/

#ifndef NESTED_COUNT
	/*
		Add only one block
	*/

	total_covered++;
	//printf("::: %lx %lu \n",b->id,total_covered);
	//printf(" %lx:%d %d\n",b->id,1,total_covered);
#else
	/*
		Add the block weight
		Keep in mind that it's only for reporting in
		add_stat_entry(). This is not the number of
		basic blocks based on our CFG and no the number
		that is used in Queue secion of the board.
		Since neted blocks are not stored as independent
		blocks in the CFG, we use the weight of their
		independent block to count them.
	*/
	/*if (*((u8*)comm_adr+T(b->id))==0)
	{
		aexit("Invalid compound weight: init:%lx %lx:%d",init_section,b->id,*((u8*)comm_adr+T(b->id)));
	}*/
	//printf("NC %lx:%d %d\n",b->id,*((u8*)comm_adr+T(b->id)),total_covered);
	total_covered+=*((u8*)comm_adr+T(b->id));
#endif

	cur=&sorted_blocks[++block_ind];
	*cur=*b;
	while (cur != sorted_blocks){
		if (cur->id<(cur-1)->id){
			temp=*cur;
			*cur=*(cur-1);
			*(cur -1)=temp;
		}else{
			break;
		}
		cur--;
	}

}
/*
	return
		1- Existing block, bigger hit
		2- New block
		3- Existing block, not bigger hit
*/
u8 check_block(struct block_info_local *b){
	long low=0,high=block_ind,mid;
	while (low<=high){
		mid=(high+low)/2;
		//printf("HEREv %ld %ld %ld\n",low,high,mid);
		if (b->id==sorted_blocks[mid].id){
			/*
				Ok found
				check hits
			*/

			if (sorted_blocks[mid].hits >= b->hits){

				return 3; //disregard
			}else{
				/*
					His increased
				*/
				sorted_blocks[mid].hits =b->hits;
				return 1;
			}
		}else if(b->id>sorted_blocks[mid].id){
			low = mid+1;
		}else{
			high = mid-1;
		}
	}
	return 2;
}
int next_queue_ind(){
	/*
		queue_ind must always be greater
		than queue_use_ind
	*/
	if (queue_use_ind+1 == queue_ind){
		queue_use_ind=-1;
		qc++;
	}
	return ++queue_use_ind;
}
void prepare_file_feed(char *base_input){
	/*char cmd[2048];
	sprintf(cmd,"cp %s %s",base_input,feed_file_path);
	system(cmd);*/

	raw_cp(base_input,feed_file_path,-1);
}
/*
	init function
*/
void prepare_input_feed(){

	if (file_feed){
		/*
			Write the current input to the
			given file in -f
		*/
		prepare_file_feed(input_queue[0].i_path);

	}else{
		/*
			This will be redirected to stdin
			O_EXCL will help us to see whether
			previous run of fuzzer has cleaned up
			or not
			FIXME: change to read only
		*/
		feed_fd = open(cinp_file_path,O_RDWR | O_CREAT /*| O_EXCL*/,0600);
		if (feed_fd<0){
			printf("%s\n",strerror(errno));
			aexit("Can't create input file");
		}
	}
}
void run_starter(){

	char buf;
	int n;

	/*
		Create shared memory segments
	*/
	shm_id = shmget(IPC_PRIVATE,SHM_SIZE,IPC_CREAT | IPC_EXCL | 0600);

	if (shm_id<0){
		aexit("Can't create shared memory (main)");
	}
	shm_adr = shmat(shm_id,0,0);

	if (shm_adr==(void*)-1){
		aexit("Can't attach to shared memory (main).");
	}
	memset(shm_adr,0,SHM_SIZE);
	shm_end = shm_adr+SHM_SIZE;

	comm_id = shmget(IPC_PRIVATE,COMM_SHM_SIZE,IPC_CREAT | IPC_EXCL | 0600);

	if (comm_id<0){
		aexit("Can't create shared memory (comm)");
	}
	comm_adr = shmat(comm_id,0,0);

	if (comm_adr==(void*)-1){
		aexit("Can't attach to shared memory (comm).");
	}
	memset(shm_adr,0,SHM_SIZE);
	memset(comm_adr,0,COMM_SHM_SIZE);

	/*
		Set lock
		This will not change
	*/
	lib_lock = shm_adr;

	pipe(cmd_send);
	pipe(response_recv);

	starter_id = fork();

	if (!starter_id){

		/*
			I'm starter
			Establish communication channel
		*/
		dup2(cmd_send[0],FD_IN);
		dup2(response_recv[1],FD_OUT);

		close(cmd_send[1]);
		close(response_recv[0]);
		close(cmd_send[0]);
		close(response_recv[1]);

		if (file_feed){
			close(0);
			/*
				We expect the user to have correctly written
				the input filename as the argument to the target
				It has to be the same as -f for the fuzzer

				NOTE: There are cases that in this mode
				target still prompts for user input
				since stdin is closed, it will fail
				If it keeps trying, it will eventually timeout.
			*/
		}else{
			dup2(feed_fd,0);
			close(feed_fd);
		}
		dup2(dev_null,1);
		dup2(dev_null,2);


		/*
			Send it to its own session to prevent
			from potentially messing up our fuzzer's terminal.
			Note that in this case reoppeing /dev/tty won't
			give the forkserver terminal access. Calling Open on
			/dev/tty fails with 'No such device or address'.
		*/
#ifndef DEBUG_MODE
		if (setsid()==-1){
			aexit("setsid()");
		}
#endif
		close(dev_null);

		/*
			In case our lib is not installed
			system-wide.
			Is that case we assume the user is
			running the fuzzer from the directory
			that libzh and zharf reside.
		*/
		setenv("LD_LIBRARY_PATH",".",1);
		setenv("LD_PRELOAD","libarv.so",1);

		execv(target_path,target_argv);
		n=open("/dev/tty",O_WRONLY);
		if (n!=1){
			dup2(n,1);
			close(n);
		}
		aexit("Running starter (%s) failed. %s",target_argv[0],strerror(errno));
		/*
			TODO: Terminate parent
		*/
	}

	/*
		Back to fuzzer
	*/
	close(response_recv[1]);
	close(cmd_send[0]);

	/*
		Wait for stage 1 child to terminate
		and reap it
	*/
	arep("Waiting for libarv to set up internal structures...");
	waitpid(starter_id,0,0);

	/*
		Now read the main forkserver pid
	*/

	n=read(response_recv[0],&starter_id,4);
	if (n<4){
		aexit("Communication with fork server ceased unexpectedly (read()). Check your target program.");
	}

	n=write(cmd_send[1],&shm_id,4);
	if (n<1){
		aexit("Communication with fork server ceased unexpectedly. (write())");
	}
	/*
		Must use same mapping for consistency
		in tree structures
	*/
	n=write(cmd_send[1],&shm_adr,8);
	if (n<1){
		aexit("Can't write to starter");
	}

	n=write(cmd_send[1],&comm_id,4);
	if (n<1){
		aexit("Can't write to starter");
	}

	/*
		communication channel doesn't need to be
		mapped to the same base address in fuzzer
		and library.
	*/

#ifdef NESTED_COUNT
	n=read(response_recv[0],&init_section,8);
	if (n<8){
		aexit("Can't read response from starter");
	}
#endif
	/*
		Wait for ready signal from stage 2 starter
	*/

	n=read(response_recv[0],&buf,1);
	if (n<1){
		aexit("Can't read response from starter");
	}
	/*
		Starter is ready to receive
		fork command. We're done here
	*/

	arep("Starter ready.");
	arep("Wait for DCG to finish first pass...");

//aexit("TEST");
}
void read_inputs(){
	/*
		Read all inputs
		add them to the queue
	*/
	struct dirent **entry_list;
	int ent_count;
	int i;
	struct stat st;


	ent_count = scandir(input_dir,&entry_list,0,alphasort);

	for (i=0;i<ent_count;i++){
		char *ent_name;
		char ent_path[MAX_PATH];
		char m_path[MAX_PATH];

		ent_name = 	entry_list[i]->d_name;
		if (strcmp(ent_name,".")==0 || strcmp(ent_name,"..")==0) continue;

		strcpy(ent_path,input_dir);
		if (input_dir[strlen(input_dir)-1]!='/')
			strcat(ent_path,"/");
		strcat(ent_path,ent_name);
		if (lstat(ent_path,&st)<0){
			printf("%s\n",strerror(errno));
			aexit("stat failed");
		}

		if(!S_ISDIR(st.st_mode)){
			/*
				Add to queue
			*/

			if ((st.st_size < MIN_INPUT_SIZE || st.st_size > MAX_INPUT_SIZE)){
				aexit("Input '%s' violates default size limit (%lu)",ent_path,st.st_size);
			}
			sprintf(m_path,"%s/states/%s",output_dir,ent_name);

			queue_add(ent_path,m_path,1,st.st_size);
			free(entry_list[i]);
			input_count++;
		}
	}
	if (entry_list)
		free(entry_list);

}

size_t raw_cp(char *src,char *dst,int dstfd){
	ssize_t sz;
	int fd_in,fd_out;
	u8 buf[2048];
	size_t written_size=0;

	fd_in=open(src,O_RDONLY);
	if (dstfd==-1){
		/*
			Destination might not exist.
			Pass O_CREAT
		*/
		fd_out=open(dst,O_WRONLY|O_CREAT,0644);

	}
	else
		fd_out=dstfd;

	if (fd_in < 0 || fd_out<0)
		aexit("raw_cp: open() ");

	while((sz=read(fd_in,buf,2048))){
		if (sz<0){
			aexit("read()");
		}

		if (write(fd_out,buf,sz)!=sz){
			aexit("write()");
		}
		written_size+=sz;
	}

	close(fd_in);
	if (dstfd==-1)
		close(fd_out);
	return written_size;

}
/*
	Write input_base to current_input
	reset feed_fd
	Input base is either a path in queue (init phase)
	or tmp_input (generator)
*/

void modify_input(char *input_base){
	int new_input_len=0;

	lseek(feed_fd,0,SEEK_SET);

	new_input_len=raw_cp(input_base,0,feed_fd);

	if (ftruncate(feed_fd,new_input_len))
		aexit("can't truncate current input");

	lseek(feed_fd,0,SEEK_SET);


}
void start_timer(){
	struct itimerval itimer;

	/*
		no repition
	*/
	//printf("waiting for %luus\n",active_timeout);
	if (active_timeout>1000000){
		/*
			We are in initialization stage
			Let ptrace run and library identify
			expensive nodes.
			3 Sec seems reasonable
		*/
		itimer.it_interval.tv_sec = 0;
		itimer.it_interval.tv_usec = 0;

		itimer.it_value.tv_sec = 3;
		itimer.it_value.tv_usec = 0;

		setitimer(ITIMER_REAL,&itimer,0);
	}else{
		/*
			Fuzzing timeout
		*/
		itimer.it_interval.tv_sec = 0;
		itimer.it_interval.tv_usec = 0;

		itimer.it_value.tv_sec = 0;
		itimer.it_value.tv_usec = active_timeout;

		setitimer(ITIMER_REAL,&itimer,0);
	}
	target_timedout = 0;

}
void stop_timer(){
	struct itimerval itimer;

	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;

	itimer.it_value.tv_sec = 0;
	itimer.it_value.tv_usec = 0;

	setitimer(ITIMER_REAL,&itimer,0);

}
/*
	When this function is called
	target is either dead or will be killed here
*/
int do_term_check(){
	int n;
	int exit_status;
	int sig_val;

	/*
		Set the timeout
	*/
	start_timer();
	/*
		Wait for child exit
	*/
	n = read(RIN, &exit_status, 4);

	if (n<4){
		aexit("Read exit status failed: %s",strerror(errno));
	}

	if (exit_status==-2){
		aexit("Library crashed.");
	}

	/*
		Do these two lines fast
	*/
	target_id = 0;
	stop_timer();
	//*********************

	/*
		Process may have died while running
		in lib. Release the lock to allow
		our evaluator read the map

	*/

	if (!parallel_mode)
		RELEASE_LOCK

	debug_exit_stat = exit_status;

	/*
		Why did it exit?
	*/

	if (WIFEXITED(exit_status)){
		//arep("target exited normally with %d",WEXITSTATUS(exit_status));
		return TRG_EX_NORMAL;
	}



	if (WIFSIGNALED(exit_status)){
		sig_val = WTERMSIG(exit_status);

		if (target_timedout && sig_val==SIGKILL){
			//awarn("tm out");
			return TRG_EX_TMOUT;
		}else {
			/*
				No other signal can happen when target is traced.
				If we're here either the child process is not traced
				for example the library might have crashed for signals
				ofther than SEGV (since SEGV is caught in library) or
				the status word is not valid.
				The former happens with LLVM compiled programs with
				address sanitizer enabled.
			*/
			aexit("Unexpected state. Possible corrupted status word. SIGVAL: %d, status word: %x",sig_val,exit_status);
		}
	}else{

		if(WIFSTOPPED(exit_status)){
			/*
				Crashed in blackbox mode
			*/
			sig_val = WSTOPSIG(exit_status);


			/*if (sig_val==SIGCHLD){
				//aexit("sig pipe");
				return TRG_EX_NORMAL;
			}*/
			//printf("Target crashed with signal %d",sig_val);

			last_crash_sig = sig_val;
			return TRG_EX_CRASH;
		}
	}


	printf("[!] Reap is invalid: %d %d %d %x\n",WIFSTOPPED(exit_status),WSTOPSIG(exit_status),
		WTERMSIG(exit_status),exit_status);
	return -1;
}
void set_recv_timeout(struct timeval * t){
    if (setsockopt(net_sock,SOL_SOCKET,SO_RCVTIMEO,(char *)t,sizeof(struct timeval))<0){
        aexit("setsockopt failed\n");
    }
}
void init_socket(){
	struct timeval twait;

	/*
		Currently it's assumed the connection is TCP
	*/
	if (net_sock>0)
		close(net_sock);
	net_sock = socket(AF_INET,SOCK_STREAM,0);
	if (net_sock<1){
		aexit("socket() failed %s",strerror(errno));
	}
	twait.tv_sec = 0; //More than enough for local net
	twait.tv_usec =RECV_WAIT;
	set_recv_timeout(&twait);
}
int reconnect(){
	init_socket();
	if (connect(net_sock,(struct sockaddr*)&target_saddr,sizeof(target_saddr)))
		return 0; //failed
	return 1;
}

void reset_structures(){
	/*
		Reset everything for the new execution
		including clearing the shared memory
		In net mode this will make the initialization
		parts of the target disappear from the
		output graph if saved.


	*/
	dl_index=-1;
	graph_e_count=0;

	ACQUIRE_LOCK_CONDITIONED
	/*
		Be careful not to clear the lock
	*/
	memset(shm_adr+LOCK_DELTA,0,SHM_SIZE-LOCK_DELTA);
	RELEASE_LOCK

	nested_counter=0;
	indp_counter=0;
	skip_nodes=0;
}

/*
	Some programs may need cleanup before
	execution. This may be necessary because
	termination of the program due to timeout
	might have deprived it from carrying out
	its clean up stage!
	We accept user-defined terminal commands here
	to be run for this purpose.

	Users must be careful what commands
	are passed to arvin as the commands
	are not sanitized.
*/
void custom_cleanup(){
	int i;

	for (i=0;i<sizeof(cleanup_cmds)/8;i++){
		system(cleanup_cmds[i]);
	}

}
/*
	Spawn a new instance of the target

	init_run: only used in net mode
*/
int execute(int init_run){
	int n;
	int res;
	u8 con_waits =0 ;

	/*
		The execution will end because of
		3 reasons:
		1- Normal exit
		2- Crash
		3- Timeout

	*/

	if (!parallel_mode)
		reset_structures();

	if (enable_custom_cleanup)
		custom_cleanup();

	write(COUT,".",1);

	n=read(RIN,&target_id,4);
	//arep("Create target %d",target_id);
	if (n<4){
		aexit("target execution failed");
	}



	if (init_run || !net_mode){
		/*
			We want the target to exit
			It either exits itself or we
			terminate it
		*/

		res = do_term_check();
		if (res!=-1){
			//aexit("returning");
			return res;
		}
	}else{
		/*
			In network mode we don't restart the target
			for each input
			We send inputs until it crashes and fails to
			send a response back
			If that happens, only then we read the return status
			wait until target gets its stuff together and
			establish the connection
		*/


		while (1){
			usleep(CONNECT_WAIT);
			init_socket();

			con_waits++;
			if (connect(net_sock,(struct sockaddr*)&target_saddr,sizeof(target_saddr))){
				if (con_waits==CONN_MAX_RETRIES){
					do_term_check();
					//printf("%s\n",strerror(errno));
					aexit("Target doesn't respond to new connections.");
				}
			}else{

				break;
			}
		}
		return TRG_NET_MODE;

	}

	/*
		If we are here then it's an unexpected
		state and must be further investigated
	*/
	return -1;
}
void target_timeout(int signal){

	if (target_id>0){	//race condition prevention
		target_timedout = 1;


		/*

		//recursive scan - too slow, now worth it
		int i;
		FILE *f;
		char cmd[255];
		char buf[1024];
		int c;
		int index=0;
		int pi=0;
		int pids[1024]; // More than enough

		pids[pi++] = target_id;

		for (i=0;i<pi;i++){
			sprintf(cmd,"pgrep -P %d",pids[i]);
			f = popen(cmd,"r");
			while ((c=fgetc(f))!=EOF){
				if (c==10){
					buf[index]=0;
					index=0;
					sscanf(buf,"%d",&pids[pi++]);
					continue;
				}
				buf[index++] = c;
			}
			pclose(f);

		}

		for (i=pi-1;i>-1;i--){
			//awarn(">> %d",pids[i]);
			kill(pids[i],SIGKILL);
		}
		*/

		/*
			forget about getting the lock
			after this this function. This way fuzzer
			won't get stuck for a locked shm.
			This is done by ACQUIRE_LOCK_CONDITIONED
			In this case if shm is locked by children
			which may have crashed in lib tree will be discarded
			which is okay since the tree is probably
			corrupted anyways.
		*/

		if(kill(target_id,SIGKILL)){
			if (errno!=3) //3: target already dead
				aexit("kill(): %s",strerror(errno));
		}
		target_id=0;
	}
}
/*
	This saves the whole shared memory
	for debugging
*/
void save_memory(char *dir_path,char *stage){
	char fname[255];
	FILE *f;
	u64 n;

	if (debug_mode)
		return;

	sprintf(fname,"%s/shared_mem_%016lx_%s",dir_path,(u64)shm_adr,stage);

	f=fopen(fname,"w");
	if (!f){
		aexit("save_mem: Can't open file\n");
		return;
	}

	if ((n=fwrite(shm_adr,1,SHM_SIZE,f))<SHM_SIZE){
		printf("Incomplete memory save %lu\n",n);
	}
	fclose(f);

	arep("Memroy saved in %s",fname);
	output_count++;
}

void record(void *data,size_t size){
	FILE *f;
	char file_path[255];
	char tmp[128];

	strcpy(file_path,"tmp/");
	sprintf(tmp,"inp%lu",debug_rec_ind++);
	strcat(file_path,tmp);
	if (access("tmp",F_OK)){
		if (mkdir("tmp",0775)){
			printf("%s\n",strerror(errno));
			aexit("mkdir: record (tmp)");
		}
	}

	f=fopen(file_path,"w");
	if (!f){
		aexit("fopen(): record");
	}
	if (fwrite(data,1,size,f)<size){
		awarn("fwrite(): record");
	}
	fclose(f);
}

void store_reset(u8 store){
	char dir_path[255];
	char tmp[128];
	char cmd[1024];
	u64 i;

	if (store){
		strcpy(dir_path,"debug/");
		sprintf(tmp,"crash%lu",debug_rec_dir);
		strcat(dir_path,tmp);

		if (access(dir_path,F_OK)){
			if (mkdir(dir_path,0775)){
				printf("%s\n",strerror(errno));
				aexit("mkdir: store_reset (%s)",dir_path);
			}
		}

		for (i=0;i<debug_rec_ind;i++){
			sprintf(cmd,"cp tmp/inp%lu debug/crash%lu/",i,debug_rec_dir);
			system(cmd);
		}
		debug_rec_dir++;
	}
	debug_rec_ind = 0;

}
void znormal_exit(){
	terminate_units();
	arep("Exiting normally");
	rep_use_time();
	//print_queue();

	exit(0);
}
void int_handler(int signal){
	znormal_exit();
}
void pipe_handler(int signal){
	aexit("Received SIGPIPE, forkserver down?");
}

void rep_adrs(){
	printf("\n\nSHM: start %016lx end %016lx\n",(u64)shm_adr,(u64)(shm_adr+SHM_SIZE));
	printf("Root node: %016lx\n",(u64)shared_trace_tree->root);
	printf("Corrupted node: %016lx\n",(u64)_debug_node);
	printf("Corrupted node's child list: %016lx\n",(u64)_debug_node->children);
	if (_debug_node->children)
		printf("Corrupted node child node: %016lx\n",(u64)(_debug_node->children)->child_node);
	arep("HITS: %lu, Tree Nodes: %lu",shared_trace_tree->total_hits,shared_trace_tree->count);
}
void show_maps(){
	char cmd[255];
	int c;
	FILE *f;
	char buf[8192];
	int index=0;

	memset(buf,0,8192);
	sprintf(cmd,"cat /proc/%d/maps",getpid());
	f = popen(cmd,"r");

	while ((c=fgetc(f))!=EOF){
		buf[index++] = c;
	}

	pclose(f);

	printf("\nMemory mappings: \n%s",buf);
}

void sigf_handler(int signum, siginfo_t *si, void *ucontext){
	void *buf[100];
	int n;
	char **names;
	int i;

	/*
		Avoid potential recursion
	*/
	signal(SIGSEGV,SIG_DFL);

	n=backtrace(buf,100);
	names=backtrace_symbols(buf,n);



	printf("\n\n*************** " CRED "FATAL STATE " CNORM "***************\n");
	for (i=0;i<n;i++){
		printf("> %s\n",names[i]);
	}
	printf("Address fault: %016lx\n",(u64)si->si_addr);
	//rep_adrs();

	show_maps();

	aexit("Exiting due to segmentation fault");
}
void dfs_add(ID_SIZE id){
	ID_SIZE *cur;
	ID_SIZE temp;
	cur=&dfs_list[++dl_index];
	/*
		add sorted
	*/
	*cur=id;

	while (cur != dfs_list){
		if (*cur<*(cur-1)){
			temp=*cur;
			*cur=*(cur-1);
			*(cur -1)=temp;
		}else{
			break;
		}
		cur--;
	}

}

u8 visited(ID_SIZE id){
	long low=0,high=dl_index,mid;
	while (low<=high){
		mid=(high+low)/2;
		if (id==dfs_list[mid]){
			return 1;
		}else if(id > dfs_list[mid]){
			low = mid+1;
		}else{
			high = mid-1;
		}
	}
	return 0;
}
void save_netdata(void *data,u64 size,char *file_path){
	FILE *nets ;
	char fp[255];
	char tmp[16];

	strcpy(fp,file_path);
	sprintf(tmp,"%d",save_net_i++);
	strcat(fp,tmp);
	nets = fopen(fp,"w");
	if (!nets){
		aexit("save net");
	}
	fwrite(data,size,1,nets);
	fclose(nets);

}
void save_debug_info(){
	static int debug_index = 0;
	FILE *f;
	char dir_path[255];
	char file_path[255];
	char tmp[128];
	char content[1024];

	strcpy(dir_path,"debug/");
	sprintf(tmp,"debug%d",debug_index++);
	strcat(dir_path,tmp);
	if (access("debug",F_OK)){
		if (mkdir("debug",0775)){
			printf("%s\n",strerror(errno));
			aexit("mkdir: save_debug_info (debug)");
		}
	}
	if (access(dir_path,F_OK)){
		if (mkdir(dir_path,0775)){
			printf("%s\n",strerror(errno));
			aexit("mkdir: save_debug_info (%s)",dir_path);
		}
	}
	strcpy(file_path,dir_path);
	strcat(file_path,"/");
	strcat(file_path,"info");

	f=fopen(file_path,"w");
	if (!f){
		printf("%s , %s\n",strerror(errno),file_path);
		aexit("open(): save_debug_info");
	}

	strcpy(content,"Exit reason: ");
	switch (debug_exit_code){
		case TRG_EX_NORMAL:
			strcat(content,"normal exit\n");
			break;
		case TRG_EX_TMOUT:
			strcat(content,"timeout\n");
			break;
		case TRG_EX_CRASH:
			strcat(content,"crash\n");
			break;
		case TRG_NET_MODE:
			strcat(content,"Traget is alive\n");
			break;
		default:
			sprintf(tmp,"Unknown: %d %08x\n",debug_exit_code,debug_exit_stat);
			strcat(content,tmp);
	}
	strcat(content,"Elapsed time: ");
	strcat(content,convert_time(tmp));
	strcat(content,"\n");
	fwrite(content,1,strlen(content),f);
	fclose(f);

	strcpy(file_path,dir_path);
	strcat(file_path,"/");
	strcat(file_path,"input");

	save_netdata(debug_data,debug_data_size,file_path);

	if (should_save_mem){
		save_memory(dir_path,"debug");
	}
	rep_adrs();


}
#define get_node_type(info_byte)	(info_byte & BLOCK_TYPE_NESTED)
#define get_node_mark(info_byte)	(info_byte & BLOCK_MARKED)




void rep_paths(){
	FILE *f;
	time_t cur_time;


	time(&cur_time);
    cur_time-=start_time;



	f=fopen("./all_paths","a");
	if (!f){
		aexit("add_path_stat_entrey: fopen");
	}
	fprintf(f,"%d:%lu:%lu\n",_all_found_paths,total_exec,cur_time);
	fclose(f);

}

/*
	TODO:PAPER STUFF

int _total_edges=0,_total_edges_first=0,_total_nodes_first=0;


u64 __first_exe,__third_exe;
*/
//u64 tot_dfs=0;
int dfs(struct node *n,FILE *graph_file){
//_total_edges++;
	char line[255];
	int node_depth=0;
	char label[2];
	char color[10];
	ID_SIZE _pid;


	if (!check_adr(n)){
		//awarn("Invalid node entered dfs(): %016lx",n);
		has_corrupted_shm=1;
		return 0;
	}
	_debug_node=n;


	/*
		Check for possible loops in tree
	*/
	if (visited(n->id)){
		return 0; //No double count
	}
	//printf("%016lx\n",n->id);
	//tot_dfs++;
	dfs_add(n->id);


	/*
			This is where we check for block type and
			add it to the respective counter
			The reason for doing it here is to be able
			to set correct colors
	*/
	if (get_node_type(n->info)==0){
		strcpy(color,"red");
		label[0]='I';
		indp_counter++;
	}else{
		strcpy(color,"orange");
		label[0]='N'; //nested
		nested_counter++;
	}

	if (get_node_mark(n->info)){
		marked_tree = 1;

		//awarn("MARKED TREE DETECTED %d",all_marked_inputs);

		//rep_marked();
	}

	if (n->children){
		struct child_ptr *c;
		struct node *temp;

		c=n->children;
		if (!check_adr(c)){
			//awarn("dfs(): Invalid child");
			has_corrupted_shm=1;
			return 0;
		}
		label[1]=0;
		_pid = n->id;

		last_trace_leaves--;
		//printf("%lu\n",n->id);

		while (c){
			int branch_depth;

			//printf("%08lx->%08lx %016lx\n",n->id,c->child_node->id,(u64)c->next_child);
			last_trace_leaves++;
			if (should_store_graph){
				ID_SIZE _cid;

				if (skip_nodes<SHOULD_SKIP_COUNT){
					skip_nodes++;
					goto graph_end;
				}


				if (!check_adr(c->child_node) ){
					/*save_debug_info();*/
					//awarn("Got invalid child node%016lx\n",c->child_node);
					has_corrupted_shm=1;
					return 0;
				}

				_cid=(c->child_node)->id;

				/*
					Nested blocks have two possible meanings:
					1- It contains anouther block with the same
						id like when there's a function call
						in between
						ID -> call -> ID
						^
						|
						This one. the nested bit is turned on after
						returning from call

					2- It's the internal block in a nested structure
						with the same id as the container
						ID -> call -> ID
									  ^
									  |
								This one
						It's unlikely that the second is visited
						before the first one in target execution.

				*/
				if (graph_e_count<MAX_EDGES_VISIBLE){
					sprintf(line,"\"0x%016lx\" [color=%s, label=\"%s\" penwidth=3.0]\n"
					/*
						L is either leaf or a block
						that at the latest hit has not jumped
						to a basic block other than its direct
						parent
					*/
								 "\"0x%016lx\" [color=green, label=\"L\"]\n"
								 "\"0x%016lx\"->\"0x%016lx\" [color=white]\n"
									,_pid,color,label,_cid,_pid,_cid);
					//printf("%s",line);
					if(fwrite(line,1,strlen(line),graph_file) < strlen(line)){
						aexit("fwrite(): build graph");
					}
					graph_e_count++;
				}
			}//end if should_store_graph
graph_end:
			if (!check_adr(c->child_node) ){
				/*save_debug_info();*/
				//awarn("sending Invalid child node %016lx\n",c->child_node);
				has_corrupted_shm=1;
				return 0;
			}
			temp = c->child_node;
			branch_depth = dfs(temp,graph_file);
			if (branch_depth > node_depth){
				node_depth = branch_depth;
			}

			c=c->next_child;
		}

	}else{

	}

	return node_depth+1;

}
/*
	This function can only be called after eval_tree (not before)

	The function checks both the coverage of the
	last iteration and changes the global coverage
	accordingly in the local memory.
	Read all blocks in the linear list in shared memory
	and search them in local blocks
	Add new blocks and adjust hit counts

	Note that the shape of the tree may change with
	the same blocks and hit counts. To find that
	kind of change we can calculate a checksum but
	comparing it with all members of the queue may
	not be necessary. Instead we only check the depth

	return value: 1 (new block or increased depth)
				  2 (only hit count increased)

*/


int coverage_changes(){
	u8 changed=0;
	struct block_info *b = (struct block_info *) (shm_adr + 256);
	struct block_info_local temp_block;



	if(!check_adr(b)){
		//awarn("coverage_changes(): enter: Corrupted %016lx",b);
		return -1;
	}
	while(b->id){

		u8 r;
		temp_block.id = b->id;

		if (!check_adr(b->ptr)){
			//awarn("coverage_changes(): Corrupted %016lx",b->ptr);
			return -1;
		}
		temp_block.hits = (b->ptr)->hits;

		r=check_block(&temp_block);
		if (r==1){
			/*
				existing block with
				new hit
				check_block has updated the hit

				Starvation hazard
				The problem with storing a new input only
				because it has a bigger hit number can
				result in consequentive similar inputs
				which starv other inputs in the queue.
				To resolve this issue I decrease the priority of such
				inputs
			*/
			//arep("New hit %016lx %lu",temp_block.id,temp_block.hits);
			if (!changed) changed = 2; //low priority input


		}else if(r==2){
			/*
				new block
			*/
			block_add_traced(&temp_block);
			//arep("New block %016lx %lu",temp_block.id,temp_block.hits);
			changed = 1;
		}
		b++;

	}


	if (depth_grew){
		/*
			we always consider increasing depth
			a good sign.
			This also makes the fuzzer store the
			input even if changed=0 but depth has increased
		*/
		changed=1;

	}

	return changed;


}

void vars(u64 blocks_count,u64 children_count,u64 linear_size,u64 nodes_size,u64 children_size,u64 total_extract_size){
	printf(" >> %lu %lu %lu %lu %lu %016lx\n",
	blocks_count,children_count,linear_size,nodes_size,children_size,total_extract_size);
}
/*
	Returns a pointer to a temporary memory
	containing the extracted map to hash
*/

//nodes only
/*
void *extract_map(u64 *size){

	struct block_info * blocks= (struct block_info *) (shm_adr + 256);
	u64 blocks_count ; //which is also node count
	u64 linear_size;
	u64 total_extract_size;
	void *tmp_map;


	blocks_count = shared_trace_tree->count;
	linear_size = blocks_count*sizeof(struct block_info);
	total_extract_size = linear_size ;
	//vars(blocks_count,children_count,linear_size,nodes_size,children_size,total_extract_size);
	if (total_extract_size > SHM_SIZE){
		aexit("Extract size %016lx exceeds map size %016lx",total_extract_size,SHM_SIZE);
	}
	tmp_map= mmap(0,total_extract_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);

	if (!tmp_map){
		aexit("extract_map: mmap");
	}





	memcpy(tmp_map ,(void *)blocks,linear_size);


	*size = total_extract_size;
	//printf(">>>>>> HERE2\n");
	return tmp_map;


}
*/
//full hash
void *extract_map(u64 *size){

	struct block_info * blocks= (struct block_info *) (shm_adr + 256);
	struct node * node_pool = (struct node *)(shm_adr+(u64)(SHM_SIZE>>2));
	struct child_ptr* child_pool = (struct child_ptr*)(shm_adr+(u64)(3*(SHM_SIZE>>2)));
	u64 blocks_count ; //which is also node count
	u64 children_count; //Number of allocated child blocks
	u64 linear_size;
	u64 nodes_size;
	u64 children_size;
	u64 total_extract_size;
	void *tmp_map;

	blocks_count = shared_trace_tree->count;
	children_count = shared_trace_tree->total_children;

	linear_size = blocks_count*sizeof(struct block_info);
	nodes_size = blocks_count*sizeof(struct node);
	children_size = children_count*sizeof(struct child_ptr);

	total_extract_size = 256 + linear_size + nodes_size + children_size;
	//vars(blocks_count,children_count,linear_size,nodes_size,children_size,total_extract_size);
	if (total_extract_size > SHM_SIZE){
		aexit("Extract size %016lx exceeds map size %016lx",total_extract_size,SHM_SIZE);
	}
	tmp_map= mmap(0,total_extract_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);

	if (tmp_map==MAP_FAILED){
		aexit("extract_map: mmap");
	}

	memcpy(tmp_map,shm_adr,256);
	memcpy(tmp_map + 256,(void *)blocks,linear_size);
	memcpy(tmp_map + 256 + linear_size,(void *)node_pool,nodes_size);

	memcpy(tmp_map + 256 + linear_size + nodes_size,(void *)child_pool,children_size);

	*size = total_extract_size;
	//printf(">>>>>> HERE2\n");
	return tmp_map;


}

/*
	Our shared memory is huge. For most programs
	only a small part of the shm is used. We
	want to extract the used part and hash it
*/
u32 do_hash(){
	void *tmp_adr;
	u64 size;
	u32 hash;

	tmp_adr=extract_map(&size);
	hash = hashmap(tmp_adr,size,HASH_SEED);

	//release memory
	if (munmap(tmp_adr,size)){
		aexit("do_hash() - munmap");
	}
	//printf("size %lu hash %d\n",size,hash);
	//aexit("HASH: %u",hash);
	return hash;
}
/*
	When a corruption in the tree is detected we
	don't decide about what to do with the target
	and we'll leave that decision to the generator
	At the situation of tree corruptoin the targt
	might already be dead or still running in net mode
*/
void eval_tree(struct input_val *iv){
	FILE *graph_file;
	char gname[1064];
	//char cmd[1064];
	char g_head[]="digraph trace{\n"
				"graph [bgcolor=black]\n"
				"labelloc=t\n"
				"fontcolor=white\n"
				"label= \"\\n\"\n"
				"node [color=red "
				"fontcolor=white fillcolor=black style=filled]\n";
	char g_tail[] ="}\n";

	depth_grew = 0;
	invd_tree_nodes_grew = 0;
	last_trace_leaves = 1; //1 for root
	marked_tree = 0;

	if (!check_adr(shared_trace_tree->root)){
		/*save_debug_info();*/
		//awarn("Invalid tree; no root: %016lx %016lx-%016lx",shared_trace_tree->root,shm_adr,shm_end);

		/*
			We use this to catch premature death
			So we don't set has_corrupted_shm
		*/
		//has_corrupted_shm=1;
		iv->depth = 0;
		return ;
	}
	if (debug_mode)
		arep("HITS: %lu, Tree Nodes: %lu",shared_trace_tree->total_hits,shared_trace_tree->count);



	/*
		Graph building
	*/
	if (should_store_graph){

		sprintf(gname,"%s/graph%d.dot",output_dir,(gcount % GRAPH_FILES_COUNT));
		//sprintf(cmd,"dot -Tpng -o %s/graph%d.png %s/graph%d.dot",output_dir,(gcount % GRAPH_FILES_COUNT)
		//		  ,output_dir,(gcount % GRAPH_FILES_COUNT));

		gcount++;
		graph_file=fopen(gname,"w");
		if (!graph_file){
			printf("%s\n",strerror(errno));
			aexit("fopen(): graph: %s",gname);
		}
		fwrite(g_head,strlen(g_head),1,graph_file);

		//printf("::: %lu\n",shared_trace_tree->count);

		shared_trace_tree->depth=dfs(shared_trace_tree->root,graph_file);
		fwrite(g_tail,strlen(g_tail),1,graph_file);
		fclose(graph_file);
		//system(cmd);
		//unlink(gname);


	}else{
		shared_trace_tree->depth=dfs(shared_trace_tree->root,0);
		//aexit("??? %d %d %d",tot_dfs==shared_trace_tree->depth,tot_dfs,shared_trace_tree->depth);
	}

	if (debug_mode)
			arep("Tree Depth: %d\n",shared_trace_tree->depth);

	/*
		Set iv
	*/
	if (shared_trace_tree->depth < 1){
		//awarn("Invalid depth in trace tree");
	}

	iv->depth = shared_trace_tree->depth;
	if (iv->depth > max_depth){
		max_depth = iv->depth;
		depth_grew = 1;

	}
//TODO: paper stuff. remove
//if (iv->depth < min_depth){
//	min_depth=iv->depth;}


	iv->total_blocks=shared_trace_tree->count;
	if (iv->total_blocks > max_coverage_tree_nodes){
		max_coverage_tree_nodes = iv->total_blocks;
		invd_tree_nodes_grew = 1;
	}
	iv->total_hits=shared_trace_tree->total_hits;
	iv->leaves = last_trace_leaves;

	/*
		Calculate checksum here
	*/
	if (need_csum){
		iv->hash = do_hash();
		current_csum = iv->hash;
	}
	total_target_hits+=shared_trace_tree->total_hits;
	last_trace_nodes=shared_trace_tree->count;



}
/*
	debug and exit
*/
void debug_memory(void *adr){
	char fname[255];
	FILE *f;

	sprintf(fname,"tmp/shared_mem_%016lx",(u64)adr);
	printf("Openning %s\n",fname);
	f=fopen(fname,"r");
	struct input_val tmp;

	if (!f){
		printf("Can't load memory\n");
		return;
	}

	shm_adr = mmap(adr,SHM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	shm_end = shm_adr + SHM_SIZE;
	if (shm_adr==MAP_FAILED){
		aexit("Can't allocate memory");
		return;
	}
	if (shm_adr != adr){
		aexit("Couln't load memory where it should be");
	}
	shared_trace_tree = shm_adr + LOCK_DELTA + PROT_VARS_DELTA;

	if (fread(shm_adr,1,SHM_SIZE,f)<SHM_SIZE){
		aexit("Incomplete memory read");
	}
	fclose(f);

	eval_tree(&tmp);
	arep("eval complete");

	if (munmap(shm_adr,SHM_SIZE)){
		aexit("debug_memory: munmap()");
	}
}



/*
	Check whether this mutation could already been
	produced my bitflip phase
	Idea from AFL
*/
u8 bitflip_check(u32 val_diff){
	u8 shift_count=0;

	//printf("bf check %08x\n",val_diff);
	if (!val_diff) return 0;

	while((val_diff & 1) !=1){
		val_diff>>=1;
		shift_count++;
	}

	if (val_diff==3 || val_diff==5 || val_diff==15){
		return 1;
	}

	if (shift_count & 7) return 0;

	if (val_diff == 0xff || val_diff==0xffff || val_diff==0xffffffff){
		return 1;
	}

	return 0;
}
u8 check_interesting(size_t byte_index,int value){
	u8 *loc8;
	u16 *loc16;
	u8 *i_adr = (u8 *)&value;
	int i=0,j=0;
	/*
		Check s8
	*/
	for (i=0;i<4;i++){
		if (!mut_map[byte_index+i]) continue;
		loc8 = i_adr + i;
		for (j=0;j<sizeof(set_8_ints);j++){
			/*
				Be careful not to compare signed and unsigned
				entities since the compiler might behave
				in contrast to what you expect
			*/
			if (((u8)set_8_ints[j]) == *loc8){
				return 1;
			}
		}
	}

	/*
		Check s16
	*/
	for (i=0;i<3;i++){
		if (!mut_map[byte_index+i] && !mut_map[byte_index+i+1]) continue;
		loc16 = (u16*)(i_adr + i);
		for (j=0;j<sizeof(set_16_ints);j++){
			if (((u16)set_16_ints[j]) == *loc16){
				return 1;
			}
		}
	}

	/*
		Check s32
	*/

	if (!mut_map[byte_index]  && !mut_map[byte_index+i+1]
		 && !mut_map[byte_index+i+2]  && !mut_map[byte_index+i+3]) return 0;

	for (j=0;j<sizeof(set_32_ints);j++){
		if (set_32_ints[j] == value){
			return 1;
		}
	}


	return 0;
}
/*********** Mutation functions ****************/


#define FLIP(start_adr,bit_n)	do{\
									u8 *st  = (u8 *)start_adr;\
									u64 bn  = (u64)bit_n;\
									st[(bn)/8] ^= (1 << (7 & bn));\
								}while(0)

#define REPMUT(cmt) arep("Current mut technique: %s",cmt)
/*
	param start: report back where mutation started

*/
u8 bit_flip(void *data,size_t size,u8 op,size_t *start){
	/*
		This is bit number
		0 ... size*8-1
	*/
	static size_t bit_flip_index=0;
	u8 ignore =0 ;

bit_flip_start:


	*start = bit_flip_index >> 3;
//if (bit_flip_index==309*8)	{xtemp=1; }else xtemp=0;
	if (bit_flip_index > (size<<3)){
		aexit("FLIP1: index out of range");
	}

	if (!mut_map[*start]){
		ignore = 1;

		*start=-1;

	}else{
		FLIP(data,bit_flip_index);
	}

	cmt = "[MUT-B-F-1]";

	if (!bit_flip_index) REPMUT(cmt);

	bit_flip_index++;


	if (bit_flip_index==size*8){
		bit_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto bit_flip_start;
	}

	return op;
}

u8 bit2_flip(void *data,size_t size,u8 op,size_t *start){
	static size_t bit_flip_index=0;
	u8 ignore = 0;

bit2_flip_start:

	*start = bit_flip_index >> 3;

	if (bit_flip_index > (size<<3)-1){
		aexit("FLIP2: index out of range");
	}

	if (!mut_map[*start] /*&& !mut_map[((bit_flip_index+1) >> 3)]*/){
		ignore = 1;

		*start=-1;

	}else{
		FLIP(data,bit_flip_index);
		FLIP(data,bit_flip_index+1);
	}
	cmt = "[MUT-B-F-2]";
	if (!bit_flip_index) REPMUT(cmt);
	bit_flip_index++;


	if (bit_flip_index==size*8 - 1){
		bit_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto bit2_flip_start;
	}

	return op;
}

u8 bit4_flip(void *data,size_t size,u8 op,size_t *start){
	static size_t bit_flip_index=0;
	u8 ignore = 0;

bit4_flip_start:

	*start = bit_flip_index >> 3;

	if (bit_flip_index > (size<<3)-3){
		aexit("FLIP4: index out of range");
	}

	if (!mut_map[*start] /*&& !mut_map[((bit_flip_index+3) >> 3)]*/){
		ignore = 1;
		*start=-1;
	}else{
		FLIP(data,bit_flip_index);
		FLIP(data,bit_flip_index+1);
		FLIP(data,bit_flip_index+2);
		FLIP(data,bit_flip_index+3);
	}
	cmt = "[MUT-B-F-4]";
	if (!bit_flip_index) REPMUT(cmt);
	bit_flip_index++;

	if (bit_flip_index==(size*8)-3){
		bit_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto bit4_flip_start;
	}

	return op;
}
/*
	STARTER MUTATION FUNCTION:
	One byte_flip pass is run for any input and using this
	we set mut_map and find key words. Then it's mut_map that
	specifies how next IT functions should perform.
*/
size_t mut_bfi=0;
u8 byte_flip(void *data,size_t size,u8 op,size_t *start){
	static size_t byte_flip_index=0;
	u8 ignore = 0;
	u8 im_initiator = 1;

byte_flip_start:

	if (byte_flip_index > size){
		aexit("FLIP8: index out of range");
	}

	if (!im_initiator && !mut_map[byte_flip_index]){
		ignore = 1;
		*start=-1;
	}else{
		*start = byte_flip_index;

		((u8 *)data )[byte_flip_index] ^= 0xFF;
		cmt = "[MUT-B-F-8]";
		if (!byte_flip_index) REPMUT(cmt);
	}
	mut_bfi = byte_flip_index;
	byte_flip_index++;


	if (byte_flip_index==size){
		byte_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto byte_flip_start;
	}

	return op;
}

u8 byte2_flip(void *data,size_t size,u8 op,size_t *start){
	static size_t byte_flip_index=0;
	u8 ignore = 0;

byte2_flip_start:

	if (byte_flip_index > size-1){
		aexit("FLIP16: index out of range");
	}

	if (!mut_map[byte_flip_index] && !mut_map[byte_flip_index+1]){
		ignore = 1;
		*start=-1;
	}else{
		*start = byte_flip_index;

		*(u16*)&( ((u8 *)data)[byte_flip_index] ) ^= 0xFFFF;

		cmt="[MUT-B-F-16]";
		if (!byte_flip_index) REPMUT(cmt);
	}
	byte_flip_index++;

	if (byte_flip_index==size-1){
		byte_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto byte2_flip_start;
	}

	return op;
}

u8 byte4_flip(void *data,size_t size,u8 op,size_t *start){
	static size_t byte_flip_index=0;
	u8 ignore = 0;

byte4_flip_start:

	if (byte_flip_index > size-3){
		aexit("FLIP32: index out of range");
	}

	if (!mut_map[byte_flip_index] && !mut_map[byte_flip_index+1] &&
			!mut_map[byte_flip_index+2] && !mut_map[byte_flip_index+3]){
		ignore = 1;
		*start=-1;
	}else{
		*start = byte_flip_index;

		*(u32*)&( ((u8 *)data)[byte_flip_index] ) ^= 0xFFFFFFFF;

		cmt="[MUT-B-F-32]";
		if (!byte_flip_index) REPMUT(cmt);
	}
	byte_flip_index++;


	if (byte_flip_index==size-3){
		byte_flip_index=0;
		return op+1;
	}

	if (ignore){
		ignore = 0;
		goto byte4_flip_start;
	}
	return op;
}




u8 overw_8_int(void *data,size_t size,u8 op,size_t *start){
	static size_t i8_inp_index=0;
	static u8 i8_set_index = 0;
	u8 *byte;
	u8 orig_value;
	u8 ignore = 8;

overw_8_int_start:

	byte = ((u8*)data) + i8_inp_index;

	*start = i8_inp_index;

	orig_value = *byte;
	*byte = set_8_ints[i8_set_index];

	cmt = "[MUT-I-O-8]";
	if (!i8_inp_index) REPMUT(cmt);

	if (!mut_map[i8_inp_index] || bitflip_check(orig_value ^ *byte)){
		ignore = 1;
		*start=-1;
		*byte=orig_value;
	}

	i8_set_index++;

	if (i8_set_index== sizeof(set_8_ints)){
		i8_set_index = 0;
		i8_inp_index++;
	}

	if (i8_inp_index==size){
		i8_inp_index = 0;
		i8_set_index = 0;
		return op + 1;
	}

	if (ignore){
		ignore = 0;
		goto overw_8_int_start;
	}

	return op;

}

u8 overw_16_int(void *data,size_t size,u8 op,size_t *start){
	static size_t i16_inp_index=0;
	static u8 i16_set_index = 0;
	u16 *word;
	u16 orig_value;
	u8 ignore =0;

overw_16_int_start:

	word = (u16*)( ((u8*)data) + i16_inp_index );

	orig_value = *word;
	*word = set_16_ints[i16_set_index];

	*start = i16_inp_index;

	cmt = "[MUT-I-O-16]";
	if (!i16_inp_index) REPMUT(cmt);

	if ((!mut_map[i16_inp_index] && !mut_map[i16_inp_index+1]) ||
			bitflip_check(orig_value ^ *word)){
		ignore = 1;
		*start=-1;
		*word=orig_value;
	}

	i16_set_index++;

	if (i16_set_index== sizeof(set_16_ints)/2){
		i16_set_index = 0;
		i16_inp_index++;
	}

	if (i16_inp_index==size-1){
		i16_inp_index = 0;
		i16_set_index = 0;
		return op + 1;
	}

	if (ignore){
		ignore = 0;
		goto overw_16_int_start;
	}

	return op;
}

u8 overw_32_int(void *data,size_t size,u8 op,size_t *start){
	static size_t i32_inp_index=0;
	static u8 i32_set_index = 0;
	u32 *dword;
	u32 orig_value;
	u8 ignore = 0;

overw_32_int_start:

	dword = (u32*)( ((u8*)data) + i32_inp_index );

	orig_value = *dword;
	*dword = set_32_ints[i32_set_index];

	*start = i32_inp_index;

	cmt = "[MUT-I-O-32]";
	if (!i32_inp_index) REPMUT(cmt);

	if ((!mut_map[i32_inp_index] && !mut_map[i32_inp_index+1] &&
			!mut_map[i32_inp_index+2] && !mut_map[i32_inp_index+3] ) ||
			bitflip_check(orig_value ^ *dword)){
		ignore = 1;
		*start=-1;
		*dword=orig_value;
	}

	i32_set_index++;

	if (i32_set_index== sizeof(set_32_ints)/4){
		i32_set_index = 0;
		i32_inp_index++;
	}


	if (i32_inp_index==size-3){
		i32_inp_index = 0;
		i32_set_index = 0;
		return op + 1;
	}

	if (ignore){
		ignore = 0;
		goto overw_32_int_start;
	}

	return op;

}


/*
	Mutate interesting locatoins.
	Doesn't check mut_map since a chosen location
	as an 'interesting' location, is prioritized over what has been
	recorded in mut_map.
*/
u8 iter_intr_locs(void *data,size_t size,u8 op,size_t *start,struct inp_intr_locs *cur){
	static unsigned int intr_indx = 0;
	u8 b;

	if (intr_indx>cur->intr_locs_index){
		aexit("iter_intr_locs: i_indx out of range.");
	}

	if (!cur->intr_locs_index){
		*start=-1;
		return op+1;
	}

	if (intr_indx==0){
		if (cur->intr_locs[intr_indx] >= size){
			*start=-1;
			return op+1;
		}
	}

	if (!intr_indx){
		cmt = "[MUT-ITER-INTR-LOCS]";
		REPMUT(cmt);
	}

	*start = cur->intr_locs[intr_indx];

	/*
		Set from 1 to 8 bytes
		May overflow the input near its end border
		but won't overflow local buffer.
		No sanity check necessary.
	*/
	b=RU8(4);
	switch(b){
		case 0:
			*(u8*)(data+cur->intr_locs[intr_indx++])=RU8(256);
			break;
		case 1:
			*(u16*)(data+cur->intr_locs[intr_indx++])=RST(0xFFFFFFFFFFFFFF);
			break;
		case 2:
			*(u32*)(data+cur->intr_locs[intr_indx++])=RST(0xFFFFFFFFFFFFFF);
			break;
		case 3:
			*(u64*)(data+cur->intr_locs[intr_indx++])=RST(0xFFFFFFFFFFFFFF);
			break;
	}

	//*(u8*)(data+26)=RU8(256);

	/*
		Check next intr_indx
	*/
	if (intr_indx == cur->intr_locs_index ||
		cur->intr_locs[intr_indx] >= size){

		intr_indx = 0;
		return op+1;
	}

	return op;

}

/*
	try all keywords on all locations basd on mut_map
*/
u8 kw_ow_linear(void *data,size_t size,u8 op,size_t *start){
	static size_t kw_inp_index=0;
	static u8 kw_set_index = 0;
	u8 ignore = 0;

	if (!kw_index){
		*start=-1;
		return op+1;
	}

kw_ow_linear_start:

	ignore=1;

	if (keywords[kw_set_index].size + kw_inp_index -1 < size){

		if (memcmp(data+kw_inp_index,&keywords[kw_set_index].kw,keywords[kw_set_index].size)){
			/*
				Ok now check mut_map
			*/
			if (memchr(&mut_map[kw_inp_index],1,keywords[kw_set_index].size)){
				/*
					Good. Write it.
				*/
				memcpy(data+kw_inp_index,&keywords[kw_set_index].kw,keywords[kw_set_index].size);
				ignore=0;
				*start=kw_inp_index;
				//awarn("%s %d [%d] %d\n",keywords[kw_set_index].kw,keywords[kw_set_index].size,kw_inp_index,mut_map[kw_inp_index]);
			}
		}
	}


	cmt = "[MUT-KW-LINEAR]";
	if (!kw_inp_index) REPMUT(cmt);



	kw_set_index++;

	if (kw_set_index== kw_index){
		kw_set_index = 0;
		kw_inp_index++;
	}


	if (kw_inp_index==size-3){
		/*
			keywords have varaible length
			we only go forward until size - 3
			This should be fine as there's enough extra
			memory allocated after the buffer.
		*/
		kw_inp_index = 0;
		kw_set_index = 0;
		return op + 1;
	}

	if (ignore){
		goto kw_ow_linear_start;
	}

	return op;
}

/* Non-iterative functions */

/*
	Overwrite one random keyword at a random place
	Writing keyword at the end of input might have
	some of its bytes to be discarded (not written
	file) since we don't change the size here.
*/
void mut_kw_ow(void *data,size_t size,u8 op,size_t *start,size_t *end){

	u8 kw_i;
	size_t pos;

	if (kw_index==0)
	{
		*start=-1;
		return;
	}
	cmt = "[MUT-KW-O-N]";
	REPMUT(cmt);


	kw_i = RU8(kw_index);
	pos = RST(size);

	if (keywords[kw_i].size ==0){
		aexit("mut_kw_ow: Invalid size %d %d",kw_index,kw_i);
	}
	memcpy(data + pos , (void *)keywords[kw_i].kw,keywords[kw_i].size);



	*start = pos;
	*end = pos + keywords[kw_i].size;

}

int pop=-1;

#define SIZE_LIMIT_CHECK	if (size > MAX_INPUT_SIZE )\
								aexit("mut %d, input too big, prev %d %luB",op,pop,size);\
 							else if(size < MIN_INPUT_SIZE)\
								aexit("mut %d, input too small, prev %d",op,pop);\


#define INSERT_CHECK	if (size == MAX_INPUT_SIZE) {*start=-1; return;}
/*
	Insert one random keyword at a random place
	This function *increases* size
*/
void mut_kw_ins(void *data,size_t size,u8 op,size_t *start,size_t *end,size_t *new_size){
	u8 kw_i;
	size_t pos;

	SIZE_LIMIT_CHECK
	INSERT_CHECK

	if (kw_index==0)
	{
		*start=-1;
		return;
	}

	cmt = "[MUT-KW-I-N]";
	REPMUT(cmt);

	kw_i = RU8(kw_index);
	pos = RST(size);

	if (keywords[kw_i].size ==0){
		aexit("mut_kw_ins: Invalid size %d %d",kw_index,kw_i);
	}

	/*
		We use memmove which is safe for overlapping ranges
	*/
	memmove(data + pos + keywords[kw_i].size, data + pos,size-pos);

	memcpy(data + pos , (void *)keywords[kw_i].kw,keywords[kw_i].size);



	*start = pos;
	*end = pos + keywords[kw_i].size;
	*new_size = size + keywords[kw_i].size;

}

/*
	Overwrite one random integer at a random place
	Do tot_count passes for each execution.

*/
void mut_random_ow(void *data,size_t size,u8 op,size_t *start){
	long rand_i;
	size_t pos;
	int count=0;
	int tot_count=1;

	SIZE_LIMIT_CHECK

	cmt = "[MUT-RAND-O-32]";
	REPMUT(cmt);

mut_random_ow_start:


	pos = RST(size);

	/*
		Non iterative functions don't necessarily
		work with original mutated input
		We can skip check_interesting
		of bitflip_check
	*/
	/*do{
		rand_i = (int)((rand()/(double)(((long)RAND_MAX)+1))*0xFFFFFFFF);
	}while(check_interesting(pos,rand_i));*/
	rand_i = RS64(0xFFFFFFFF);

	*(int*)(data + pos) = (int)rand_i;

	if (size>32){
		if (count++ < tot_count){
			goto mut_random_ow_start;
		}
	}
	*start = pos;
}

/*
	Insert one random integer at a random place
	This functions changes size by 4
*/
void mut_random_ins(void *data,size_t size,u8 op,size_t *start,size_t *new_size){
	long rand_i;
	size_t pos;

	SIZE_LIMIT_CHECK
	INSERT_CHECK

	cmt = "[MUT-RAND-I-32]";
	REPMUT(cmt);

	rand_i = RS64(0xFFFFFFFF);
	pos = RST(size);

	memmove(data + pos + 4 , data+pos, size-pos);
	*(int*)(data + pos) = (int)rand_i;


	*start = pos;
	*new_size=size + 4;
}

/*
	Copy a random chunk to a random location
	31 < size < 65
*/
void mut_copy_ow(void *data,size_t size,u8 op,size_t *start,size_t *end){
	size_t pos_src = RST(size-64);
	size_t pos_dst = RST(size-64);
	u8 chunk_size = 32 + RU8(33);

	if (size < 128){
		/*
			Not worth it
		*/
		*start=-1;
		return ;
	}
	cmt = "[MUT-COPY-O-N]";
	REPMUT(cmt);

	memmove(data+pos_dst,data+pos_src,chunk_size);

	*start = pos_dst;
	*end = pos_dst+chunk_size;

}

/*
	Same as above but insert
	*Increases* size
	Maximum size increase: 64

*/
void mut_copy_ins(void *data,size_t size,u8 op,size_t *start,size_t *end,size_t *new_size){
	size_t pos_src = RST(size-64);
	size_t pos_dst = RST(size-64);
	u8 chunk_size = 32 + RU8(33);
	u8 chunk[64];

	SIZE_LIMIT_CHECK
	INSERT_CHECK

	if (size < 128){
		*start=-1;
		return ;
	}

	cmt = "[MUT-COPY-I-N]";
	REPMUT(cmt);

	memcpy(chunk,data+pos_src,chunk_size);
	memmove(data+pos_dst+chunk_size,data+pos_dst,size - pos_dst);
	memcpy(data+pos_dst,chunk,chunk_size);

	*start = pos_dst;
	*end = pos_dst+chunk_size;
	*new_size= size + chunk_size;

}
/*
	Make the input smaller
	something between 16 and size/3 bytes
	This will remove the chunk from a randomly chosen
	position.
*/
void mut_shrink_size(void *data,size_t size,u8 op,size_t *start,size_t *new_size){
	size_t pos ;
	u8 chunk_size;

	SIZE_LIMIT_CHECK

	cmt = "[MUT-SHRINK-N]";
	REPMUT(cmt);

	if (size == MIN_INPUT_SIZE){
		*start=-1;
		return;
	}else if(size < 64){
		/*
			Remove only one byte
		*/
		chunk_size = 1;
	}else{
		/*
			Input is big enough
		*/
		chunk_size = 16 + RU8(size/3 - 16);
	}

	pos = RST(size-(chunk_size*2));
	memmove(data+pos,data+pos+chunk_size, size - (pos+chunk_size));

	*start = pos;
	*new_size = size - chunk_size;
}


#define ADDSUB_MAX	35
#define DECREASE_PROB	if (RU8(2)){*start=-1;return;}
/*
	input1
	|---------- break_pos +
						   input2: mbreak_pos ------| end of input2

	Final size: min(MAX_INPUT, break_pos + (input2.size - mbreak_pos) )
*/
void mut_mix_inputs(void *data,size_t size,u8 op,size_t *start,size_t *new_size){
	int q_i;

	size_t break_pos,m_break_pos,append_size,n,read_size;
	FILE *mix_f;
	struct stat st;
	void *mdata;

	SIZE_LIMIT_CHECK
	DECREASE_PROB


	cmt = "[MUT-MIX-N]";
	REPMUT(cmt);
	if (queue_ind==1){
		/*
			This input is the only
			input in the queue. (q_i=zero)
		*/
		*start = -1;
		return;
	}

mut_mix_inputs_start:

	q_i = RS32(queue_ind);

	if (q_i == queue_use_ind){
		goto mut_mix_inputs_start;
	}

	if (lstat(input_queue[q_i].i_path,&st)==-1){
		aexit("mix_inputs: stat: %s | %d %d %d | (%s)",strerror(errno),q_i,queue_ind,queue_use_ind,input_queue[q_i].i_path);
	}

choose_offsets:
	break_pos = RST(size);
	m_break_pos = RST(st.st_size);

	if (break_pos + (st.st_size - m_break_pos) < MIN_INPUT_SIZE){
		goto choose_offsets;
	}

	mix_f = fopen(input_queue[q_i].i_path,"r");
	if (!mix_f){
		aexit("mix_inputs: fopen()");
	}



	if (fseek(mix_f,m_break_pos,SEEK_SET)==-1){
		aexit("mix_inputs: fseek");
	}

	read_size=st.st_size - m_break_pos;
	mdata = mmap(0,read_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	if (mdata==MAP_FAILED){
		aexit("mix_inputs: madata");
	}

	n = fread(mdata,1,read_size,mix_f);
	if (n!=read_size){
		aexit("mix_inputs: fread");
	}

	fclose(mix_f);

	/*
		Careful about size here
		Don't overflow data in local memory
		If read_size doesn't exceed max, use it
		as append size.
	*/

	append_size = (read_size + break_pos <= MAX_INPUT_SIZE) ?
			read_size :
			MAX_INPUT_SIZE - break_pos;


	memcpy(data+break_pos,mdata,append_size);

	*new_size = break_pos + append_size;

	if (munmap(mdata,st.st_size - m_break_pos)){
		aexit("mix_inputs: munmap()");
	}

	*start=break_pos;
	//aexit("size=%ld break_pos=%lu q_i=%ld mbreak=%lu append=%lu",size,break_pos,q_i,m_break_pos,append_size);
}



void mut_rand_flip(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size<<3);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RAND_FLIP_1]";
	REPMUT(cmt);

	FLIP(data,pos);

	*start = pos >> 3;

}

#define SW16(x) ({\
					u16 d16 = (u16)x;\
					(u16)((d16<<8)|(d16>>8));\
				})

#define SW32(x) ({\
					u32 d32 = x;\
					(u32)((d32<<24)|(d32>>24)|\
						((0x00FF0000 & d32)>>8)|\
						((0x0000FF00 & d32)<<8));\
				})

void mut_over_rand_8_int(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_OVER_RAND_8_INT]";
	REPMUT(cmt);
	*(u8*)(data + pos) = set_8_ints[RU8(sizeof(set_8_ints))];


	*start = pos;
}

void mut_over_rand_16_int(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size-1);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_OVER_RAND_16_INT]";
	REPMUT(cmt);

	*(u16*)(data + pos) = RU8(2) ? set_16_ints[RU8(sizeof(set_16_ints))]:
								SW16(set_16_ints[RU8(sizeof(set_16_ints))]);

	*start = pos;

}

void mut_over_rand_32_int(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size-3);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_OVER_RAND_32_INT]";
	REPMUT(cmt);

	*(u32*)(data + pos) = RU8(2) ? set_32_ints[RU8(sizeof(set_32_ints))]:
								SW32(set_32_ints[RU8(sizeof(set_32_ints))]);
	*start = pos;
}

void mut_rand_8_add_sub(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RAND_8_ADSB]";
	REPMUT(cmt);

	if (RU8(2)){//add
		*(u8*)(data + pos) += RU8(ADDSUB_MAX);
	}else{//sub
		*(u8*)(data + pos) -= RU8(ADDSUB_MAX);
	}

	*start = pos;
}

void mut_rand_16_add_sub(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size-1);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RAND_16_ADSB]";
	REPMUT(cmt);

	if (RU8(2)){//add
		*(u16*)(data + pos) += 1+RU8(ADDSUB_MAX);
	}else{//sub
		*(u16*)(data + pos) -= 1+RU8(ADDSUB_MAX);
	}

	*start = pos;
}

void mut_rand_32_add_sub(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size-3);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RAND_32_ADSB]";
	REPMUT(cmt);

	if (RU8(2)){//add
		*(u32*)(data + pos) += 1+RU8(ADDSUB_MAX);
	}else{//sub
		*(u32*)(data + pos) -= 1+RU8(ADDSUB_MAX);
	}

	*start = pos;
}

void mut_rand_8_byte(void *data,size_t size,u8 op,size_t *start){
	size_t pos= RST(size);

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RAND_8_BYTE]";
	REPMUT(cmt);

	*(u8*)(data + pos)=RU8(256);

	*start = pos;
}


/*
	chunk size [4,size/4+8]
*/
void mut_insert_const(void *data,size_t size,u8 op,size_t *start,size_t *end,size_t *new_size){
	size_t pos= RST(size);
	size_t len = 4 + RST((size/3));
	u8 val;

	SIZE_LIMIT_CHECK
	INSERT_CHECK
	DECREASE_PROB

	cmt = "[MUT_INSERT_CONST]";
	REPMUT(cmt);

	if (len + size > MAX_INPUT_SIZE){
		*start = -1;
		return;
	}

	val = RU8(2) ? RU8(256) : *(u8 *)(data + pos);
	memmove(data + pos + len,data+pos,size-pos);
	memset(data+pos,val,len);

	*new_size = size + len;

	*start = pos;
	*end=*start+len;
}

/*
	chunk size [4,size/3]
*/
void mut_ow_const(void *data,size_t size,u8 op,size_t *start){
	size_t len = 4 + RST((size/3)-4);
	size_t pos = RST(size-len+1);
	u8 val;

	SIZE_LIMIT_CHECK

	cmt = "[MUT_OW_CONST]";
	REPMUT(cmt);

	val = RU8(2) ? RU8(256) : *(u8 *)(data + pos);

	memset(data+pos,val,len);

	*start = pos;
}

void mut_sw_bytes(void *data,size_t size,u8 op,size_t *start){
	size_t pos1=RST(size);
	size_t pos2;
	u8 t;

	SIZE_LIMIT_CHECK

	cmt = "[MUT_SW_BYTES]";
	REPMUT(cmt);

	do{
		pos2=RST(size);
		if (pos2 != pos1 )
			break;
	}while(1);

	/*
		We may have a data buffer whose all/most
		bytes are all equal. We don't want to waste
		a lot of time here.
	*/
	if (*(u8*)(data + pos1)==*(u8*)(data + pos2))
	{
		*start = -1;
		return;
	}

	t=*(u8*)(data + pos1);
	*(u8*)(data + pos1)=*(u8*)(data + pos2);
	*(u8*)(data + pos2)=t;

	*start = pos1;

}

/*
	Write a random chunk of size/4 bytes
*/
void mut_ow_rand_chunk(void *data,size_t size,u8 op,size_t *start){
	size_t pos = RST(size-size/4);
	u8 *chunk=malloc(size/4);
	size_t i;

	SIZE_LIMIT_CHECK

	cmt = "[MUT_RND_CHUNK]";
	REPMUT(cmt);

	for (i=0;i<size/4;i++){
		chunk[i] = RU8(256);
	}

	memcpy(data+pos,chunk,size/4);

	*start=pos;

	free(chunk);
}

/*
	Write size/4 random bytes to random locations
*/
void mut_scatter_rand(void *data,size_t size,u8 op,size_t *start){
	size_t pos;
	size_t i;

	SIZE_LIMIT_CHECK

	cmt = "[MUT_SCATTER_RND]";
	REPMUT(cmt);

	for (i=0;i<size/4;i++){
		pos = RST(size);
		*(u8*)(data+pos) = RU8(256);
	}

	*start = 0;

}

/*
	Set multiple intresting locations to a random value
*/
void mut_intr_locs(void *data,size_t size,u8 op,size_t *start,struct inp_intr_locs *cur){
	size_t pos;
	int i,si,i_indx = cur->intr_locs_index;
	u8 b;

	if (i_indx>MAX_INTR_LOCS || i_indx<0){
		aexit("mut_intr_locs: i_indx out of range.");
	}

	if (!i_indx){
		*start=-1;
		return;
	}


	/*
		Since intr_locs is inherited
		we should check not to go beyond
		the size of this input
	*/
	for (i=0;i<i_indx;i++){
		if (cur->intr_locs[i] >= size){
			i_indx = i;
			break;
		}
	}

	if (i_indx==0){
		*start=-1;
		return;
	}

	cmt = "[MUT-INTR-LOCS]";
	REPMUT(cmt);

	/*
		Set to random value for log(i_indx) times
	*/
	si = i_indx;
	do{
		pos = cur->intr_locs[RST(i_indx)];
		b=RU8(4);
		switch(b){
			case 0:
				*(u8*)(data+pos)=RU8(256);
				break;
			case 1:
				*(u16*)(data+pos)=RST(0xFFFFFFFFFFFFFF);
				break;
			case 2:
				*(u32*)(data+pos)=RST(0xFFFFFFFFFFFFFF);
				break;
			case 3:
				*(u64*)(data+pos)=RST(0xFFFFFFFFFFFFFF);
				break;
		}
		si/=2;
	}while(si);

	*start = pos;

}

void print_dict_kws(){
	int i;
	for (i=0;i<dict_kw_count;i++){
		char tmp[100];
		strcpy(tmp,dict_kws[i]);
		printf("%d '%s'\n",i,tmp);
	}

}
/*
	Overwrite one random dictionary keyword at a random place
*/
void mut_dict_kw_ow(void *data,size_t size,u8 op,size_t *start,size_t *end){

	int kw_i;
	size_t pos;
	char keyword[DICT_MAX_KW_SIZE+1];
	int kwlen;

	if (!dict_file){
		aexit("Requested dictionary operation while no dictionary has been given. (OW)");
	}

	cmt = "[MUT-DICT-KW-O]";
	REPMUT(cmt);


	kw_i = RS32(dict_kw_count);
	strcpy(keyword,dict_kws[kw_i]);
	kwlen = strlen(dict_kws[kw_i]);

	if (kwlen==0){
		aexit("Invalid kw len");
	}

	pos = RST(size);

	memcpy(data + pos , (void *)keyword,kwlen);



	*start = pos;
	*end = pos + kwlen;
//awarn("Used kw %d %s\n",kw_i,dict_kws[kw_i]);print_dict_kws();
}


/*
	Insert one random dictionary keyword at a random place
	This function *increases* size
*/
void mut_dict_kw_ins(void *data,size_t size,u8 op,size_t *start,size_t *end,size_t *new_size){
	int kw_i;
	size_t pos;
	char keyword[DICT_MAX_KW_SIZE+1];
	int kwlen;

	SIZE_LIMIT_CHECK
	INSERT_CHECK

	if (!dict_file){
		aexit("Requested dictionary operation while no dictionary has been given. (INS)");
	}

	cmt = "[MUT-DICT-KW-I]";
	REPMUT(cmt);

	kw_i = RS32(dict_kw_count);
	strcpy(keyword,dict_kws[kw_i]);
	kwlen = strlen(dict_kws[kw_i]);

	if (kwlen<1){
		aexit("Inavlid keyword len: %d",kwlen);
	}

	pos = RST(size);

	/*
		We use memmove which is safe for overlapping ranges
	*/
	memmove(data + pos + kwlen, data + pos,size-pos);

	memcpy(data + pos , (void *)keyword,kwlen);



	*start = pos;
	*end = pos + kwlen;
	*new_size = size + kwlen;

}

/*********** End mutation functions ************/





void write_g_input(u8 *data,size_t size){

	if (file_feed){
		FILE *out_f;
		size_t n;

		out_f = fopen(feed_file_path,"w");
		if (!out_f){
			aexit("open() : generated input");
		}
		if ((n=fwrite(data,1,size,out_f)) <size){
			aexit("write() : generated input");
		}
		fclose(out_f);
	}else{
		u8 *p=data;
		u8 *end=data+size;
		size_t chk_sz=0;

		lseek(feed_fd,0,SEEK_SET);

		while(p<end){

			if (end-p > 2048){
				chk_sz=2048;
			}else{
				chk_sz=end-p;
			}

			if (write(feed_fd,p,chk_sz)!=chk_sz){
				aexit("write");
			}

			p+=chk_sz;
		}


		if (ftruncate(feed_fd,size))
			aexit("can't truncate current input");

		lseek(feed_fd,0,SEEK_SET);


	}

}

int feed_net(void *data,long size){
	long n;
	char buf[1<<16];
	int ret=-1;
	u8 soft_crash = 0;

	//printf("Stage 2: Clear memory\n");

	reset_structures();


	/*
		send data and wait for TCP response
	*/
	/*FILE *f=fopen("net_file","r");
	fread(buf,279,1,f);
	fwrite(buf,279,1,stdout);
	fclose(f);
	data=buf;
	size=279;*/
	//printf("%s",(char*)data);
	if (save_net){
		save_netdata(data,size,NET_FILE);
	}
	//printf("Stage 3: Feed data\n");
	n=send(net_sock,data,size,0);

	if (n<size){
		aexit("send failed ");
		goto check_failure;
	}
	/*
		We expect the target to respond
		immediately
	*/
	buf[0]=0;
	n=recv(net_sock,buf,(1<<16),0);
	if (n<=0){

		if (errno==ERR_TMOUT){
			/*
				recv timeout
				for network mode we consider it
				a soft crash (possible DOS)
			*/
			soft_crash=1;
		}else{
			/*
				recv failed for a reason other than timeout

			*/
		}
		goto check_failure;
	}
	/*
		recv has succeeded
		reinitialize socket for next input
		since target may close the connection
		after each send
	*/
	//printf("1 %s",buf);

	if (!reconnect()){
		soft_crash=1;
		goto check_failure;
	}

	//printf("Stage 4: Received response\n");
	return TRG_NET_MODE;

check_failure:
	/*
		kill target if it's still running
		poll fork server and see what happened
		The exit status must be either crash or
		kill signal in this situation
	*/


	ret = do_term_check();
	if (ret == TRG_EX_TMOUT || ret == TRG_EX_NORMAL){
		if (soft_crash){
			return TRG_SOFT_CRASH;
		}
	}
	return ret;

}



int feed_ex_target(void *data,u64 size){
	int result;

	total_exec++;

	if (net_mode && target_id){
		result = feed_net(data,size);

		return result;
	}else{
		if (net_mode){
			if (save_net){
				save_net_i = 0;
			}

			execute(0);
			return feed_net(data,size);
		}else{
			write_g_input(data,size);
			return execute(0);
		}
	}

}
void save_rep(u8 soft_crash,u8 * data,size_t size){
	FILE *crash_input;
	char filename[1064];
	size_t n;
	u32 i_hash;
	int i;
	char prefix[32];

	/*
		If we are here, target is not alive.
		So we don't need to lock shared memory
		for this.
	*/

	if (!parallel_mode){
		i_hash = do_hash();


		for (i=0;i<crash_sums_index;i++){
			if (crash_sums[i] == i_hash)
				return; //we've already seen it
		}
		crash_sums[crash_sums_index++] = i_hash;

		strcpy(prefix,"");
	}else{
		sprintf(prefix,"%d",instance_pid);
	}

	if (crash_sums_index == CRASH_MAX) //Unlikely
		crash_sums_index=0;

	if (soft_crash){
		sprintf(filename,"%s/crashes/%s_crash%d_SOFT_",output_dir,prefix,crash_rep++);
		unique_soft_crashes++;
	}
	else{
		sprintf(filename,"%s/crashes/%s_crash%d_%s_M%d",output_dir,prefix,crash_rep++,
							get_sig_name(last_crash_sig),last_crash_mut);
		unique_crashes++;
	}
	crash_input=fopen(filename,"w");
	if (!crash_input){
		aexit("save_rep: open failed");
	}
	if ((n=fwrite(data,size,1,crash_input))<1){
		aexit("save_rep: write failed");
	}
	fclose(crash_input);

	if (soft_crash){
		//arep("Potential DOS [input saved]");
	}
	output_count++;



}
void live_rep(){
	FILE *frep=fopen(LIVE_REP_FILE,"r+");
	char line[1024];
	int i=0;


	if (!frep){
		aexit("open failed: live report file");
	}
	/*

		line1: start time
		line2: target name
		line3: total blocks
		line4: indp blocks
		line5: nested blocks

		rest is dynamic data that we should update

		Report:
			total coverage (number of blocks hit so far)
			total target hits
			number of independent blocks covered
			number of nested blocks	covered

	*/
	if (fseek(frep,0,SEEK_SET)==-1){
		aexit("fseek(): live report");
	}

	line[0]=0;
	for (i=0;i<5;i++){
		if (!fgets(line,1024,frep)){
			aexit("fgets(): live report file: %s",strerror(errno));
		}
	}

	if ((fprintf(frep,"%lu\n%lu\n%lu\n%lu\n%lu\n",total_covered
												,total_target_hits
												,indp_counter
												,nested_counter
												,last_trace_nodes))
		<0)
		aexit("write(): live report");

	fclose(frep);


}
void save_mutated_input(u8 *data,size_t size){
	FILE *mut_input;
	char filename[1064];
	size_t n;

	sprintf(filename,"%s/saved_input%d",input_dir,mut_input_saved++);
	mut_input=fopen(filename,"w");
	if (!mut_input){
		aexit("save_rep: open failed");
	}
	if ((n=fwrite(data,size,1,mut_input))<1){
		aexit("save_rep: write failed");
	}
	fclose(mut_input);

}

void add_stat_entry(){
	FILE *f;
	time_t cur_time;

	time(&cur_time);
    cur_time-=start_time;

	f=fopen(STAT_ENTRIES_FILE,"a");
	if (!f){
		aexit("add_stat_entrey: fopen");
	}
	fprintf(f,"<%s> %lu:%lu:%lu\n",cmt,total_covered,total_exec,cur_time);
	fclose(f);
}

void balance_lps(int first_ind){
	int last=queue_ind-1,mid;
	int i;
#define SW_PROB	(!RU32(20))  //5%

	if (last < first_ind){
		/*
			Cycled queue
			Very unlikey to happen when this
			function is called.
		*/
		last = INPUT_MAX-1;
	}

	mid = ((last-first_ind)+1)/2;

	for (i=first_ind;i<mid;i++){
		if (SW_PROB){
			int swi;
			struct input_val tmp;
			/*
				Switch this with a random lp
				in the second half
			*/
			swi = mid + RU32(last-mid) +1;

			if (swi >= INPUT_MAX){
				/*
					Diffinetly impossible but just in case
					if I made a mistake in above lines.
				*/
				aexit("Unexpected index to switch lp.");
			}
			if (input_queue[i].prio>0 || input_queue[swi].prio>0){
				//print_queue();
				aexit("Non lp input in lp balance %d %d %d %d",i,swi,queue_use_ind,queue_ind);
			}
			tmp = input_queue[i];
			input_queue[i]=input_queue[swi];
			input_queue[swi] = tmp;

		}
	}

}
u8 kw_is_new(u8 *data,size_t len){
	int i;

	if (len==4){
		for (i=0;i<sizeof(set_32_ints)/4;i++){
			if ((*(u32*)data) == set_32_ints[i])
				return 0;

		}
	}
	for (i=0;i<kw_index;i++){
		if (!memcmp(data,&keywords[i].kw,len))
			return 0;

	}
	return 1;
}

void store_mut_map(){
	char *mp=input_queue[queue_use_ind].m_path;
	FILE *fp;
	u8 *mumap_to_use;

	if (perf_check_req > 0){
		mumap_to_use=mut_map_orig;
	}else{
		mumap_to_use=mut_map;
	}

	fp=fopen(mp,"w");
	if (!fp){
		aexit("fopen()");
	}
	if (fwrite(mumap_to_use,1,MAX_INPUT_SIZE,fp)<MAX_INPUT_SIZE){
		aexit("fwrite");
	}
	fclose(fp);


}

/*
	If perf_check > 0, this function loads
	mut_map_orig which has been originally stored
	in states file and stores it in mut_map.
*/
void load_mut_map(){
	char *mp=input_queue[queue_use_ind].m_path;
	FILE *fp;


	fp=fopen(mp,"r");
	if (!fp){
		aexit("fopen()");
	}
	if (fread(mut_map,1,MAX_INPUT_SIZE,fp)<MAX_INPUT_SIZE){
		aexit("fread()");
	}
	fclose(fp);

}



void arvin_generate(){
	void *saved_input;
	void *mutated_input;
	struct stat st;
	u64 alloc_size;
	u64 tcount=0;
	size_t actual_size;
	size_t mutated_size;
	FILE *f_out;
	char f_o_path[1064];
	int c;
	u8 mut_op=0,mut_op_used;
	u8 mutation_finished = 0;
	int t_exit_stat;
	u8 should_store = 0;
	size_t m_start,m_end;
	u64 last_kw_pos = 0;
	struct input_val *curq;
	u8 must_refresh_input = 0;
	u64 ni_counter = 0;
	u64 ni_total_tries=0;
	int ni_burst_counter=0;
#define BURST_P_MIN	1
	int ni_mut_burst=BURST_P_MIN;
	u64 ni_exec_total=0;
	u8 it_end=10; /* Last iterative index */
	u8 ni_start=15;
	u8 ni_functions_count = 22 + (dict_file?2:0);
	u64 total_covered_sav;

	struct inp_intr_locs *cur_intr_locs;

	u8 tm_perfcheck=0,tm_perfcheck_prev;
	u8 candidate_useful=0;
	int set_check_burst=0;
	int unset_check_burst=0;
	int init_check_burst=0;
#define INIT_CH_BURST	32
#define SET_CH_BURST	20
#define UNSET_CH_BURST	5

	/*
		How many elements have been added to the
		queue in NI rounds for this input?
	*/
	int useful_prop_ni = 0;
#define USEFUL_PROP_BURST	2



#define NI_MAX_ROUNDS_INIT	(1<<(20 + BURST_P_MIN))
#define NI_MAX_ROUNDS_MUT	(1<<(17 + BURST_P_MIN))

#define NI_REF_PROB_WEIGTH	0.5

#define NI_MAX_ROUNDS	((input_queue[queue_use_ind].initial) ? NI_MAX_ROUNDS_INIT : NI_MAX_ROUNDS_MUT)

	u64 ni_ceil_warn=2*NI_MAX_ROUNDS;
	u8 recharged_ni_max_times=0;

	/*
		We access NI_MAX_ROUNDS enough
		to justify creating a new variable for it
	*/
	u64 current_ni_max_rounds=NI_MAX_ROUNDS;
	u64 recharge_max = (1<<(20 + BURST_P_MIN))/NI_MAX_ROUNDS_MUT;	//17: 8

	//int max_depth_sav;

	curq=&input_queue[queue_use_ind];
	/*
		Load the whole input in memory
		change as needed
		write changed input in current_input
		execute()

	*/
	//arep("using %s",input_queue[queue_use_ind].i_path);

	if (lstat(curq->i_path,&st)==-1){
		aexit("generator: stat");
	}

	actual_size = st.st_size;





	if (actual_size > MAX_INPUT_SIZE || actual_size < MIN_INPUT_SIZE){
		aexit("Input %s: Invalid size",curq->i_path);
	}


	/*
		Allocate at most 2KB more than
		the actual size for the synthesized input
	*/
	alloc_size = MAX_INPUT_SIZE*2;//((actual_size/1024)+2)*1024;// == actual_size ? actual_size : ((actual_size/1024)+1)*1024 ;
	saved_input = mmap(0,alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	mutated_input = mmap(0,alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);

	if (saved_input==MAP_FAILED || mutated_input==MAP_FAILED){
		aexit("generator failed due to memory inaccessiblity");
	}
	raw_f_in(saved_input,curq);

	/*********** Variable initializations of the input here ***************/

	/*
		For lpq_balance==1:
		If this is the first lp input to use,
		perform a one time lp balane.
	*/
	if (pm_mode!=2){ //in TNS mode, there's not a sorted queue
		if (lpq_balance==1 && !visited_lp && curq->prio==0){
			balance_lps(queue_use_ind);
			visited_lp=1;
		}
	}
	if (curq->initial && !curq->passed){
		/*
			create a new interesting locs map
			that will be inherited by the inputs
			created from this input
		*/
		cur_intr_locs = malloc(sizeof(struct inp_intr_locs));
		if (!cur_intr_locs){
			aexit("cur_intr_locs, malloc");
		}
		cur_intr_locs->intr_locs_index = 0;
		curq->i_intr_locs = cur_intr_locs;
	}else{
		cur_intr_locs = curq->i_intr_locs;
		if (cur_intr_locs==0){
			aexit("cur_intr_locs is NULL");
		}
		if (cur_intr_locs->intr_locs_index == 0){
			awarn("Got EMP INTR set (poor ITR response?)");
		}
	}





	if (!curq->passed){
		memset(mut_map,0,MAX_INPUT_SIZE);
		mut_map[0]=1;

		need_csum = 1;

		/*
			Update keywords for the input
		*/
		kw_index=0;

		/*
			Set dynamic performance check when
			the requested performance check is 1.
			This is only in effect for lp inputs.
		*/
		if (perf_check_req ==1){
			if (curq->prio>0){
				perf_check = 1;
			}else {
				if (RU8(4)){//75%
					perf_check = 2; //speed up low prio processing for this input
				}else{
					perf_check = 1; //revert back to 1 if it's 2 from previous round
				}
			}
		}
	}else{
		/*
			If we have already completed the queue
			we want to wield the iterative functions
			more aggressively if the requested performance mode
			by the user has been 1 or 2 (except initials which
			have been evaluated by iterative functions extensively
			in their first run).
			Otherwise Load the previous map and skip directly to
			ni functions.
			In either case, checksum is not activated, keywords are
			not updated.
		*/
		load_mut_map();
		if (!curq->initial && perf_check_req > 0 &&
			curq->invalidated_i==0){
			/*
				Skip byte flip
				The rest of iterative function will be
				called.
			*/
			mut_op=1;

			/*
				Disallow using iterative functions for next passes.
				After this pass, only ni functions will be used
				for this input.
			*/
			curq->invalidated_i=1;
		}else{
			/*
				Jump to ni
				iterative functions have been used
				in previous pass.
				Fix timeout and go ahead.
			*/
			ni_counter = NI_MAX_ROUNDS;

		}
	}




	/**********************************************************************/


	/*
		TODO: dismiss iterative functions for low priority
		inputs
	*/
	while(!mutation_finished){
		/*
			Temp container to be sent to queue_add_traced
		*/
		struct input_val new_input ;

		if (ni_counter){
			/*
				NI functions must be executed.
				ni_counter == 1 ends this
			*/
			ni_total_tries++;
			if (m_start!=-1) ni_counter--;
			//else awarn("pop: %d",pop);

			/*
				Check whether the PREVIOUS ni operation
				produced a useful candidate.
				We want to increase the number of ni iterations
				for fruitful inputs.
				In case the last iterative mutation was useful
				we'll have one extra addition here. But that's
				tolerable.
			*/
			if (candidate_useful){
				useful_prop_ni++;
			}
			if (useful_prop_ni>USEFUL_PROP_BURST && !curq->initial){//aexit("Found a good one");
				/*
					Don't stay on this input forever
				*/
				if (recharged_ni_max_times < recharge_max){
					recharged_ni_max_times++;
					ni_counter+=current_ni_max_rounds;
					ni_ceil_warn+=current_ni_max_rounds;
				}
				useful_prop_ni=0;
			}
			if (ni_total_tries > ni_ceil_warn){
				/*
					This condition happens when NI functions
					fail too many times because either the generated input
					or the current input under mutation isn't valid.
					This invalidity usually happens when input size
					is close to min/max boundaries.
				*/
				mut_op = 255; //Finish mutation
				awarn("Input stayed for too many tries in NI stage [%d]",++_overstay_tot);
			}else{
				if (ni_counter==0){
					mut_op = 255; //Finish mutation
				}else{
					mut_op = ni_start + RU8(ni_functions_count);
					if (!dict_file && mut_op>36)
						aexit("Invalid mut for non-dict mode.");
//mut_op=33;
				}
			}
		}

		/*
		if (mut_op >= ni_start){
			u16 remp = (u16)((rand()/(double)(((long)RAND_MAX)+1))*1000);
			if (remp<=(NI_REF_PROB_WEIGTH*10)){
				must_refresh_input = 1;
			}
		}
		*/
		if (mut_op < ni_start || must_refresh_input){
			memcpy(mutated_input,saved_input,actual_size);
			mutated_size = actual_size;
			must_refresh_input = 0;
		}

		/* ALL loop initializations here */

		m_start = (size_t)-1; //~0UL
		m_end =(size_t)-1;
		has_corrupted_shm = 0;
		//max_depth_sav = max_depth;
		total_covered_sav = total_covered;
		candidate_useful = 0;
		tm_perfcheck_prev = tm_perfcheck;

		/********************************/

/*
	Change muts that don't seem to be useful for strcutured
	inputs to dict mut

*/
#define CHECK_FOR_STRUCTURED() if (dict_file){\
									if (RU8(2))\
										mut_dict_kw_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);\
									else\
										mut_dict_kw_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);\
										break;\
					}

		mut_op_used = mut_op;
		switch (mut_op){
			case 0:
				mut_op=byte_flip(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+1;

				break;
			case 1:
				/*
					hash operation is very expensive even with
					simple hash algorithms. Limit its use to
					only byte_flip()
				*/
				need_csum=0;

				if (perf_check==2){
					active_timeout = active_timeout_sav;
					tm_perfcheck = 0;
				}

				/*
				if (input_queue[queue_use_ind].prio == 0){

					//	Dismiss the rest of flip functions

					mut_op = 6;
					continue;
				}else if(input_queue[queue_use_ind].passed){

					ni_counter = NI_MAX_ROUNDS;
					continue;
				}
				*/

				mut_op=bit_flip(mutated_input,actual_size,mut_op,&m_start);

				/*
					only one byte changes
				*/
				m_end=m_start+1;

				break;
			case 2:
				mut_op=bit4_flip(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 3:
				mut_op=bit2_flip(mutated_input,actual_size,mut_op,&m_start);

				m_end=m_start+1;
				break;
			case 4:
				mut_op=byte2_flip(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 5:
				mut_op=byte4_flip(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+4;
				break;

			case 6:
				mut_op=overw_8_int(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 7:
				mut_op=overw_16_int(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 8:
				mut_op=overw_32_int(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+4;
				if (mut_op==9){
					/*
						If too few interesting locs, inherit it.
					*/
					if (cur_intr_locs->intr_locs_index < MIN_INTR_LOC_COUNT && queue_use_ind > 0
						&& curq->initial &&
						input_queue[0].i_intr_locs->intr_locs_index > cur_intr_locs->intr_locs_index){
						/*
							Use the first input intr_locs
						*/
						memcpy(cur_intr_locs,input_queue[0].i_intr_locs,sizeof(struct inp_intr_locs));
					}
				}
				break;
			case 9:
				mut_op=iter_intr_locs(mutated_input,actual_size,mut_op,&m_start,cur_intr_locs);
				m_end=m_start+1;

				break;
			case 10:
				mut_op=kw_ow_linear(mutated_input,actual_size,mut_op,&m_start);
				m_end=m_start+1;
				if (mut_op == it_end+1){ /* Prepare for non iterative mutators */
					ni_counter = NI_MAX_ROUNDS;
				}
				break;

			/*
				Non iterative start
				Mutated size is used instead of actual size
				since the input is now dynamic
			*/
			case 15:
				mut_kw_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);
				break;
			case 16:
				mut_kw_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
				break;
			case 17:
				//CHECK_FOR_STRUCTURED();

				mut_random_ow(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+4;
				break;
			case 18:
				//CHECK_FOR_STRUCTURED();

				mut_random_ins(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
				m_end=m_start+4;
				break;
			case 19:
				mut_copy_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);
				break;
			case 20:
				mut_copy_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
				break;
			case 21:
				mut_shrink_size(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
				m_end=m_start+1;
				break;
			case 22:
				mut_sw_bytes(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;

			case 23:
				mut_rand_flip(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 24:
				mut_over_rand_8_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 25:
				mut_over_rand_16_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 26:
				mut_over_rand_32_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+3;
				break;
			case 27:
				mut_rand_8_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 28:
				mut_rand_16_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 29:
				mut_rand_32_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+3;
				break;
			case 30:
				mut_rand_8_byte(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 31:

				CHECK_FOR_STRUCTURED();

				mut_insert_const(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);

				break;
			case 32:
				CHECK_FOR_STRUCTURED();

				mut_ow_const(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 33:
				mut_mix_inputs(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
				/*
					This check shouldn't be necessary
					I haven't done it for flip functions
					but just to play the role of a reminder
					that m_end might incorrectly be set to 0
					if not checked
				*/
				if (m_start!=-1){
					m_end=m_start+1;
				}
				break;
			case 34:
				CHECK_FOR_STRUCTURED();

				mut_ow_rand_chunk(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 35:

				mut_scatter_rand(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;

				break;
			case 36:

				mut_intr_locs(mutated_input,mutated_size,mut_op,&m_start,cur_intr_locs);

				m_end=m_start+1;
				break;

			/*
				Dictionary muts
				These must be the last two muts
			*/
			case 37:
				mut_dict_kw_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);

				break;
			case 38:
				mut_dict_kw_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
				break;
			default: //255
				mutation_finished =1 ;

				break;

		} //end switch

#ifdef _DEBUG
process_exec:
#endif
		pop=mut_op;
		if (need_csum && mut_op>1){
			aexit("Checksum enabled for wrong mut.");
		}
		if (mutation_finished){
			break;
		}

		/*************************************************************************************
			Fix potential problems in the generated inputs
			Do input-based tasks in general
		*/
		/*
			Check and trim generated input if it's
			bigger than allowed. This may happen after
			insertion functions.
			What's important is that we don't save or use
			anything beyond the size limit as violating
			this will break fuzzing logic regarding mut_map
		*/
		if (mutated_size > MAX_INPUT_SIZE){
			mutated_size = MAX_INPUT_SIZE;
		}

		/*
			mut functions are supposed not to return anything
			smaller than minimum bound.
		*/
		if (mutated_size < MIN_INPUT_SIZE){
			aexit("Current mut: %d - Detected too small input: %d",mut_op,mutated_size);
		}

		if (m_start == -1){
			/*
				Ignore this mut result.
				Either the called mut function didn't accept
				the input or it's not useful for exec.
			*/
			if (mut_op>=ni_start){
				tcount++;
				if (tcount>10){
					awarn("Detected poor NI response from target [%d]",++_poor_ni_tot);
				}
			}
			continue;
		}
		tcount=0;
if (m_start>mutated_size)aexit("wrong start:: %d %d %s",m_start,mutated_size,cmt);
		/*
			Since the save_debug info will only be called while this
			funtion's stack frame is still alive we can have debug_data
			safely point to data

		*/
		debug_data=(u8*)mutated_input;
		debug_data_size=mutated_size;



		/******************************************************************************/


		if (mut_op >= ni_start){
			ni_burst_counter++;

			/*
				We are in ni stage
				Check if it's time for another exec
			*/
			if (ni_counter==1 || ni_burst_counter == ni_mut_burst){
				ni_burst_counter = 0;
				must_refresh_input = 1;
				ni_mut_burst = 1 << (BURST_P_MIN + RU8(7));
				ni_exec_total++;
			}else{
				/*
					Nope, carry on mutating
				*/
				continue;
			}

		}

#ifdef RECORD_FEEDS
		if (net_mode){
			record(mutated_input,mutated_size);
		}
#endif

		//arep("Mutation technique %d started",mut_op);
		/*
			Execute with mutated input
		*/
		t_exit_stat=feed_ex_target(mutated_input,mutated_size);

		//printf("Stage 5: Evaluation\n");
		if (net_mode && !shared_trace_tree->root){
			/*
				We don't consider this a memory fault
				This is a strong sign that target has already died
				and packets are received by another instance
				of the target not run by arvin

				What happens in this scenario is that target
				is executed by arvin but dies soon because
				for example the src port is in use. However
				since the port is open in another instance
				outside the context of fuzzer, packets are
				delivered correctly to that instance instead
				of our executed instance. In this case fuzzer
				thinks target is still alive.
			*/
			awarn("Detected potential pre-mature death in target.");

		}
		debug_exit_code = t_exit_stat;

		/*
			Check result now
		*/
		//if (t_exit_stat == TRG_EX_CRASH) printf("crash\n");

		/*
			Any access to shared memory for
			multithread programs in network mode
			must be locked since the program is alive
		*/



		ACQUIRE_LOCK_CONDITIONED



		eval_tree(&new_input);

		c=coverage_changes();
		RELEASE_LOCK


		if (c>0){

			/*
				Consider adding to queue
				The input structure MUST be filled
				before sending it to queue_add_trace() as
				it's copied there to the queue and any change
				after that to this temp container isn't reflceted
				in the queue.
			*/

			int add_indx=-1;

			/*
				build and save m_path
			*/
			sprintf(f_o_path,"%s/states/g_input_%d",output_dir,save_i_count);
			strcpy(new_input.m_path,f_o_path);
			sprintf(f_o_path,"%s/q/g_input_%d",output_dir,save_i_count);
			strcpy(new_input.i_path,f_o_path);
			new_input.prio= (c==2?0:1);
			new_input.passed = 0;
			new_input.leaves = 0;
			new_input.fixed_loc = 0;
			new_input.marked=marked_tree;
			new_input.i_intr_locs = cur_intr_locs;
			new_input.invalidated_i=0;
			new_input.size=mutated_size;



			if (queue_add_traced(&new_input,&add_indx)){
				/*
					Was accepted to add.
					Save the new input in output directory
					No change to new_input container is now
					reflected in the queue.
				*/

				size_t wsz;
				f_out = fopen(f_o_path,"w");
				if (!f_out){
					aexit("generator: out_open");
				}
				wsz=fwrite(mutated_input,1,mutated_size,f_out);
				if (wsz!=mutated_size)
					aexit("write(): %lu %lu %s",mutated_size,wsz,f_o_path);
				fclose(f_out);



				if ((add_indx < queue_use_ind && queue_hit_sl==0) ||
					add_indx < 0 ||
					add_indx > INPUT_MAX){

					aexit("Queue violation: Input %s, QI %d, QA %d, prio %d",
					f_o_path,queue_use_ind,add_indx,new_input.prio);
				}



				save_i_count++;
				candidate_useful=1;

			}

		}else if(c==-1){
			/*
				Map is corrupted
				One important reason can be timeout
				when the target is terminated while
				lib is running
				This can or cannot be an intersting
				case.
				We'd better disregard it as a regular timeout
				like when target is waiting for user input
				But if it's not timeout then it can be dereferencing
				a wrong pointer in the target itself which has
				ended up in the shared memory.
				So it can be an interesting case.
			*/

			has_corrupted_shm = 1;
		}else{ //c==0
//if (xtemp)aexit("NOt accepted");
		}

		if (use_term_gui && brefr_freq_counter++ == brefr_freq){


			refresh_board(mutated_input,mutated_size,m_start,m_end);
			/*
				brefr_freq is dynamic and changes afte calling
				refresh_board()
			*/
			brefr_freq_counter=0;
		}

		/*
			We build the mutation map here and
			only for byte_flip. Along with mut_map
			keywords are also identified.

			This operation only happens when we meet
			the input for the first time in the queue.
		*/
		if (mut_op == 0){
			if (m_start >= MAX_INPUT_SIZE){
				aexit("There's an invalid input in the queue.");
			}


			if (current_csum != last_csum){
				/*
					Mark this location for
					future iterative mut operations
					as a good one.
					Also set it in mut_map_orig if perf_check
					is 1 or 2
				*/
				mut_map[m_start] = 1;
				if (perf_check_req>0){
					mut_map_orig[m_start]=1;
				}
				last_csum = current_csum;
				/*
					Check if we have any free slot in keywords
				*/
				if (kw_index < MAX_KEYWORDS ){
					/*
						We assume that any magic number or
						key word is at least 3 bytes long
					*/
					u64 kw_len = m_start - last_kw_pos;

					if (kw_len >2 && kw_len<= MAX_KW_SIZE){
						/*
							Do we already have it somewhere?
						*/
						if ( kw_is_new(mutated_input+last_kw_pos,kw_len)){
							memcpy(&keywords[kw_index].kw,mutated_input+last_kw_pos,kw_len) ;
							keywords[kw_index].size = (u8)kw_len;
							last_kw_pos = m_start;

							//printf("%02x %02x %02x\n",keywords[kw_index].kw[0],keywords[kw_index].kw[1],keywords[kw_index].kw[2]);
							/*if (kw_len==4 && !memcmp(keywords[kw_index].kw,"\x0c\x00\x00\x00",4))
								aexit("found %d",kw_index);*/
							/*keywords[kw_index].kw[kw_len]=0;
							if (kw_index==1){
								printf("%s\n",mutated_input);
								aexit("new keyword with len %s %d\n",keywords[kw_index].kw,keywords[kw_index].size);
							}*/

							kw_index++;
						}
					}
				}
			}else{
				/*
					else corresponding element remains zero
					dismiss this byte
				*/
				//if (!mut_map[m_start])printf("Skip %lu\n",m_start);
				//aexit("EQUAL %d %d %d",shared_trace_tree->depth,shared_trace_tree->count,shared_trace_tree->total_hits);
			}




			/*
				We  check performance here (mut_op=0)
				Last kw pos has already been saved
				We may zero current mut_map element
				We may reduce timeout
			*/
			if (curq->initial){
				/*
					It's worth to spend a little more time
					on the initial inputs.
					Dismiss performance check.
				*/

			}
			else if (!perf_check){
				/*
					Perfomance check is disabled
					per user request.
				*/

			}else if(perf_check == 1){
				/*
					Build map based on candidate_useful
					Don't change timeout.
					For low priority inputs, flip functions
					will be ignored regardless and this doesn't have
					any effect.

					TODO: Make sure this is a wise decision.
						- Apparently it's not
					Keep in mind that candidate useful is only
					used here for mut_op=0
				*/
				/*if(input_queue[queue_use_ind].prio==0)*/mut_map[m_start]= candidate_useful;

			}else if(perf_check == 2){
				/*
					Build based on candidate_useful
					and adjust timeout
				*/
				mut_map[m_start]= candidate_useful;
				if (!candidate_useful){
					if (init_check_burst < INIT_CH_BURST){
						init_check_burst++;
					}else{
						if (set_check_burst== SET_CH_BURST){
							if (unset_check_burst == UNSET_CH_BURST){
								set_check_burst = 0;
								unset_check_burst = 0;
								active_timeout = MIN_TIMEOUT_VAL;
								tm_perfcheck=1;
							}else{
								active_timeout = active_timeout_sav;
								tm_perfcheck=0;
								unset_check_burst++;
							}
						}else{
							active_timeout = MIN_TIMEOUT_VAL;
							tm_perfcheck=1;
							set_check_burst++;

						}
					}
					mut_map[m_start]=0; //no future iterative exec
				}else{
					active_timeout = active_timeout_sav;
					set_check_burst = 0;
					unset_check_burst = 0;
					if (init_check_burst < INIT_CH_BURST){
						init_check_burst = 0;
					}
				}
			}else{
				aexit("Invalid perf_check mode");
			}
			/*
				Performance check
			*/


		}//end if mut_op 0



		if(mut_op < it_end){
			/*
				Iterative functions can help us
				find interesting locations accurately
				iter_intr_locs is not used for this.

				This is only build for initial inputs
				and next inputs inherit them.
			*/
			if (curq->initial && cur_intr_locs->intr_locs_index < MAX_INTR_LOCS
				&& candidate_useful){
				int i;

				for (i=0;i<cur_intr_locs->intr_locs_index;i++){
					if (cur_intr_locs->intr_locs[i]==m_start){
						i=-1;
						break;
					}
				}
				if (i!=-1)
					cur_intr_locs->intr_locs[cur_intr_locs->intr_locs_index++] = m_start;

			}

		}


		/*
			exit status check
		*/
		if (t_exit_stat==-1){
			aexit("Unexpected state (%d) in target execution. (generator)",t_exit_stat);
		}
#define CHECK_ABNORMAL_MEMFAULT if (has_corrupted_shm){\
									save_rep(1,(u8*)mutated_input,mutated_size);\
									awarn("Detected possible memory fault. Input saved.");\
								}\

		if (target_id && t_exit_stat == TRG_NET_MODE){
			/*
					only happens in network mode
					target is alive and has responded to
					out input
					continue fuzzing the same target instance
			*/
			CHECK_ABNORMAL_MEMFAULT

		}else{
			/*
				target has terminated
				must be restarted in any mode
			*/
			if (t_exit_stat == TRG_EX_NORMAL){
				//arep("normal exit");
				CHECK_ABNORMAL_MEMFAULT
			}else if (t_exit_stat == TRG_SOFT_CRASH ){
				if (save_soft_crash){
					soft_crashes++;
					save_rep(1,(u8*)mutated_input,mutated_size);
				}

			}else if (t_exit_stat == TRG_EX_TMOUT){
				//awarn("Detected Target Timeout");

				if (!tm_perfcheck_prev && save_soft_crash){
					//if (active_timeout==2)aexit("TM");
					//aexit("zart %d",active_timeout);
					soft_crashes++;
					save_rep(1,(u8*)mutated_input,mutated_size);
				}

			}else {
				/*
					Crash
				*/
				//arep("CRASH : %d",t_exit_stat);

				total_crashes++;
				should_store=1;
				last_crash_mut=mut_op_used;
				save_rep(0,(u8*)mutated_input,mutated_size);

				//aexit("CRASH SAVED");
			}

#ifdef RECORD_FEEDS
			if (net_mode){
				store_reset(should_store);
				should_store = 0;
			}
#endif


		}

		//arep("Mutation technique %d completed",mut_op);

#ifdef LIVE_STAT
		live_rep();
#endif

		/*
			Do Statistics about
			this input here
		*/



		switch (pm_mode){
			case 0:
				if (depth_grew){
					if (add_to_inputs){
						save_mutated_input(mutated_input,mutated_size);
					}
				}
				break;
			case 1:
			case 2:
				/*
					In contrast to max_depth which is the depth
					of the deepest tree, total_covered
					is the cumulative variable showing total
					number of nodes in all trees.
					However for saving inputs we have 2 options:
					1- we consider those
					with individual nodes growth like those
					with individual depth growth for the pm0

					2- We consider total_covered instead. This
					will save many more new inputs which can be
					a little excessive.

				*/
				if (invd_tree_nodes_grew){
					if (add_to_inputs){
						save_mutated_input(mutated_input,mutated_size);
					}
				}

				break;
		}

		if (total_covered > total_covered_sav){
			add_stat_entry();
		}

		/*
			Post process: comm map TODO LOCK
			Report all time consuming bbs
		*/

		//report_bb_candidates();

		//if (c)aexit("Requested to remove %d",c);
	} //end while

	/*
		Final post-process of the given original input

		Save mut_map of this input so that in the next cycle
		we don't waste time to make it.
	*/
	if (!curq->passed)
		store_mut_map();


	/*
		We're done with this input
		release memory
	*/
	c=munmap(saved_input,alloc_size);
	if (c) aexit("generator: munmap()");
	c=munmap(mutated_input,alloc_size);
	if (c) aexit("generator: munmap()");

	//aexit("ONE PASS TEST, ni:%lu",ni_exec_total);
	//aexit("P %d",cur_intr_locs->intr_locs_index);
	return;
}


void adjust_timeout(){
	int i=0,c;
	void *data;
	FILE *f;
	int input_ind;
	size_t actual_size;
	u8 *p;
	struct stat st;
	struct timespec stime,etime;
	u64 stime_us,etime_us;
	u64 max_exec=0,exec_time;



	for (i=0;i<10;i++){
		input_ind = i%input_count;

		if (lstat(input_queue[input_ind].i_path,&st)==-1){
			aexit("adjust_timeout: stat");
		}

		actual_size = st.st_size;


		data = mmap(0,actual_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);


		if (data==MAP_FAILED ){
			aexit("adjust_timeout failed due to memory inaccessiblity");
		}
		f = fopen(input_queue[input_ind].i_path,"r");
		if (!f){
			aexit("adjust_timeout: open failed");
		}
		p = data;
		while((c=fgetc(f)) != -1){
			*p++ = c;
		}
		fclose(f);


		write_g_input(data,actual_size);
		clock_gettime(CLOCK_REALTIME,&stime);

		execute(0);

		clock_gettime(CLOCK_REALTIME,&etime);
		stime_us=stime.tv_sec*1000000 + stime.tv_nsec/1000;
		etime_us=etime.tv_sec*1000000 + etime.tv_nsec/1000;
		exec_time = etime_us- stime_us;
		printf("%d : %s : %luus\n",input_ind,input_queue[input_ind].i_path,exec_time);


		if (exec_time > MAX_TIMEOUT_VAL_INIT){
			aexit("Running target with input '%s' is too slow (>%lums).\n"
				  "\t Either change your arguments or remove this input and restart arvin."
				  ,input_queue[input_ind].i_path,MAX_TIMEOUT_VAL_INIT/1000);
		}
		if (exec_time > max_exec){
			max_exec=exec_time;
		}

		if (munmap(data,actual_size)){
			aexit("adjust_timeout(): munmap()");
		}
	}

	/*
		Add a little more time to tolerate
		execution time variances.
	*/

	max_exec+=(TOI_RATE)*(max_exec);

	if (max_exec > MAX_TIMEOUT_VAL_RUN)
		max_exec=MAX_TIMEOUT_VAL_RUN;

	active_timeout = max_exec;

	/*
		We don't want to take the opportunity of
		running more number of nodes from the
		target completely.
	*/
	if (active_timeout < MIN_TIMEOUT_VAL){
		active_timeout = MIN_TIMEOUT_VAL;
		//printf("too short\n");
	}
	//active_timeout=50000;

	arep("Chosen timeout: %lums",active_timeout/1000);

}
void do_post_process(){
	/*
		We mark the last input that we passed to prevent
		re-running all the iterative functions in case
		we cycle the queue after it's full.
	*/
	input_queue[queue_use_ind].passed = 1;

	switch (pm_mode){
		case 0:
			/*
				We also do not want to be too hard
				on LPSD inputs.
				Do a threshold check here and
				recharge LPSD back to full space
				if it's ready.
			*/
			if (input_queue[queue_use_ind].prio == 0){
				if (input_queue[queue_use_ind].depth < LPSD_MAX_DEPTH){
					if (LPSD_queue[input_queue[queue_use_ind].depth] > MLPSD_ALLOWED){
						if (LPSD_queue_wait[input_queue[queue_use_ind].depth] < MLPSD_ALLOWED){
							LPSD_queue_wait[input_queue[queue_use_ind].depth]++;
						}else{
							LPSD_queue_wait[input_queue[queue_use_ind].depth]=0;
							LPSD_queue[input_queue[queue_use_ind].depth]=0;
							/*
								More entries of this depth can be
								added now by the generator
							*/
						}
					}
				}
			}

			break;
		case 1:
			if (input_queue[queue_use_ind].prio == 0){
				if (input_queue[queue_use_ind].total_blocks < LPSC_MAX_NODES){
					if (LPSC_queue[input_queue[queue_use_ind].total_blocks] > MLPSC_ALLOWED){
						if (LPSC_queue_wait[input_queue[queue_use_ind].total_blocks] < MLPSC_ALLOWED){
							LPSC_queue_wait[input_queue[queue_use_ind].total_blocks]++;
						}else{
							LPSC_queue_wait[input_queue[queue_use_ind].total_blocks]=0;
							LPSC_queue[input_queue[queue_use_ind].total_blocks]=0;
							/*
								More entries of this size of nodes can be
								added now by the generator
							*/
						}
					}
				}
			}
			break;

		case 2:

			break;

	}//end switch

}


/*
	We don't return from this function
	until fuzer is closed by user or an irrecoverable
	error happens.

	Initial execution sequence:
		1- arvin_start() -> execute() -> Store Graph
		       |
		2-    `-> adjust_timeout() -> execute() -> Don't store graph
		      |
		3-    `-> arvin_generate()

	'1' is the slowest. Before next execution happens (either in stage 1 or 2
	depending on the number of seeds) the fist CFG pass happens. Removing
	bbs itself is time comnsuming so the second execute() is the second slowest
	one. However the difference between the first and second execution is
	significant.

*/
void arvin_start(){
	int t_exit_stat;
	int i;
//need_csum=1;
	shared_trace_tree = shm_adr + LOCK_DELTA + PROT_VARS_DELTA;//(struct tree*)shmat(shm_id,0,0);
	if (!shared_trace_tree){
		aexit("trace_tree");
	}
	arep("tree at: %016lx",shared_trace_tree);

	for (i=0;i<input_count;i++){

		if (file_feed){
			prepare_file_feed(input_queue[i].i_path);
		}else{

			modify_input(input_queue[i].i_path);
		}


		/*
				execute for this input and see
				it works as expected or not
				it shouldn't crash the target
				Input is fed only for regular mode
				target is then terminated.
				For net mode input is not sent

				TODO:Don't activate timer for initial
				runs of inputs since library has not
				identified expensive nodes yet.
		*/


		t_exit_stat = execute(1);

		if (t_exit_stat==-1){
			aexit("Unexpected state in target execution");
		}
		if (t_exit_stat==TRG_EX_CRASH){
			aexit("Input %s crashed target with signal %s",input_queue[i].i_path,get_sig_name(last_crash_sig));
		}
		if (net_mode && t_exit_stat == TRG_EX_NORMAL){
			aexit("Target does not stay alive to handle connections.");
		}

		if (!net_mode){
			/*
					Save the tree
			*/

			eval_tree(&input_queue[i]);

			/*
				Just add to local blocks
				don't need to check return value
			*/
			coverage_changes();//aexit("%lu",total_covered);

		}
		arep("Checked %s",input_queue[i].i_path);

	}

	if (coverage_only){
		arep("Coverage with this input set: %lf",(double)total_covered/_st_indp);
		znormal_exit();
	}
	if (user_timeout==-1){
		if (!net_mode){
			adjust_timeout();
			//aexit("TEST");
		}
	}else{
		active_timeout = user_timeout;
	}

	if (cov_show_only){
		arep("Total covered basic blocks: %lu",total_covered);
		znormal_exit();
	}

	active_timeout_sav = active_timeout;

	while(1){

		next_queue_ind();
		/*
			Now start generating new inputs
			based on this input
		*/

		arvin_generate();
		/*
			at this point new inputs might have been added
			to the queue by the generator.
		*/
		do_post_process();


		//sleep(1);


	}

}

static inline void raw_f_in(void *mem,struct input_val *curq){
	FILE *f_in;
	size_t c;

	f_in = fopen(curq->i_path,"r");
	if (!f_in)
		aexit("fopen(): raw_f_in %s",curq->i_path);

	size_t read_size=curq->size;

	c=fread(mem,1,read_size,f_in);

	if (c != read_size){
		if (parallel_mode)
			awarn("fread(): raw_f_in file size changed: %s %lu to %lu ",curq->i_path,curq->size,c);
		else{
			aexit("fread(): raw_f_in file size changed: %s %lu to %lu ",curq->i_path,curq->size,c);
			print_queue();

		}
	}

	fclose(f_in);

}
void arvp_mut(){
	u64 max_ni_rounds=1<<16;
	u64 ni_exec=0;
	u8 arvp_ni_count=(dict_file)?19+2:19;
	size_t m_start,m_end;
	void *saved_input, *mutated_input;
	size_t alloc_size,actual_size,mutated_size;
	struct input_val *curq;
	u8 mut_op;
	int c;

	curq=&input_queue[queue_use_ind];
	actual_size = curq->size;
	alloc_size = MAX_INPUT_SIZE*2;//((actual_size/1024)+2)*1024;// == actual_size ? actual_size : ((actual_size/1024)+1)*1024 ;
	saved_input = mmap(0,alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	mutated_input = mmap(0,alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);

	if (saved_input==MAP_FAILED || mutated_input==MAP_FAILED){
		aexit("generator failed due to memory inaccessiblity");
	}

	raw_f_in(saved_input,curq);


	while(ni_exec<max_ni_rounds){
		int t_exit_stat;
		u32 i=0;

		memcpy(mutated_input,saved_input,actual_size);
		mutated_size = actual_size;

		/*
			We don't have mut_map in parallel mode.
			Reserve some initial bytes of the input to avoid
			corrupting header and easily get stuck in an error handler
		*/

		if (actual_size>2*MIN_INPUT_SIZE && reserve_hdr){
			mutated_input+=rsv_hdr_size;
			mutated_size-=rsv_hdr_size;
		}

		while(1){
			u8 rand_ni;

			i++;
			rand_ni=RU8(arvp_ni_count) + 1;
			mut_op = rand_ni;
			switch (mut_op){
				case 1:
					mut_random_ow(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 2:
					mut_random_ins(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
					m_end=m_start+4;
					break;
				case 3:
					mut_copy_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);
					break;
				case 4:
					mut_copy_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
					break;
				case 5:
					mut_shrink_size(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
					m_end=m_start+1;
					break;
				case 6:
					mut_sw_bytes(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 7:
					CHECK_FOR_STRUCTURED();

					mut_rand_flip(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 8:
					mut_over_rand_8_int(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 9:
					mut_over_rand_16_int(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+2;
					break;
				case 10:
					mut_over_rand_32_int(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+3;
					break;
				case 11:
					mut_rand_8_add_sub(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 12:
					mut_rand_16_add_sub(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+2;
					break;
				case 13:
					mut_rand_32_add_sub(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+3;
					break;
				case 14:
					mut_rand_8_byte(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 15:
					CHECK_FOR_STRUCTURED();

					mut_insert_const(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
					break;
				case 16:
					CHECK_FOR_STRUCTURED();

					mut_ow_const(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 17:
					mut_mix_inputs(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
					if (m_start!=-1){
						m_end=m_start+1;
					}
					break;
				case 18:
					CHECK_FOR_STRUCTURED();

					mut_ow_rand_chunk(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 19:
					mut_scatter_rand(mutated_input,mutated_size,mut_op,&m_start);
					m_end=m_start+1;
					break;
				case 20:
					mut_dict_kw_ow(mutated_input,mutated_size,mut_op,&m_start,&m_end);
					break;
				case 21:
					mut_dict_kw_ins(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);
					break;

				default:
					aexit("arvp_mut: Invalid ni code");
			}

			pop=mut_op;

			/*
				Possible clean up after EACH mutation
				no matter we'll have an execution for this
				input or not.
				We check this before m_start
			*/
			if (mutated_size > MAX_INPUT_SIZE){
				/*
					Trim it if an insertion mut
					has execeeded the limit
				*/
				mutated_size = MAX_INPUT_SIZE;
			}

			if (mutated_size < MIN_INPUT_SIZE){
				aexit("Current mut: %d - Detected too small input: %d",mut_op,mutated_size);
			}

			if (m_start==-1)
				continue;

if (m_start>mutated_size)aexit("wrong start:: %d %d %s",m_start,mutated_size,cmt);
			if (i>2 && !RU8(16))
				break;
		}

		ni_exec++;

		/*
			Revert changes to pointers and size after rsv_header.
		*/

		if (actual_size>2*MIN_INPUT_SIZE && reserve_hdr){
			mutated_input-=rsv_hdr_size;
			mutated_size+=rsv_hdr_size;

			/*
				Final boundary check after header change.
			*/
			if (mutated_size > MAX_INPUT_SIZE){
				mutated_size = MAX_INPUT_SIZE;
			}
		}

		/*
			Execute
		*/
		t_exit_stat = feed_ex_target(mutated_input,mutated_size);

		if (t_exit_stat==-1){
			aexit("Invalid exit status");
		}
		if (t_exit_stat == TRG_SOFT_CRASH ){
			/*
				Soft crash in net mode.
			*/
			if (save_soft_crash){
				soft_crashes++;
				save_rep(1,(u8*)mutated_input,mutated_size);
			}

		}else if (t_exit_stat == TRG_EX_TMOUT){
			if (save_soft_crash){

				soft_crashes++;
				save_rep(1,(u8*)mutated_input,mutated_size);
			}

		}else if (t_exit_stat==TRG_EX_CRASH) {
			total_crashes++;
			last_crash_mut=mut_op;
			save_rep(0,(u8*)mutated_input,mutated_size);

		}

		if (use_term_gui && brefr_freq_counter++ == brefr_freq){

			refresh_board(mutated_input,mutated_size,m_start,m_end);
			/*
				brefr_freq is dynamic and changes afte calling
				refresh_board()
			*/
			brefr_freq_counter=0;
		}



	} //end while

	c=munmap(saved_input,alloc_size);
	if (c) aexit("arvp_mut: munmap()");
	c=munmap(mutated_input,alloc_size);
	if (c) aexit("arvp_mut: munmap()");
}

/*
	Parallel instance fast fuzzing
	This function doesn't return.
*/
void arvin_fast_parallel_fuzz(){
	if (user_timeout==-1)
		active_timeout = MIN_TIMEOUT_VAL;
	else
		active_timeout = user_timeout;
	while (1){
		next_queue_ind();
		arvp_mut();
		arvp_queue_update();
	}
}
u8 in_queue(char *p){
	int i;

	for (i=0;i<queue_ind;i++){
		if (!strcmp(p,input_queue[i].i_path))
			return 1;
	}

	return 0;

}
void arvp_queue_update(){
	struct dirent **entry_list;
	int ent_count;
	int i=0;
	struct stat st;
	char qp[MAX_PATH];

	sprintf(qp,"%s/q",output_dir);

	ent_count = scandir(qp,&entry_list,0,alphasort);

	if (ent_count - 2== queue_ind - input_count ) //2: . and ..
		return;

	for (i=0;i<ent_count;i++){
		char *ent_name;
		char ent_path[MAX_PATH];


		ent_name = 	entry_list[i]->d_name;
		if (strcmp(ent_name,".")==0 || strcmp(ent_name,"..")==0) continue;

		sprintf(ent_path,"%s/q/%s",output_dir,ent_name);

		if (in_queue(ent_path)){
			continue;
		}

		if (lstat(ent_path,&st)<0){
			printf("%s\n",strerror(errno));
			aexit("stat failed");
		}

		if(!S_ISDIR(st.st_mode)){
			/*
				Add to queue
			*/
			if (st.st_size < MIN_INPUT_SIZE || st.st_size > MAX_INPUT_SIZE){
				aexit("Input '%s' violates default size limit",ent_path);
			}

			queue_add(ent_path,0,0,st.st_size);
			free(entry_list[i]);

		}
	}
	if (entry_list)
		free(entry_list);
}

void arvp_fserver(){
	int n;
	char buf;

	pipe(cmd_send);
	pipe(response_recv);

	starter_id = fork();

	if (!starter_id){

		/*
			I'm starter
			Establish communication channel
		*/
		dup2(cmd_send[0],FD_IN);
		dup2(response_recv[1],FD_OUT);
		close(cmd_send[1]);
		close(response_recv[0]);
		close(cmd_send[0]);
		close(response_recv[1]);

		if (file_feed){
			close(0);
			/*
				We expect the user to have correctly written
				the input filename as the argument to the target
				It has to be the same as -f for zharf

				NOTE: There are cases that in this mode
				target still prompts for user input
				since stdin is closed, it will fail
				If it keeps trying, it will eventually timeout.
			*/
		}else{
			dup2(feed_fd,0);
			close(feed_fd);
		}

		dup2(dev_null,1);
		dup2(dev_null,2);

		/*
			Send it to its own session to prevent
			from potentially messing up our fuzzer's terminal.
		*/
		if (setsid()==-1){
			aexit("setsid()");
		}

		close(dev_null);

		/*
			In case lib is not installed
			system-wide.
			Is that case we assume the user is
			running the fuzzer from the directory
			that libzh and zharf reside.
		*/
		setenv("LD_LIBRARY_PATH",".",1);
		setenv("LD_PRELOAD","libarvp.so",1);

		execv(target_path,target_argv);
		n=open("/dev/tty",O_WRONLY);
		if (n!=1){
			dup2(n,1);
			close(n);
		}
		aexit("Running starter (%s) failed. %s",target_argv[0],strerror(errno));
		/*
			TODO: Terminate parent
		*/
	}

	/*
		Back to fuzzer
	*/
	close(response_recv[1]);
	close(cmd_send[0]);

	arep("Verifying forkserver in parallel mode is active...");
	n=read(response_recv[0],&buf,1);
	if (n<1){
		aexit("Can't read response from starter");
	}
	/*
		Starter is ready to receive
		fork command. We're done here
	*/

	arep("Starter ready");
}

/*
	It's a normal assumption for keywords to be ACCII data.
	But fgets() read reads and stores anything except new line.
	Keywords aren't essentially ASCII data and we accept this
	in our design.
	Keyword can be anything as long as its size
	is limited to DICT_MAX_KW_SIZE and it doesn't
	contain a new line char.

	TODO: Currently non-string data as dictionary entries is
	not supported. The reason is entries are read during
	mutation using strcpy() which stops at null byte.
*/
void load_dictionary(){

	int kwp=0;
	FILE *f;
	char kw_line[2*DICT_MAX_KW_SIZE];

	memset(kw_line,0,2*DICT_MAX_KW_SIZE);

	f=fopen(dict_file,"r");

	if (!f){
		aexit("Loading dictionary failed");
	}

	while (fgets(kw_line,2*DICT_MAX_KW_SIZE,f)){
		if (dict_kw_count == DICT_MAX_KEYWORDS){
			aexit("Too many keywords in dictionary. Max %d",DICT_MAX_KEYWORDS);
		}
		if (kw_line[0]=='\n')
			continue;

		dict_kws[dict_kw_count]=calloc(DICT_MAX_KW_SIZE+1,1); //+1 is null terminator

		if (!dict_kws[dict_kw_count]){
			aexit("calloc");
		}

		kwp=0;
		while (kw_line[kwp]!='\n' && kw_line[kwp]){
			if (kwp==DICT_MAX_KW_SIZE){
				kw_line[strlen(kw_line)-1]=0;
				aexit("Keyword '%s...' is too long.",kw_line);
			}
			dict_kws[dict_kw_count][kwp]=kw_line[kwp];
			kwp++;
		}

		dict_kw_count++;
		memset(kw_line,0,2*DICT_MAX_KW_SIZE);
	}


	fclose(f);



}
void print_banner(){

	printf(CRED"--------------------<[ ARVIN ]>------------------\n"CNORM);
	printf(CGREEN"*\t  V: 1.0 - ~cyn\t\t\t\t*\n"CNORM);
#ifdef ARV_BIG_TARGET
	printf(CGREEN"*\t  Compile mode: BT\t\t\t*\n"CNORM);
#else
	printf(CGREEN"*\t  Compile mode: N\t\t\t*\n"CNORM);
#endif


	printf(CGREEN"*************************************************\n\n"CNORM);
}


void init_net_essentials(){
	/*
		First check and make sure the given port
		is initially closed
	*/


	init_socket();
	inet_pton(AF_INET,target_ip,&(target_saddr.sin_addr));
	target_saddr.sin_family = AF_INET;
    target_saddr.sin_port = htons(tcp_port);
}
void read_to_mem(void *data,char *path){
	FILE *f;
	int c;
	u8* p=data;

	f=fopen(path,"r");
	if (!f){
		aexit("read_to_mem()");
	}

	while ((c=fgetc(f))!=-1){
		*p++ = c;

	}

	fclose(f);
}

void pin_to_cpu(){
	u8 cpu_count = 0;
	FILE *stat_file;
	char line[1024];
	struct stat st;
	DIR *d;
	struct dirent* entry;
	u8 *cpus;
	int i;
	cpu_set_t free_cpu;

	stat_file=fopen("/proc/stat","r");
	if (!stat_file){
		aexit("pin_to_cpu(): fopen");
	}

	while (fgets(line,sizeof(line),stat_file)){
		if (!strncmp(line,"cpu",3) && isdigit(line[3]))
			cpu_count++;
	}

	fclose(stat_file);

	arep("Found %d CPU cores in your machine",cpu_count);

	cpus = malloc(cpu_count);
	memset(cpus,0,cpu_count);

	d = opendir("/proc");
	while ((entry=readdir(d))!=NULL){
		char p_path[MAX_PATH];
		char *proc_file;
		char *cpu_aff_line;
		int cpu_num;
		int alloc_size = 4096;

		if (!isdigit(entry->d_name[0])) continue;

		sprintf(p_path,"/proc/%s/status",entry->d_name);

		if (lstat(p_path,&st)<0){
			/* Process has already died */
			continue;
		}
		if (st.st_size > 0){
			/*
				files in proc file system
				usually don't have a size stamp
				So we probably won't be here
			*/
			alloc_size = st.st_size;
		}

		proc_file = mmap(0,alloc_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
		if (proc_file==MAP_FAILED)
			aexit("pin_to_cpu(): mmap()");
		memset(proc_file,0,alloc_size);

		read_to_mem(proc_file,p_path);

		if (!strstr(proc_file,"VmSize")) continue;

		cpu_aff_line = strstr(proc_file,"Cpus_allowed_list");

		*(strchr(cpu_aff_line,'\n')+1) = 0;

		if (strchr(cpu_aff_line,'-') || strchr(cpu_aff_line,',')) continue;

		if (sscanf(cpu_aff_line+strlen("Cpus_allowed_list:\t"),"%u",&cpu_num)>0){
			cpus[cpu_num] = 1;
		}

		if (munmap(proc_file,alloc_size)){
			aexit("pin_to_cpu(): munmap");
		}
	}



	for (i=0;i<cpu_count;i++){
		if (!cpus[i]) break;
	}

	if (i==cpu_count){
		awarn("No free CPU. You are advised against running multiple instances of Arvin.");
	}

	/*
		There's at least one free core
	*/

	CPU_ZERO(&free_cpu);
	CPU_SET(i,&free_cpu);
	sched_setaffinity(0,sizeof(cpu_set_t),&free_cpu);

	attached_core = i;
	arep("Attached Arvin to cpu%d",i);

	free(cpus);
	closedir(d);

}
#ifdef _DEBUG
void test_muts(){
	u8 buf_test[256];
	u8 buf_test_save[64];
	int i;
	size_t dummy;

	memset(mut_map+2,1,5);
	printf("> %02x\n",check_interesting(2,0xffffFFFF));
	memset(mut_map+2,0,5);
	mut_map[8]=1;
	for(i=0;i<256;i++){
		buf_test_save[i] = RU8(255);
	}
	printf("Init: \n");
	dump_hex(buf_test_save,64,-1,-1);
	//char buf[256];
	//printf("%s\n",(char*)show_data(buf,buf_test_save,16,2,2));
	//exit(0);
	while(1){
		u8 mut_op = 16+RU8(10);
		printf("%d\n",mut_op);
		size_t m_start,m_end,mutated_size,actual_size=0;;
		void *mutated_input = buf_test;
		mutated_size=65;
		memcpy(buf_test,buf_test_save,64);
		switch (mut_op){
			case 17:
				mut_rand_flip(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 18:
				mut_over_rand_8_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 19:
				mut_over_rand_16_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 20:
				mut_over_rand_32_int(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+3;
				break;
			case 21:
				mut_rand_8_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 22:
				mut_rand_16_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+2;
				break;
			case 23:
				mut_rand_32_add_sub(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+3;
				break;
			case 24:
				mut_rand_8_byte(mutated_input,mutated_size,mut_op,&m_start);
				m_end=m_start+1;
				break;
			case 25:
				mut_insert_const(mutated_input,mutated_size,mut_op,&m_start,&m_end,&mutated_size);

				break;
			case 26:
				mut_ow_const(mutated_input,mutated_size,mut_op,&m_start);
		}

		//mut_insert_const(mutated_input,mutated_size,mut_op,&m_start,&mutated_size);
		dummy=m_start;
		dump_hex(buf_test,64,dummy,dummy+1);
		fgetc(stdin);
	}

}
#endif

void dir_check_create(char *p){
	if (access(p,F_OK))
		if (mkdir(p,0775))
			aexit("mkdir() failed: %s",strerror(errno));
}

void test_lib(char **args){
	pin_to_cpu();
	if (fork()==0){
		setenv("LD_PRELOAD","./libarv.so",1);
		setenv("LD_LIBRARY_PATH",".",1);
		//printf(">> %s\n",argv[2]);
		execvp(args[0],args);
		aexit("execlp(): %s - %s",strerror(errno),args[0]);
	}

	wait(0);


	aexit("Test Lib: Finished Operation\n");
}

int prepare_bhf(char *target_path){
	FILE *f;
	char hash_onfile[255];
	char hash_extract[255];
	int index=0,c;
	char cmd[4096];

	arep("Basic block profiling...");
	/*
		If bb_file doesn't exist or it's not
		for this target run the python script and
		extract basic blocks.
	*/
	f=fopen(BB_FNAME,"r");
	if (!f)
		goto extract;

	/*
		Read file hash from first line.
	*/
	fgets(hash_onfile,33,f);
	fclose(f);

	sprintf(cmd,"md5sum %s",target_path);

	f = popen(cmd,"r");
	while ((c=fgetc(f))!=EOF && index<32){
		hash_extract[index++] = c;
	}
	hash_extract[32]=0;
	pclose(f);
	if (strcmp(hash_onfile,hash_extract))
		goto extract;

	/*
		We have the correct file
	*/
	arep("A valid basic block information file for this target found from previous runs.");
	return 1;

extract:
	/*
		Check if angr is installed?
	*/



	f=popen("python3 -c 'import angr' 2>/dev/null","r");
	if (pclose(f)){
		aexit("angr not installed. Preprocessing of target failed.\n\t"
			"Install with: pip3 install angr");
	}

	f=fopen("/tmp/.bbextract","w");

	if (!f)
		aexit("/tmp directory is not accessible.");

	if (fwrite(bbextract_helper,sizeof(bbextract_helper),1,f)<1){
		aexit("fwrite(): %s",strerror(errno));
	}

	fclose(f);

	//sprintf(cmd,"python3 %s/bbextract.py %s >%s 2>/dev/null",base_dir,target_path,BB_FNAME);
	sprintf(cmd,"python3 /tmp/.bbextract %s >%s 2>/dev/null",target_path,BB_FNAME);

	arep("> > Extracting basic block information...");
	if (system(cmd)){
		aexit("Preprocessing stage not completed. Angr failed to parse the file.");
	}
	arep("> > Basic block information stored in './%s'.",BB_FNAME);

	return 1;

}
int main(int argc,char **argv){
	int opt;
	char *tmp_s=0;
	char tmp_path[1024];
	char gbuf[255];
	FILE *fgov,*f;
	int i;
#ifdef LIVE_STAT
	FILE *frep;
	char _sname[1024];
#endif
    struct sigaction sig_act;



	_st_bl=0;
	_st_indp=0;
	_st_nes=0;

	feed_file_path = 0;
	output_dir=0;
	input_dir=0;
	file_feed = 0;
	queue_ind = 0;
	queue_use_ind = -1;
	qc = 0;
	opterr = 0;

	/*
		Before anything, check terminal size
	*/
	struct winsize win_s;

	if (use_term_gui){
		if (!ioctl(0,TIOCGWINSZ,&win_s)){
			if (win_s.ws_row && win_s.ws_col){
				if (win_s.ws_row < 37 || win_s.ws_col<80){
					aexit("Your terminal window doesn't have enough space for output.\n"
						NLI"Either resize the terminal window or decrease your font size.");
				}
			}
		}
	}

	strcpy(base_dir, dirname(argv[0]));
	/*
		short options suffice for our case
		hence getopt instead of getopt_long
	*/
	while ((opt = getopt (argc, argv, "+i:o:n:df:mcp:gB:t:esak:ry:bPj")) != -1)
	switch (opt)
	{
		case 'P':
			parallel_mode=1;
			break;
		case 'b':
			test_lib(&argv[2]);
			break;
		case 'd':
			debug_mode=1;
        		break;
		case 'i':
			if (input_dir)
				aexit("Multiple input directories found in args");
			input_dir=optarg;
        		break;
		case 'o':
			if (output_dir)
				aexit("Multiple output directories found in args");
			output_dir=optarg;
			break;
		case 'f':
			file_feed = 1;
			feed_file_path = optarg;
			break;
		case 'n':
			net_mode = 1;
			tmp_s = optarg;
			break;
		case 'm':
			target_mult_threaded = 1;
			break;
		case 'c':
			enable_custom_cleanup = 1;
			break;
		case 'p':
			pm_str = optarg;
			break;
		case 'g':
			coverage_only = 1;
			use_term_gui = 0;
			break;
		case 'B':
			_st_indp = 0;
			sscanf(optarg,"%lu",&_st_indp);
			break;
		case 'T':
			_st_bl = 0;
			sscanf(optarg,"%lu",&_st_bl);
			break;
		case 'N':
			_st_nes = 0;
			sscanf(optarg,"%lu",&_st_nes);
			break;
		case 't':
			sscanf(optarg,"%lu",&user_timeout);
			break;
		case 'e':
			cov_show_only = 1;
			use_term_gui =0;
			break;
		case 's':
			save_soft_crash=1;
			break;
		case 'a':
			add_to_inputs = 1;
			break;
		case 'k':
			sscanf(optarg,"%d",&perf_check_req);
			break;
		case 'r':
			should_store_graph=1;
			break;
		case 'y':
			dict_file = optarg;
			break;
		case 'j':
			reserve_hdr=1;
			break;
		case 'h':
			print_usage();
			break;
      		case '?':
      			if (isprint(optopt))
      				printf("Unsupported option: %c\n",optopt);
      			print_usage();
	}
	if (debug_mode){
		debug_memory((void *)0x00007efde8863000);
		return 0;
	}
	if (optind==argc || input_dir==0 || output_dir==0){
		print_usage();
	}

	if (reserve_hdr && !parallel_mode){
		aexit("Option -j is only supported in parallel mode.");
	}

	/*
		Remove trailing slash
	*/
	if (output_dir[strlen(output_dir)-1]=='/'){
		output_dir[strlen(output_dir)-1]=0;
	}
	print_banner();

	instance_pid = getpid();
	arep("Running as %d",instance_pid);
	target_path=argv[optind];
	target_argv=&argv[optind];

	/*
		TODO: Check if the target is LLVM ASAN compiled.
		Those binaries don't work with arvin. Because the library
		code itself will end up running code (probably glibc functions)
		that is part of the sanitizer code. The library will hit a 0xCC
		instruction and crash.

		We have either to abort if the binary is ASAN compiled or we have
		to fix this problem by identifying ASAN functions and exclude them
		from instrumentation. Probably the easiest way to do this is to instruct
		angr to ignore the problematic basic blocks.
	*/

	/*
		Check for FILE_ARG
	*/
	for (i=optind;i<argc;i++){
		if (!strcmp(argv[i],FILE_ARG)){
			if (file_feed){
				aexit("-f and %s can't be used together.",FILE_ARG);
			}
			file_feed=1;

			/*
				This memory we'll be used thourghout execution
				Freeing isn't necessary.
			*/
			feed_file_path=calloc(255,1);
			if (!feed_file_path)
				aexit("Memory allocation failed.");

			sprintf(feed_file_path,"%s%d",FILE_ARG_NAME,instance_pid);
			arep("Using %s as file feed to the target",feed_file_path);
			argv[i]=feed_file_path;

			break;

		}
	}

	/*
		For stdin-based feed, similarly create
		instance specific files.
	*/
	if (!file_feed){
		cinp_file_path=calloc(255,1);
		if (!cinp_file_path)
				aexit("Memory allocation failed.");
		sprintf(cinp_file_path,"%s%d",CURRENT_INPUT,instance_pid);
	}

	if (access(input_dir,F_OK) || access(output_dir,F_OK)){
		aexit("input/output path not accessible");
	}

	if(access(target_path,F_OK)){
		aexit("The given target program is not accessible. Check the path.");
	}



	pin_to_cpu();



	if (dict_file){
		arep("Using dictionary file: %s",dict_file);
		load_dictionary();
	}

	starter_id = 0;
	time(&start_time);

	dev_null = open("/dev/null",O_RDWR);
	read_inputs();
	arep("Found total %u inputs",input_count);
	if (queue_ind==0) {
		aexit("No input file");
	}
	prepare_input_feed();

	signal(SIGINT,int_handler);
	signal(SIGALRM,target_timeout);
	signal(SIGPIPE,pipe_handler);
	//signal(SIGSEGV,sigf_handler); //We use sigaction for this instead.
	sig_act.sa_flags=SA_SIGINFO;
	sig_act.sa_sigaction = &sigf_handler;
	sigaction(SIGSEGV, (const struct sigaction *)&sig_act,NULL);

	sprintf(master_instance_p,"%s/masterp",output_dir);

	if (user_timeout!=-1){
		user_timeout*=1000; //mili to micro
		if (user_timeout > MAX_TIMEOUT_VAL_RUN || user_timeout < MIN_TIMEOUT_VAL){
			aexit("Invalid timeout value.");
		}else{
			/*
				We set it in arvin_start() or arvin_fast_parallel_fuzz()
			*/
			//active_timeout = user_timeout;
		}
	}

	f=fopen("/proc/sys/kernel/randomize_va_space","r");
	int mode = fgetc(f);
	if (mode==-1){
		aexit("ASLR check status failed.");
	}
	if (mode-48==0){
		aexit("ASLR is disabled. Enable ASLR before running Arvin.");
	}
	fclose(f);

	arep("Fuzzing: '%s'",target_path);
	arep("Reading input from '%s'",input_dir);
	arep("Writing results in '%s'",output_dir);

	if (parallel_mode){
		if (net_mode)
			aexit("-P switch in network mode is not allowed.");
		/*
			Do we already have a master instance
		*/
		if (access(master_instance_p,F_OK)){
			aexit("Missing master instance: First run one instance of arvin without -P option.");
		}

		awarn("Unique crash detectin will be disabled in parallel mode.");

		arvp_fserver();
		goto finalize_init;

	}else{
		int fd;

		/*
			We need a clear queue for parallel instances.
			See if there's remnants of previous run in
			the queue and remove if there's any
		*/
		sprintf(tmp_path,"%s/q/g_input_0",output_dir);
		if (!access(tmp_path,F_OK)){
			awarn("Clearing the queue of a previous run");
			sprintf(tmp_path,"rm -rf %s/q/*",output_dir);
			system(tmp_path);
		}

		fd=open(master_instance_p,O_WRONLY|O_CREAT);
		if (fd==-1)
			aexit("open() %s",master_instance_p);
		close(fd);

		prepare_bhf(target_path);
	}

	fgov=fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor","r");
	if (fgov){
		fgets(gbuf,12,fgov);
		if (strcmp(gbuf,"performance"))
			awarn("CPU governor mode is not optimial.");
		fclose(fgov);
	}else{
		//It's not Intel
		//aexit("fopen(): CPU governer core 0");
	}


	if (_st_indp<1){
		if (coverage_only)
			aexit("User didn't provide total number of basic blocks (can be obtained from arvin stats)");
		else
			arep("User didn't provide total number of basic blocks (can be obtained from arvin stats)");
	}

	if (perf_check_req > 2 || perf_check_req < 0){
		aexit("Invalid mode of performance check. (use 0,1,2. default:0)");
	}

	/*
		For network mode performance check
		won't have much effect since we work
		with RECV_TIMEOUT.
		The only time active_timeout is used in
		that mode is for respawning a dead server.
		I prefer not to change socket timeout dynamically.
	*/
	perf_check = perf_check_req;



	//printf("%d %d\n",getpid(),getpgrp());


	if (net_mode){
		sscanf(tmp_s,"%d",&tcp_port);
		if (tcp_port <=0)
			aexit("Watch your port number input");
		init_net_essentials();
	}

	if (net_mode){
		arep("TCP mode is active, using port number %d",tcp_port);
	}
	if (target_mult_threaded){
		arep("Got -m switch. Depth comparison will be disabled.");
	}






	/*
		Priority mode check here
	*/
	if (pm_str){
		if (strcmp(pm_str,"TDF")==0){
			if (target_mult_threaded){
				aexit("A multithreaded program cannot be fuzzed in this "
						"priority model. Change either -p or -m options.");
			}
			pm_mode = 0;
		}else if (strcmp(pm_str,"TNF")==0){
			pm_mode = 1;
		}else if (strcmp(pm_str,"TNS")==0){
			pm_mode = 2;
		}else{
			aexit("Priority model not supported. Watch your -p option");
		}

		arep("Requested priority model: %s",pm_str);
	}else{
		arep("Priority model set to default as TDF");
		pm_str = DEFAULT_PM_STR;
	}

	if (_st_bl)
		arep("Total number of basic blocks: %lu",_st_bl);
	else
		arep("Total number of basic blocks: Not provided");

	if (user_timeout!=-1){
		arep("User requested timeout: %lums",user_timeout/1000);
	}

	arep("Performance-check mode: %d",perf_check);

	if (should_store_graph){
		arep("Graphs will be stored per user request.");
	}

	if (lpq_balance){
		arep("LPQ Balance mode 1 in effect.");
	}else{
		arep("LPQ Balance mode 0 in effect.");
	}





	/*
		Run the starter and initialize it
		This is first transition to libarv.so.
	*/


	run_starter();

	if (coverage_only){
		arep("Coverage mode requested: will only run one iteration and exit.");

	}



#ifdef LIVE_STAT
	frep=fopen(LIVE_REP_FILE,"w");
	if (!frep){
		aexit("Creating live stat file failed");
	}
	strcpy(_sname,target_path);
	fprintf(frep,"%lu\n%s\n%lu\n%lu\n%lu\n",
				 start_time,basename(_sname),_st_bl,_st_indp,_st_nes);
	fclose(frep);
#endif




/************ PM modes initializations here ****************/

	switch(pm_mode){
		case 0:
			memset(LPSD_queue,0,sizeof(LPSD_queue));
			memset(LPSD_queue_wait,0,sizeof(LPSD_queue));
			break;
		case 1:
			/*
				This is only allcoated once and lives
				until fuzzer termination. No need to unmap
			*/
			LPSC_queue=mmap(0,LPSC_MAX_NODES,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
			if (LPSC_queue==MAP_FAILED)
				aexit("mmap()");
			memset(LPSC_queue,0,LPSC_MAX_NODES);

			LPSC_queue_wait=mmap(0,LPSC_MAX_NODES,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
			if (LPSC_queue_wait==MAP_FAILED)
				aexit("mmap()");
			memset(LPSC_queue_wait,0,LPSC_MAX_NODES);
			break;
		case 2:

			break;

	}

/***********************************************************/


/******** All File cleanup from previous runs here ********/



/**********************************************************/

finalize_init:
	clear_warn();


	arvin_init_state = 0;


	srand(start_time);

	if (use_term_gui)
		printf(HC);

	if (!parallel_mode){
		/* Set up output directories */
		sprintf(tmp_path,"%s/q",output_dir);
		dir_check_create(tmp_path);
		sprintf(tmp_path,"%s/crashes",output_dir);
		dir_check_create(tmp_path);
		sprintf(tmp_path,"%s/states",output_dir);
		dir_check_create(tmp_path);
		/*
			Fire up the fuzzer
		*/
		arvin_start();
	}else{
		arvin_fast_parallel_fuzz();
	}
	return 0;
}


