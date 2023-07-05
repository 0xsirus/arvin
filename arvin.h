/*
	By Sirus Shahini
	~cyn
*/

#include "head.h"

#include <sys/ioctl.h>


char		*target_path;
char		*input_dir;
char		*output_dir;
char		**target_argv;

/*
	These pipes are used for communicating with
	the starter
*/
int		cmd_send[2];
int		response_recv[2];
#define COUT cmd_send[1]
#define RIN	response_recv[0]
//#define BUILD_GRAPH
u8		should_store_graph=0;

#define ERR_TMOUT EAGAIN

int		dev_null;
int		feed_fd; //input to target
/*
	If the target is going to be fuzzed
	using an input file in ti's arguments
	we close is its stdin for now
*/
int		file_feed;
char		*feed_file_path;
char		*cinp_file_path;
int		shm_id = -1;
void		*shm_adr=0;
int		comm_id = -1;
void		*comm_adr;
void		*shm_end;
int		starter_id;

char		CURRENT_INPUT[] = "/dev/shm/.arv_cinput_";
char		TMP_INPUT[] = "tmp_input";
char		base_dir[1024];

#define MAX_INPUT_SIZE		(1<<15)	//32KB
#define MIN_INPUT_SIZE		(16)
u8		mut_map[MAX_INPUT_SIZE];
u8		mut_map_orig[MAX_INPUT_SIZE];
#define MAX_KEYWORDS		32
#define MAX_KW_SIZE			16
struct keyword {
	u8 kw[MAX_KW_SIZE];
	u8 size;
} keywords[MAX_KEYWORDS];
u8		kw_index = 0;


#define DICT_MAX_KEYWORDS		1024
#define DICT_MAX_KW_SIZE	32
char		*dict_kws[DICT_MAX_KEYWORDS];
char		*dict_file=0;
int		dict_kw_count=0;

u32		current_csum;
u32		last_csum;
u8		need_csum;

int		gcount =0 ;
int		save_i_count = 0;
u8		sort_low_prio = 1;

u8		net_mode=0;
int		tcp_port;

ID_SIZE		dfs_list[NODES_MAX];
long		dl_index=-1;
u32		graph_e_count=0;

int		instance_pid;
/*
	crash report number
*/
int		crash_rep=0;
/*
	We don't draw more edges than this number
	not only the output will be messy but also
	graphviz fails to draw at all for very big
	numbers of edges
*/
#define MAX_EDGES_VISIBLE 200
#define GRAPH_FILES_COUNT	8
/*
	This is shm_adr + LOCK_DELTA
*/
struct tree	*shared_trace_tree;

/*
	Local linear block storage
*/
#define MAX_LOCAL_BLOCKS	NODES_MAX
struct block_info_local{
	ID_SIZE id;
	u64 hits;
} sorted_blocks[MAX_LOCAL_BLOCKS];
long		block_ind = -1;

/*
	Child id created by forkserver
*/
int		target_id;
int		target_timedout=0;
#define TRG_EX_NORMAL	1
#define TRG_EX_TMOUT	2
#define TRG_EX_CRASH	3
#define TRG_SOFT_CRASH	4
#define TRG_NET_MODE	10

#define MAX_TIMEOUT_VAL_INIT 5000000
#define MAX_TIMEOUT_VAL_RUN 90000 //50ms
#define MIN_TIMEOUT_VAL 20000 //20ms
#define TOI_RATE	3

/*
	This will be saved in tv_usec and
	must be less than 1 second.
*/
u64		active_timeout = MAX_TIMEOUT_VAL_INIT ;
u64		active_timeout_sav;

/*
	User provided timeout
*/
long		user_timeout=-1;

u8		save_soft_crash=0;
/*
	This is the time we wait after executing
	in network mode and before trying to
	connect to the given TCP port

*/
#define CONNECT_WAIT	100000 //100 ms
#define CONN_MAX_RETRIES	5
#define RECV_WAIT		20000 //20ms
int		net_sock=-1;
struct sockaddr_in	target_saddr;
char		*target_ip = "127.0.0.1";

u32		input_count = 0;

#define LIVE_REP_FILE "/var/www/html/zharflive/zharf_live"
//#define LIVE_STAT

#define STAT_ENTRIES_FILE	"stat_entries"

char		stat_line[255];
u8		use_term_gui = 1;
u64		total_crashes=0;
u64		soft_crashes=0;
u64		unique_soft_crashes=0;
u64		unique_crashes=0;
int		mut_input_saved=0;

