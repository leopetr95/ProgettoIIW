#include "configurations.h"
#include "basic.h"
#include "data_types.h"
#include "common.h"

#define BUFFERSIZE 1000
#define WINDOWSIZE 4
#define CHUNCK 512
#define TIMEOUT 3

int n_request = 0;
extern int n_win;
int adaptive;

/*definisce la struttura del pacchetto dati*/
typedef struct segmentPacket{

  int type;
  int seq_no;
  int length;
  char data[CHUNCK];

}segmentPacket;

/*definisce la struttura del pacchetto ack*/
typedef struct ACKPacket{

  int type;
  int ack_no;

}ACKPacket;

/*Crea e restituisce un pacchetto ack*/
struct ACKPacket createACKPacket (int ack_type, int base){
        struct ACKPacket ack;
        ack.type = ack_type;
        ack.ack_no = base;
        return ack;
}

/*Crea e restituisce un pacchetto dati*/
struct segmentPacket createDataPacket(int seqNO, int length, char* data){

  struct segmentPacket pkd;

  pkd.type = 1;
  pkd.seq_no = seqNO;
  pkd.length = length;
  memset(pkd.data, 0, sizeof(pkd.data));
  strcpy(pkd.data, data);

  return pkd;

}

/*Crea e restituisce l'ultimo pacchetto del flusso di dati*/
struct segmentPacket createFinalPacket(int seqNO, int length){

  struct segmentPacket pkd;

  pkd.type = 4;
  pkd.seq_no = seqNO;
  pkd.length = length;
  memset(pkd.data, 0, sizeof(pkd.data));

  return pkd;

}

/*Stabilisce la casualità nella perdita di pacchetti per la simulazione*/
int is_lost(float loss_rate) {
    double rv;
    rv = drand48();
    if (rv < loss_rate)
    {
        return(1);
    } else {
        return(0);
    }
}

/*Gestisce i segnali per evitare un numero eccessivo di processi zombie*/
void sighandler(int sign)
{
	(void)sign;
	int status;
	pid_t pid;

	--n_request;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0)
		;
	return;
}

void handle_sigchild(struct sigaction* sa)
{

    sa->sa_handler = sighandler;
    sa->sa_flags = SA_RESTART;
    sigemptyset(&sa->sa_mask);

    if (sigaction(SIGCHLD, sa, NULL) == -1) {
        fprintf(stderr, "Error in sigaction()\n");
        exit(EXIT_FAILURE);
    }
}

/*Invia la richiesta di connessione al server*/
int request_to_server(int sockfd,Header* x,struct sockaddr_in* addr, char *string){

    printf("Stampo il mio pid %d\n", getpid());

    int n;
    struct sockaddr_in s = *addr;
    struct timespec conn_time = {2,0};
    int attempts = 0;
    socklen_t len = sizeof(s);
    x->n_seq = generate_casual();

    char temp_buff[128];

    for(;;) {
        if (sendto(sockfd, string, 512, 0, (struct sockaddr *) &s, sizeof(s)) < 0) {
            printf("errore\n");
            perror("sendto\n");
            exit(EXIT_FAILURE);
        }

        printf("richiesta inviata\n");

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &conn_time, sizeof(conn_time)) < 0){
            perror("setsockopt failed\n");
            exit(EXIT_FAILURE);
        }
        n = recvfrom(sockfd,temp_buff,sizeof(temp_buff),0,(struct sockaddr*)&s,&len);
      if(n < 0){

        perror("Error while receiving from\n");
        exit(EXIT_FAILURE);

      }

      printf("printo n%d\n", n);

    	if(n < 0){
    		if(errno == EWOULDBLOCK){
    			printf("server not responding; trying again..\n");
    			++attempts;
    			if(attempts == 3)
    				break;
    			conn_time.tv_sec += conn_time.tv_sec;
    		}
    		else
    			perror("recvfrom");
    	}
    	if(n>0){

        printf("Stampo sta roba %s\n", temp_buff);
    		conn_time.tv_sec = 0;
    		conn_time.tv_nsec = 0;
        	if(setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&conn_time,sizeof(conn_time)) < 0){
                perror("setsockopt failed\n");
                exit(EXIT_FAILURE);
            }
    		printf("client connected!\n");
    	    *addr = s;
    	    return 1;
    	}
    }

    return 0;					/*not available server*/
}

