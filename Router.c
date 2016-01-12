/*
 ============================================================================
 Name        : ASD.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */




#include <netdb.h>
#include <arpa/inet.h>//for using the socket
#include <sys/socket.h>//Creating a socket
#include<pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#define  INFINITY 10000
//the neighbors
typedef struct {
	char * to;
	int cost;
	int port;
}neighbors;


//the Dv
typedef struct {
	int to;
	int cost;
	int via;
}routers;


//the main vector
typedef struct {
	routers * rout;//all routers
	neighbors * neighbor;
	int  myPort;//ports of nebrhood
	int numOfRouters;
	int numOfNebrhood;
	char * ip;

}belmman;



//the information for the server and client
typedef struct {
	int port;
	int argv3;
	int routerId;
	int Dvchange;// 0 -> yes.   -1 -> no
	int flag1;
	pthread_mutex_t *lock;
	pthread_cond_t *cv1;
	pthread_cond_t *cv2;
	int *ndone1;//for server
	int *ndone2;//for client
}myDetails;


typedef struct {
	int flag;
	int * dvFromNeb;
	int routerId;
	int relaxFinish;
	int servFinish;

}relaxDetails;


belmman* instal(char * nativ,char * head);//install from file the routers information
void begginnigBelman(char * head);//the first Dv change [if] u are n Neighborhood the cost is the edge [else] is INFINITY
char* getStrFromFile(FILE* fd);//getStrFromFile
void printall ();//print the answer
void *createServer(void* td);
void *createClient(void* td);
int  getPort (char * name,int port);//get the port for open server
void error(const char *msg);//error msg
void relaxPower(int * a,int routerId);//help to relax function
void *relax(void* rd);
void weChangeDv();//if the server ChangeDv
void finishClient();//if all the server finish the client we not enter wait --- pthread_cond_wait(cv2,lock) ---
void installThread(int argv3,char * head);//install the thread struct that they go we him
void goThreads();//connecting the thread
void freeAllMalloc(char * head);
////////////////////Elkana David !!!!!!!!!!!



belmman* belm;//the main vector
myDetails  * my;//the information for the server and client
relaxDetails * relaxDet;
int nThread=0;//the number of numOfNebrhood/2
int finishServer=0;//counter the Server are finish, its help to wake the client
int i=0, ndone1, ndone2;
pthread_mutex_t lock;
pthread_cond_t cv1;
pthread_cond_t cv2;
int flaggggg=0;
int main(int argc, char** argv) {


	if (argc!=4){
		printf("please insert in this follow (%s) (file Name) (Router Name) (time to live the thread)",argv[0]);
		exit(1);
	}
	char * head=malloc(sizeof(char)*8);
	strcpy(head,argv[2]);
	char * nativ=argv[1];



	belm=instal(nativ,head);
	begginnigBelman(head);





	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cv1, NULL);
	pthread_cond_init(&cv2, NULL);
	ndone1 = 0;
	ndone2 = 0;


	installThread(atoi(argv[3]),head);

	goThreads();

	printall ();



	freeAllMalloc(head);

	return EXIT_SUCCESS;

}



int  getPort (char * name,int port){
	return 641+name[6]+port;
}


void finishClient(){
	int i;
	for (i=0;i<belm->numOfNebrhood*2;i++){
		my[i].flag1=-1;

	}
}

void weChangeDv(){
	int i;
	for (i=0;i<belm->numOfNebrhood*2;i++){

		my[i].Dvchange=0;

	}
}

