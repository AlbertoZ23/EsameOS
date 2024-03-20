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

void print_message(struct message mess);

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
    printf("worker: Usage: %s <IPC key> <K> <P>\n", argv[0]);
    return 1;
  }

  printf("worker %d running...\n", getpid());

  // Get parameters from command line
  key_t key_ipc = atoi(argv[1]);
  int K = atoi(argv[2]);
  int nPoints = atoi(argv[3]);

  printf("worker: Key = %i \t %i \t %d\n", key_ipc, K, nPoints);
  
  int cluster[nPoints];
  // Get shared memory ID
  //int shmid = shmget(key_ipc, nPoints * sizeof(Point), SHM_RDONLY);
  //if (shmid == -1) {
    // errExit("shmget");
    //return 1;
   //}

  printf("worker: Semget riuscita\n");

  int shm_id =

      shmget(key_ipc, sizeof(Point) * nPoints, IPC_CREAT | S_IRUSR | S_IWUSR);

  // Attach to shared memory
  Point *points = (Point *)shmat(shm_id, NULL, 0);
  if (points == (Point *)-1) {
    errExit("worker: error shmat");
    return 1;
  }
  printf("worker: attach riuscita \n");

  // Get master's message queue ID
  int msgid = msgget(key_ipc, S_IRUSR | S_IWUSR);
  if (msgid == -1) {
    errExit("work: error getting message queue ID");
  }else{
    printf("worker: presa la coda dei messaggi id = %i\n", msgid);
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

    printf("worker: Inizializzazione centroidi fatta!\n");

    Centroid prev_centroids[K];
    while (1) {
      // K-means iterations
      int max_iterations = 100;
      for (int iter = 0; iter < max_iterations; iter++) {
        // Assign each point to the closest cluster
        for (int i = 0; i < nPoints; i++) {
          double min_distance = INFINITY;
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

        printf("worker: Assegnamento dei punti al cluster\n");

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

        printf("worker: Centroide calcolato\n");

        // Calculate the difference between new and previous centroids
        double centroid_diff = 0.0;
        for (int i = 0; i < K; i++) {
          double d =
              euclidean_distance(centroids[i].point, prev_centroids[i].point);
          centroid_diff += d;
        }

        printf("worker: Calcolata variazione\n");

        // Check for convergence
        if (centroid_diff < convergence_threshold) {
          break;
        }

        // Update prev_centroids with the new centroids
        for (int i = 0; i < K; i++) {
          prev_centroids[i] = centroids[i];
        }

        printf("worker: Aggiorna il centroide nel caso di variazioni\n");

      } 

      break;

    }  

    printf("worker: Test\n");

    double variance = calculateVariance(points, centroids, cluster, nPoints);

    // Create message
    struct message mess;
    mess.mtype = 2;
    mess.variance = variance;
    for (int i = 0; i < K; i++) {
      mess.centroids[i].point.x = centroids->point.x;
      mess.centroids[i].point.y = centroids->point.y;
      mess.centroids[i].cluster_id = centroids->cluster_id;
    }

    print_message(mess);

    printf("worker: msgid = %i\n", msgid);

    printf("worker: Message size = %li\n", sizeof(struct message));

    size_t mSize = sizeof(struct message) - sizeof(long);

    printf("worker: mSize = %li\n", mSize);

    printf("worker: arrivato all'invio del messaggio \n");

    // Send message to queue
    if (msgsnd(msgid, &mess, mSize, 0) == -1) {
      errExit("work: msgsnd");
      return 1;
    }
printf("worker: HO MANDATO IL MESSAGGIO\n");
  }  // while

  shmdt(points);  

  return 0;
}

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