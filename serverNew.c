
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <arpa/inet.h>

#include <errno.h>

#include <net/ethernet.h> /* the L2 protocols */


#define SERVER_PORT 22000
#define TRUE 1

// 91.189.91.157 - server NTP

/*
array de stucturi care tin informatii despre fiecare router

struct {

	ipRouter
	total sent
	last
	avg
	min
	max


}

*/

int recvSegment(int sock, char *buff, int max_cap){
    
	int len;

	if(-1==read(sock, &len, sizeof(int))){
		perror("Eroare la read");
		return errno;
	}

	if(len>max_cap)
		len=max_cap;

	if(-1==read(sock, buff, len)){
		perror("Eroare la read");
		return errno;
	}

    return len;

}

int sendSegment(int socket, char *buff, int len){

	int returnValue;

    if(-1==write(socket, &len, sizeof(int))){
		perror("Eroare la write");
		return errno;
	}
	
	
    returnValue=write(socket, buff, len);

	if(returnValue==-1){
		perror("Eroare la write");
		return errno;
	}

	return returnValue;

}

struct routerInfo{

	unsigned char ip[20];
	unsigned short totalSent;
	unsigned short last;
	unsigned short avg;
	unsigned short min;
	unsigned short max;


};


void routerInfoToString(char *str, int maxLen, struct routerInfo info){
	// posibil ca functia asta sa fie stricata

	snprintf(str, maxLen, "IP:%d.%d.%d.%d  Sent:%hd  Last:%hd  Avg:%hd  Min:%hd   Max:%hd\n",info.ip[0], info.ip[1], info.ip[2], info.ip[3]
	, info.totalSent, info.last, info.avg, info.min, info.max);

	
}



long time_in_msec(){

	// returneaza un timestamp cu rezolutia de 1ms
	struct timeval timp;
	gettimeofday(&timp,0);

	return timp.tv_sec*1000+timp.tv_usec/1000;

}




void updateUDPpacket(struct iphdr *ipHeader, struct udphdr *udpHeader, char *date){

	//updates TTL, ID number, destination port and timestamp

	ipHeader->ttl++;
	ipHeader->id=htons(ntohs(ipHeader->id)+1);
	udpHeader->dest=htons(ntohs(udpHeader->dest)+1);

	
	long timp=time_in_msec();
	long *timestampLocation=(long*)date;
	*timestampLocation=timp;
	

}

void updateUDPpacketv2(char *packet){

	struct iphdr *ipHeader = (struct iphdr*)packet;
	struct udphdr *udpHeader= (struct udphdr*) (packet+sizeof(struct iphdr));
	char *date=(packet+sizeof(struct iphdr)+sizeof(struct udphdr));


	ipHeader->ttl++;
	ipHeader->id=htons(ntohs(ipHeader->id)+1);
	udpHeader->dest=htons(ntohs(udpHeader->dest)+1);


	long timp=time_in_msec();
	long *timestampLocation=(long*)date;
	*timestampLocation=timp;


}

void resetUDPpacket(char *packet, unsigned short portDest){

	struct iphdr *ipHeader = (struct iphdr*)packet;
	struct udphdr *udpHeader= (struct udphdr*) (packet+sizeof(struct iphdr));
	char *date=(packet+sizeof(struct iphdr)+sizeof(struct udphdr));

	ipHeader->ttl=0;
	ipHeader->id=htons(rand());
	udpHeader->dest=htons(portDest);


}

int isEndHop(char *buffer){
	// routerul care ne da port unreachable este destinatia

	struct iphdr *ipHeader=(struct iphdr*) buffer;
	struct icmphdr *icmpHeader=(struct icmphdr*) (buffer+sizeof(struct iphdr));
	char *date=buffer+sizeof(struct iphdr)+sizeof(struct icmphdr);


	char tipICMP=icmpHeader->type;
	
	if(tipICMP==3){
		//port unreachable (am ajuns la destinatie)

		// aflam al catelea router a trimis mesajul astfel: la fiecare mesaj incrementam portul destinatie, initial e 33441, deci nrRouter = nrPort-33440
		// nr portului la care am trimis pachetul UDP se regaseste la octetii 23-24 dupa headerul ICMP

		unsigned short port=*((unsigned short*)(date+22));
		port=ntohs(port);    // aducem in little endian

		unsigned short nrRouter=port-33440;

		return nrRouter;
		
	}
	else
		return 0;


}