void *createClient(void* td){

	//CASTING
	myDetails * ts;
	ts = (myDetails *) td;

	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	server = gethostbyname(belm->ip);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(ts->port);

	//NUMBER OF WAITING TO CONNECT
	int ttl=0;
	while(ttl<ts->argv3){
		if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
			sleep(1);
			ttl++;
		}
		else
			break;
	}

	int flag=0;//SAY TO SEND THE DV
	int j;
	int arraySender[belm->numOfRouters];
	while(1)
	{


		// -FIRT TIME- WE SEND THE DV ARRAY, AND WE DONT WAIT FOR SIGNAL
		if (flag==0){
			arraySender[0]=1;
			for (j=1;j<belm->numOfRouters;j++){
				arraySender[j]=belm->rout[j].cost;
			}
			write(sockfd,arraySender,sizeof(arraySender));

		}

		flag=1;

		//HERE THE SERVER SHOULD WAKE US
		if (ts->flag1==0){
			pthread_cond_wait(ts->cv2, ts->lock);
			pthread_mutex_unlock(ts->lock);
		}


		//IF WAS CHANGE WE NEED SEND AGAIN
		if (ts->Dvchange==0){
			flag=0;
			ts->Dvchange=-1;
		}


		//IF NO CHANGE WE FINISH
		else{
			arraySender[0]=0;
			write(sockfd,&arraySender[0],sizeof(int));
			break;

		}
	}

	close(sockfd);
	return 0;
}



void *createServer(void* td){
	myDetails * ts;
	ts = (myDetails *) td;
	int sock1,sock2;
	socklen_t client_len;
	sock1 = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in serv,cli;
	serv.sin_family = AF_INET;
	serv.sin_port = htons(ts->port);
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(sock1,(struct sockaddr *)&serv, sizeof(serv));
	listen(sock1,5);
	client_len = sizeof(cli);
	sock2 = accept(sock1,(struct sockaddr *)&cli,&client_len);
	int n,i;
	int a [belm->numOfRouters];


	while(1){
		n=read(sock2,a,sizeof(a));

		//////////IF ARRAY FROM NEBRHOOD WAS CHANGE
		if (a[0]==1){
			pthread_mutex_lock(ts->lock);//WAIT TO ALL BE COME
			*ts->ndone1 = *ts->ndone1 + 1;
			if (*ts->ndone1 < nThread-finishServer) {
				pthread_cond_wait(ts->cv1, ts->lock);
			}

			else {
				for (i = 1; i < nThread-finishServer; i++){
					pthread_cond_signal(ts->cv1);//THE LAST WAKE ALL SLEEP

				}
				*ts->ndone1 =0;
			}


			//////// WAITING FOR CALCULATOR
			relaxDet->relaxFinish=-1;
			relaxPower(a,ts->routerId);
			while(relaxDet->relaxFinish==-1);




			pthread_mutex_unlock(ts->lock);

		}



		//////////// WAKE UP THE CLIENT (SENDERS)
		pthread_mutex_lock(ts->lock);
		*ts->ndone2 = *ts->ndone2 + 1;
		if (*ts->ndone2==nThread){
			for (i=0;i<nThread;i++)
				pthread_cond_signal(ts->cv2);
			*ts->ndone2=0;
		}




		pthread_mutex_unlock(ts->lock);


		//IF WE GOT 0 MUST BE N=4 SO WE FINISH
		if(a[0]==0 && n==4){
			pthread_mutex_lock(ts->lock);


			finishServer++;
			///////////////  IF FINISH SO WE TELL TO CLIENT TO FINISH AND TO THE CALCULATOR TO FINISH
			if (finishServer==nThread){
				relaxDet->servFinish=0;
				for (i=0;i<nThread;i++)
					pthread_cond_signal(ts->cv2);
				finishClient();
			}


			pthread_mutex_unlock(ts->lock);
			//Extreme case
			pthread_cond_signal(ts->cv1);
			break;

		}

	}
	close(sock2);
	close(sock1);
	return 0;
}









char* getStrFromFile(FILE* fd){

	char* line;
	char c;
	int i = 0;

	//count the length of the correct line
	while((c = fgetc(fd)) != '\n'){
		if (c==' ')
			break;
		if(c == EOF)
			return "EOF";
		i++;
	}
	if((line = (char*)malloc((i + 3) * sizeof(char))) == NULL)
		return NULL;

	fseek(fd, -1 * (i + 1), SEEK_CUR);
	fgets(line, i + 1, fd);
	fgetc(fd);


	return line;
}

void printall (){
	int i;
	for (i=1;i<belm->numOfRouters;i++){
		printf("\nRouter%d   ",i);
		printf("%d   ",belm->rout[i].cost);
		printf("Router%d   ",belm->rout[i].via);
		printf("\n");
	}
	printf("\n");
}



