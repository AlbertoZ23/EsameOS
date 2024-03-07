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

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: %s <K> <N> <key> <dataset>\n", argv[0]);
    return 1;
  };

  printf("%s, %s, %s, %s\n", argv[1], argv[2], argv[3], argv[4]);

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
  FILE *fp = fopen(datasetFile, "r");
  if (fp == NULL) {
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

  printf("Lette tutte le righe!\n");

  // rewind file pointer to beginning of file
  fseek(fp, 0, SEEK_SET);

  printf("lines = %i\n", lines);

  // can't have more clusters than points
  if (lines < K) {
    fprintf(stderr, "Number of clusters must be less than number of points\n");
  };

  // Create shared memory segment
  int shm_id =
      shmget(key_ipc, sizeof(Point) * lines, IPC_CREAT | S_IRUSR | S_IWUSR);

  if (shm_id == -1) {
    errExit("Error creating shared memory segment");
    return 1;
  }

  // Attach to shared memory segment
  Point *points = (Point *)shmat(shm_id, NULL, 0);
  if (points == (Point *)-1) {
    errExit("Error attaching to shared memory segment");
    return 1;
  };

  int i = 0;
  char line[100];

  // read each line of the file
  while (fgets(line, 100, fp)) {
    char *token = strtok(line, ",");
    // convert the string to double
    points[i].x = atof(token);
    token = strtok(NULL, ",");
    points[i].y = atof(token);
    i++;
  }
  fclose(fp);

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
  };
  // to store the pids of the worker processes
  pid_t pids[N];

  // Generate N child processes called "worker"
  for (int i = 0; i < N; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      errExit("Error creating child process");
    } else if (pids[i] == 0) {
      char Kstr[10];
      char keystr[10];
      char linesstr[100];
      printf("%s \t %d \t %s\n", Kstr, shm_id, linesstr);
      sprintf(Kstr, "%d", K);
      sprintf(keystr, "%d", shm_id);
      sprintf(linesstr, "%d", lines);
      if (execl("worker", "worker", keystr, Kstr, linesstr, (char *)NULL) ==
          -1) {
        errExit("execl failed");
      };
    };
  };

  int noImprov = 0;

  // keep receiving messages from workers
  while (1) {
    if (noImprov == MAX_NO_IMPROVEMENT) {
      // dump to file
      int fp = open("centroids.csv", O_WRONLY);
      if (fp == -1) {
        errExit("fopen");
      };
      close(fp);

      // send SIGINT to all workers
      for (int i = 0; i < N; i++) {
        kill(pids[i], SIGINT);
      };
      // break out of the loop
      break;
    };
  };

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

  return 0;
};

