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

sem_t *sem;
int *available_proc;

void child_job(int queue_id, int shm_id, pid_t pid);

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

	sem_close(sem);
	sem_unlink("sem");
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

int create_queue(){

	key_t key = ftok(".", 'a');
	if(key == -1){

		error("Errore nella ftok queue\n");

	}

	int ret = msgget(key, IPC_CREAT | 0666);
	if(ret == -1){

		error("Errore nella msgget\n");

	}

	return ret;

}

int create_shared_mem(){

	key_t key = ftok(".", 'b');
	if(key == -1){

		error("Errore nella ftok shared memory\n");

	}

	int ret = shmget(key, sizeof(int), IPC_CREAT | 0666);
	if(ret == -1){

		error("Errore nella shmget\n");

	}

	return ret;

}

void write_queue(int queue_id, struct sockaddr_in addr, segmentPacket seg, char* ret){

	struct msgbuf msg;
	msg.s = addr;
	strcpy(msg.message, ret);
	size_t size = sizeof(struct sockaddr_in) + sizeof(int);
	msg.mtype = 1;
	msg.client_seq = seg.seq_no; 

	int msgret = msgsnd(queue_id, &msg, size, 0);
	if(msgret == -1){

		error("Errore in msgsnd \n");

	}

	char bufferone[512];

	inet_ntop(AF_INET, &msg.s, bufferone, 512);
	printf("Stampo la struttura %s\n", bufferone);

}
/*Crea ed inizializza il socket*/
void initialize_socket(int* sock_fd,struct sockaddr_in* s){

	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("errore in socket");

	if (bind(sockfd, (struct sockaddr *)s, sizeof(*s)) < 0)
		error("error in bind");
	*sock_fd = sockfd;

}


char stringa[128];

/*Ascolta le richieste dai client*/
char* listen_request(int sockfd, segmentPacket* seg, struct sockaddr_in* addr,socklen_t* len){

    struct sockaddr_in servaddr = *addr;
    socklen_t l = *len;
    l = sizeof(servaddr);

    int n = recvfrom(sockfd, stringa, sizeof(stringa), 0, (struct sockaddr *)&servaddr, &l);
         	

    if(n < 0){

         error("recvfrom\n");

     }

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

void prefork(int queue_id, int shm_id){

	pid_t pid;

	for(int i = 0; i < 10; i++){

		pid = fork();
		if(pid == -1){

			error("Errore nella fork\n");

		}else if(pid == 0){

			child_job(queue_id, shm_id, getpid());

		}

	}

	return; 

}

/*void new_fork(int queue_id, int shm_id, sem_t sem){

	pid_t pid;

	for(int i = 0; i < 5; i++){

		pid = fork();
		if(pid == -1){

			error("Errore nella fork()\n");

		}else if(pid == 0){

			available_proc = shmat(shm_id, NULL, 0);

			if(available_proc == (void*)-1){

				error("Errore nella shmat\n");

			}		

			if(sem_wait(sem) < 0){

					error("Errore nella semwait\n");

			}

			available_proc++;

			if(sem_post(sem) < 0){

				error("Errore nella sempost\n");

			}

			child_job(queue_id, shm_id, getpid(), "asdas");

		}

	}

}*/

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
      if(sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){

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
void manage_client(int sockfd, char* message, struct sockaddr_in* addr){

	struct sockaddr_in servaddr = *addr;

	if(strncmp(message, "put", 3) == 0){

		printf("Sono in put\n");

		int ret = check_existence(message + 4);
		if(ret == 0){

			printf("File does not exists, I can receive it\n");
			//get_file_server(message, sockfd, addr, msg.s);

		}

		printf("File already exists! No need to receive it again\n");
		exit(1);

	}

	else if((strncmp(message,"get",3) == 0)  ){

		printf("Sono in get\n");

		char cwd[512];
		getcwd(cwd, sizeof(cwd));

		int ret;

		ret = check_existence(message + 4);
		if(ret == 0){

			perror("File does not exists\n");
			exit(1);

		}

		send_file_server(message + 4,sockfd, servaddr);

	}else if(strncmp(message, "list", 4) == 0){

		printf("Sono in list\n");

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
void child_job(int queue_id, int shm_id, pid_t pid){

	printf("Sono in child job\n");

	struct sockaddr_in addr;
	int sockfd;
	segmentPacket seg;
	ACKPacket ackpkt;

	struct msgbuf msg;
	msg.mtype = 1;

	available_proc = shmat(shm_id, NULL, 0);
	if(available_proc == (void*)-1){

		error("Errore nella shmat\n");

	}	

	while(1){

		int ret = msgrcv(queue_id, &msg, sizeof(struct sockaddr_in) + sizeof(int) + 20, 1, 0);
		if(ret == -1){

			error("Errore in msgrcv\n");

		}

		printf("Stampo il messaggio da child %s\n", msg.message);

		sem_wait(sem);
		--available_proc;
		sem_post(sem);

		memset((void *)&addr,0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(0);

		initialize_socket(&sockfd,&addr);				//every child process creates a new socket

		seg.seq_no = -1;
		ackpkt.ack_no = msg.client_seq;

		if(sendto(sockfd, &ackpkt, sizeof(ackpkt), 0, (struct sockaddr *)&msg.s, sizeof(msg.s)) < 0){

			error("Errore nella sendto\n");

		}

		manage_client(sockfd, msg.message, &addr);

		sem_wait(sem);
		++available_proc;
		sem_post(sem);

	}

	exit(EXIT_SUCCESS);

}

int main(int argc, char **argv){

  (void) argc;
  (void) argv;
  int sockfd;
  socklen_t len;
  struct sockaddr_in addr;
  struct sigaction sa;
  segmentPacket seg;

  int queue_id, shm_id;

  handle_sigchild(&sa);		/*handle SIGHCLD to avoid zombie processes*/

  queue_id = create_queue();

  shm_id = create_shared_mem();

  sem = sem_open("sem", O_CREAT | O_EXCL, 0666, 1);
  if(sem == SEM_FAILED){

  	error("Errore in sem_open\n");

  }

  prefork(queue_id, shm_id);

  if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){		//create listen socket

    error("errore in socket");

  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVPORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){

     perror("errore in bind\n");
     exit(1);

  }

  while(1){

	char *ret = listen_request(sockfd, &seg, &addr, &len);

	write_queue(queue_id, addr, seg, ret);

	/*if(available_proc < 5){i

		new_fork(queue_id, shm_id);

	}*/
	  
  }

  wait(NULL);
  return 0;

}