int computeNumberOfHops(int sendingSocket, int receivingSocket, char* packetToSend, short lungimePachet, int maxNumberOfHops){
		
	char receivingBuffer[300];

	struct sockaddr_in dest, capturedSource;
	dest.sin_family = AF_INET;
	dest.sin_port = htons(33440);
  	dest.sin_addr.s_addr = inet_addr("8.8.8.8");

	int capturedSourceSize;
	int returnValue;
	int hopCount=0;

	

	for(int i=1;i<=maxNumberOfHops;i++){
		updateUDPpacketv2(packetToSend);


		if(-1==sendto(sendingSocket, packetToSend, lungimePachet, 0, (struct sockaddr*) &dest, sizeof(dest))){
			perror("Eroare la send in functie");
			return errno;
		}

		if(-1==recvfrom(receivingSocket, receivingBuffer, 290, 0, (struct sockaddr *)&capturedSource, &capturedSourceSize)){
			if(errno==11) // un router nu a raspuns cu ICMP
				continue;
			else{ //alta eroare
				perror("Eroare la recv in functie");
				return errno;
			} 
		}

		returnValue=isEndHop(receivingBuffer);
		if(returnValue>0){   // a venit un pachet ICMP port unreachable ( deci unul din pachetele noastre a ajuns la destinatie). Dar daca a venit intai un altul decat primul
			hopCount=returnValue; // cu TTL suficient de mare pentru a ajunge la dest?
			
			break;
		}	

	}
		
	return hopCount;

}

/*

int processICMP(char *buffer){

	// icmp header is 8 bytes long
	// ip header is 20 bytes long
	// udp header is 8 bytes long


	struct iphdr *ipHeader=(struct iphdr*) buffer;
	struct icmphdr *icmpHeader=(struct icmphdr*) (buffer+sizeof(struct iphdr));
	char *date=buffer+sizeof(struct iphdr)+sizeof(struct icmphdr);

	char tipICMP=icmpHeader->type;
	unsigned char routerAddr[20];
	
	memcpy(routerAddr, &ipHeader->saddr,4);

	if(tipICMP==11){
		// TTL expirat

		// aflam al catelea router a trimis mesajul astfel: la fiecare mesaj incrementam portul destinatie, initial e 33441, deci nrRouter = nrPort-33440
		// nr portului la care am trimis pachetul UDP se regaseste la octetii 23-24 dupa headerul ICMP

		unsigned short port=*((unsigned short*)(date+22));
		port=ntohs(port);    // aducem in little endian

		int nrRouter=port-33440;

		printf("%d TTL expirat ", nrRouter);

		for(int i=0;i<4;i++)
			printf("%d ", routerAddr[i]);

		printf("\n");

		return 0;

	}


	if(tipICMP==3){
		// port unreachable (am ajuns la destinatie)

		unsigned short port=*((short*)(date+22));
		port=ntohs(port);    // aducem in little endian
		int nrRouter=port-33440;

		printf("%d Port unreachable ", nrRouter);

		for(int i=0;i<4;i++)
			printf("%d ", routerAddr[i]);

		printf("\n");

		return nrRouter;
	}
	

}


*/

