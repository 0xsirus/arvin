/*
	By Sirus Shahini
	~cyn
*/

#include "head.h"
#include "libarv.h"

u8 fserv_active = 0;
void *shm,*comm;
u8 debug_print=1;

void arep(char *fmt, ...){
	char s_format[2048];

	if (!debug_print) return;

	strcpy(s_format,CGREEN "[-] Lib: "CNORM);
	strcat(s_format,fmt);
	strcat(s_format,"\n");
	va_list argp;
	va_start(argp,s_format);
	vprintf(s_format,argp);
	va_end(argp);

}

/*void aexit(char *fmt, ...){
	char err_format[2048];

	strcpy(err_format,CRED "[!]" CNORM " Lib: ");
	strcat(err_format,fmt);
	strcat(err_format,"\n");
	va_list argp;
	va_start(argp,err_format);
	vprintf(err_format,argp);
	va_end(argp);
	exit(-1);
}*/
#define aexit(fmt,...) force_write_exit(1,fmt __VA_OPT__(,) __VA_ARGS__)

//************************* Graph Management ***********************************

#define HITS_THR	1

u64 *global_lock   ;
void save_memory(char *msg){
	char fname[255];
	FILE *f;

	sprintf(fname,"/tmp/lib_shared_mem_%016lx_%s",(u64)shm,".");

	f=fopen(fname,"w");

	if (!f){
		printf("Can't open memory");
		return;
	}

	if (fwrite(shm,1,SHM_SIZE,f)<SHM_SIZE){
		printf("Incomplete memory save\n");
	}
	fclose(f);
	printf("LIB: Memroy saved in %s\n",fname);
}
void force_write_exit(u8 flush,char *fmt, ...){
	char err_format[2048];
	int n;
	char out[512];

	if (flush){
		/*
			discard buffered data
		*/
		fflush(stdout);
	}

	n=open("/dev/tty",O_WRONLY);
	if (n!=-1 && n!=1){
		dup2(n,1);
		close(n);
	}

	sprintf(err_format,CRED "[!]" CNORM " Lib: [Target %d] ",getpid());
	strcat(err_format,fmt);
	strcat(err_format,"\n");
	va_list argp;
	va_start(argp,err_format);
	vsprintf(out,err_format,argp);
	va_end(argp);

	//save_memory(out);
	fflush(stdout);

	write(1,out,strlen(out));

	exit(-1);
	printf("LIB: not exited\n");
	//kill(getpid(),SIGTERM);
}







#ifdef DEBUG_MODE
#define TERMINATE(fl,fmt,...) force_write_exit(fl,fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define TERMINATE(fl,fmt,...) exit(-1)
#endif

/********** Tree management ************/

/*
	A tree resides in shared memory
	for this reason we will not use malloc to allocate
	new nodes. Nodes will be selected from the existing
	shared memory free area

*/


/*
	These variables are written only once by fork server
	So they don't change in case of fork in different
	processes
*/
/******* static lib variables start **************/
struct tree *trace_tree;

/*
	An array of this struct will be created
	to hold all traversed basic blocks
	in a sorted order
	This array is only used for quick searching
*/
struct block_info *blocks;

struct node *node_pool;
struct child_ptr *child_pool;

/*
	Use private pointer for list position
*/
struct node *current_node; //The last basic block in current process

/******** static lib variables end **************/


/*
	All these dynamic variables have to be
	stored in shared memory. If they stay in
	process private area this library will
	not work for multiprocess programs (those
	which fork() after execution)
	These variables are stored in SHM meta area
	after global lock.

	lock | current_node (8byte pointer) | cur_block (8 byte value) | cur_node_in_pool (8 byte value) | cur_child_in_pool (8 byte value)
*/

/****** Protected shared memory variables start **************/

/*
	All these three values are indexes
*/
/*
	points to the last block in leaner list
	which has the biggest id
*/
long *cur_block ;

/*
	The first ready node for allocation
*/
u64 *cur_node_in_pool;
u64 *cur_child_in_pool;



/*********** Protected shared memory variables end ***********/


/*
	Return a new node from node pull in shared memory
*/
struct node *ret_new_node(){
	return &node_pool[(*cur_node_in_pool)++];

}

struct node *alloc_node(ID_SIZE id){
	struct node* new_node = ret_new_node();

#ifdef DEBUG_MODE
	if (new_node->id || new_node->parent || new_node->children){
		TERMINATE(1,"Stale node %08x/%016lx/%016lx for %08x"
						,new_node->id,new_node->parent,new_node->children,id);
	}
#endif

	new_node->id=id;
	return new_node;

}
struct child_ptr *alloc_child_ptr(struct node *child_node){
	struct child_ptr* new_child=&child_pool[(*cur_child_in_pool)++];

#ifdef DEBUG_MODE
	if (!child_node){
		TERMINATE(1,"alloc_child_ptr: Invalid argument");
	}
	if (!new_child){
		TERMINATE(1,"alloc_child_ptr: Invalid new child ");
	}
#endif

	new_child->next_child=0;
	new_child->child_node=child_node;
	return new_child;
}
/*
	Do a quick binary search
*/
struct block_info* search(ID_SIZE id){
	long mid;
	long low=0,high=*cur_block;

	while(low<=high){
		mid = (low+high)/2;

		if (blocks[mid].id==id){
			return &blocks[mid];
		}
		else if(id < blocks[mid].id){
			high = mid-1;
		}else{
			low = mid+1;
		}
	}
	return 0;
}

struct child_ptr *add_child(struct node *parent, struct node *child_node){
	struct child_ptr *p = parent->children;
	struct child_ptr* new_child;

#ifdef DEBUG_MODE
	if (!parent || !child_node){
		TERMINATE(1,"add_child: Invalid %016lx %016lx",(u64)parent,(u64)child_node);
	}

#endif

	if (p==0){
		new_child = alloc_child_ptr(child_node);
		parent->children = new_child;
		trace_tree->total_children++;
		return new_child;
	}