belmman* instal(char * nativ,char * head){
	FILE *fp;
	char  * buff;
	char * before=malloc (sizeof(char*));
	fp = fopen(nativ, "r");
	belmman * belm=(belmman*)malloc(sizeof(belmman));
	belm->numOfNebrhood=0;
	belm->numOfRouters=0;
	belm->neighbor=(neighbors*)malloc(sizeof(neighbors));
	//neighbors* s;
	int i=0;
	int flag=0;

	buff = getStrFromFile(fp);
	while (1){


		//numbers of routers
		if (i==0){
			belm->numOfRouters=atoi(buff);
			belm->numOfRouters++;
			belm->rout=(routers *)malloc((belm->numOfRouters)*sizeof(routers));
		}

		//get the neighborhood the name and the cost
		if (strcmp(head,buff)==0 && flag==0){

			if (before[0]=='R'){//if like "Router[i] head cost"
				belm->numOfNebrhood++;
				belm->neighbor=(neighbors *)realloc(belm->neighbor,(belm->numOfNebrhood)*sizeof(neighbors));
				belm->neighbor[belm->numOfNebrhood-1].to=(char *)malloc((strlen(before)+1)*sizeof(char));
				strcpy(belm->neighbor[belm->numOfNebrhood-1].to,before);
				free(buff);
				buff = getStrFromFile(fp);
				belm->neighbor[belm->numOfNebrhood-1].cost=atoi(buff);
			}
			else{//if like "head Router[i] cost"
				free(buff);
				buff = getStrFromFile(fp);
				if (buff[0]=='R' ){
					belm->numOfNebrhood++;
					belm->neighbor=(neighbors *)realloc(belm->neighbor,(belm->numOfNebrhood)*sizeof(neighbors));
					belm->neighbor[belm->numOfNebrhood-1].to=(char *)malloc((strlen(buff)+1)*sizeof(char));
					strcpy(belm->neighbor[belm->numOfNebrhood-1].to,buff);
					free(buff);
					buff = getStrFromFile(fp);
					belm->neighbor[belm->numOfNebrhood-1].cost=atoi(buff);
				}
			}
		}


		//get the ports of neighborhood
		while(flag==1){
			//get the port and ip of the head (argv[2])
			if (strcmp(head,buff)==0){
				free(buff);
				buff = getStrFromFile(fp);
				if (buff[0]!='R' && buff[3]=='.'){
					belm->ip=(char *)malloc((strlen(buff)+1)*sizeof(char));
					strcpy(belm->ip,buff);
					free(buff);
					buff = getStrFromFile(fp);
					belm->myPort=atoi(buff);
				}
			}
			//get the port of the neighborhood
			for(i=0;i<belm->numOfNebrhood;i++){
				if (strcmp(belm->neighbor[i].to,buff)==0){
					free(buff);
					buff = getStrFromFile(fp);
					if (buff[0]!='R' && buff[3]=='.'){
						free(buff);
						buff = getStrFromFile(fp);
						belm->neighbor[i].port=atoi(buff);
					}
				}
			}



			free(buff);
			buff = getStrFromFile(fp);
			if (strcmp(buff, "EOF")==0){
				i=-1;
				free(before);
				break;
			}
		}

		if (i<0)
			break;


		free(before);
		before=(char*)malloc((strlen(buff)+1)*sizeof(char));
		strcpy(before,buff);
		free(buff);
		buff = getStrFromFile(fp);

		//start in the beginning to get the ports
		if (strcmp(buff, "EOF")==0 && flag==0){
			flag=1;
			fseek(fp,0,SEEK_SET);
			buff = getStrFromFile(fp);
		}
		i++;
	}
	fclose(fp);
	return belm;

}