int processICMPv2(char *buffer, struct routerInfo *tabelRoutere){

	struct iphdr *ipHeader=(struct iphdr*) buffer;
	struct icmphdr *icmpHeader=(struct icmphdr*) (buffer+sizeof(struct iphdr));
	char *date=buffer+sizeof(struct iphdr)+sizeof(struct icmphdr);

	char tipICMP=icmpHeader->type;
	//unsigned char routerAddr[20];
	int nrRouter;

	//memcpy(routerAddr, &ipHeader->saddr,4);


	// aflam al catelea router a trimis mesajul astfel: la fiecare mesaj incrementam portul destinatie, initial e 33441, deci nrRouter = nrPort-33440
	// nr portului la care am trimis pachetul UDP se regaseste la octetii 23-24 dupa headerul ICMP

	

	unsigned short port=*((short*)(date+22));
	port=ntohs(port);    // aducem in little endian

	nrRouter=port-33440;


	// acum actualizam informatiile despre routerul cu nr. de ordine nrRouter

	//printf("nrRouter=%d\n", nrRouter);

	memcpy((*(tabelRoutere+nrRouter)).ip, &ipHeader->saddr, 4); //IP
	

	// timestampul se gaseste la octetii 29-36 dupa headerul ICMP (la inceputul sectiunii de date din pachetul original)
	
	long timestamp=*((long*)(date+28));
	long timeElapsed=time_in_msec()-timestamp;
	
	//printf("%d Time elapsed: %ld\n",nrRouter, timeElapsed);

	(*(tabelRoutere+nrRouter)).last=timeElapsed;


	if((*(tabelRoutere+nrRouter)).max<timeElapsed || (*(tabelRoutere+nrRouter)).max==0)
		(*(tabelRoutere+nrRouter)).max=timeElapsed;

	if((*(tabelRoutere+nrRouter)).min>timeElapsed || (*(tabelRoutere+nrRouter)).min==0)
		(*(tabelRoutere+nrRouter)).min=timeElapsed;
	
	short prevAvg=(*(tabelRoutere+nrRouter)).avg;

	(*(tabelRoutere+nrRouter)).avg=( (*(tabelRoutere+nrRouter)).totalSent * prevAvg + timeElapsed ) / ( (*(tabelRoutere+nrRouter)).totalSent + 1 );

	(*(tabelRoutere+nrRouter)).totalSent++;

	//printf("FunctienrRouter=%d\n", nrRouter);

	return nrRouter;

	}