	/*
		Check the child exists or not
		this happens when we go through the same path
		multiple times
	*/
	while(1){
		if (p->child_node == child_node){
			return 0; //No edge added
		}
		if (!p->next_child)break;
		p=p->next_child;
	}
	/*
		New child
		allocate a child node and add it
	*/
	new_child = alloc_child_ptr(child_node);
	p->next_child=new_child;

	trace_tree->total_children++;

	return new_child;

}
struct node * add_node(ID_SIZE new_id,ID_SIZE parent_id,BLOCK_INFO_SIZE blkinfo){
	struct block_info * cur,*tmp_pointer;
	struct block_info new_block,temp;
	struct node *new_node;
	struct node *parent_node;
	struct node * pparent;


	if (trace_tree->count ==0){
		/*  Reset shared memory structures */
		*cur_block = -1 ;
		*cur_node_in_pool=0;
		*cur_child_in_pool=0;
		current_node = 0;

		new_node = alloc_node(new_id);
		new_block.ptr = new_node;
		new_block.id = new_id;
		blocks[++(*cur_block)] = new_block;
		trace_tree->count = 1;
		trace_tree->root = new_node;
		//arep("initialized root %lu\n",new_id);
		current_node = new_node;
		trace_tree->total_hits++;
		(current_node)->hits++;
		return new_node;
	}


	tmp_pointer = search(parent_id);

#ifdef DEBUG_MODE
	if (!tmp_pointer){
		TERMINATE(1,"add_node: Invalid parent ID \"%016lx\" corrupted tree",parent_id);
	}
#endif

	parent_node = tmp_pointer->ptr;

#ifdef DEBUG_MODE
	if (!parent_node){
		TERMINATE(1,"add_node: Invalid parent, corrupted tree");
	}
#endif

	pparent = parent_node->parent;
	if (parent_node->id == new_id){
		/*
			This will happen either for external function calls
			that have not been instrumented by zharf
			for whcih this new block has been definitely marked
			as nested
			or simply it's a conditional jump from a basic block to
			itself. Like a simple for/while.
			Either case we don't want to create a loop
			on any node.
		*/

		(current_node)->info=blkinfo;
		return current_node;
	}


	if ( (cur=search(new_id)) ){
		(current_node) = cur->ptr;
#ifdef DEBUG_MODE
		if (!(current_node)){
			TERMINATE(1,"add_node: linear node doesn't have node ptr for id %08x",new_id);
		}
#endif
		/*
			From this point
			current_node is the new hit node

		*/
		/*
			Do we need a new edge in the tree?
			1. It's a simple return to the parent
				no edge will be added
			2. It's not the parent
				add to current_nodes's children
		*/

		if (pparent){
			if (pparent->id == new_id){
				/*
					We're done here
					we don't want two nodes
					have two edges to each other.
					save block info and return immediatly.

				*/

				//printf("lib: return to parent \n");
				//printf ("%016lx %016lx %016lx\n",new_id,parent_node->id,pparent->id);
			}else{
				/*
					Two possible cases here:


					1	We have a basic block that is
						running from beginning, this is not
						a regular return
						add an edge and increase hit
						We will have a loop now
						This may happen in recursive functions
						or for/while loops
						current_node here points to a previous
						node (tail/arrow of edge)
						head of edge is parent node

						parent----->(previous node)
					2	This is a return instruction from a basic block
						deep in an instrumented function. We don't want
						a new edge in this case.
						Regarding hits:
							increase for DCG? Not here

					To differentiate these two cases we check the info
					of the new block.
					This dosen't change depth or analysis in general
					but it makes the tree cleaner.

					We don't increase hit count for the second case,
					because nested blocks always run after the main block
					Nested blocks can't be executed independently.

					Since we check blkinfo and not (current_node)->info
					we're safe to distinguish between nested and independent
					block here (becase (current_node)->info is updated)
				*/

				if ( ( blkinfo & BLOCK_TYPE_NESTED ) ==0){
					/*
						First case, try to add an edge.
					*/
					if (add_child(parent_node,current_node)){
						//new edge


					}else{
						//edge exists
					}


					(current_node)->hits++;
					trace_tree->total_hits++;

				}else{
					/*
						Second case
						We don't add hit count since we don't
						consider this executing a basic block
						from beginning but resuming it.
						DCG Helper has been written in forkserver.
					*/
					/*(current_node)->hits++;
					trace_tree->total_hits++;*/

				}

			}
		}
		/*
			else pparent=NULL which means parent is root
			no new edge in that case

			We save blkinfo regardelss
		*/
		(current_node)->info=blkinfo;



		return current_node;
	}
	/*
		New ID not found
		This also implies that no edge to such a node
		exists in the tree.
		allocate a new node
	*/

	new_block.id = new_id;


	if (!parent_node)
		return 0;
	/*
		add to the linear list by copying new_block
		to block list
		Keep the list sorted
	*/


#ifdef DEBUG_MODE

//	if (blkinfo && BLOCK_TYPE==1){
		/*
			This check is problematic in blackbox fuzzing
		*/
//		TERMINATE(1,"Nested block visited as new block: %016lx",new_id);
//	}
#endif

	blocks[++(*cur_block)] = new_block;
	cur = &blocks[*cur_block];


	while (cur != blocks){
		if ( (cur-1)->id > cur->id){
			/*
				swap
			*/
			temp = *cur;
			*cur = *(cur-1);
			*(cur -1) = temp;
			cur--;
		}
		else{
			break;
		}
	}

	/*
		Allocate a new node for it
	*/
	new_node = alloc_node(new_id);



#ifdef DEBUG_MODE
	if (!new_node){
		TERMINATE(1,"add_node: Got invalid node");
	}
	if (cur->id != new_id){
		TERMINATE(1,"add_node: cur not pointing to the original entry");
	}
#endif

	cur->ptr = new_node;
	new_node->parent = parent_node;
	new_node->info=blkinfo;


