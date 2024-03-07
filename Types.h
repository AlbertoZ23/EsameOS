
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

#define MAX 10000

// Definizione della struttura per il messaggio
typedef struct {
  double variance;
  Centroid centroids[MAX];
} Message;