char		current_stat[255];
u64		_st_bl,_st_indp,_st_nes;
#define MAX_BREFR_FREQ 8000
u32		brefr_freq = 20;
u32		brefr_freq_counter=0;
u8		arvin_init_state = 1;

s8		set_8_ints[] = {_8_ints};
s16		set_16_ints[] = {_8_ints , _16_ints};
s32		set_32_ints[] = {_8_ints , _16_ints , _32_ints};

u8		has_corrupted_shm=0;

//***************** Debug Definitions *************************
/*
	This structure is used for the result
	of each execution of the target that
	we want to save.
	All useful information regarding the
	execution is inside this structure.
*/

struct node	*_debug_node;
u8		debug_mode = 0;
int		debug_exit_code;
int		debug_exit_stat;
u8		*debug_data;
size_t		debug_data_size;

u64		debug_rec_ind = 0;
u64		debug_rec_dir = 0;
#define RECORD_FEEDS
u8		should_save_mem = 0;
u8		coverage_only = 0;
u8		state_slow_warn=0;
//***************************************************************

/*
	Performance check disabled
	by default
*/

/*
	Original performance check
	requested by user
*/
int		perf_check_req;
/*
	Dynamic performance check
*/
int		perf_check = 0;

u8		queue_hit_sl=0;

u8		cov_show_only = 0;

char		* cmt ;

int		attached_core;

#define CRASH_MAX	10000
u32		crash_sums[CRASH_MAX];
int		crash_sums_index = 0;

#define MAX_INTR_LOCS	1024
#define MIN_INTR_LOC_COUNT 8

struct inp_intr_locs{
	int intr_locs_index;
	size_t intr_locs[MAX_INTR_LOCS];
};

u8		add_to_inputs = 0;

u64		*lib_lock;

u8		save_net=0;
int		save_net_i = 0;
#define NET_FILE "debug/net/net_file"

time_t		start_time;

#define INPUT_MAX			8000
/*
	Descriptor for each queue element
	Including both file information
	and execution information
*/
struct input_val{
	char i_path[MAX_PATH];
	char m_path[MAX_PATH];
	u8 initial;
	int depth;
	u64 total_blocks;
	u64 total_hits;
	u32 hash;
	u8 prio;
	u8 passed;
	u64 leaves;
	struct inp_intr_locs *i_intr_locs;
	u8 fixed_loc;
	u8 marked;
	u8 invalidated_i;
	size_t size;
};

u8		lpq_balance =1 ;
u8		visited_lp=0;
/*
	Specifies wether current tree
	has an important function
*/
u8		marked_tree=0;

/*
	Total covered blocks
*/
u64		total_covered=0;
u64		total_exec=0;

/*
	These two counters are reset
	for each execution
*/
u64		nested_counter=0;
u64		indp_counter=0;
/*
	Total hits since we started
	fuzzing the target
	May overflow but unlikely and it doesn't
	matter that much
*/
u64		total_target_hits=0;
/*
	Blocks count of the latest tree
	only used for live report
*/
u64		last_trace_nodes=0;

u64		last_trace_leaves=0;

int		last_crash_sig;
u8		last_crash_mut;
/*
	skip these number of nodes
	to produce a more understandable graph
	the graph created by graphviz
	will be different from what is stored in memory
	because of this step
*/
#define SHOULD_SKIP_COUNT	100
u32		skip_nodes=0;

u8		target_mult_threaded = 0;

struct input_val	input_queue[INPUT_MAX];
/*
	We cycle through the queue
*/
int		queue_ind;
#define last_added_q_i (queue_ind-1)
int		queue_use_ind;
int		qc;
int		max_depth=0;
//TODO: paper stuff, remove
int		min_depth=10000;

u8		depth_grew=0;

#define FILE_ARG	"@FILE@"
#define FILE_ARG_NAME	"/dev/shm/.arv_farg_"
/*
	For one individual tree
	not sum of all trees
	Be careful about difference of this
	and total_covered
*/
u64		max_coverage_tree_nodes=0;
u8		invd_tree_nodes_grew = 0;

int		output_count=0;

u8		enable_custom_cleanup = 0;
char		* cleanup_cmds[]={"rm /users/sirus/apache2/logs/* > /dev/null 2>&1"};
u8		reserve_hdr=0;
u8		rsv_hdr_size=16; //Constant for now

/**************************** PRIORITY MODEL DEFINITIONS HERE *******************/

