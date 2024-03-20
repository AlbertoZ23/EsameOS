
// Definizione della struttura per i punti
typedef struct {
  double x;
  double y;
} Point;

// Definizione della struttura per i centroidi
typedef struct {
  Point point;
  int cluster_id;
} Centroid;

#define MAX 50

// Definizione della struttura per il messaggio
struct message {
  long mtype;
  double variance;
  Centroid centroids[MAX];
};
