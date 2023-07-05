/*
	By Sirus Shahini
	~cyn

	Helper library to aid arvin in fast parallel
	fuzzing mode.

*/

#include "head.h"

u8 debug_print=1;

void aexit(char *fmt, ...){
	char err_format[2048];

	strcpy(err_format,CRED "[!] Lib arvp: " CNORM );
	strcat(err_format,fmt);
	strcat(err_format,"\n");
	va_list argp;
	va_start(argp,err_format);
	vprintf(err_format,argp);
	va_end(argp);
	exit(-1);
}

void arep(char *fmt, ...){
	char s_format[2048];

	if (!debug_print) return;

	strcpy(s_format,CGREEN "[-] Lib arvp: "CNORM);
	strcat(s_format,fmt);
	strcat(s_format,"\n");
	va_list argp;
	va_start(argp,s_format);
	vprintf(s_format,argp);
	va_end(argp);

}

void sigf_handler(int sig){
	int crash_rep_stat=-2;

	write(FD_OUT,&crash_rep_stat,4);
	exit(0);
}

__attribute__ ((__constructor__))
void arvlib_constructor(){
	int cid,stat,rep_stat,n;
	char command;

	arep("Initializing starter (arvp)\n");
	setenv("LD_PRELOAD","",1);

	signal(SIGSEGV,sigf_handler);

	/*
		Let the parent know we're ready
	*/
	write(FD_OUT,".",1);

	rep_stat=0;

	while (1){
		int wrpid;
		int term=0;

		n =read(FD_IN,&command,1);

		if (n<1){
			aexit("FATAL: Can't read command");
		}

		cid=fork();
		if (cid==-1)
			aexit("fork()");
		if (!cid){

			signal(SIGSEGV,SIG_DFL);
			close(FD_IN);
			close(FD_OUT);

			ptrace(PTRACE_TRACEME,0,0,0);
			raise(SIGTRAP);
			return ;

		}


		n=write(FD_OUT,&cid,4);
		if (n<4){
			aexit("FATAL: Can't write response; %d %s\n",n,strerror(errno));
		}



		/*
			Set ptrace options
		*/
		wrpid=wait(0);
		if (wrpid!=cid)
			aexit("waitpid()");
		ptrace(PTRACE_SETOPTIONS,cid,0,PTRACE_O_TRACECLONE);
		ptrace(PTRACE_CONT,cid,0,0);

		/*
			Wait for the new instance to end
			We need to do it in a loop to support
			multi-threaded targets.

			This is the same loop as in libarv.c
		*/

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


						If we are here it's kill signal, either by timeout
						or the one we sent after a crash was detected.
					*/
					term=1;
					if (rep_stat){
						/*
							A crash has already happened and recorded in a non-main thread.
							SIGKILL after crash.
						*/

					}
					else{
						/*
							SIGKILL due to timeout
						*/
						rep_stat=stat;
					}

				}else if(WIFSTOPPED(stat)){
					if ((stat>>8) == ((PTRACE_EVENT_CLONE<<8) | SIGTRAP)){
						//New thread notifier in parent
						ptrace(PTRACE_CONT,wrpid,0,0);
						continue;
					}
					else if (WSTOPSIG(stat)==SIGSTOP){
						ptrace(PTRACE_CONT,wrpid,0,0);
						continue;
					}else if(WSTOPSIG(stat)==SIGCHLD){
						/*
							A forked process has exited or changed status.

						*/
						ptrace(PTRACE_CONT,wrpid,0,SIGCHLD);
					}else{

						term=1;
						rep_stat=stat;
						kill(cid,9);

					}

				}else{
					aexit("waitpid(): invalid stat: %08x\n",stat);
				}


				if (term){

					while(waitpid(-1,&stat,0)!=-1);

					n=write(FD_OUT,&rep_stat,4);
					if (n<4){
						printf("%d %s\n",n,strerror(errno));
						aexit("FATAL: Can't write response\n");
					}

					break;
				}

			}else{

				if (WIFEXITED(stat)){
					/*
						Non-main thread exited normally.
					*/
					continue;

				}else if (WIFSIGNALED(stat)){

					/*
						Kill signal (most probably from ourselves)
					*/

					continue;

				}else if(WIFSTOPPED(stat)){
					if ((stat>>8) == ((PTRACE_EVENT_CLONE<<8) | SIGTRAP)){
							//New thread notifier in parent
							ptrace(PTRACE_CONT,wrpid,0,0);
							continue;

					}
					else if (WSTOPSIG(stat)==SIGSTOP){
						ptrace(PTRACE_CONT,wrpid,0,0);
						continue;
					}else{
						/*
							Other signals
						*/

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
