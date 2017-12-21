//select a process to migrate, using the per-process processor utilization stats from atop

/*
 * Improvement: Using the %CPU utilization as a measure of the load a process exerts on the
 *   system is not a good idea. Coz, if there are 2 heavy processes executing simultaneously, 
 *   each will use roughly 50% of the CPU. However, if each were to run separately, they would
 *   use nearly 100% of the cycles. So, if there's some sort of 'load prediction' mechanism,
 *   that would be better.
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/types.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "less.h"

#define BIN_FILE "acct/acct.bin"
#define ASC_FILE "acct/acct.asc"
/* Switches used:
 *   -v : show per-process info
 *   -w : write output to this file
 *    1 : use 1 second refresh
 * Improvement: put the bin & asc files in a sys dir rather than the current dir
 */
#define INIT_COMMAND "atop -v -w " BIN_FILE " 1 &"
//#define READ_COMMAND(time) "atop -b " #(time) " -r " BIN_FILE " 1 > " ASC_FILE
#define WATCH_INTERVAL 10
#define LESS_GROUP 7001

int SkipWspaces(FILE *);
void GetNextRecord(FILE *, char *, int);
int GetNoOfHeaders(char *);
int Getegid(char *);

struct timeval atop_start_time;

// set up atop
int AtopInit()
{
	//check whether root
	//check whether atop is available
	system("killall -15 atop");
	//system("touch "BIN_FILE);
	system("rm -f "BIN_FILE);
	system(INIT_COMMAND);
	gettimeofday(&atop_start_time, NULL);
	return 0;	//success
}

/***
 * SelectProcess - selects a process to migrate
 * @otherCpuLoad - % cpu utilization of the destination node
 *
 * returns pid of selected process
 */
int SelectProcess(int otherCpuLoad)
{
	pid_t pid = 0;
	struct timeval tv;
	char time[26], time_hhmm[6], time_ss[3], group[15], strPid[6];
	char *rec = (char *)malloc(200 * 80);
	char *word = (char *)malloc(20);
	int i, j, sec, pidList[WATCH_INTERVAL], loadList[WATCH_INTERVAL], frq[WATCH_INTERVAL];
	int maxFrq, selected, nSkip, len, load, egid;
	time_t diff;
		
	//get current time
	gettimeofday(&tv, NULL);
	diff = tv.tv_sec - atop_start_time.tv_sec;
	tv.tv_sec -= WATCH_INTERVAL;	//10 seconds before
	ctime_r(&tv.tv_sec, time);
	strncpy(time_hhmm, time+11, 5);
	time_hhmm[5] = 0;
	strncpy(time_ss, time+17, 2);
	time_ss[2] = 0;

	//read atop stats from 10s before
	system("rm -f "ASC_FILE);
	char read_cmd[40];
	strcpy(read_cmd, "atop -b ");
	strcat(read_cmd, time_hhmm);
	strcat(read_cmd, " -v -r " BIN_FILE " 1 > " ASC_FILE);
	
	if(diff<10) {
		nSkip = 0;
		sleep(11 - diff);
	}
	else if(diff<70) {
		nSkip = diff - 10 -1;
	}
	else nSkip = atoi(time_ss) + 1;
	
	system(read_cmd);
	
	FILE *f_asc = fopen(ASC_FILE, "r");
	if(!f_asc) {
		perror("File error");
	}
	
	if (diff < 70) GetNextRecord(f_asc, rec, 1);	//skips initial record

	//sec = atoi(time_ss);
	for(i=0; i<nSkip; i++) {
		GetNextRecord(f_asc, rec, 0);
		if (!rec) {printf("!"); break;}
	}
	
	for(i=0; i<WATCH_INTERVAL; i++) {
		pidList[i] = loadList[i] = frq[i] = 0;
	}
	
	for(i=0; i<WATCH_INTERVAL; i++) {
		GetNextRecord(f_asc, rec, 0);
		if(!rec) break;	//EOF
		nSkip = GetNoOfHeaders(rec);
		ReadNextWord(rec+nSkip*80+1, word);
		strcpy(strPid, word);
		pid = atoi(word);
		ReadNextWord(rec+nSkip*80+61, word);
		len = strlen(word);
		word[len - 1] = 0;
		load = atoi(word);
		egid = Getegid(strPid);
		if (egid == -1) continue;
		
// 		if(load + otherCpuLoad > MAX_TOLERABLE_LOAD)
// 			continue;
		
		//check group
		if (egid != LESS_GROUP) continue;
		
		printf("%u - %d (%d)\n", pid, load, egid);
		
		for(j=0; j<i; j++) {
			if(pidList[j] == pid) {
				frq[j]++;
				if(load > loadList[j]) //load at a later instant is greater, 
							//but within acceptable limits
					loadList[j] = load;
				break;
			}
		}
		if(j==i) {
			pidList[i] = pid;
			loadList[i] = load;
			frq[i] = 1;
		}
	}
	//select most frequent pid from array, 
	//with preference to those occuring later in the array
	maxFrq = 0;
	selected = -1;
	for(--i; i>=0; i--) {
		if(frq[i] > maxFrq) {
			maxFrq = frq[i];
			selected = i;
		}
		else if((frq[i] == maxFrq) && (loadList[i] > loadList[selected]))
			selected = i;
	}
	if(maxFrq) printf("Maxfrq:%d", maxFrq);
	if (selected != -1) printf(", Pid:%d\n", pidList[selected]);
	
	else {
		 printf("Nothing selected!\n");
		 return -1;
	}
// 	free(rec);
// 	free(word);
	fclose(f_asc);
	
	return pidList[selected];
}