void begginnigBelman(char * head){

	int i,j;
	for (i=1;i<belm->numOfRouters;i++){
		belm->rout[i].cost=-1;
		//if its neighborhood
		for (j=0;j<belm->numOfNebrhood;j++){
			if (belm->neighbor[j].to[6]-'0'==i){
				belm->rout[i].cost=belm->neighbor[j].cost;
				belm->rout[i].via=i;
			}
		}
		//if its me
		if (i==head[6]-'0'){
			belm->rout[i].cost=0;
			belm->rout[i].via=i;
		}

		//if its no neighborhood
		if (belm->rout[i].cost==-1){
			belm->rout[i].cost=INFINITY;
			belm->rout[i].via=-1;
		}
	}

}


void error(const char *msg)
{
	perror(msg);
	exit(0);
}



void relaxPower(int * a,int routerId){
	int i;
	for (i=0;i<belm->numOfRouters;i++){
		relaxDet->dvFromNeb[i]=a[i];
	}

	relaxDet->routerId=routerId;
	relaxDet->flag=0;



}


void *relax(void* rd){
	int i,power;
	while(1){
		if (relaxDet->flag==0){
			for (i=0;i<belm->numOfNebrhood;i++){
				if (belm->neighbor[i].to[6]-'0'==relaxDet->routerId){
					power=belm->neighbor[i].cost;
				}
			}
			for (i=1;i<belm->numOfRouters;i++){
				if (belm->rout[i].cost>relaxDet->dvFromNeb[i]+power){
					belm->rout[i].cost=relaxDet->dvFromNeb[i]+power;
					belm->rout[i].via=relaxDet->routerId;
					weChangeDv();
				}
			}

			relaxDet->flag=-1;
			relaxDet->relaxFinish=0;

		}

		if (finishServer==nThread || relaxDet->servFinish==0){
			break;
		}


	}





	return 0;

}
void freeAllMalloc(char * head){

	for (i=0;i<belm->numOfNebrhood;i++){
		free(belm->neighbor[i].to);
	}

	free(belm->neighbor);
	free(belm->rout);
	free(belm->ip);
	free(belm);
	free(my);
	free(relaxDet->dvFromNeb);
	free(relaxDet);
	free(head);
}



void goThreads(){

	int j=belm->numOfNebrhood;
	pthread_t thread[belm->numOfNebrhood*2];
	pthread_t relaxThr;


	///////////////////* relax*//////////////
	pthread_create(&relaxThr,NULL,relax,NULL);



	///////////////////* client*//////////////
	for(i=j;i<belm->numOfNebrhood*2;i++)
		pthread_create(&thread[i],NULL,createClient,&my[i]);

	///////////////////* servers*//////////////
	for(i=0;i<belm->numOfNebrhood;i++)
		pthread_create(&thread[i],NULL,createServer,&my[i]);






	///////////m////////* pthread_join*//////////////
	for (i=0;i<belm->numOfNebrhood*2;i++)
		pthread_join(thread[i],NULL);
	pthread_join(relaxThr,NULL);

}




void installThread(int argv3,char * head){
	int i;
	int j=belm->numOfNebrhood;
	nThread=belm->numOfNebrhood;

	my  =(myDetails*)malloc((belm->numOfNebrhood*2)*sizeof(myDetails));


	//for server
	for (i=0;i<belm->numOfNebrhood;i++){
		my[i].port=getPort(belm->neighbor[i].to,belm->myPort)  ;
		my[i].routerId=belm->neighbor[i].to[6]-48;


	}
	//for client
	for (i=j;i<belm->numOfNebrhood*2;i++){
		my[i].port=getPort(head,belm->neighbor[i-j].port) ;
		my[i].argv3 = argv3;
	}

	//for server and client
	for (i=0;i<belm->numOfNebrhood*2;i++){
		my[i].cv1 = &cv1;
		my[i].Dvchange=-1;
		my[i].lock=&lock;
		my[i].cv2 = &cv2;
		my[i].ndone1 = &ndone1;
		my[i].ndone2 = &ndone2;
		my[i].flag1 = 0;

	}

	relaxDet=(relaxDetails*)malloc(sizeof(relaxDetails));
	relaxDet->flag=-1;
	relaxDet->relaxFinish=0;
	relaxDet->routerId=0;
	relaxDet->servFinish=-1;
	relaxDet->dvFromNeb=(int*)malloc(belm->numOfRouters*sizeof(int));


}


