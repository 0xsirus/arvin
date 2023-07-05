/*
	By Sirus Shahini
	~cyn
*/

#include "head.h"


float instrumentation_ratio=1.0;
u8 parent_instrumented=0;
int total_instrumented=0;

#ifdef ARV_BIG_TARGET
#define MAX_BLOCKS	600000
#define MAX_INSTRUCTIONS 2000000
#else
#define MAX_BLOCKS	200000
#define MAX_INSTRUCTIONS 500000
#endif

#define RE_TMP	"/tmp/.arv_readelf.txt"
#define OD_TMP	"/tmp/.arv_instr.txt"
#define DBYTE	(0xCC)
/*
	Transform offset to bb in O(1)
	This O(1) array will map 0 to 0
	which is the file offset - init section.
	0 is actually mapped to init.
	We do this because all instructions that
	we work with are after init.
*/
#define T(x) (x-init_section)
//file offset to va
#define _va(x)	(x+prog_va_start)
//va into bb
#define _offbb(x)	(x-prog_va_start-init_section)
//va to file offset
#define _off(x) (x-prog_va_start)
struct call_cell{
	u64 call_offset;
	u64 nested_offset;
};
//offsets of calls and the next instruction
struct call_cell *calls_offsets; //{(offset,next) , (offset,next) , ...}

/*
	offsets of all basic blocks in ELF file
	INDEX-ACCESS array
*/
ID_SIZE bbs_offsets[MAX_BLOCKS];

struct basic_block{
	u8 orig; //original byte to recover from 0xcc
	u8 meta; //additional bb information
	//int index; //index in bbs_offsets
	u32 indp_off; //if nested, this is parent offset

} ;

/*
	DIRECT-OFFSET arrays
*/

struct basic_block *bbs;
s8 *bbs_hits;
//s8 *bbs_empty;
#ifdef NESTED_COUNT
u8 *compound_weight;
#endif


u64 bbs_o1_size=0;
u64 bbs_o1_count=0;
u64 bbs_o1_hits_size=0;

u32 _start_offset=0;
u64 text_start;
u64 init_section;
u64 text_size;
int total_bbs=0;
int total_nested;
u64 prog_va_start;
u8 pd_exe=0;
u64 prev_bb_va=0;
ID_SIZE *bp_id;
#define PD_START	0x400000


u32 *all_instructions;
int inst_count=0;
int tot_marked=0;

#define THS_HITS_BBS 1

void 			arep(char *, ...);
void 			save_memory(char *);
void 			force_write_exit(u8 ,char *, ...);
struct node 		*ret_new_node();
struct node 		*alloc_node(ID_SIZE);
struct child_ptr 	*alloc_child_ptr(struct node *);
struct block_info	*search(ID_SIZE );
struct child_ptr 	*add_child(struct node *, struct node *);
struct node 		*add_node(ID_SIZE ,ID_SIZE ,BLOCK_INFO_SIZE );
int 			ulock(void * );
void 			log_block(ID_SIZE ,BLOCK_INFO_SIZE );
void 			strcp(char * , char *, char *);
void 			get_adr_range(char *,unsigned long *);
void 			print_map();
int 			add_breakpoint(int ,void *);
int 			recover_instruction(int ,void *);
void 			sigf_handler(int , siginfo_t *, void *);
int 			run_fserver();
int 			repetitive(int ,int );
int 			initialize();
void 			arvlib_constructor();
int 			is_invalid_bb(int );


