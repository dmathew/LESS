#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "less.h"

int replyavailable, confirmok, fnameack;

void *Listen (void *arg)
{

	int sock, sock_new, addrlen, yes=1;
	struct sockaddr_in my_addr, cli_addr;
	struct packet p, *pkt=&p;
	struct packet reply_pkt;
	char addr[20], str[30], fname[200];
	
	
	addrlen = sizeof(struct sockaddr_in);
	//pkt = (struct packet *) malloc(sizeof(struct packet));
	
	if( (sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(LESS_LISTEN_PORT);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));
	if( bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
   while(1) {	//continue forever!
	
	printf("Listening...\n");
	if (listen(sock, 10) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	if ((sock_new = accept(sock, (struct sockaddr *)&cli_addr, &addrlen))==-1) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	
	if (recv(sock_new, (void *)&p, sizeof(struct packet), 0) == -1) {
		perror("recv");
		exit(EXIT_FAILURE);
	}
	strcpy(addr, inet_ntoa(cli_addr.sin_addr));
	//printf("Got %s from %s\n", str, addr);
	
	//opening the packet...
	switch(pkt->command) {
		case CMD_FIND_NODE:
			printf("Got a FIND_NODE packet\n");
			//improvement: use avg load instead of instantaneous
			if(SysLoad() > MAX_TOLERABLE_LOAD) break;
			//create pkt with SysLoad() and send
			reply_pkt.command = CMD_REPLY_AVAILABLE;
			reply_pkt.cpu = SysLoad();
			reply_pkt.mem = 0;
			reply_pkt.ar = 0;
			reply_pkt.seq = pkt->seq;
			
			if (Send(&reply_pkt, addr) < 0) {
				fprintf(stderr, "Error while sending REPLY_AVAILABLE packet.\n");
				exit(EXIT_FAILURE);
			}
			
			if (close(sock_new) == -1) {
				perror("close: after REPLY_AVAILABLE");
				exit(EXIT_FAILURE);
			}
			break;
		
		case CMD_REPLY_AVAILABLE:
			printf("Got a REPLY_AVAILABLE packet with load %d\%\n", pkt->cpu);
			replyavailable = pkt->cpu + 1;
			break;
			
		case CMD_NODE_SELECTED:
			printf("Got a NODE_SELECTED packet with seq %d\n", pkt->seq);
			reply_pkt.command = CMD_CONFIRM_OK;
			reply_pkt.cpu = 0;
			reply_pkt.mem = 0;
			reply_pkt.ar = 0;
			reply_pkt.seq = pkt->seq;
			if (Send(&reply_pkt, addr) < 0) {
				fprintf(stderr, "Error while sending CONFIRM_OK packet.\n");
				exit(EXIT_FAILURE);
			}
			
			if (close(sock_new) == -1) {
				perror("close: after CONFIRM_OK");
				exit(EXIT_FAILURE);
			}
			break;
			
		case CMD_CONFIRM_OK:
			printf("Got a CONFIRM_OK packet with seq %d\n", pkt->seq);
			confirmok = 1;
			break;
		
		case CMD_FILENAME:
			printf("Got a FILENAME packet: %s\n", pkt->fname);
			
			reply_pkt.command = CMD_FILENAME_ACK;
			reply_pkt.seq = pkt->seq;
			
			if (Send(&reply_pkt, addr) < 0) {
				fprintf(stderr, "Error while sending CONFIRM_OK packet.\n");
				exit(EXIT_FAILURE);
			}
			
			if (close(sock_new) == -1) {
				perror("close: after CONFIRM_OK");
				exit(EXIT_FAILURE);
			}
#include "migration.h"			
			strcpy(fname, NFS_ROOT_MOUNT);
			strcat(fname, pkt->fname);

		{	struct stat buf;
			printf("%s: ", fname);
			if (lstat(fname, &buf) < 0)
				perror("file does not exist\n");
			
			if (fork() == 0) {
// 				printf("Hi!********************\n");
				yresume(fname);
			}
			
			
		}
			break;
			
		case CMD_FILENAME_ACK:
			printf("Got a FILENAME_ACK packet with seq %d\n", pkt->seq);
			fnameack = 1;
			break;
		
		default:
			fprintf(stderr, "Invalid command from %s: %d (%d)\n",
				addr, pkt->command, pkt->cpu);
	}
	
   } //end of while(1)
	
	return NULL;

}

int Listen4_replyavail()
{
	int otherCpuLoad;
	while(!replyavailable) ;
	otherCpuLoad = replyavailable;
	replyavailable = 0;
	return otherCpuLoad;
}

void Listen4_confirmok()
{
	while(!confirmok) ;
	confirmok = 0;
}

void Listen4_fnameack()
{
	while(!fnameack) ;
	fnameack = 0;
}
