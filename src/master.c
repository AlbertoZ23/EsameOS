//MASTER

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "Types.h"
#include "errExit.h"

#define MAX_NO_IMPROVEMENT 1000

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: %s <K> <N> <key> <dataset>\n", argv[0]);
    return 1;
  }

  // K : number of clusters
  int K = atoi(argv[1]);
  // N : number of worker processes
  int N = atoi(argv[2]);
  // key: key for shared memory and message queue
  key_t key = atoi(argv[3]);
  // datasetFile: file containing the dataset
  char *datasetFile = argv[4];

  // open dataset file
  FILE *fp = fopen(datasetFile, "r");
  if (!fp) {
    errExit("Error opening dataset file");
    return 1;
  }
  
  // count number of lines in dataset file
  int lines = 0;
  char c;
  while ((c = fgetc(fp)) != EOF) {
    if (c == '\n') {
      lines++;
    }
  }
  printf("num linee: %i\n",lines);

  // rewind file pointer to beginning of file
  fseek(fp, 0, SEEK_SET);

  // can't have more clusters than points
  if (lines < K) {
    fprintf(stderr, "Number of clusters must be less than number of points\n");
  }

  // Create shared memory segment
  int shm_id =
      shmget(key, lines*sizeof(Point) , IPC_CREAT | S_IRUSR | S_IWUSR);
  printf("mem creata, dim memoria è: %li\n",lines*sizeof(Point));

  if (shm_id == -1) {
    errExit("Error creating shared memory segment");
    return 1;
  }

  // Attach to shared memory segment
  Point *points = (Point *)shmat(shm_id, NULL, 0);

  if (points == (Point *)-1) {
    errExit("Error attaching to shared memory segment");
    return 1;
  }

  int i = 0;
  char line[100];
  while (fgets(line, 100, fp) != NULL) {
    char *token = strtok(line, ",");
    // convert the string to double
    points[i].x = atof(token);
    token = strtok(NULL, ",");
    points[i].y = atof(token);
    i++;
  }
  fclose(fp);

  // DEBUG - print the dataset
   for (int i = 0; i < lines; i++) {
    printf("%f %f\n", points[i].x, points[i].y);
  }


  // create message queue
  int msg_queue = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);

  if (msg_queue == -1) {
    errExit("Error creating message queue");
    return 1;
  }
  // to store the pids of the worker processes
  pid_t pids[N];
 printf("----debug albi entrerà nel worker---\n");
  // Generate N child processes called "worker"
  for (int i = 0; i < N; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      errExit("Error creating child process");
    } else if (pids[i] == 0) {
      // child process
      char Kstr[10];
      char keystr[10];
      char linesstr[100];
      sprintf(Kstr, "%d", K);
      sprintf(keystr, "%d", key);
      sprintf(linesstr, "%d", lines);
      if (execl("worker", "worker", keystr, Kstr, linesstr, (char *)NULL) ==
          -1) {
        errExit("execl failed");
      }
    }
  }
 printf("albi è uscito dal worker");
 
  //===========================================================================
  // IMPLEMENT THE REST OF THE MASTER

  // counter for number of messages that did not improve the clustering
  int noImprov = 0;
  Message mess;
  size_t mSize = sizeof(Message) - sizeof(long);
  //double min_variance=100000;
  Message best_message;
  best_message.variance=10e30;
  // keep receiving messages from workers
  while (1){
    // read a single message
    msgrcv(msg_queue, &mess, mSize, 0, 0);
    
    // stampa messaggio che riceve
    // update best clustering
    if(mess.variance<best_message.variance){
      best_message=mess;

    }else{
      noImprov=0;
    }
  //stampa del miglior messaggio
    // increment noImprov if the variance did not improve
    
    if (noImprov == MAX_NO_IMPROVEMENT) {
      // dump to file
      FILE *fp = fopen("centroids.csv", "w");
      if (fp == NULL) {
        errExit("fopen");
      }
      for (int i = 0; i < K; i++) {
        // write the centroid to file
        
      }
      fclose(fp);

      // send SIGINT to all workers
      
      // break out of the loop
      break;
    }
    noImprov++;
  }
  
  
  

  for(int i=0;i<N;i++){
    kill(pids[i], SIGINT);
  }
  
  //===========================================================================

  // gather exit status of all worker processes
  for (int i = 0; i < N; i++) {
    wait(NULL);
  }

  for (int i = 0; i < lines; i++) {
   printf("%f %f\n", points[i].x, points[i].y);
  };


  

  //FORZA VERONA

  close(STDOUT_FILENO);
  
  int fd_file_out = open("centroidi.txt", O_WRONLY | O_TRUNC| O_CREAT, S_IRUSR | S_IWUSR);

  

  for (int i = 0; i < K; i++){
    printf("Messaggio ricevuto centroide.x%i: %f\n",i, mess.centroids[i].point.x);
    printf("Messaggio ricevuto centroide.y%i: %f\n\n",i, mess.centroids[i].point.y);
  }

  //msg.centroids[i].point.y;
  //msg.centroids[i].cluster_id;
  // Controlla se l'errore è "la coda di messaggi è vuota"
  /*if (errno != ENOMSG) {
      perror("msgrcv");
      exit(1);
  }*/

  // Detach from shared memory segment
  shmdt(points);

  //dealloco shared memory
  shmctl(shm_id,IPC_RMID,NULL);

  // Deallocate message queue
  msgctl(msg_queue, IPC_RMID, NULL);

  

  close(fd_file_out);
  printf("\nmaster:----fine----\n");
  return 0;
}
