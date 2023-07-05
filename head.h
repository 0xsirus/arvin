/*
	Headers and generic definitions
	--

	By Sirus Shahini
	sirus.shahini@gmail.com

*/


#ifndef   _HEADER_H
#define  _HEADER_H

//#define DEBUG_MODE

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <libgen.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <execinfo.h>
#include <sched.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#define u8 					uint8_t
#define u16 				uint16_t
#define u32 				uint32_t
#define u64 				uint64_t

#define s8 					int8_t
#define s16 				int16_t
#define s32 				int32_t
#define s64 				int64_t


#define FD_IN				200
#define FD_OUT				201

#define MAX_PATH			1024

/*
	Shared memory size has an important affect
	on fuzzing speed. Growing shared memory will slow
	down fuzzing. 1M should be enough for most cases
	but if the program is small 512K should suffice.

	Default:19 (sufficient for up to 4000 nodes)

	Speed difference between 19 and 20 is negligible.
*/
//#define ARV_BIG_TARGET

#ifdef ARV_BIG_TARGET
#define SHM_SIZE			(1<<20)
#else
#define SHM_SIZE			(1<<19)
#endif

#define COMM_SHM_SIZE			(1<<25) //32M commication channel

//TODO: paper stuff, remove later
//#define NESTED_COUNT

#define ID_SIZE				u64
#define ID_LEN				((u8)sizeof(ID_SIZE))
#define NODES_MAX			(1<<18) //256k for now
#define BLOCK_INFO_SIZE		u8
#define BLOCK_TYPE_NESTED	1 //bit 0
#define BLOCK_MARKED		2 //bit 1

#define LOCK_DELTA			8
#define PROT_VARS_DELTA		24

#define BB_FNAME	"arv_bb_info"




struct node{
	ID_SIZE id;
	BLOCK_INFO_SIZE info;
	struct node *parent;
	struct child_ptr *children; //linked list
	u32 hits;
	u8 current_child_idx;
};
struct tree{
	u64 count; //total basic blocks = nodes
	struct node *root;
	int depth;
	u64 total_hits;
	/*
		This one is for helping do_hash()
	*/
	u64 total_children ;
} ;

struct block_info{
	ID_SIZE id;
	struct node *ptr;
};

/*
	child descriptor in children linked list
	child_node points to the actual chile 'node' structure
*/
struct child_ptr{
	struct node *child_node;
	struct child_ptr *next_child;
};

#define RNDBASE		(	rand()/(double)(((u64)RAND_MAX)+1)	)
#define RST(max)	((size_t)	(RNDBASE*(max)))
#define RU64(max)	((u64)		(RNDBASE*(max)))
#define RU32(max)	((u32)		(RNDBASE*(max)))
#define RU16(max)	((u16)		(RNDBASE*(max)))
#define RU8(max)	((u8)		(RNDBASE*(max)))
#define RS64(max)	((s64)		(RNDBASE*(max)))
#define RS32(max)	((s32)		(RNDBASE*(max)))
#define RS16(max)	((s16)		(RNDBASE*(max)))
#define RS8(max)	((s8)		(RNDBASE*(max)))
/*
	\x1b or 27 is shell escape character
*/
#define CNORM				"\033[0m" // \033 octal = 27 = 0x1b
#define CRED				"\x1b[1;31m"
#define CGREEN				"\x1b[1;32m"
#define CORANGE				"\x1b[1;33m"
#define CVIOLET				"\x1b[1;35m"
#define CDBlue				"\x1b[1;36m"
#define CGRAY				"\x1b[1;37m"
#define CLGREEN				"\x1b[1;92m"
#define CYELLOW				"\x1b[1;93m"
#define CLVIOLET			"\x1b[1;35m"
#define CWHITE				"\x1b[1;97m"
#define CLCYAN				"\x1b[1;98m"

/*
	Box drawing
*/

#define DStart 	"\x1b)0\x0e"
#define DStop	"\x1b)B\x0f"

#define HO		"q"
#define _2HO		HO HO
#define _3HO		_2HO HO
#define _4HO		_2HO _2HO
#define _5HO		_4HO HO
#define _6HO		_4HO _2HO
#define _7HO		_4HO _3HO
#define _8HO		_4HO _4HO
#define _9HO		_8HO HO
#define _14HO	_8HO _6HO
#define _16HO	_8HO _8HO
#define _32HO	_16HO _16HO
#define _64HO	_32HO _32HO

#define VR		"x"
#define LCD		"l"  //left corner down
#define RCD		"k"	 //right corner down
#define LCU		"m"  //left corner up
#define RCU		"j"  //right corner up
#define DP		"n"  //plus
#define BVR		"t"	//branch vertical right
#define BVL		"u" //branch vertical left
#define BHU		"v" //branch horizontal up
#define BHD		"w" //branch horizontal down

#define DH		"\x1b[H" //diplay home
#define DC		DH "\x1b[2J" //display clear
#define EOL		"\x1b[OK"
#define HC		"\x1b[?25l" //hide cursor
#define SC		"\x1b[?25h" //show cursor

#define _14SP	"              "
#define _15SP	"               "

#define NLI	"\t   "

/* Based on AFL  */

#define _8_ints		\
		-128,		\
		-1,			\
		0,			\
		1,			\
		16,			\
		32,			\
		64,			\
		100,		\
		127

#define _16_ints	\
		-32768,		\
		-129,		\
		128,		\
		255,		\
		256,		\
		512,		\
		1000,		\
		1024,		\
		4096,		\
		32767

#define _32_ints			\
		-2147483648LL,		\
		-100663046,			\
		-32769,				\
		32768,				\
		65535,				\
		65536,				\
		100663045,			\
		2147483647			\


#endif //_HEADER_H
