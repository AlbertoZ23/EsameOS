// WORKER
#include <float.h>
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

#define convergence_threshold 1e-10

// Function to calculate the Euclidean distance between two points
double euclidean_distance(Point p1, Point p2) {
  double dx = p1.x - p2.x;
  double dy = p1.y - p2.y;
  return sqrt((dx * dx) + (dy * dy));
}

// Function to check if the random centroids are duplicate
int is_duplicate(Centroid c, Centroid centroids[], int num_centroids) {
  for (int i = 0; i < num_centroids; i++) {
    if (c.point.x == centroids[i].point.x &&
        c.point.y == centroids[i].point.y) {
      return 1;  // Duplicato trovato
    }
  }
  return 0;  // Nessun duplicato
}

// Function to update the centroid of a cluster
void update_centroid(Centroid *centroid, double sum_x, double sum_y,
                     int num_points) {
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
//qui mancava di randomizzare il seme del worker, altrimenti tutti i worker fanno la stessa cosa
  srand(getpid() + time(NULL));

  if (argc != 4) {
    printf("Usage: %s <IPC_key> <K> <P>\n", argv[0]);
    return 1;
  }

  // Get parameters from command line
  key_t ipc_key = atoi(argv[1]);
  int K = atoi(argv[2]);
  int nPoints = atoi(argv[3]);

  // printf("key:%s,k:%s,nPoints:%s\n",argv[1],argv[2],argv[3]);

  int cluster[nPoints];
  // Get shared memory ID
  int shmid = shmget(ipc_key, nPoints * sizeof(Point), SHM_RDONLY);
  if (shmid == -1) {
    errExit("shmget");
    return 1;
  }

  // Attach to shared memory
  Point *points = (Point *)shmat(shmid, NULL, 0);
  if (points == (Point *)-1) {
    errExit("shmat");
    return 1;
  }

  // Get master's message queue ID
  int msgid = msgget(ipc_key, S_IWUSR);
  if (msgid == -1) {
    errExit("Error getting message queue ID");
  }

  // se il worker esce con segnale SIGINT per starccare dalla memoria condivisa
  // e uscire con 0

  Centroid prev_centroids[K];

  while (1) {
    // Initialize centroids randomly
    Centroid centroids[K];

    for (int i = 0; i < K; i++) {
      Centroid c;

      do {
        int random_index = rand() % nPoints;
        c.point = points[random_index];

      } while (is_duplicate(c, centroids, i));
      centroids[i] = c;
      prev_centroids[i] = c;
    }

    while (1) {
      // K-means iterations
      // Assign each point to the closest cluster
      for (int i = 0; i < nPoints; i++) {
        double min_distance = DBL_MAX;
        int closest_cluster = 0;
        for (int j = 0; j < K; j++) {
          double d = euclidean_distance(points[i], centroids[j].point);
          if (d < min_distance) {
            min_distance = d;
            closest_cluster = j;
          }
        }
        centroids[closest_cluster].cluster_id = closest_cluster;
        cluster[i] = closest_cluster;
      }

      // Calculate the new centroids
      for (int i = 0; i < K; i++) {  // scorro lungo i centroidi
        double sum_point_x = 0;
        double sum_point_y = 0;
        int num_points = 0;
        for (int j = 0; j < nPoints; j++) {  // scorro lungo i punti
          if (cluster[j] == i) {  // trovo tutti i punti che sono stati
                                  // assegnati al centroide iesimo
            sum_point_x += points[j].x;
            sum_point_y += points[j].y;
            num_points++;
          }
        }
        // printf("sum point x= %f\n",sum_point_x);
        if (num_points > 1) {
          update_centroid(&centroids[i], sum_point_x, sum_point_y, num_points);
        } else {  // riinizializzo il centroide iesimo se non vi è assegnato
                  // nessun punto
          Centroid c;
          do {
            int random_index = rand() % nPoints;
            c.point = points[random_index];
          } while (is_duplicate(c, centroids, K));
          centroids[i] = c;
        }
      }  // for

      // Calculate the difference between new and previous centroids
      double centroid_diff = 0.0;
      for (int i = 0; i < K; i++) {
        double d =
            euclidean_distance(centroids[i].point, prev_centroids[i].point);
        centroid_diff += d;
      }
      // printf("worker: albi ha calcolato la variazione dei cluster\n");

      // Check for convergence
      if (centroid_diff < convergence_threshold) {
        break;
      }

      // Update prev_centroids with the new centroids
      for (int i = 0; i < K; i++) {
        prev_centroids[i] = centroids[i];
        /*
        printf("Messaggio ricevuto: %f\n", centroids[i].point.x);
        printf("Messaggio ricevuto: %f\n", centroids[i].point.y);ù
        */
      }

    }  // while

    double variance = calculateVariance(points, centroids, cluster, nPoints);
    // printf("Worker: la varizione di albi ha var\n");

    // Create message
    Message msg;
    //qui veniva inutilmente assegnato il tipo di messaggio
    //msg.mtype=2;
    msg.msg.variance = variance;
    for (int i = 0; i < K; i++) {
      msg.msg.centroids[i].point.x = centroids[i].point.x;
      //Qui stavi assegnado due volte x invece di y.
//WRONG:      msg.msg.centroids[i].point.y = centroids[i].point.x;
      msg.msg.centroids[i].point.y = centroids[i].point.y;
      msg.msg.centroids[i].cluster_id = centroids[i].cluster_id;
    }

    size_t mSize = sizeof(Message) - sizeof(long);

    // Send message to queue
    if (msgsnd(msgid, &msg, mSize, 0) == -1) {
      errExit("msgsnd");
      return 1;
    }


  }  // while

  // manca : ripetere finche non si riceve SIGINT o dal master o dall'user

  // Detach shared memory
  shmdt(points);

  printf("Worker %d: terminating...\n", getpid());
  return 0;
}