int mtr(int tcpSocket){


	char sendingBuffer[100]={0};
	char receivingBuffer[300]={0};

	const char IPsursa[20]="192.168.43.241";
	const char IPdest[20]="8.8.8.8";

	const unsigned short portSursa=30000;
	const unsigned short portDest=33440;   // portul destinatie ar trebui sa fie intre 33430 si 33530, altfel unele routere nu dau ICMP inapoi

	const short lungimePachet=60;

	const int maxNumberOfHops=30;

	const int packet_count=5;

	int sendingSocket, receivingSocket;






	if(-1==(sendingSocket=socket(AF_INET, SOCK_RAW, IPPROTO_UDP))){
		perror("Eroare la creare socket scriere");
		return errno;
	}

	int one = 1;

	// inform the kernel do not fill up the packet structure, we will build our own
  	if(setsockopt(sendingSocket, IPPROTO_IP, IP_HDRINCL, &one, sizeof(int)) < 0) {
    	perror("setsockopt() error");
    	return errno;
  	}
  	printf("OK: socket option IP_HDRINCL is set.\n");





																					// asa se declara un socket raw care primeste toate pachetele incepand cu layer 2
	//if(-1==(receivingSocket=socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))){    //  trebuie inclus net/ethernet.h
	if(-1==(receivingSocket=socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))){     // un socket care primeste doar pachete ICMP, incepand cu layer 3
		perror("Eroare la creare socket citire");
		return errno;
	}



	struct timeval readingTimeout;

	readingTimeout.tv_sec=0;
	readingTimeout.tv_usec=300000;


	if(setsockopt(receivingSocket, SOL_SOCKET, SO_RCVTIMEO, &readingTimeout, sizeof(readingTimeout))==-1){
		perror("setsockopt() error");
		return errno;
	}

	printf("OK: socket option SO_RCVTIMEO is set.\n");





	struct iphdr *ipHeader = (struct iphdr*)sendingBuffer;
	struct udphdr *udpHeader= (struct udphdr*) (sendingBuffer+sizeof(struct iphdr));
	char *date=(sendingBuffer+sizeof(struct iphdr)+sizeof(struct udphdr));
	struct icmphdr *icmpHeader;

	  // fabricate the IP header
  	ipHeader->ihl      = 5;
  	ipHeader->version  = 4;
  	ipHeader->tos      = 0; 
  	ipHeader->tot_len  = htons(sizeof(struct iphdr) + sizeof(struct udphdr));
  	ipHeader->id       = htons(rand());
	ipHeader->frag_off = htons(0x0);
  	ipHeader->ttl      = 0; // hops
  	ipHeader->protocol = 17; // UDP
  	ipHeader->saddr = inet_addr(IPsursa);
  	ipHeader->daddr = inet_addr(IPdest);



  	udpHeader->source = htons(portSursa);
  	udpHeader->dest = htons(portDest);
  	udpHeader->len = htons(lungimePachet-sizeof(struct iphdr));
	udpHeader->check = htons(0x0);




	struct sockaddr_in dest;

	dest.sin_family = AF_INET;
  	dest.sin_port = htons(portDest);
  	dest.sin_addr.s_addr = inet_addr(IPdest);

	struct sockaddr_in capturedSource;
	int capturedSourceSize=sizeof(capturedSource);

	int hopCount=0;
	int returnValue=0;

	char **stringuriDeAfisat=(char**)calloc(maxNumberOfHops, sizeof(char*));  //free
	for(int i=0;i<maxNumberOfHops;i++)
		*(stringuriDeAfisat+i)=(char*) calloc(150, sizeof(char));

	struct routerInfo *routerTable = calloc(50,sizeof(struct routerInfo)); // free


	//calculam al catelea router este destinatia

	/*
	for(int i=1;i<=30;i++){

		//updateUDPpacket(ipHeader, udpHeader, date);  
		updateUDPpacketv2(sendingBuffer);

		if(-1==sendto(sendingSocket, sendingBuffer, lungimePachet, 0, (struct sockaddr*) &dest, sizeof(dest))){
			perror("Eroare la send");
			return errno;
		}

		if(-1==recvfrom(receivingSocket, receivingBuffer, 290, 0, (struct sockaddr *)&capturedSource, &capturedSourceSize)){
			if(errno==11) // un router nu a raspuns cu ICMP
				continue;
			else{ //alta eroare
				perror("Eroare la recv");
				return errno;
			} 
		}

		returnValue=isEndHop(receivingBuffer);
		if(returnValue>0){   // a venit un pachet ICMP port unreachable ( deci unul din pachetele noastre a ajuns la destinatie). Dar daca a venit intai un altul decat primul
			hopCount=returnValue; // cu TTL suficient de mare pentru a ajunge la dest?
			
			break;
		}	
	}
	*/

	hopCount=computeNumberOfHops(sendingSocket, receivingSocket, sendingBuffer, lungimePachet, maxNumberOfHops);
	
	printf("nrRoutere=%d\n", hopCount);
	sendSegment(tcpSocket, (char*)&packet_count, sizeof(packet_count)); // trimitem nr. de pachete per router
	sendSegment(tcpSocket, (char*)&hopCount, sizeof(hopCount));   // trimitem nr. de routere gasite


	


	for(int k=1;k<=packet_count;k++){

	// resetam TTL-ul si portul pachetului
	resetUDPpacket(sendingBuffer, portDest);


	for(int i=1;i<=hopCount;i++){

		//updateUDPpacket(ipHeader, udpHeader, date);
		updateUDPpacketv2(sendingBuffer);

		if(-1==sendto(sendingSocket, sendingBuffer, lungimePachet, 0, (struct sockaddr*) &dest, sizeof(dest))){
			perror("Eroare la send");
			return errno;
		}

	}

	for(int i=1;i<=hopCount;i++){

		if(-1==recvfrom(receivingSocket, receivingBuffer, 290, 0, (struct sockaddr *)&capturedSource, &capturedSourceSize)){
			if(errno==11){ // un router nu a raspuns cu ICMP
				//printf("Unknown router\n");
				continue;
			}
			else{ //alta eroare
				perror("Eroare la recv");
				return errno;
			} 
			
		}

		processICMPv2(receivingBuffer, routerTable);

	}

	for(int i=1;i<=hopCount;i++){
		/*
		printf("%3d IP: %3d.%3d.%3d.%3d  Sent:%3hd  Last:%3hd  Avg:%3hd  Min:%3hd   Max:%3hd\n",i,routerTable[i].ip[0], routerTable[i].ip[1],
		 routerTable[i].ip[2], routerTable[i].ip[3], routerTable[i].totalSent, routerTable[i].last, 
	routerTable[i].avg, routerTable[i].min, routerTable[i].max );

	*/

	snprintf(stringuriDeAfisat[i-1], 145, "%3d IP: %3d.%3d.%3d.%3d  Recv:%3hd  Last:%3hd  Avg:%3hd  Min:%3hd  Max:%3hd\n",i,routerTable[i].ip[0], routerTable[i].ip[1],
		 routerTable[i].ip[2], routerTable[i].ip[3], routerTable[i].totalSent, routerTable[i].last, 
	routerTable[i].avg, routerTable[i].min, routerTable[i].max );


	}



	for(int i=0;i<hopCount;i++){
		sendSegment(tcpSocket, stringuriDeAfisat[i], 150);
		//printf("%s", stringuriDeAfisat[i]);
	}

	//printf("\n");

	}

	for(int i=0;i<maxNumberOfHops;i++)
		free(*(stringuriDeAfisat+i));
	free(stringuriDeAfisat);

	free(routerTable);
	return 0;
	

}