/*
	Tree Depth First : TDF : 0
	Tree Nodes First : TNF : 1
	Tree No Sort     : TNS : 2


*/
u8		pm_mode = 0;
char		*pm_str=0;
#define DEFAULT_PM_STR	"TDF"
/*
	Maximum low priority same depth
	Like timeout this value should be
	chosen with care
*/
#define MLPSD_ALLOWED	2
/*
	It's very unlikely we come across a
	trace tree deeper than this number
*/
#define LPSD_MAX_DEPTH	2048
u8		LPSD_queue[LPSD_MAX_DEPTH];
u8		LPSD_queue_wait[LPSD_MAX_DEPTH];


#define MLPSC_ALLOWED	2

/*
	Similarly we won't see an execution
	exceed 64000 nodes unless in very rare
	cases for which we disregard profiling.
	However it's big enough to allocate
	the necessary memory only on-demand.
*/
#define LPSC_MAX_NODES	(1<<16)
u8		*LPSC_queue;
u8		*LPSC_queue_wait;

u64		last_exec_inq=0, last_etime_inq=0;
u8		parallel_mode = 0;
char		master_instance_p[MAX_PATH];

int		_poor_ni_tot=0, _overstay_tot=0;
struct sig_descriptor{
	int id;
	char name[20];
};
#define SIG_SET_SIZE	32
struct sig_descriptor sig_set[SIG_SET_SIZE]={
		{1,		"SIGHUP"	},
		{2,		"SIGINT"	},
		{3,		"SIGQUIT"	},
		{4,		"SIGILL"	},
		{5,		"SIGTRAP"	},
		{6,		"SIGABRT"	},
		{7,		"SIGBUS"	},
		{8,		"SIGFPE"	},
		{9,		"SIGKILL"	},
		{10,		"SIGUSR1"	},
		{11,		"SIGSEGV"	},
		{12,		"SIGUSR2"	},
		{13,		"SIGPIPE"	},
		{14,		"SIGALRM"	},
		{15,		"SIGTERM"	},
		{16,		"SIGSTKFLT"	},
		{17,		"SIGCHLD"	},
		{18,		"SIGCONT"	},
		{19,		"SIGSTOP"	},
		{20,		"SIGTSTP"	},
		{21,		"SIGTTIN"	},
		{22,		"SIGTTOU"	},
		{23,		"SIGURG"	},
		{24,		"SIGXCPU"	},
		{25,		"SIGXFSZ"	},
		{26,		"SIGVTALRM"	},
		{27,		"SIGPROF"	},
		{28,		"SIGWINCH"	},
		{29,		"SIGIO"		},
		{30,		"SIGPWR"	},
		{31,		"SIGSYS"	},
		{32,		"SIGRTMIN"	}


};

#ifdef NESTED_COUNT

#define T(x) (x-init_section)
u64 init_section;

#endif

char bbextract_helper[]="import angr\n"
"from subprocess import Popen,PIPE\n"
"from sys import argv\n"
"from os import system\n"
"SO_OFFSET=0x400000\n"
"target_name = './target'\n"
"if len(argv)>1:\n"
"	target_name=argv[1]\n"
"p=angr.Project(target_name,load_options={'auto_load_libs':False})\n"
"cfg = p.analyses.CFGFast()\n"
"graph=cfg.graph\n"
"nodes=graph.nodes()\n"
"system(\"md5sum {} | awk -F' ' '{{print $1}}'\".format(target_name))\n"
"for n in nodes:\n"
"	print(hex(n.addr-SO_OFFSET)+ '\\t' +str(n.name))";