	/*
		Now add it to the tree
	*/
	if (add_child(parent_node,new_node)){
		//new edge
	}else{
		/*
			edge exists
			Under no situation this can here happen unless
			the memory is corrupted.
		*/

#ifdef DEBUG_MODE
		TERMINATE(1,"add_node: Unexpected edge.");
#endif
	}
	trace_tree->total_hits++;
	trace_tree->count++;
	current_node = new_node;
	(current_node)->hits++;


/*
#define get_node_mark(info_byte)	(info_byte & BLOCK_MARKED)
		if (get_node_mark((current_node)->info))
		aexit("LIB:  MARKED TREE DETECTED ");
*/

	return new_node;
}

/*
	r must be volatile.
*/
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

#define ACQUIRE_LOCK while(!ulock((void *)global_lock));
#define RELEASE_LOCK *global_lock = 0;

#else

#define ACQUIRE_LOCK ;
#define RELEASE_LOCK ;

#endif

void log_block(ID_SIZE id,BLOCK_INFO_SIZE blkinfo){

	//printf("[-] logging %016lx\n",id);
#define add_node_locked(x) 	ACQUIRE_LOCK\
							add_node(id,x,blkinfo);\
							RELEASE_LOCK
	/*
		We have to use lock for multithreaded programs
		It will slow down the whole process for such
		progrmas but there's no other choice.
	*/
	//printf("LOG START %lu\n",*global_lock);
	if (current_node){
		add_node_locked((current_node)->id);
	}else{
		add_node_locked(0);
	}
	//printf("LOG END\n");
}









//************************* Instrumentation ************************************
/*
		Stage 1: Basic block identification

		Stage 2: In memory instrumentation

		Stage 3: Tracing the child
			Child shouldn't stay in the library.
			Return and let the cotrol goes back to
			the target process.
			When each breakpoint happens:
				Trace it in the graph.
				Restore the overwritten byte.
				Add a breakpoint to the beginnig of the previous basic block.
				Resume the suspended child.

*/





void strcp(char *s , char *start, char *end){
    char *p = start;
    while (p<end)
        *s++ = *p++;
    *s = 0;
}

void get_adr_range(char *line,unsigned long *adrs){
    char buf_part[100];
    char *sp,*sp_space;
    sp = strstr(line,"-");
    strcp(buf_part,line,sp);
    sscanf(buf_part,"%016lx",adrs);
    sp_space = strstr(line," ");
    strcp(buf_part,sp+1,sp_space);
    sscanf(buf_part,"%016lx",adrs+1);

}

void print_map(){
	int i;

	for (i=0;i<total_bbs;i++){
		printf(" > [%d,%lx]: %d ",i,bbs_offsets[i],
			bbs[T(bbs_offsets[i])].orig);
		if ((bbs[T(bbs_offsets[i])].meta & BLOCK_TYPE_NESTED)){
			printf("%d:%x\n",bbs[T(bbs_offsets[i])].meta,bbs[T(bbs_offsets[i])].indp_off);
		}else printf("\n");
	}
}

int add_breakpoint(int pid,void *adr){
	u64 mem_word;
	mem_word=ptrace(PTRACE_PEEKDATA,pid,adr,0);
	mem_word &= (~0 << 8);
	mem_word |= DBYTE;
	if (ptrace(PTRACE_POKEDATA,pid,adr,mem_word)){
			return 0;
	}
	return 1;
}
/*
	TODO: store 8 bytes in orig so that you
	don't have to peek_data here.
*/
int recover_instruction(int pid,void *adr){
	u64 mem_word;
	mem_word=ptrace(PTRACE_PEEKDATA,pid,adr,0);
	mem_word &= (~0 << 8);
	mem_word |= (bbs[_offbb((u64)adr)].orig);

	if (ptrace(PTRACE_POKEDATA,pid,adr,mem_word)){
			return 0;
	}

	return 1;
}

void sigf_handler(int signal, siginfo_t *si, void *ucontext){
	void *buf[100];
	int n=0;
	char **names;
	int i;
	//kill(getpid(),SIGTERM);
	//printf("LIB >> f %016lx\n",(u64)si->si_addr);

	int crash_rep_stat=-2;

	write(FD_OUT,&crash_rep_stat,4);

	n=backtrace(buf,100);

	names=backtrace_symbols(buf,n);


	fflush(stdout);
	printf("\n\n******* " CRED "LIBRARY FATAL STATE (SIGSEGV) (pid %d)" CNORM "*******\n",getpid());

	for (i=0;i<n;i++){
		printf("> %s\n",names[i]);
	}
	//raise(SIGABRT);
	TERMINATE(0,"Address fault: %016lx ",(u64)si->si_addr);
	//exit(1);
}
int recover_instruction_self_file(int fd,void* adr){
    int offs=0;
    unsigned long passed_off = 0;
    unsigned long total_off = (unsigned long)adr;
    lseek(fd,0,SEEK_SET);
#define MAXINT 0x7FFFFFFF
    while (passed_off<total_off){
        if (total_off - passed_off > MAXINT)
            offs=MAXINT;
        else
            offs = total_off-passed_off;
        lseek(fd,offs,SEEK_CUR);
        passed_off+=offs;
    }

    int r=write(fd,&bbs[_offbb((u64)adr)].orig,1);


    if (r<=0){
        aexit("self write: %s",strerror(errno));
    }
    return 1;
}
int recover_instruction_self(void* adr){

	if (mprotect((void *)((u64)adr & ~(0x0FFFUL)),4096,PROT_READ | PROT_WRITE | PROT_EXEC)){
		aexit("mprotect");
	}
	*(u8*)adr=bbs[_offbb((u64)adr)].orig;
	if (mprotect((void *)((u64)adr & ~(0x0FFFUL)),4096,PROT_READ | PROT_EXEC)){
		aexit("mprotect");
	}

	return 1;
}

void mointor_perthread(int *cid){


}

