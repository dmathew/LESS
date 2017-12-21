#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include "less.h"

/***
 * Send - initiates a new TCP connection with dst and sends packet pkt
 *  @dst - IP of destination
 *
 */
int Send(struct packet *pkt, char *dst)
{
	int sock;
	struct sockaddr_in other_node;
	struct hostent *he;
	
	if( (sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return -1;
	}
	
	if ((he=gethostbyname(dst)) == NULL) {  // get the host info 
		herror("gethostbyname");
		return -1;
	}
	
	other_node.sin_family = AF_INET;
	other_node.sin_port = htons(LESS_LISTEN_PORT);
	other_node.sin_addr = *((struct in_addr *)he->h_addr);
	memset(other_node.sin_zero, '\0', sizeof(other_node.sin_zero));

	if ( (connect(sock, (struct sockaddr *)&other_node, sizeof (struct sockaddr_in)) == -1)) {
		perror("connect: sending find_node pkt");
		return -1;
	}
	//char str[30] = "Hey, that worked!";
	printf("Sending packet %d\n", pkt->command);
	if (send(sock, (void *)pkt, sizeof(struct packet), 0) == -1) {
		perror("send");
		return -1;
	}
	
	if (close(sock) == -1) {
		perror("close");
		return -1;
	}
	return 0;
}

void Send_findnode(char *dst)
{
	struct packet *p = (struct packet *) malloc(sizeof(struct packet));
	p->command = CMD_FIND_NODE;
	p->cpu = SysLoad();
	p->mem = 0;
	p->ar = 0;
	p->seq = NEXT_SEQ;
	if(Send(p, dst) < 0) {
		fprintf(stderr, "Error sending FIND_NODE packet.\n");
		exit(EXIT_FAILURE);
	}
}

void Send_nodeselected(char *dst)
{
	struct packet *p = (struct packet *) malloc(sizeof(struct packet));
	p->command = CMD_NODE_SELECTED;
	p->cpu = 0;
	p->mem = 0;
	p->ar = 0;
	p->seq = NEXT_SEQ;
	if(Send(p, dst) < 0) {
		fprintf(stderr, "Error sending NODE_SELECTED packet.\n");
		exit(EXIT_FAILURE);
	}
}

void Send_fname(char *fname, char *dst)
{
	struct packet *p = (struct packet *) malloc(sizeof(struct packet));
	p->command = CMD_FILENAME;
	strcpy (p->fname, fname);
	p->seq = NEXT_SEQ;
	
	if(Send(p, dst) < 0) {
		fprintf(stderr, "Error sending FILENAME packet.\n");
		exit(EXIT_FAILURE);
	}
	Listen4_fnameack();
}

/***
 * Probe - probes dst node to see if it can accept some load
 *
 * returns the load of dst node
 */
int Probe (char *dst)
{
	int otherCpuLoad;
	Send_findnode(dst);
	otherCpuLoad = Listen4_replyavail() - 1;
	Send_nodeselected(dst);
	Listen4_confirmok();
	return otherCpuLoad;
}
