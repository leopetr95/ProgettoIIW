#include "basic.h"
#include "configurations.h"
#include "data_types.h"
#include "common.h"
#include "thread_functions.h"
#include "timer_functions.h"
#include "packet_functions.h"
#include "window_operations.h"

#define BUFFER_SIZE 1200

#define CHUNCK 512
#define WINDOWSIZE 4
#define TIMEOUT 3
extern int n_win;
int adaptive;

/*Definisce la struttura del pacchetto dati*/
typedef struct segmentPacket{

	int type;
	int seq_no;
	int length;
	char data[CHUNCK];

}segmentPacket;

/*Definisce la struttura del pacchetto ack*/
typedef struct ACKPacket{

	int type;
	int ack_no;

}ACKPacket;

/*Gestisce i segnali per evitare un numero eccessivo di processi zombie*/
void sighandler(int sign)
{
	(void)sign;
	int status;
	pid_t pid;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0)
		;
	return;
}

void handle_sigchild(struct sigaction* sa){

	sa->sa_handler = sighandler;
	sa->sa_flags = SA_RESTART;
	sigemptyset(&sa->sa_mask);

	if (sigaction(SIGCHLD, sa, NULL) == -1) {
		fprintf(stderr, "Error in sigaction()\n");
		exit(EXIT_FAILURE);
	}
}


/* Stabilisce la casualità nella perdita di pacchetti per la simulazione*/
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

/*Crea ed inizializza il socket*/
void initialize_socket(int* sock_fd,struct sockaddr_in* s){

	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err_exit("errore in socket");

	if (bind(sockfd, (struct sockaddr *)s, sizeof(*s)) < 0)
		err_exit("error in bind");
	*sock_fd = sockfd;

}


char stringa[128];

/*Ascolta le richieste dai client*/
char* listen_request(int sockfd, struct sockaddr_in* addr,socklen_t* len){

    struct sockaddr_in servaddr = *addr;
    socklen_t l = *len;
    l = sizeof(servaddr);
    printf("listening request\n");

    int n = recvfrom(sockfd, stringa, 128, 0, (struct sockaddr *)&servaddr, &l);
    printf("Stampo n: %s\n", stringa);
         	
    if(n < 0){

         err_exit("recvfrom\n");

     }

    printf("Something received\n");

    char *ack = "ACK";

    n = sendto(sockfd, ack, sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if(n < 0){

    	err_exit("sendto\n");
    	exit(1);

    }

    printf("Io qualcosa ho mandato: %d\n", n);

    *addr = servaddr;
    *len = l;
    return stringa;
}

/*Invia la lista dei nomi dei file presenti nella cartella server*/
char* list_file_server(){

	FILE* proc = popen("ls", "r");
	if(proc == NULL){

		perror("Error while popen\n");
		exit(1);

	}

	int c;
	int i = 0;
	char *buff = malloc(BUFFER_SIZE*sizeof(char));

	while(( c = fgetc(proc)) != EOF && i < BUFFER_SIZE){

		buff[i++] = c;

	}

	buff[i] = 0;
	pclose(proc);

	return buff;

}

int check_existence(char* filename){

	if(access(filename, F_OK)!= 0){

		return 0;

	}

	return 1;

}

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

/*Crea e restituisce il pacchetto finale del flusso di dati*/
struct segmentPacket createFinalPacket(int seqNO, int length){

	struct segmentPacket pkd;

	pkd.type = 4;
	pkd.seq_no = seqNO;
	pkd.length = length;
	memset(pkd.data, 0, sizeof(pkd.data));

	return pkd;

}

/*Invia al client il file richiesto tramite comando get*/
void send_file_server(char *filename, int sockfd, struct sockaddr_in servaddr){

	printf("Sono dentro a send_file_server\n");

	//opening file
	FILE* file = fopen(filename, "r");
	if(file == NULL){

		perror("Error while opening file\n");
		exit(1);

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
	int base = -1;	//highest segment ACK received
	int seqNum = 0;	//highest segment sent, reset by base
	int dataLenght = 0;	//chunck size
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

				printf("Stampo quello che ho letto dal file: \n%s\n", data);

				dataPacket = createDataPacket(seqNum, dataLenght, data);
				printf("Sending packet: %d\n", seqNum);

			}

			if(sendto(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))<0){

				perror("Error while sending packet\n");
				exit(1);

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
							exit(1);

						}

						seqNum++;

					}

					alarm(TIMEOUT);

				}

				tries++;

			}else{

				perror("Error while recvrom\n");
				exit(1);

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

	printf("File sent correctly\n");

	close(sockfd);
	exit(0);

}