inline void arvin_trace_internal(int tid){
	struct basic_block *btmp;
	struct user_regs_struct regs;

	/*
	TODO: Check whether this is due to
	calling exec in child.
	*/

	/*
		New thread?
	*/


	/*
		Re-install the previous breakpoint
		Limit the number of hits to avoid
		getting stuck with extermely slow
		performance of ptrace.
	*/
	if (ptrace(PTRACE_GETREGS,tid,0,&regs))
	{

		return;
	}
	if (prev_bb_va){
		if (!add_breakpoint(tid,(void*)prev_bb_va))
			return;

	}


	regs.rip--;

	btmp=&bbs[_offbb(regs.rip)];


	if (btmp->meta & BLOCK_TYPE_NESTED){
		/*
			Adding a nested block
		*/
		log_block(btmp->indp_off,btmp->meta);
	}else{
		log_block(_off(regs.rip),btmp->meta);
	}


	/*
		DCG HELPER (solution 2)
	*/
	s8 *bbh=&bbs_hits[_offbb(regs.rip)];
	if (*bbh!=-1){
		(*bbh)++;
		if (*bbh>THS_HITS_BBS)
		{

			/*if ((((u8*)bp_id-(u8*)excl_node_board))==COMM_SHM_SIZE){
				aexit("Overflow ");
			}*/

			*bp_id++=_off(regs.rip);
			*bbh=-1;
		}

	}



	/*
		Store current bb
		Resume child
	*/
	prev_bb_va=regs.rip;
	if (!recover_instruction(tid,(void*)regs.rip)){
		/*
			Child killed by fuzzer.
		*/
		//awarn("Recovery failed");
		return;
	}

	if (ptrace(PTRACE_SETREGS,tid,0,&regs)){
		return;
	}

	/*
	if (_offbb((u64)regs.rip)==0x29721){
		u64 mem_word;
		mem_word=ptrace(PTRACE_PEEKDATA,cid,(void*)regs.rip,0);
		//mem_word &= 0xFF;
		printf("%016lx %016lx %02x %016lx\n",regs.rip,_offbb((u64)regs.rip),bbs[_offbb((u64)regs.rip)].orig,mem_word);
	}
	*/

	ptrace(PTRACE_CONT,tid,0,0);

}

