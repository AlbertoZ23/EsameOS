//WORKER
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "Types.h"
#include "errExit.h"

#define convergence_threshold 1e-6



// Function to calculate the Euclidean distance between two points
double euclidean_distance(Point p1, Point p2) {
  double dx = p1.x - p2.x;
  double dy = p1.y - p2.y;
  return sqrt(dx * dx + dy * dy);
}

// Function to update the centroid of a cluster
void update_centroid(Centroid *centroid, Point *points, int num_points) {
  double sum_x = 0.0;
  double sum_y = 0.0;
  for (int i = 0; i < num_points; i++) {
    sum_x += points[i].x;
    sum_y += points[i].y;
  }
  centroid->point.x = sum_x / num_points;
  centroid->point.y = sum_y / num_points;
}

// Function to calculate variance
double calculateVariance(Point points[], Centroid centroids[], int cluster[],
                         int n) {
  double sumDistances = 0.0;

  // Calculate sum of Euclidean distances between all points
  for (int i = 0; i < n; ++i) {
    sumDistances +=
        pow(euclidean_distance(points[i], centroids[cluster[i]].point), 2);
  }
  return sumDistances / n;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Usage: %s <IPC key> <K> <P>\n", argv[0]);
    return 1;
  }

  printf("Albi entra nel worker \n");

  // Debug
  // exit(0);
  printf("Worker %d running...\n", getpid());

  // Get parameters from command line
  key_t ipc_key = atoi(argv[1]);
  int K = atoi(argv[2]);
  int nPoints = atoi(argv[3]);

  printf("key:%s,k:%s,nPoints:%s\n",argv[1],argv[2],argv[3]);


  int cluster[nPoints];
  // Get shared memory ID
  /*/int shmid = shmget(ipc_key, nPoints * sizeof(Point), SHM_RDONLY);
  if (shmid == -1) {
    errExit("shmget");
    return 1;
  }*/

  int shm_id=
      shmget(ipc_key,sizeof(Point)*nPoints,IPC_CREAT|S_IRUSR|S_IWUSR);

  // Attach to shared memory
  Point *points = (Point *)shmat(shm_id, NULL, 0);
  if (points == (Point *)-1) {
    errExit("shmat");
    return 1;
  }

  // Get master's message queue ID
  int msgid = msgget(ipc_key, S_IWUSR);
  if (msgid == -1) {
    errExit("Error getting message queue ID");
  }

  while (1) {
    // Initialize centroids randomly
    srand(time(NULL));
    Centroid centroids[K];
    for (int i = 0; i < K; i++) {
      int random_index = rand() % nPoints;
      centroids[i].point = points[random_index];
      centroids[i].cluster_id = i;
    }

    Centroid prev_centroids[K];
    while (1) {  //un MILF
      // K-means iterations
      int max_iterations = 10;
      for (int iter = 0; iter < max_iterations; iter++) {
        // Assign each point to the closest cluster
        for (int i = 0; i < nPoints; i++) {
          double min_distance = INFINITY;
          int closest_cluster = 0;
          for (int j = 0; j < K; j++) {
            double d = euclidean_distance(points[i], centroids[j].point);
            if (d < min_distance) {
              min_distance = d;
              printf("dddddddd: %f\n\n",min_distance);
              closest_cluster = j;
              printf("closessssst cll: %i\n", closest_cluster);
            }
          }
          centroids[closest_cluster].cluster_id = closest_cluster;
          cluster[i] = closest_cluster;
        }

        printf("albi ha appena assegnato i punti al cluster\n");

        // Calculate the new centroids
        for (int i = 0; i < K; i++) {
          Point cluster_points[nPoints];
          int num_points = 0;
          for (int j = 0; j < nPoints; j++) {
            if (centroids[i].cluster_id == i) {
              cluster_points[num_points] = points[j];
              num_points++;
            }
          }
          update_centroid(&centroids[i], cluster_points, num_points);
        }

        // Calculate the difference between new and previous centroids
        double centroid_diff = 0.0;
        for (int i = 0; i < K; i++) {
          double d =
              euclidean_distance(centroids[i].point, prev_centroids[i].point);
          centroid_diff += d;
        }
        printf("worker: albi ha calcolato la variazione dei cluster\n");

        // Check for convergence
        if (centroid_diff < convergence_threshold) {
          break;
        }

        // Update prev_centroids with the new centroids
        for (int i = 0; i < K; i++) {
          prev_centroids[i] = centroids[i];
          printf("Messaggio ricevuto: %f\n", centroids[i].point.x);
          printf("Messaggio ricevuto: %f\n", centroids[i].point.y);
        }
        

        printf("wio\n");

      }  // for

      break;

    }  // while

    printf("il brak di albi ha breakkato\n");
    
    double variance = calculateVariance(points, centroids, cluster, nPoints);
    printf("la varizione di albi ha var\n");
    
    // Create message
    Message msg;
    msg.mtype=2;
    msg.variance = variance;
    for (int i = 0; i < K; i++) {
      msg.centroids[i].point.x = centroids->point.x;
      msg.centroids[i].point.y = centroids->point.x;
      msg.centroids[i].cluster_id = centroids->cluster_id;
    }

    printf("aggiorno i tupac\n");

    size_t mSize=sizeof(Message)-sizeof(long);
    
    //--------------------------------------------------------------------------------

    // Send message to queue
    if (msgsnd(msgid, &msg, mSize, 0) == -1) {
      errExit("msgsnd");
      return 1;
    }

    //break;

  }  // while

  // Detach shared memory
  shmdt(points);

 printf("albi esce dal worker\n");
  return 0;
}