/*Stampa il nome dei file richiesti tramite comando list*/
void list_file_client(int sockfd, struct sockaddr_in* serv){

  struct sockaddr_in s = *serv;
  socklen_t len = sizeof(s);

  printf("I am in listfileclient\n");

  char buffer[BUFFERSIZE];

  int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&s, &len);
  if(n < 0){

    perror("Error while receving from socket\n");
    exit(EXIT_FAILURE);

  }

  printf("Printing received data\n");
  printf("%s\n", buffer);

  return;

}


/*Ottiene il file richiesto dal server tramite il comando get*/
void get_file_client(int sockfd, char* comm, struct sockaddr_in* servaddr, int loss_rate){

  char dataBuffer[8192];
  char test[8192];
  int base = -2;
  int seqNum = 0;
  socklen_t len = sizeof(servaddr);


  printf("let me first print the name of the required file %s\n", comm+4);

  FILE* file = fopen(comm+4, "w+");
  if(file == NULL){

    perror("Error while opening file\n");
    exit(EXIT_FAILURE);

  }


  while(1){

    struct segmentPacket dataPacket;
    struct ACKPacket ack;
    
    int recvMsgSize = recvfrom(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&servaddr, &len);
    if(recvMsgSize < 0){

      perror("Error while receiving from\n");
      exit(EXIT_FAILURE);


    }
    printf("ciaone\n");

    if(!is_lost(loss_rate)){

      if(dataPacket.seq_no == 0 && dataPacket.type == 1){

        memset(dataBuffer, 0, sizeof(dataBuffer));

        fprintf(file, "%s", dataPacket.data);

        strcpy(dataBuffer, dataPacket.data);
        base = 0;
        ack = createACKPacket(2, base);

      }else if(dataPacket.seq_no == base + 1){

        printf("Received subsequent packet #%d\n", dataPacket.seq_no);
        printf("Stampo quello che ho ricevuto\n %s\n", dataPacket.data);

        strcpy(test, dataPacket.data);

        fprintf(file, "%s", test);

        strcat(dataBuffer, dataPacket.data);

        base = dataPacket.seq_no;
        ack = createACKPacket(2, base);

      }else if(dataPacket.type == 1 &&dataPacket.seq_no != base +1){

        printf("Received out of sync packet #%d\n", dataPacket.seq_no);
        ack = createACKPacket(2, base);

      }

      if(base >= 0){

        printf("------------ Sending ACK #%d\n", base);
        if(sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))!= sizeof(ack)){

          perror("Error while sending to\n");
          exit(EXIT_FAILURE);

        }


      }else if(base == -1){

        printf("Received teardown packet\n");
        printf("Sending terminale ack\n",base);

        if(sendto(sockfd, &ack , sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))!= sizeof(ack)){

          perror("Error while sending to\n");
          exit(EXIT_FAILURE);

        }

      }

      if(dataPacket.type == 4 && base == -1){


        printf("\nMESSAGE RECIEVED \n %s\n\n", dataBuffer);
        memset(dataBuffer, 0, sizeof(dataBuffer));
        fclose(file);

      }


    }else{

      printf("SIMULATED LOSE\n");

    }

  }

}