int run_fserver(){

	int cid;

	int starter_id;
	void *pools_start;
	int shm_id,comm_id;
	int n,wrpid;
	char command;
	void *shm_adr;
	ID_SIZE *excl_node_board;
	int stat;


	arep("Stage2: Running forkserver");


	starter_id=getpid();
	n = write(FD_OUT,&starter_id,4);

	if (n<4){
		aexit("FATAL: Can't report pid");
	}

	/*
		Init SHM
	*/
	n =read(FD_IN,&shm_id,4);

	if (n<4){
		aexit("FATAL: Can't read shared memory id\n");
	}

	n =read(FD_IN,&shm_adr,8);

	if (n<8){
		aexit("FATAL: Can't read shared memory adr\n");
	}
	shm = shmat(shm_id,shm_adr,0);
	if (shm==(void*)-1){
		/*
			It seems that you can't map it to the same start address
			when ASLR is disabled!
			So keep it enabled.
		*/
		aexit("FATAL: Can't attach to shared memory[%d %016lx]: %s\n",shm_id,shm_adr,strerror(errno));
	}

	if (shm_adr != shm){
		aexit("FATAL: Requested %016lx Got %d %016lx",(u64)shm_adr,shm_id,(u64)shm);
	}
	printf("[-] Lib: shm: %d %016lx\n",shm_id,(u64)shm);


	/*
		Connect to comm memory region
	*/
	n =read(FD_IN,&comm_id,4);
	comm = shmat(comm_id,0,0);

	if (comm==(void *)-1){
		aexit("FATAL: Can't attach to comm shared memory");
	}

	if (n<4){
		aexit("FATAL: Can't read comm shared memory id\n");
	}

#ifdef NESTED_COUNT
	/*
		Report init_start
	*/
	n=write(FD_OUT,&init_section,8);
	if (n<8){
		aexit("FATAL: Can't write to fuzzer pipe\n");
	}
#endif

#define EXCL_NODE_BOARD_SIZE (1<<17) //16k blocks

	excl_node_board =mmap(0,EXCL_NODE_BOARD_SIZE,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);;
	if (excl_node_board==MAP_FAILED){
		aexit("mmap()");
	}

#ifdef NESTED_COUNT
	/*
		Relay bbs weight data to fuzzer
	*/

	memcpy(comm, compound_weight , COMM_SHM_SIZE);
	if (munmap(compound_weight,COMM_SHM_SIZE))
		aexit("munmap()");

#endif

	write(FD_OUT,".",1);

	/*
		Init graph
	*/

	pools_start = shm + 256;


	blocks= (struct block_info *) pools_start;
	node_pool = (struct node *)(shm+(u64)(SHM_SIZE>>2));
	child_pool = (struct child_ptr*)(shm+(u64)(3*(SHM_SIZE>>2)));


	trace_tree= (struct tree*)(shm + LOCK_DELTA + PROT_VARS_DELTA);

	/*
		Init protected shared vars
	*/

	global_lock = shm;
	cur_block = (long *)(shm + LOCK_DELTA );
	cur_node_in_pool = (u64 *)(shm + LOCK_DELTA + 8);
	cur_child_in_pool = (u64 *)(shm + LOCK_DELTA + 16);

	*cur_block = -1;
	/* Fuzzer must have already zeroed these*/
	*cur_node_in_pool = 0;
	*cur_child_in_pool = 0;
	current_node = 0;
	*global_lock = 0;



	/*
		For DCG solution 1
	*/
	int fd=open("/proc/self/mem",O_RDWR);
	if (fd<0)
		aexit("open() maps: %s",strerror(errno));


	/*
		Fork server busy loop
	*/
	while (1){

		n =read(FD_IN,&command,1);

		if (n<1){
			aexit("FATAL: Can't read command");
		}

		/*struct timespec stime,etime;

		clock_gettime(CLOCK_REALTIME,&stime);*/




		bp_id=excl_node_board;

		/*
			DCG Helper: solution 1
			Check all basic blocks and exclude
			expensive ones.
		*/
#ifdef COMMENTT

		for (i=0;i<total_bbs;i++){
			ID_SIZE *tmp=&bbs_offsets[i];

			if (bbs_hits[T(*tmp)]>THS_HITS_BBS){

				recover_instruction_file(fd,(void *)_va(*tmp)) ;
			}


		}
		//if (f)aexit("%d=%d ```",c,f);



#endif


		/*
			DCG Helper: solution 2
			Mark all bad nodes during target execution and
			exclude them in next round before forking another instance.
			The first exclusion happens in the second iteration.
		*/


		while(*bp_id){//f++;
			/*if (*bp_id > 0xffffff)
				aexit("Too big index %lx",*bp_id);*/

			recover_instruction_self((void *)_va(*bp_id)) ;


			if (*(u8*)_va(*bp_id) != bbs[T(*bp_id)].orig)
				aexit("REC FAILED");
			*bp_id=0;
			bp_id++;


		}


		/*
			Revert bp_id pointer
		*/
		bp_id=excl_node_board;


		/*clock_gettime(CLOCK_REALTIME,&etime);
		u64 stime_us,etime_us;
		stime_us=stime.tv_sec*1000000 + stime.tv_nsec/1000;
		etime_us=etime.tv_sec*1000000 + etime.tv_nsec/1000;
		if (f)aexit(", time %dus",(etime_us-stime_us));*/



		/*
			Clear hits
		*/
		memset(bbs_hits,0,bbs_o1_hits_size);
		//memcpy(bbs_hits,bbs_empty,bbs_o1_hits_size);


		/*
			We're ready to fork
		*/
		cid=fork();
		if (cid==-1)
			aexit("fork()");
		if (!cid){
			/*
				Child
			*/
			signal(SIGSEGV,SIG_DFL);
			close(FD_IN);
			close(FD_OUT);
			ptrace(PTRACE_TRACEME,0,0,0);

			/*
				Let the parent set ptrace options
			*/
			raise(SIGTRAP);
			/*
				We're ready here
			*/
			return 1;

		}


		n=write(FD_OUT,&cid,4);
		if (n<4){
			aexit("FATAL: Can't write response; %d %s\n",n,strerror(errno));
		}


		/*
			Only one of PTRACE_O_TRACECLONE and PTRACE_O_TRACEFORK
			can be set. We only support multi-threaded targets and we set
			PTRACE_O_TRACECLONE.

			If the target process forks, it will crash after hitting an instrumented
			basic block.
		*/
		wrpid=wait(0);
		if (wrpid!=cid)
			aexit("waitpid(): %d\n",wrpid);
		ptrace(PTRACE_SETOPTIONS,cid,0,PTRACE_O_TRACECLONE);
		ptrace(PTRACE_CONT,cid,0,0);

		/*
			ptrace loop
		*/
		int term=0;
		int rep_stat=0;

		while (1){


			wrpid=waitpid(-1,&stat,0);

			if (wrpid==-1){
				/*
					We shouldn't be here
				*/
				aexit("Unexpected state. waitpid() failed.");

			}else if (wrpid==cid){
				/*
					Main thread
				*/
				if (WIFEXITED(stat)){
					term=1;
					rep_stat=stat;

				}else if (WIFSIGNALED(stat)){
					/*

						Fatal signals are handed out to the tracer and the child will
						have its WIFSTOPPED set to true.

						WIFEXITED works normally in traced mode.

						If we are here it's kill signal, either by timeout
						or the one we sent after a crash was detected.
					*/
					term=1;

					if (rep_stat){


					}
					else{

						rep_stat=stat;
					}

				}else if(WIFSTOPPED(stat)){
					if (WSTOPSIG(stat)==SIGTRAP){
						if ((stat>>8) == ((PTRACE_EVENT_CLONE<<8) | SIGTRAP)){
							//New thread notifier in parent
							ptrace(PTRACE_CONT,wrpid,0,0);
							continue;
						}
						arvin_trace_internal(wrpid);
						continue;
					}

					/*
						waitpid usually reaps the new created thread
						so we get SIGSTOP of the new thread and then
						SIGTRAP notification of its parent.
					*/
					if (WSTOPSIG(stat)==SIGSTOP){
						ptrace(PTRACE_CONT,wrpid,0,0);
						continue;
					}else if(WSTOPSIG(stat)==SIGCHLD){

						ptrace(PTRACE_CONT,wrpid,0,SIGCHLD);
					}
					else{

						term=1;

						/*struct user_regs_struct regs;
						ptrace(PTRACE_GETREGS,cid,0,&regs);
						//u64 temp=ptrace(PTRACE_PEEKDATA,cid,(void *)regs.rip,0);
						u64 temp=ptrace(PTRACE_PEEKDATA,cid,(void *)(regs.r12-0x33fffa6c),0);
						printf("CRASH in main thread with stat %08x, sig: %d, Offset: %lx, : %lx \n",
							stat,WSTOPSIG(stat)
							,_off(regs.rip),temp);*/



						rep_stat=stat;
						kill(cid,9);

					}

				}else{
					aexit("waitpid(): invalid stat: %08x\n",stat);
				}


				if (term){
					/*
						If any thread has crashed, we must reap it to
						evict the process from zombie state.
						Probably this double-reaping is necessary because
						the thread is a tracee.

						For a crashed case, we've already sent SIGKILL, and
						for a normal exit, all threads are this way reaped
						properly.
					*/
					while(waitpid(-1,&stat,0)!=-1);

					/*
						We're done with this target.
						Report and leave the loop.
					*/
					n=write(FD_OUT,&rep_stat,4);

					if (n<4){
						printf("%d %s\n",n,strerror(errno));
						aexit("FATAL: Can't write response\n");
					}

					break;
				}

			}else{


				if (WIFEXITED(stat)){

					continue;

				}else if (WIFSIGNALED(stat)){

					/*
						Kill signal (most probably from ourselves)
					*/

					continue;

				}else if(WIFSTOPPED(stat)){
					if (WSTOPSIG(stat)==SIGTRAP){
						if ((stat>>8) == ((PTRACE_EVENT_CLONE<<8) | SIGTRAP)){
							//New thread notifier in parent
							ptrace(PTRACE_CONT,wrpid,0,0);
							continue;
						}
						arvin_trace_internal(wrpid);
						continue;
					}

					if (WSTOPSIG(stat)==SIGSTOP){

						ptrace(PTRACE_CONT,wrpid,0,0);
						continue;
					}else{

						rep_stat=stat;
						kill(cid,9);
						continue;

					}

				}else{
					aexit("waitpid(): invalid stat: %08x\n",stat);
				}


			}


		} //end while ptrace
	}

}
int run_fserver2(){
	int cid;

	struct timespec stime,etime;
	u64 prev_bb_va=0;

	arep("Stage2: Running test fork");
	clock_gettime(CLOCK_REALTIME,&stime);
	cid=fork();
	if (cid==-1)
		aexit("fork()");
	if (!cid){
		//child
		int n=open("/dev/null",O_WRONLY);
		dup2(n,1);
		dup2(n,2);
		close(n);
		ptrace(PTRACE_TRACEME,0,0,0);
		return 1;

	}
	int c=0;
	while (1){c++;
			int state;
			struct user_regs_struct regs;


			waitpid(cid,&state,0);

			/*
				TODO: Check whether this is due to
				calling exec in child.
			*/
			if (WIFSTOPPED(state) && WSTOPSIG(state)==5){
				ptrace(PTRACE_GETREGS,cid,0,&regs);
				regs.rip--;

				if (prev_bb_va){
					add_breakpoint(cid,(void*)prev_bb_va);
					/*if (bbs_hits[_offbb(prev_bb_va)] >= HITS_THR){
						if (!RU32(4))
							if(!add_breakpoint(cid,(void*)prev_bb_va)){
								continue;
							}
					}else{
						if (!add_breakpoint(cid,(void*)prev_bb_va))
							continue;
						bbs_hits[_offbb(prev_bb_va)]++;
					}*/
				}


				if (!recover_instruction(cid,(void*)regs.rip)){
					aexit("Recovery failed");
				}


				if (ptrace(PTRACE_SETREGS,cid,0,&regs))
					aexit("SETREGS");



				ptrace(PTRACE_CONT,cid,0,0);

				prev_bb_va=regs.rip;

			}else{
				if (WIFEXITED(state))
					arep("Exited normally");
				else
					arep("Child crashed");


				kill(cid,9);


				clock_gettime(CLOCK_REALTIME,&etime);
				u64 stime_us,etime_us;
				stime_us=stime.tv_sec*1000000 + stime.tv_nsec/1000;
				etime_us=etime.tv_sec*1000000 + etime.tv_nsec/1000;
				arep("Finished , time %dus",(etime_us-stime_us));


				exit(0);

			}
	}


}
int repetitive(int offset,int last){
	int i;
	for (i=0;i<last;i++)
		if (bbs_offsets[i]==offset)
			return 1;
	return 0;
}
int is_invalid_bb(int offset){
	int i;

	for (i=0;i<inst_count;i++){
		if (all_instructions[i]==offset)
		{

					return 0;
		}
	}

	//arep("Detected invalid basic block: %lx",offset);
	return 1;
}