/*** 
 * ReadNextWord - reads the next word seen in the string
 * @rec - string
 * @word - on return, this will point to the word read
 *
 * returns the number of characters actually read (including whitespaces)
 */
int ReadNextWord(char *rec, char *word)
{
	char ch;
	int i, nWsp=0;	//nWsp: no of whitespaces
	while( (*rec == ' ') || (*rec == '\t') || (*rec == '\n')) { //skips whitespaces
		rec++;	
		nWsp++;
	}
	for(i=0; ( (*rec != ' ') && (*rec != '\t') && (*rec != '\n')); i++, rec++)
		word[i] = *rec;
	word[i] = 0;
	return nWsp+i;
}

/***
 * SkipWspaces - skips whitespaces in the file stream
 * @fp: pointer to FILE
 */
int SkipWspaces(FILE *fp)
{
	int ch;
	for(ch = fgetc(fp); (ch==' ' || ch=='\n' || ch=='\t'); ch = fgetc(fp))
		if(ch == EOF) return -1;
	fseek(fp, -1, SEEK_CUR);	//put back the non-whitespace char read
	return 0;
}

/***
 * GetNextRecord - get next record from atop ASCII output file
 * @fp: pointer to ASCII output file of atop
 * @skipBoot: is 1 if the file contains a record of activity since boot
 *
 * records are separated by 2 successive newlines in the ASCII output file.
 * returns pointer to the buffer containing the record if there's at least
 * one more record; else returns NULL.
 */
void GetNextRecord(FILE *fp, char *rec, int skipBoot)
{
	char *line, ch;
	int nRead, size=80;	//size of a line
	line = (char *)malloc(size); 
	
	if(SkipWspaces(fp) == -1) return;
	*rec = 0;
	
	if(skipBoot) goto Only4BootRec;
	
	while ( ((nRead = getline(&line, &size, fp)) != 1) && (nRead != -1)) 
		 strcat(rec, line);
	
	if(nRead == -1) return;	//returns with rec=NULL
	
	strcat(rec, "\n");
   Only4BootRec:	
	while ( ((nRead = getline(&line, &size, fp)) != 1) && (nRead != -1)) 
		strcat(rec, line);
	
	
	free(line);
}

int GetNoOfHeaders(char *rec)
{
	int i;
	for(i=0; i<20; i++)
		if(rec[i * 80] == '\n') break;
	return i+1;
}

int Getegid(char *pid)
{
	int egid, size=80, nRead;
	FILE *f_status;
	char *line, word[20], fileStr[20] = "/proc/";
	
	strcat (fileStr, pid);
	strcat (fileStr, "/status");
	f_status = fopen(fileStr, "r");
	line = (char *) malloc(size);
	
	if(!f_status) 	//probably the process has finished executing by now
		return -1;
	
	while(!feof(f_status)) {
		getline (&line, &size, f_status);
		ReadNextWord (line, word);
		if( !strcmp(word, "Gid:") ) break;
	}
	
	if (feof(f_status)) {
		fprintf(stderr, "Error getting egid from %s\n", fileStr);
		return -1;
	}
	
	line += strlen(word);
	nRead = ReadNextWord (line, word);	//gid
	ReadNextWord (line+nRead, word);	//egid
	
	fclose (f_status);
// 	free(line);
	
	return atoi(word);
}
