#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INF 1000000000

/* ===================== Structures de Base ===================== */

/* Structure pour représenter un nœud (arrêt) */
typedef struct {
    int ID;
    char Nom[50];
    char Type[50];
    int X;
    int Y;
} Noeud;

/* Structure pour représenter une arête (trajet) */
typedef struct {
    int Source;
    int Destination;
    double Distance;
    int embouteillages;
    int feuxRouges;
    int passagers;
} Arete;

/* Structure pour représenter un feu rouge */
typedef struct {
    int ID;
    int PositionNoeud;
    int Etat;      // 0 = Vert, 1 = Rouge
    int DureeRouge;
    int DureeVert;
    int TempsRestant;
} FeuRouge;

/* Structure pour représenter un graphe */
typedef struct {
    int nbnoeuds;
    int nbaretes;
    int nbFeux;
    Noeud* N;
    Arete* A;
    FeuRouge* F;
} Graphe;

/* ===================== Création et initialisation du graphe ===================== */

/**
 * Alloue et initialise un graphe avec le nombre de nœuds, d'arêtes et de feux rouges indiqués.
 */
Graphe* creergraphe(int Nbnoeuds, int Nbaretes, int NbFeux) {
    Graphe* graph = (Graphe*)malloc(sizeof(Graphe));
    if (!graph) {
        return NULL;
    }
    graph->nbnoeuds = Nbnoeuds;
    graph->nbaretes = Nbaretes;
    graph->nbFeux = NbFeux;
    graph->N = (Noeud*)malloc(sizeof(Noeud) * Nbnoeuds);
    graph->A = (Arete*)malloc(sizeof(Arete) * Nbaretes);
    graph->F = (FeuRouge*)malloc(sizeof(FeuRouge) * NbFeux);
    if (!graph->N || !graph->A || !graph->F) {
        printf("Erreur d'allocation mémoire !\n");
        if (graph->N) free(graph->N);
        if (graph->A) free(graph->A);
        if (graph->F) free(graph->F);
        free(graph);
        return NULL;
    }
    return graph;
}

/* ===================== Fonctions d'ajout ===================== */

/**
 * Ajoute un nœud au graphe.
 */
void ajouternoeud(Graphe* graph, int id, char* name, char* type, int x, int y) {
    if (id >= graph->nbnoeuds) {
        return;
    }
    Noeud* noeud = &graph->N[id];
    noeud->ID = id;
    strcpy(noeud->Nom, name);
    strcpy(noeud->Type, type);
    noeud->X = x;
    noeud->Y = y;
}

/**
 * Ajoute une arête au graphe.
 * Les champs 'embouteillages', 'feuxRouges' et 'passagers' sont initialisés à 0.
 */
void ajouterarete(Graphe* graph, int indice, int source, int destination, double distance) {
    if (indice >= graph->nbaretes) {
        return;
    }
    Arete* arete = &graph->A[indice];
    arete->Source = source;
    arete->Destination = destination;
    arete->Distance = distance;
    arete->embouteillages = 0;
    arete->feuxRouges = 0;
    arete->passagers = 0;
}

/**
 * Ajoute un feu rouge au graphe.
 */
void ajouterFeuRouge(Graphe* graph, int id, int position, int etat, int dureeRouge, int dureeVert) {
    if (id >= graph->nbFeux) {
        return;
    }
    FeuRouge* feu = &graph->F[id];
    feu->ID = id;
    feu->PositionNoeud = position;
    feu->Etat = etat;
    feu->DureeRouge = dureeRouge;
    feu->DureeVert = dureeVert;
    feu->TempsRestant = dureeRouge;
}

/* ===================== Fonction utilitaire ===================== */

/**
 * Calcule la distance euclidienne entre deux nœuds.
 */
double Euclidean_distance(Noeud a, Noeud b) {
    return sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));
}


/** Structure pour stocker le résultat de Dijkstra */

typedef struct {
    int *chemin;    // Tableau d'indices représentant le chemin
    int longueur;   // Nombre de nœuds dans le chemin
} CheminResult;

/* Fonction Dijkstra */
CheminResult dijkstra(Graphe* graph, int source, int target) {
    CheminResult result;
    result.chemin = NULL;
    result.longueur = 0;
    int n = graph->nbnoeuds;
    int dist[n], prev[n], visited[n];
    int i, j, k;
    for(i = 0; i < n; i++){
        dist[i] = INF;
        prev[i] = -1;
        visited[i] = 0;
    }
    dist[source] = 0;
    for(i = 0; i < n; i++){
        int u = -1, minDist = INF;
        for(j = 0; j < n; j++){
            if(!visited[j] && dist[j] < minDist){
                minDist = dist[j];
                u = j;
            }
        }
        if(u == -1)
            break;
        visited[u] = 1;
        if(u == target)
            break;
        for(k = 0; k < graph->nbaretes; k++){
            Arete* a = &graph->A[k];
            if(a->Source == u){
                int v = a->Destination;
                if(!visited[v]){
                    int alt = dist[u] + (int)a->Distance;
                    if(alt < dist[v]){
                        dist[v] = alt;
                        prev[v] = u;
                    }
                }
            }
        }
    }
    if(dist[target] == INF){
        return result;
    } else {
        int path[n], count = 0, current = target;
        while(current != -1){
            path[count++] = current;
            current = prev[current];
        }
        result.chemin = malloc(count * sizeof(int));
        if(!result.chemin) {
            result.longueur = 0;
            return result;
        }
        for(i = 0; i < count; i++){
            result.chemin[i] = path[count - 1 - i];
        }
        result.longueur = count;
        return result;
    }
}

