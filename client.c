#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <errno.h>


#define SERVER_PORT 22000
#define TRUE 1


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

void mtr(int socket){

	int nr_pachete_per_router;
	int nrRoutere;
	char stringBuffer[150];

	recvSegment(socket, (char*)&nr_pachete_per_router, sizeof(int));
	recvSegment(socket, (char*)&nrRoutere, sizeof(int));

	for(int i=0;i<nr_pachete_per_router;i++){  
		for(int j=0;j<nrRoutere;j++){
			recvSegment(socket, stringBuffer, 150);
			printf("%s", stringBuffer);
		}
		//printf("\n");

		printf("\033[%dA", nrRoutere);  // mutam cursorul cu nrRoutere linii mai sus ca sa suprascriem datele pe ecran
	}

	printf("\033[%dB", nrRoutere + 1); // cand terminam mutam inapoi in jos pentru a continua cu alte comenzi
		


}



void clear(){

	printf("\033[2J"); //clear screen
	printf("\033[2J"); //clear screen
	printf("<\033[;H"); // cursor in stanga sus

}



int main(){

	int sock;

	char comanda[80];
	int opt;
	struct sockaddr_in serverAddr;

    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    serverAddr.sin_port=htons(SERVER_PORT);
    memset(serverAddr.sin_zero,0,sizeof(serverAddr.sin_zero));

    


	if(-1==(sock=socket(AF_INET, SOCK_STREAM, 0))){
		perror("Eroare la creare socket");
		return errno;
	}


	if(connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr))<0){
        perror("Eroare la connect");
        return errno;
    }

	printf("Connected to server at %s on port %d.\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

	while(TRUE){
		printf(">");
		fgets(comanda,80,stdin);


		if(strncmp(comanda, "quit", 4)==0)
			opt=1;
		else if(strncmp(comanda, "start", 5)==0)
			opt=2;
		else if(strncmp(comanda, "clear", 5)==0)
			opt=3;
		else
			opt=-1;


		switch(opt){
			case 1:{
				sendSegment(sock, comanda, strlen(comanda)+1);
				close(sock);
				return 0;
			}

			case 2:{
				sendSegment(sock, comanda, strlen(comanda)+1);
				mtr(sock);
				break;
			}

			case 3:{
				clear();
				break;
			}

			default:
				printf("Bad command\n");

		}


	}

	return 0;

}