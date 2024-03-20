#include <fcntl.h>
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

#include "Types.h"
#include "errExit.h"

#define MAX_NO_IMPROVEMENT 1000
#define MAX_READ 1000

void print_message(struct message mess);

void read_point(Point *points, int fd, int npoints);

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: %s <K> <N> <key> <dataset>\n", argv[0]);
    return 1;
  };

  printf("%s, %s, %s, %s\n", argv[1], argv[2], argv[3], argv[4]);

  close(STDOUT_FILENO);
  int fd_file_out = open("output.txt", O_WRONLY | O_TRUNC| O_CREAT, S_IRUSR | S_IWUSR);

  // K : number of clusters
  int K = atoi(argv[1]);
  // N : number of worker processes
  int N = atoi(argv[2]);
  // key: key for shared memory and message queue
  key_t key_ipc = atoi(argv[3]);
  // datasetFile: file containing the dataset
  char *datasetFile = argv[4];

  printf("K = %i \t N = %i \t Key = %i \n", K, N, key_ipc);
  // open dataset file
  int fp = open(datasetFile, O_RDONLY);
  if (fp == -1) {
    errExit("Error opening dataset file");
    return 1;
  } 

  // count number of lines in dataset file
  int lines = 0;
  char c;

  int checkread = 0;

  do {
    checkread = read(fp, &c, sizeof(char));

    if (checkread == -1)
      errExit("read");

    if (c == '\n') {
      lines++;
      printf("master: lines ++\n");
    }
  } while (checkread > 0);
  printf("master: Lette tutte le righe!\n");

  // rewind file pointer to beginning of file
  lseek(fp, 0, SEEK_SET);

  printf("master: lines = %i\n", lines);

  // can't have more clusters than points
  if (lines < K) {
    fprintf(stderr, "Number of clusters must be less than number of points\n");
  };

  // Create shared memory segment
  int shm_id =
      shmget(key_ipc, sizeof(Point) * lines, IPC_CREAT | S_IRUSR | S_IWUSR);

      printf("Mem \n");

  if (shm_id == -1) {
    errExit("Error creating shared memory segment");
    return 1;
  }else{

    printf("master: Memoria creata\n");
  }

  // Attach to shared memory segment
  Point *points = (Point *)shmat(shm_id, NULL, 0);
  if (points == (Point *)-1) {
    errExit("Error attaching to shared memory segment");
    return 1;
  };

  printf("worker: leggo i punti!\n");

  read_point(points, fp, lines);

  printf("worker: lettura punti terminata");

  // read each line of the file
  // while (fgets(line, 100, fp)) {
  //   char *token = strtok(line, ",");
  //   // convert the string to double
  //   points[i].x = atof(token);
  //   token = strtok(NULL, ",");
  //   points[i].y = atof(token);
  //   i++;
  // }
  close(fp);

  // debug
  // print the points
  for (int i = 0; i < lines; i++) {
    printf("%f, %f\n", points[i].x, points[i].y);
  }

  // create message queue
  int msg_queue = msgget(key_ipc, IPC_CREAT | S_IRUSR | S_IWUSR);

  if (msg_queue == -1) {
    errExit("master: Error creating message queue");
    return 1;
  } else {
    printf("master: message queue creata id = %i!\n", msg_queue);
  }
  // to store the pids of the worker processes
  pid_t pids[N];

  char Kstr[10];
  char linesstr[100];

  sprintf(Kstr, "%d", K);
  sprintf(linesstr, "%d", lines);

  printf("master: key = %i\n", key_ipc);


  for (int noImprov = 0; noImprov < MAX_NO_IMPROVEMENT; noImprov++) {

  // Generate N child processes called "worker"
  for (int i = 0; i < N; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      errExit("Error creating child process");
    } else if (pids[i] == 0) {
      printf("master: Creazione figlio %i\n", i);
      if (execl("worker", "worker", argv[3], Kstr, linesstr, (char *)NULL) == -1) {
        errExit("execl failed");
      };
    };
  };

printf("master: chiusa la chiamata del work\n");

  struct message mess;
  size_t mSize = sizeof(struct message) - sizeof(long);

  printf("\n\n\n");

  // keep receiving messages from workers
      // dump to file
      // int fp = open("centroids.csv", O_WRONLY);
      // if (fp == -1) {
      //   errExit("fopen");
      // };

      printf("master: lettura del messaggio!\n");

      // send SIGINT to all workers
      for (int i = 0; i < N; i++) {
        if (msgrcv(msg_queue, &mess, mSize, -2, 0) == -1) {
          errExit("master: msgrcv");
          return 1;
        }

        printf("master: Stampa del messaggio!\n");

        // print_message(mess);

        printf("master: ho letto il messaggio!\n");
          kill(pids[i], SIGINT);
      };

      printf("master: test\n");
      // break out of the loop
  };
  
  printf("Master: finito di ricevere\n");

  // gather exit status of all worker processes
  for (int i = 0; i < N; i++) {
    wait(NULL);
  };

  // Detach from shared memory segment
  shmdt(points);

  // Deallocate shared memory segment
  if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
    errExit("Error deallocating shared memory segment");
    return 1;
  }

  // Deallocate message queue
  if (msgctl(msg_queue, IPC_RMID, NULL) == -1) {
    errExit("Error deallocating message queue");
    return 1;
  }

  close(fd_file_out);

  return 0;
};

void print_message(struct message mess)
{
//   long mtype;
//   double variance;
//   Centroid centroids[MAX];

  printf("Message: \n");
  printf("\tvariance: %f\n", mess.variance);
  printf("\tCentroids: \n");
  for (int i = 0; i < MAX; i++) {
    printf("\t\t x: %f, y: %f\n", mess.centroids[i].point.x, mess.centroids[i].point.y);
    printf("\t\t cluster_id : %i\n", mess.centroids[i].cluster_id);
  }
}

void read_point(Point *points, int fd, int npoints)
{
  char buffer[MAX_READ];

  ssize_t nread;

  do {
    nread = read(fd, buffer, MAX_READ);

    if (nread == -1)
      errExit("read");
    
    for (int i = 0; i < npoints; i++)
      sscanf(buffer, "%lf,%lf\n", &points[i].x, &points[i].y);
  } while (nread > 0);
}