/*Invia il file desiderato al server tramite il comando put*/
void send_file_client(char *filename, int sockfd, struct sockaddr_in servaddr){

  printf("Sono dentro a send_file_server\n");

  //opening file
  FILE* file = fopen(filename, "r");
  if(file == NULL){

    perror("Error while opening file\n");
    exit(EXIT_FAILURE);

  }

  int tries = 0;


  //getting file size
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);

  //back to the beginning of the file
  fseek(file, 0L, SEEK_SET);

  int numberOfSegments = size / CHUNCK;

  //if there are leftovers
  if(size % CHUNCK > 0){

    numberOfSegments++;

  }

  //setting window parameter
  int base = -1;  //highest segment ACK received
  int seqNum = 0; //highest segment sent, reset by base
  int dataLenght = 0; //chunck size
  int windowSize = WINDOWSIZE;
  unsigned int fromSize;

  int noTearDownAck = 1;

  while(noTearDownAck){

    //send packets from base up to window size
    while(seqNum <= numberOfSegments && (seqNum - base) <= windowSize){

      struct segmentPacket dataPacket;

      if(seqNum == numberOfSegments){

        dataPacket = createFinalPacket(seqNum, 0);
        printf("Sending final packet\n");

      }else{

        char data[CHUNCK];
        fread(data, CHUNCK, 1, file);

        dataPacket = createDataPacket(seqNum, dataLenght, data);
        printf("Sending packet: %d\n", seqNum);

      }

      if(sendto(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))<0){

        perror("Error while sending packet\n");
        exit(EXIT_FAILURE);

      }

      seqNum++;

    }

    alarm(TIMEOUT);

    int respStringlen;

    printf("Window full: waiting for acks\n");

    struct ACKPacket ack;

    while((respStringlen = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&servaddr, &fromSize)) < 0){

      if(errno == EINTR){

        seqNum = base + 1;

        printf("Timeout: resending\n");
        if(tries >= 10){

          printf("Tries exceeded: Closing\n");
          exit(1);

        }else{

          alarm(0);

          while(seqNum <= numberOfSegments &&(seqNum - base) <= windowSize){

            struct segmentPacket dataPacket;

            if(seqNum == numberOfSegments){

              dataPacket = createFinalPacket(seqNum, 0);
              printf("Sending final packet");

            }else{

              char data[CHUNCK];
              fread(data, CHUNCK, 1, file);

              dataPacket = createDataPacket(seqNum, dataLenght, data);
              printf("Sending packet: %d\n", seqNum);


            }

            if(sendto(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))<0){

              perror("Error while sending to socket\n");
              exit(EXIT_FAILURE);

            }

            seqNum++;


          }

          alarm(TIMEOUT);


        }

        tries++;


      }else{

        perror("Error while recvrom\n");
        exit(EXIT_FAILURE);

      }


    }

    if(ack.type != 8){

      printf("Received ack: %d\n", ack.ack_no);
      if(ack.ack_no > base){

        base = ack.ack_no;

      }

    }else{


      printf("Received terminal ack\n");
      noTearDownAck = 0;

    }

    alarm(0);
    tries = 0;

  }

  close(sockfd);
  exit(0);


}


int main(int argc, char *argv[]) {

  int sockfd;
  struct sockaddr_in   servaddr;
  Header p;
  struct sigaction sa;

  handle_sigchild(&sa);


  if (argc < 3) {
    fprintf(stderr, "utilizzo: daytime_clientUDP <indirizzo IP  server> lossrate\n");
    exit(1);
  }

  int lossrate = atof(argv[2]);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket   */
	  perror("socket");
      exit(EXIT_FAILURE);
  }

   memset((void *)&servaddr, 0, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons(SERVPORT);

   if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) < 0) {
        perror("error in inet_pton for %s");
        exit(EXIT_FAILURE);

   }


  pid_t pid;
  char* line;

  /*************************************************************
   * father process waits command from standard input; then,   *
   * creates a new process to execute. If CTRL + D received,   *
   * waits all children; then, father terminates.               *
   *************************************************************/



  while(feof(stdin) == 0){
	  //ssize_t len_line;
	  char comm[512];

	  printf("write command\n");
	  line = read_from_stdin();

	  if(line == NULL){
		  wait(NULL);
		  printf("terminated all request; closing connection\n");
		  break;
	  }

	  pid = fork();

	  if(pid != 0)
		  continue;

	  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* create new socket   */
		   perror("socket");
		   exit(EXIT_FAILURE);

      }

	  if(!request_to_server(sockfd,&p,&servaddr, line)){
		  printf("not available server\n");
		  exit(EXIT_SUCCESS);
	  }

	  strcpy(comm, line);
	  comm[strlen(comm)] = '\0';

	  if(strncmp(comm, "put", 3) == 0){

		  if(!existing_file(line+4,"./clientDir/")){
        
			  printf("not existing file\n");
			  break;

		  }
		  //send_file_client(sockfd,line,p,servaddr);
		  break;
	  }

	  else if((strncmp(comm,"get",3) == 0)){

		  get_file_client(sockfd,line,&servaddr, lossrate);
		  break;

	  }else if(strncmp(comm, "list", 4) == 0){

	  	list_file_client(sockfd, &servaddr);
      printf("io sto qua\n");
	  	break;

	  }


	  else{

	  		fprintf(stderr, "command not recognize. USE COMMAND  LIST or GET/PUT FOLLOWEWD BY A PROPER FILE NAME \n");
	  		break;

	  }


      }
      printf("closing connection\n");

      exit(EXIT_SUCCESS);
}
