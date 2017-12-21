
#define CMD_FIND_NODE		1
#define CMD_REPLY_AVAILABLE	2
#define CMD_NODE_SELECTED	3
#define CMD_CONFIRM_OK		4
#define CMD_FILENAME		5
#define CMD_FILENAME_ACK	6

#define MAX_TOLERABLE_LOAD	50
//#define DST "192.168.29.124"
#define NEXT_SEQ ++seq
#define LESS_LISTEN_PORT 8001
#define NFS_ROOT_MOUNT		""



struct packet {
	int command;
	int cpu;
	int mem;
	short ar;
	int seq;
	char fname[100];
};

int AtopInit(void);
int SelectProcess(int otherCpuLoad);
int ReadNextWord(char *rec, char *word);

int seq;
int Send(struct packet *p, char *dst);
void Send_findnode (char *dst);

void *Listen (void *arg);

int SysLoad(void);