/*Riceve il file dal client tramite il comando put*/
void get_file_server(int sockfd, char* comm, struct sockaddr_in *servaddr, int loss_rate){


  FILE* file;

  char data[8192];
  int base = -2;
  int seqNum = 0;
  
  segmentPacket dataPacket;

  ACKPacket ack;

  unsigned int length;

  int n = recvfrom(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&servaddr, &length);
  if(n < 0){

    perror("Error while receiving from\n");
    exit(1);

  }  

  seqNum = dataPacket.seq_no;

  if(!is_lost(loss_rate)){

    if(dataPacket.seq_no == 0 && dataPacket.type == 1){

      memset(data, 0, sizeof(data));
      strcpy(data, dataPacket.data);
      base = 0;
      ack = createACKPacket(2, base);

    }else if(dataPacket.seq_no == base + 1){

      printf("Received subsequent packet %d\n", dataPacket.seq_no);
      strcat(data, dataPacket.data);
      base = dataPacket.seq_no;
      ack = createACKPacket(2, base);

    }else if(dataPacket.type == 1 &&dataPacket.seq_no != base + 1){

      printf("Received out of sunc packet %d\n", dataPacket.seq_no);
      ack = createACKPacket(2, base);


    }


    if(dataPacket.type == 4 && seqNum == base){

      base = -1;
      ack = createACKPacket(8, base);

    }

    if(base >= 0){

      printf("Sending ack %d\n", base);
      if(sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){

        perror("Error while sending to socket\n");
        exit(1);


      }

    }else if(base == -1){

      printf("Received TearDown Packet\n");
      printf("Sendint Terminal ACK\n");
      if(sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr))<0){

        perror("Error while sending to socket\n");
        exit(1);


      }


    }

    if(dataPacket.type == 4 && base == -1){

      file = fopen("prova.txt", "w+");
      if(file == NULL){

        perror("Error while opening file\n");
        exit(1);

      }

      fwrite(data, sizeof(data), 1, file);
      printf("Message received\n");
      memset(data, 0, sizeof(data));

    }

  }else{

    printf("Simulated lose\n");

  }

}



/*Gestisce le richieste dei client in base al comando inserito*/
void manage_client(int sockfd, char* string, struct sockaddr_in* addr){

	struct sockaddr_in servaddr = *addr;

	printf("Il mio pid è: %d\n", getpid());
	printf("The string is %s\n", string);

    char comm[30];

    strcpy(comm, string);

	if(strncmp(comm, "put", 3) == 0){

		int ret = check_existence(comm + 4);
		if(ret == 0){

			printf("File does not exists, I can receive it\n");
			//get_file_server(comm,sockfd,r,msg.s);

		}

		printf("File already exists! No need to receive it again\n");
		exit(1);

	}

	else if((strncmp(comm,"get",3) == 0)  ){

		char cwd[512];
		getcwd(cwd, sizeof(cwd));

		int ret;

		ret = check_existence(comm+4);
		if(ret == 0){

			perror("File does not exists\n");
			exit(1);

		}

		send_file_server(comm + 4,sockfd, servaddr);

	}else if(strncmp(comm, "list", 4) == 0){

		printf("hi\n");

		char buff[BUFFER_SIZE];
		strcpy(buff,list_file_server());

		sleep(10);

		int n;
		n = sendto(sockfd, buff, sizeof(buff), 0, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
		if(n < 0){

			perror("Error while sending roba to client\n");
			exit(1);

		}

	}

	printf("end request\n");

}

/*Effettua le operazioni relative al processo server figlio*/
void child_job(pid_t pid, char* string){

	(void)pid;
	struct sockaddr_in addr;
	int sockfd;

	memset((void *)&addr,0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(5000);

	initialize_socket(&sockfd,&addr);				//every child process creates a new socket

	manage_client(sockfd, string, &addr);

	exit(EXIT_SUCCESS);

}

int main(int argc, char **argv){

  (void) argc;
  (void) argv;
  int sockfd;
  socklen_t len;
  struct sockaddr_in addr;
  struct sigaction sa;

  handle_sigchild(&sa);					/*handle SIGHCLD to avoid zombie processes*/

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)		//create listen socket
      err_exit("errore in socket");

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVPORT);
  addr.sin_addr.s_addr = INADDR_ANY;

   if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
     perror("errore in bind\n");
     exit(1);
   }

  while(1){

	  char *ret = listen_request(sockfd,&addr,&len);
	  printf("Stampo: %s\n", ret);
	  if(ret != NULL){

	  	printf("Ciaone\n");
	  	pid_t pid;
	  	pid = fork();

	  	if(pid == 0){

	  		printf("sono il figlio con pid %d\n", getpid());
	  		child_job(getpid(), ret);
	  		break;

	  	}else if(pid < 0){

	  		perror("Error while creating process\n");
	  		exit(1);

	  	}


	  }
	  
  }

  wait(NULL);
  return 0;
}