void		arep(char *, ...);
void		aexit(char *, ...);
void		usage();
int		main(int argc,char **argv);
void		terminate_units();
char		*convert_time(char *);
void		dump_hex(u8 *,u8 ,int ,int );
void		rep_use_time();
void		print_queue();
void		zexit(char *, ...);
void		rep(u8 , char *, ...);
/*
 https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
*/
#define zrep(fmt,...) rep(0,fmt __VA_OPT__(,) __VA_ARGS__)
#define zwarn(fmt,...) rep(1,fmt __VA_OPT__(,) __VA_ARGS__)
void		clear_warn();
char		*exec_speed(char *);
char		*get_sig_name(int );
char		* psect(char *,int ,char *, ...);
char		* show_data(char *,void *,size_t ,size_t ,size_t );
void		refresh_board(void *,size_t ,size_t ,size_t );
void		print_usage();
u8		check_adr(void *);
int		ulock(void * );
void		queue_add(char *,char *,u8 ,size_t);
u8		queue_add_traced(struct input_val *, int *);
void		block_add_traced(struct block_info_local *);
u8		check_block(struct block_info_local *);
int		next_queue_ind();
void		prepare_file_feed(char *);
void		prepare_input_feed();
void		run_starter();
void		read_inputs();
void		modify_input(char *);
void		start_timer();
void		stop_timer();
int		do_term_check();
void		set_recv_timeout(struct timeval *);
void		init_socket();
int		reconnect();
void		reset_structures();
void		custom_cleanup();
int		execute(int );
void		target_timeout(int );
void		save_memory(char *,char *);
void		record(void *,size_t );
void		store_reset(u8 );
void		znormal_exit();
void		int_handler(int );
void		pipe_handler(int );
void		rep_adrs();
void		show_maps();
void		sigf_handler(int , siginfo_t *, void *);
void		dfs_add(ID_SIZE );
u8		visited(ID_SIZE );
void		save_netdata(void *,u64 ,char *);
void		save_debug_info();
void		rep_marked();
int		dfs(struct node *,FILE *);
int		coverage_changes();
void		vars(u64 ,u64 ,u64 ,u64 ,u64 ,u64 );
void		*extract_map(u64 *);
u32		do_hash();
void		eval_tree(struct input_val *);
void		debug_memory(void *);
u8		bitflip_check(u32 );
u8		check_interesting(size_t ,int );
u8		bit_flip(void *,size_t ,u8 ,size_t *);
u8		bit2_flip(void *,size_t ,u8 ,size_t *);
u8		bit4_flip(void *,size_t ,u8 ,size_t *);
u8		byte_flip(void *,size_t ,u8 ,size_t *);
u8		byte2_flip(void *,size_t ,u8 ,size_t *);
u8		byte4_flip(void *,size_t ,u8 ,size_t *);
u8		overw_8_int(void *,size_t ,u8 ,size_t *);
u8		overw_16_int(void *,size_t ,u8 ,size_t *);
u8		overw_32_int(void *,size_t ,u8 ,size_t *);
u8		iter_intr_locs(void *,size_t ,u8 ,size_t *,struct inp_intr_locs *);
u8		kw_ow_linear(void *,size_t ,u8 ,size_t *);
void		mut_kw_ow(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_kw_ins(void *,size_t ,u8 ,size_t *,size_t *,size_t *);
void		mut_random_ow(void *,size_t ,u8 ,size_t *);
void		mut_random_ins(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_copy_ow(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_copy_ins(void *,size_t ,u8 ,size_t *,size_t *,size_t *);
void		mut_shrink_size(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_mix_inputs(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_rand_flip(void *,size_t ,u8 ,size_t *);
void		mut_over_rand_8_int(void *,size_t ,u8 ,size_t *);
void		mut_over_rand_16_int(void *,size_t ,u8 ,size_t *);
void		mut_over_rand_32_int(void *,size_t ,u8 ,size_t *);
void		mut_rand_8_add_sub(void *,size_t ,u8 ,size_t *);
void		mut_rand_16_add_sub(void *,size_t ,u8 ,size_t *);
void		mut_rand_32_add_sub(void *,size_t ,u8 ,size_t *);
void		mut_rand_8_byte(void *,size_t ,u8 ,size_t *);
void		mut_insert_const(void *,size_t ,u8 ,size_t *,size_t *,size_t *);
void		mut_ow_const(void *,size_t ,u8 ,size_t *);
void		mut_sw_bytes(void *,size_t ,u8 ,size_t *);
void		mut_ow_rand_chunk(void *,size_t ,u8 ,size_t *);
void		mut_scatter_rand(void *,size_t ,u8 ,size_t *);
void		mut_intr_locs(void *,size_t ,u8 ,size_t *,struct inp_intr_locs *);
void		print_dict_kws();
void		mut_dict_kw_ow(void *,size_t ,u8 ,size_t *,size_t *);
void		mut_dict_kw_ins(void *,size_t ,u8 ,size_t *,size_t *,size_t *);
void		write_g_input(u8 *,size_t );
int		feed_net(void *,long );
int		feed_ex_target(void *,u64 );
void		save_rep(u8 ,u8 * ,size_t );
void		live_rep();
void		save_mutated_input(u8 *,size_t );
void		add_stat_entry();
void		balance_lps(int );
u8		kw_is_new(u8 *,size_t );
void		arvin_generate();
void		adjust_timeout();
void		do_post_process();
void		arvin_start();
void		load_dictionary();
void		print_banner();
void		init_net_essentials();
void		read_to_mem(void *,char *);
void		pin_to_cpu();
void		test_muts();
void		dir_check_create(char *);
void		load_mut_map();
void		store_mut_map();
size_t		raw_cp(char *,char *,int);
void		arvin_fast_parallel_fuzz();
void		arvp_queue_update();
void		arvp_fserver();
static inline void raw_f_in(void *,struct input_val *);
u8		in_queue(char *);
int		prepare_bhf(char *);



