int processCommands(int socket){

	char commandBuffer[80];
	int opt;

	while(TRUE){
		recvSegment(socket, commandBuffer, 80);

		if(strncmp(commandBuffer, "quit", 4)==0)
			opt=1;
		else if(strncmp(commandBuffer, "start", 5)==0)
			opt=2;
		else
			opt=-1;


		switch(opt){
			case 1:{
				close(socket);
				return 0;
			}

			case 2:{
				mtr(socket);
				break;
			}

			default:
				continue;

		}	

	}

}

int main() {
	
	srand(time(NULL));


	int watchSocket;
	int spawnedSock;



	struct sockaddr_in wAddr;
    struct sockaddr_in clientAddr;
    int clientAddrSize=sizeof(clientAddr);
	int processID=1000;


    wAddr.sin_family=AF_INET;
    wAddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    wAddr.sin_port=htons(SERVER_PORT);
    memset(wAddr.sin_zero,0,sizeof(wAddr.sin_zero));



	if(-1==(watchSocket=socket(AF_INET, SOCK_STREAM, 0))){
		perror("Eroare la creare socket");
		return errno;
	}


	if(bind(watchSocket, (struct sockaddr *)&wAddr,sizeof(wAddr))<0)   // fixam socketul pe portul specificat in msAddress
    {
        perror("Eroare la bind\n");
        return errno;
    }
    printf("Watch socket la adresa %s pe portul %d.\n", inet_ntoa(wAddr.sin_addr), ntohs(wAddr.sin_port));

    listen(watchSocket,5);  // ascultam pentru conexiuni din partea clientilor
    printf("Listening...\n");

	

	while(TRUE){

		if(processID!=0){

			spawnedSock=accept(watchSocket,(struct sockaddr *) &clientAddr, &clientAddrSize);
        	printf("Accepted connection from %s port %d.\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        	processID=fork(); //generam un proces copil pentru a trata conexiunea cu fiecare client

        	if(processID!=0){
				// parinte
            	close(spawnedSock); 		
			}

			else{
				// copil
				close(watchSocket);

				processCommands(spawnedSock);


			}

		}

	}

	return 0;

}
