#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include "less.h"

#define LOAD_CLIST_LEN 4

struct cputimes {
	long us;		//user mode
	long ni;		//nice
	long sy;		//system mode
	long id;		//idle
	long io;		//iowait
	long ir;		//irq
	long si;		//softirq
}*last;

int load_clist[LOAD_CLIST_LEN], clist_next;

void ReadCpuTimes(struct cputimes *);
void Overload (char *);

/***
 * SysLoad - calculates cpu % utilization in the last second
 *
 */
int SysLoad()
{
	int load;
	long int used, total;
	struct cputimes *now;
	now = (struct cputimes *) malloc(sizeof(struct cputimes));
	ReadCpuTimes(now);
	
	used = (now->us - last->us) + (now->ni - last->ni) + (now->sy - last->sy) 
			+ (now->io - last->io) + (now->ir - last->ir) 
			+ (now->si - last->si);
	total = used + (now->id - last->id);
	load = ((long double)used / total) * 100.0;	//denominator: 100 (jiffies/s)
	
	
	return load;
}

void ReadCpuTimes(struct cputimes *t)
{
	FILE *f_stat = fopen("/proc/stat", "r");
	if(!f_stat) {
		perror("File error");
	}
	int size = 80, nRead;
	char *line = (char *)malloc(size);
	char *word = (char *)malloc(20);
	if ( (getline(&line, &size, f_stat)) == -1) {
		fprintf(stderr, "File read error!\n");
		exit(EXIT_FAILURE);
	}
	
	line += 3;	//skips 'cpu'
	
	nRead = ReadNextWord(line, word);
	t->us = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->ni = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->sy = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->id = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->io = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->ir = atol(word);
	line += nRead;
	
	nRead = ReadNextWord(line, word);
	t->si = atol(word);
	line += nRead;

}


main(int argc, char *argv[])
{
	int load, i, otherCpuLoad, pid;
	pthread_t listen_thread;
	char dst[20];
	struct packet *pkt;
	
	if (argc != 2) {
		printf("Usage: %s <ip>\n", argv[0]);
		return 0;
	}
	
	//initialization
	for(i=0, clist_next=0; i<LOAD_CLIST_LEN; i++)
		load_clist[i] = 0;
	AtopInit();
	last = (struct cputimes *) malloc(sizeof(struct cputimes));
	ReadCpuTimes(last);
	seq = 1;

	if (pthread_create(&listen_thread, NULL, Listen, NULL) < 0) {
 		fprintf(stderr, "pthread_create error!\n");
 		return 1;
 	}

	//begin monitoring
	while(1) {
		sleep(1);
// 		if(argc > 2)
		if( ((load = SysLoad()) > MAX_TOLERABLE_LOAD) 
				    && (load_clist[clist_next] > MAX_TOLERABLE_LOAD)){
			Overload(argv[1]);
			sleep(10);
			}
		
		//enter into circular list
		load_clist[clist_next] = load;
		clist_next = (++clist_next) % LOAD_CLIST_LEN;
	}
	
	pthread_join(listen_thread, (void *)&i);
	return 0;
}

#include "migration.h"
void Overload(char *dst)
{
	int otherCpuLoad;
	unsigned long pid;
	char *fname = "/less/aaa";
	
	printf("Overload.\n");
	
	if ( (otherCpuLoad = Probe(dst)) )
		printf("Dest: %s\n", dst);
	//else probe for other nodes
	
// 	otherCpuLoad = 0;
	pid = SelectProcess(otherCpuLoad);
	if(pid == -1) {
		 printf("No process selected.\n");
		 return; 
	}

	//Send() stuff
	printf("going to dump.............\n");
	ydump(pid, fname);
	kill(pid, 9);
	printf("dumped!\n");
	
	Send_fname(fname, dst);
}

/* 
 * Improvement: handle TERM/KILL signals - stop atop
 */