static inline int is_od_line_valid(char *s){
	int tc=0;
	while (*s)
		if (*s++=='\t')
			tc++;
	/*if (tc>2)
		aexit("od output anomaly");*/
	return (tc==2);
}
/*
	ret:
		0 : Stage 1 constructor
		1 : Stage 2: forkserver: new child


	Process sets:
		First process:
			fork() and instrument it. This will
			be the forkserver.
			Exit.
		Second process(fserver):
			Changes instrumentation dynamically on itself (DCG)
			After receiving fork command:
				fork() and waitpid to monitor the running
				instance. Report the exec result after
				the instance is terminated.
*/

int initialize(){
	int i,j;
	int max=0;
	FILE *f;
	char line[1025];
	int ind=0;
	int size_line=0;
	char *p;
	int c;
	int next_off=0;
	int total_calls=0;
	char cmd[2048];
	char exe_path[1024];
	int fserver_id;
	char *map_search;
	time_t tm;
	int bad_bbs=0;
	struct sigaction sig_act;

#define PD_CHECK_SET(x)	if (pd_exe)x-=PD_START

	sig_act.sa_flags=SA_SIGINFO;
	sig_act.sa_sigaction = &sigf_handler;
	sigaction(SIGSEGV, (const struct sigaction *)&sig_act,NULL);

	time(&tm);
	srand(tm);


	memset(exe_path,0,1024);
	readlink("/proc/self/exe",exe_path,1024);
	p=exe_path;

	/*
		This must not be necessary as the content
		of a symlink shouldn't have a newline character
		appended.
	*/
	/*while(*p){
		if(*p==10){fprintf(stderr,"GOT IT\n");
			*p=0;
			break;
		}
		p++;
	}*/





	arep("Exe: %s",exe_path);

	/*
		These commands arguments are directly read from
		user and they're not sanitized.
		Sanitize if the user input is not trusted.
	*/
	sprintf(cmd,"readelf -a %s > "RE_TMP,exe_path);
	if(system(cmd))
		aexit("Invoking system command failed: readelf");

	sprintf(cmd,"objdump -d %s > "OD_TMP,exe_path);
	if(system(cmd))
		aexit("Invoking system command failed: objdump");

	f=fopen(RE_TMP,"r");
	if (!f)
		aexit("fopen()");

	pd_exe=1;
	while(fgets(line,1024,f)){
		if (pd_exe){
			if (strstr(line,"DYN (Shared object")){
				pd_exe=0; //normal pi exe
			}
		}
		if (size_line){
			p=strstr(line,"0");
			sscanf(p,"%016lx",&text_size);
			break;

		}else if(strstr(line,".text ")){
			p=strstr(line,"0");
			sscanf(p,"%016lx",&text_start);
			size_line=1;
		}else if(strstr(line,".init ")){
			p=strstr(line,"00");
			sscanf(p,"%016lx",&init_section);

		}
	}
	fclose(f);

	if (init_section==0)
		aexit("Invalid executable");

	if (pd_exe){
		//fix all offsets
		text_start-=PD_START;
		init_section-=PD_START;
		arep("Target is not DYN");

	}

	arep("Text start: 0x%016lx",text_start);
	arep("Text size: 0x%016lx",text_size);
	arep(".init: 0x%016lx",init_section);

	/*
		Load all instructions
		Identify calls
	*/
	all_instructions=mmap(0,MAX_INSTRUCTIONS* sizeof(*all_instructions),PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	if (all_instructions==MAP_FAILED){
		aexit("mmap()");
	}

	f=popen("wc -l "OD_TMP,"r");

	memset(line,0,1024);
	fgets(line,1024,f);

	pclose(f);



	c=atoi(line);

	if (c>MAX_INSTRUCTIONS){
		aexit("Number of instructions exceeds the limit.");
	}
	
	if (c<1){
		aexit("Invalid number of instructions.");
	}

	//upper bound for calls
	c/=4;

	calls_offsets=malloc(c*sizeof(struct call_cell));
	memset(calls_offsets,0,c*sizeof(struct call_cell));

	ind=0; //index into call_offsets
	f=fopen(OD_TMP,"r");
	if (!f)
		aexit("fopen()");
	int outc=0;



	while(fgets(line,1024,f)){
		if (strncmp(line,"  ",2)==0){
			u32 offset;
			char *last_p;
			/*
				objdump has a problem in showing long instructions. When the instruction
				is longer than 7 bytes, it writes the rest ot the instruction as a new
				line. This new line has the rest of hex bytes with an empty 'instruction'
				column. Normal lines have 2 tabs inserted between the three columns.
				We identify broken instructions by counting tabs.

				NOTE: This won't work if objdump changes its output style in its
				future versions.

			*/


			last_p=strstr(line,":");

			if (!is_od_line_valid(last_p)){
				//arep(CORANGE"Invalid od line: %s"CNORM,line);
				continue;
			}

			*last_p=0;
			p=line;
			while(*p && *p==' ')p++;
			sscanf(p,"%x",&offset);
			PD_CHECK_SET(offset);

			if (offset >= text_start && offset < text_start+text_size){
				all_instructions[inst_count++]=offset;
			}

			*last_p=':'; //undo for strstr()


			if (next_off){
				/*last_p=strstr(line,":");
				*last_p='\t';;
				p=line;
				while(*p && *p==' ')p++;
				sscanf(p,"%x",&offset);
				PD_CHECK_SET(offset);*/

				if (offset > text_start && offset < text_start+text_size){
					/*
						We'll set meta of corresponding block in bbs[] later.
					*/
					calls_offsets[ind].nested_offset=offset;

				}
				/*
					Go to next element regardless of
					whether a nested block was found
					or not.
				*/
				next_off=0;
				ind++;
				*last_p=':';

			}

			if (strstr(line,"call") ){
				/*p=strstr(line,":");
				*p=0;
				p=line;
				while(*p && *p==' ')p++;
				sscanf(p,"%x",&offset);
				PD_CHECK_SET(offset);*/

				if (offset >= text_start && offset < text_start+text_size){
					calls_offsets[ind].call_offset=offset;
					calls_offsets[ind].nested_offset=0;
					next_off=1;
					//printf("call: %x ++ %s \n",offset,line);

				}else{
					outc++;
				}
			}else{

			}

		}

	}
	fclose(f);

	total_calls = ind;
	arep("Total calls: %d / "CORANGE"Excluded: %d"CNORM,total_calls,outc);
	arep("Total imported instructions: %d",inst_count);


	/*
		Read all basic blocks from angr output.
		%x is enough.
	*/
	ind =0;

	f=fopen(BB_FNAME,"r");
	if (!f)
		aexit("fopen()");

	//Skip hash
	fgets(line,1024,f);

	while(fgets(line,1024,f)){
		int offset;

		if (strstr(line,"_start\n"))
			sscanf(&line[0]+2,"%x",&_start_offset);
		PD_CHECK_SET(offset);
		sscanf(&line[0]+2,"%x",&offset);
		if (offset>=text_start && offset<text_start+text_size
			&& !repetitive(offset,ind))
		{
			if (is_invalid_bb(offset)){
				bad_bbs++;
			}else{
				/*
					Checked for marked functions here.
				*/

					//if (should_mark) bbs[T(offset)].meta |= BLOCK_MARKED;
				/*
					Store the offset
				*/
				bbs_offsets[ind++]=offset;


			}

		}
		if (ind>MAX_BLOCKS)
			aexit("Number of basic blocks exceeded the limit.");
	}
	fclose(f);

	arep("_start: 0x%x",_start_offset);
	arep("Imported %d basic blocks.",ind);
	arep("Marked %d basic blocks.",tot_marked);
	if (bad_bbs)
		arep("Excluded "CORANGE"%d invalid "CNORM "basic blocks.",bad_bbs);
	total_bbs=ind;




	/*
		Release instructions memory
	*/
	if (munmap(all_instructions,MAX_INSTRUCTIONS* sizeof(*all_instructions))){
		aexit("munmap()");
	}


	/*
		We want to make the bb lookup O(1).

		Keep bbs_offsets for debugging and va calcualtion.
		Create the main basic block map.

	*/

	for (i=0;i<total_bbs;i++){
		if (T(bbs_offsets[i])>max)
			max=T(bbs_offsets[i]);
	}
	arep("Max: %d (0x%x)",max,max);
	bbs_o1_count=max+1;
	bbs_o1_size= sizeof(struct basic_block)*bbs_o1_count;
	arep("bbs size: %d KB ",bbs_o1_size/1024);

	/*
		Build the map
	*/

	bbs=calloc(bbs_o1_count , sizeof(struct basic_block));
	if (!bbs)
		aexit("calloc");
	bbs_o1_count = ((bbs_o1_count)/(1024*64)+1)*(1024*64);
	arep("bbs hits size: %d KB ",bbs_o1_count/1024);
	bbs_o1_hits_size=bbs_o1_count*sizeof(*bbs_hits);
	bbs_hits=mmap(0,bbs_o1_hits_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	if (!bbs_hits)
		aexit("mmap");
	//bbs_empty=mmap(0,bbs_o1_hits_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);

#ifdef NESTED_COUNT
	compound_weight=mmap(0,COMM_SHM_SIZE,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
	if (compound_weight==MAP_FAILED){
		aexit("mmap()");
	}
	memset(compound_weight,0,COMM_SHM_SIZE);
#endif
	if (bbs_hits==MAP_FAILED /*|| bbs_empty==MAP_FAILED*/){
		aexit("mmap()");
	}

	memset(bbs_hits,0,bbs_o1_hits_size);
	//memset(bbs_empty,0,bbs_o1_hits_size);







	/*
		Initialize _va
		The permission bits of the first segment is different
		in different binaries. The first segment might or might not
		be executable. For PIE binaries it's always NX and has the
		initial sections before .init.
		But to be safe we don't search map based on permission bits,
		we just get the first line which must be the initial segment.
	*/
	f=fopen("/proc/self/maps","r");
	if (!f)
		aexit("fopen()");

	/*
	// unsafe assumption
	if (pd_exe)
		map_search = "r-xp";
	else
		map_search="r--p";
	*/

	map_search="-";
	while (fgets(line,254,f)){
		if (strstr(line,map_search)!=0 ){
			unsigned long temp_range[2];
			get_adr_range(line,temp_range);
			prog_va_start = temp_range[0];
			break;
		}
	}
	fclose(f);

	arep("VA start: 0x%016lx",prog_va_start);

	/*
		Original byte and index of each basic block
	*/
	for (i=0;i<total_bbs;i++){

		bbs[T(bbs_offsets[i])].orig = *(u8 *)_va(bbs_offsets[i]);
		//bbs[T(bbs_offsets[i])].index=i;
	}


	arep("Read all original bytes");

	/*
		To identify correct meta, we need bbs_offset sorted
		first. (it's likely already sorted)
	*/
	for (i=0;i<total_bbs;i++){
		for (j=i+1;j<total_bbs;j++){
			if (bbs_offsets[j]<bbs_offsets[i]){
				int tmp;

				tmp=bbs_offsets[i];
				bbs_offsets[i]=bbs_offsets[j];
				bbs_offsets[j]=tmp;
			}
		}
	}

#ifdef NESTED_COUNT
	for (i=0;i<total_bbs;i++){
		compound_weight[T(bbs_offsets[i])]=1;
	}
#endif
	/*
		Now for each nested block scan all basic blocks
		and identify which one is the independent block
		that has the nested block inside.
	*/
	for (i=0;i<total_calls;i++){
		int j;

		if (calls_offsets[i].nested_offset){

			for (j=0;j<total_bbs;j++){
				if(bbs_offsets[j]==calls_offsets[i].nested_offset){
					int z,prev_indp_index=-1;

					/*
						Set as nested block
					*/
					bbs[T(bbs_offsets[j])].meta |= BLOCK_TYPE_NESTED;
					/*
						Find the first previous independent block
						Assign the current nested block to it.
						Increment its compound field in the comm board.
						Since parent is very close, stepping backwards
						is less expensive than checking meta for all
						blocks while we are looping through bbs_offsets.
					*/
					for (z=j-1;z>-1;z--){
						if (!(bbs[T(bbs_offsets[z])].meta & BLOCK_TYPE_NESTED)){
							prev_indp_index = z;
							break;
						}

					}
					if (prev_indp_index==-1)
						aexit("Trace back idependent block identification failed");
					//printf("%d <%lx,%lx>\n",j,bbs_offsets[j],bbs_offsets[prev_indp_index]);
					bbs[T(bbs_offsets[j])].indp_off=bbs_offsets[prev_indp_index];//calls_offsets[i].call_offset;


#ifdef NESTED_COUNT
					/*
						Compound weight
						Map is stored from the beginning of comm
						Each element one byte
					*/

					compound_weight[T(bbs_offsets[prev_indp_index])]++;

#endif
					break;
				}
			}
		}

	}


	/*
		fork():
			The child will be instrumented and will
			run the forkserver.
	*/

	fserver_id=fork();
	if (fserver_id==-1){
		aexit("fork()\n");
	}
	if (fserver_id==0){
		/*
			Let the parent instrument you first
		*/

		ptrace(PTRACE_TRACEME,0,0,0);
		raise(SIGTRAP);
		/*
			At this stage instrumentation is complete.
			Start fork server.
		*/

		run_fserver();

		return 1;

	}

	wait(0);
	arep("Child %d ready ",fserver_id);
	/*
		Ok go ahead and make all the changes.
	*/

	char *r_s;
	if ((r_s=getenv("ARV_RATIO"))){
		sscanf(r_s,"%f",&instrumentation_ratio);
	}

	if (instrumentation_ratio > 1){
		aexit("Invalid ratio");
	}



	/* ILOOP */
	for (i=0;i<total_bbs;i++){

		/*
			Similar to zharf we have to apply instrumentation ratio
			based on basic block type.
		*/
		if ((bbs[T(bbs_offsets[i])].meta & BLOCK_TYPE_NESTED) ==0){
			/*
				Independent block
			*/
			if (RU8(100)/100.0 >= instrumentation_ratio){
				parent_instrumented=0;
				continue;
			}
			parent_instrumented=1;
		}else{
			/*
				Nested block
			*/
			if (!parent_instrumented)
				continue;

		}

		if (!add_breakpoint(fserver_id,(void *)_va(bbs_offsets[i]))){
			aexit("add_breakpoint() %s",strerror(errno));
		}

		total_instrumented++;

	}



	arep("Instrumentation Complete");
	arep("%d basic blocks instrumented",total_instrumented);
	int r=ptrace(PTRACE_DETACH,fserver_id,0,0);
	if (r==-1){
		aexit("ptrace failed (CONT): %s",strerror(errno));
	}



	//print_map();



	return 0;
}





__attribute__ ((__constructor__))
void arvlib_constructor(){
	arep("Initializing starter\n");
	setenv("LD_PRELOAD","",1);



	if (!initialize()){
		arep("Stage 1 constructor completed");
		exit(0);
	}else{
		/*
			New forked child
			Give it control.
		*/

		return;
	}
}
