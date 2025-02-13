#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // Pour sleep()
#include <time.h>
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
    int Prioritaire;
    int embouteillages;
    int feuxRouges;
    int passagers;
    int capacite; // capacité maximale de la route
    int etat; // 0 = Normal, 1 = Accident, 2 = Panne
    int flow; // flux actuel (initialisé à 0)
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

/* Structure pour représenter un passager */
typedef struct Passager {
    int ID;
    int destination;
    struct Passager* suivant;
} Passager;

/* Structure pour représenter un véhicule */
typedef struct {
    int ID;
    char type[50];
    int capaciteMax;
    int Npassager;
    int positionNoeud;
    int destinationNoeud;
    double vitesse;
    Passager* passagers;
} Vehicule;

/* Structure pour représenter un nœud dans la file/pile de véhicules */
typedef struct Node {
    Vehicule vehicule;
    struct Node* suivant;
} Node;

/* Structure pour représenter une file (queue) */
typedef struct {
    Node* tete;
    Node* queue;
} File;

/* Structure pour représenter une pile */
typedef struct {
    Node* sommet;
} Pile;

/* Structure pour représenter un nœud dans la file d’attente des passagers */
typedef struct PassagerNode {
    Passager passager;
    struct PassagerNode* suivant;
} PassagerNode;

/* Structure pour représenter une file d’attente de passagers */
typedef struct {
    PassagerNode* tete;
    PassagerNode* queue;
} FilePassagers;

/* Structure pour représenter un graphe */
typedef struct {
    int nbnoeuds;
    int nbaretes;
    int nbFeux;
    int nbVehicules;
    Noeud* N;
    Arete* A;
    FeuRouge* F;
    File* filesFeux;           // Une file par feu rouge
    Vehicule principal;
    File fileTrafic;
    Pile historiqueDeplacements;
    FilePassagers* filesAttente; // Une file d'attente de passagers par arrêt
} Graphe;


/* ===================== Fonctions de gestion de file et pile ===================== */

void enfiler(File* file, Vehicule vehicule) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return;
    newNode->vehicule = vehicule;
    newNode->suivant = NULL;
    if (file->queue)
        file->queue->suivant = newNode;
    file->queue = newNode;
    if (!file->tete)
        file->tete = newNode;
}

Vehicule defiler(File* file) {
    if (!file->tete) {
        Vehicule v = {-1, "", 0, 0, 0, 0, 0.0, NULL};
        return v;
    }
    Node* temp = file->tete;
    Vehicule v = temp->vehicule;
    file->tete = temp->suivant;
    if (!file->tete)
        file->queue = NULL;
    free(temp);
    return v;
}

void empiler(Pile* pile, Vehicule vehicule) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return;
    newNode->vehicule = vehicule;
    newNode->suivant = pile->sommet;
    pile->sommet = newNode;
}

Vehicule depiler(Pile* pile) {
    if (!pile->sommet) {
        Vehicule v = {-1, "", 0, 0, 0, 0, 0.0, NULL};
        return v;
    }
    Node* temp = pile->sommet;
    Vehicule v = temp->vehicule;
    pile->sommet = temp->suivant;
    free(temp);
    return v;
}


/* ===================== Création et initialisation du graphe ===================== */

Graphe* creergraphe(int Nbnoeuds, int Nbaretes, int NbFeux) {
    Graphe* graph = (Graphe*)malloc(sizeof(Graphe));
    if (!graph) {
        printf("Erreur d'allocation memoire pour le graphe !\n");
        return NULL;
    }
    graph->nbnoeuds = Nbnoeuds;
    graph->nbaretes = Nbaretes;
    graph->nbFeux = NbFeux;
    graph->nbVehicules = 0;
    graph->fileTrafic.tete = graph->fileTrafic.queue = NULL;
    graph->historiqueDeplacements.sommet = NULL;
    graph->N = (Noeud*)malloc(sizeof(Noeud) * Nbnoeuds);
    graph->A = (Arete*)malloc(sizeof(Arete) * Nbaretes);
    graph->F = (FeuRouge*)malloc(sizeof(FeuRouge) * NbFeux);
    graph->filesFeux = (File*)malloc(sizeof(File) * NbFeux);
    if (!graph->N || !graph->A || !graph->F || !graph->filesFeux) {
        printf("Erreur d'allocation memoire !\n");
        if (graph->N) free(graph->N);
        if (graph->A) free(graph->A);
        if (graph->F) free(graph->F);
        if (graph->filesFeux) free(graph->filesFeux);
        free(graph);
        return NULL;
    }
    for (int i = 0; i < NbFeux; i++) {
        graph->filesFeux[i].tete = NULL;
        graph->filesFeux[i].queue = NULL;
    }
    graph->filesAttente = (FilePassagers*)malloc(sizeof(FilePassagers) * Nbnoeuds);
    for (int i = 0; i < Nbnoeuds; i++) {
        graph->filesAttente[i].tete = NULL;
        graph->filesAttente[i].queue = NULL;
    }
    return graph;
}

/* ===================== Fonctions d'ajout ===================== */
/* Ajouter un noeud */
void ajouternoeud(Graphe* graph, int id, char* name, char* type, int x, int y) {
    if (id >= graph->nbnoeuds) {
        printf("ID du noeud hors limite\n");
        return;
    }
    Noeud* noeud = &graph->N[id];
    noeud->ID = id;
    strcpy(noeud->Nom, name);
    strcpy(noeud->Type, type);
    noeud->X = x;
    noeud->Y = y;
}
/* Ajouter une arête (sans capacité, on pourra par la suite la définir) */
void ajouterarete(Graphe* graph, int indice, int source, int destination, double distance, int prioritaire) {
    if (indice >= graph->nbaretes) {
        printf("ID d'arete hors limite\n");
        return;
    }
    Arete* arete = &graph->A[indice];
    arete->Source = source;
    arete->Destination = destination;
    arete->Distance = distance;
    arete->Prioritaire = prioritaire;
    arete->capacite = 0;
    arete->flow = 0;
    arete->etat = 0;
}

/* Ajouter une arête avec capacité (pour la gestion des flux) */
void ajouter_arete_flux(Graphe* graph, int indice, int source, int destination, double distance, int prioritaire, int capacity) {
    if (indice >= graph->nbaretes) {
        printf("ID d'arete hors limite\n");
        return;
    }
    Arete* arete = &graph->A[indice];
    arete->Source = source;
    arete->Destination = destination;
    arete->Distance = distance;
    arete->Prioritaire = prioritaire;
    arete->capacite = capacity;
    arete->flow = 0;
}

void ajouterFeuRouge(Graphe* graph, int id, int position, int etat, int dureeRouge, int dureeVert) {
    if (id >= graph->nbFeux) {
        printf("ID de feu rouge hors limite\n");
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

/* ===================== Affichage du graphe ===================== */

void afficher_graph(Graphe* graph) {
    printf("\n======== RESEAU DE TRANSPORT ========\n");
    printf("Stations (%d) :\n", graph->nbnoeuds);
    for (int i = 0; i < graph->nbnoeuds; i++) {
        Noeud* n = &graph->N[i];
        printf(" - Station : ID = %d : '%s' (%s), Coordonnees : (%d, %d)\n",
               n->ID, n->Nom, n->Type, n->X, n->Y);
    }
    printf("\nTrajets (%d) :\n", graph->nbaretes);
    for (int i = 0; i < graph->nbaretes; i++) {
        Arete* a = &graph->A[i];
        printf(" - Trajet %d : De l'arret %d a l'arret %d, Distance : %.2f, Prioritaire : %d, Capacite : %d, Flux : %d\n",
               i, a->Source, a->Destination, a->Distance, a->Prioritaire, a->capacite, a->flow);
    }
}

/* ========================================================================= */
/*                   ALGORITHMES DE CHEMIN (Dijkstra & A*)                 */
/* ========================================================================= */

/* --- Dijkstra Standard pour calculer le chemin le plus court --- */
void Dijkstra(Graphe* graph, int source, int target){
    int n = graph->nbnoeuds; // Nombre de noeuds du graphe
    int dist[n] /* Contient la distance minimale connue depuis la source jusqu'à chaque nœud */,
	prev[n] /* Tableau des précedents */,
	visited[n] /* Tableau pour indiquer la visite d'une arête */,
	i, j, k;
    for(i = 0; i < n; i++){
        dist[i] = INF; // Initialisé tout les arêtes par l'infini pour signifier que ces distances sont inconnues au départ
        prev[i] = -1; // Pour chaque nœud, ce tableau enregistre le nœud précédent sur le chemin le plus court (initialisé à -1 pour indiquer l'absence de prédécesseur)
        visited[i] = 0; // Indique si la distance minimale d'un nœud est déterminée. Tous les nœuds sont initialement non visités (valeur 0)
    }
    dist[source] = 0; // Initialisé la distance du nœud source par 0

    for(i = 0; i < n; i++){
        int u = -1, minDist = INF; // Initialisé la distance minimale par l'ifini
        for(j = 0; j < n; j++){
            if(!visited[j] && dist[j] < minDist){ // Pour chaque itération, on cherche parmi tous les nœuds non visités celui ayant la distance minimale actuelle
                minDist = dist[j];
                u = j;
            }
        }
        if(u == -1) break; // Si aucun nœud n'est trouvé (cas où u reste égal à -1), cela signifie que tous les nœuds accessibles ont été traités, et on sort de la boucle
        visited[u] = 1; // Marquer la visite de l'arête
        if(u == target) break; // Si u est le nœud cible, on peut arrêter car le chemin le plus court a été trouvé /* Parcours de tous les nœuds jusqu'à trouver le nœuds cible (target)*/

        // Mise à jour des distances (relaxation)
        /* Parcours de toutes les arêtes sortant de u */
        for(k = 0; k < graph->nbaretes; k++){ // Pour le nœud actuellement sélectionné u, on parcourt toutes les arêtes du graphe
            Arete* a = &graph->A[k];
            if(a->Source == u){ // Pour chaque arête qui part de u, on identifie le nœud voisin v (la destination de l'arête)
                int v = a->Destination;
                if(!visited[v]){ // Si v n’a pas encore été visité, on calcule une nouvelle distance potentielle alt pour atteindre v en passant par u (le cours)
                    int alt = dist[u] + (int)a->Distance; // (int) indique juste la partie entiere de a->Distance
                    if(alt < dist[v]){ // Si cette nouvelle distance alt est inférieure à la distance actuelle enregistrée pour v (dist[v]), on met à jour
                        dist[v] = alt; // La nouvelle distance
                        prev[v] = u; // Pour indiquer que le meilleur chemin pour atteindre v passe par u
                    }
                }
            }
        }
    }

    if(dist[target] == INF) // On vérifie si la distance vers le nœud cible est restée infinie
        printf("Aucun chemin trouve de %d vers %d.\n", source, target); // Si c'est le cas, cela signifie qu'il n'existe aucun chemin entre la source et la cible
    else{ // Sinon, on affiche la distance minimale trouvée
        printf("Chemin le plus court de %d vers %d (distance: %d): ", source, target, dist[target]);
        int path[n], count = 0, current = target;
		// Pour reconstituer le chemin, on utilise le tableau prev en partant du nœud cible et en remontant jusqu'à la source
        while(current != -1){
            path[count++] = current;
            current = prev[current];
        }
		// Les nœuds sont stockés dans le tableau path
        for(i = count - 1; i >= 0; i--) // Enfin, le chemin est affiché dans l'ordre (de la source à la cible)
            printf("%d ", path[i]);
        printf("\n");
    }
}

/* Dijkstra modifié pour intégrer les routes prioritaires (réduction des poids) */
// Même fonctionnalité que Dijkstra Standard, mais avant d'ajouter le coût d'une arête, on vérifie si celle-ci est prioritaire. Si c'est le cas, le coût est multiplié par un facteur de réduction pour favoriser ces routes
void Dijkstra_priority(Graphe* graph, int source, int target, double reduction_factor){
    int n = graph->nbnoeuds;
    int dist[n], prev[n], visited[n], i, j, k;
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
        if(u == -1) break;
        visited[u] = 1;
        if(u == target) break;

        for(k = 0; k < graph->nbaretes; k++){
            Arete* a = &graph->A[k];
            if(a->Source == u){
                int v = a->Destination;
                if(!visited[v]){
                    double weight = a->Distance;
                    if(a->Prioritaire){
                        weight *= reduction_factor;  // réduction du poids pour les routes prioritaires
                    }
                    int alt = dist[u] + (int)weight;
                    if(alt < dist[v]){
                        dist[v] = alt;
                        prev[v] = u;
                    }
                }
            }
        }
    }

    if(dist[target] == INF)
        printf("Aucun chemin trouve (prioritaire) de %d vers %d.\n", source, target);
    else{
        printf("Chemin optimise (prioritaire) de %d vers %d (distance: %d): ", source, target, dist[target]);
        int path[n], count = 0, current = target;
        while(current != -1){
            path[count++] = current;
            current = prev[current];
        }
        for(i = count - 1; i >= 0; i--)
            printf("%d ", path[i]);
        printf("\n");
    }
}

/* Fonction utilitaire : calcul de la distance euclidienne entre deux noeuds */
double Euclidean_distance(Noeud a, Noeud b) {
    return sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));
}
/* Algorithme A* pour trouver un chemin optimisé à l'aide d'une heuristique (distance euclidienne) */
void A_star(Graphe* graph, int source, int target){
    int n = graph->nbnoeuds;
    double g[n], f[n];
    int prev[n], closed[n], open[n], i, k;
    for(i = 0; i < n; i++){
        g[i] = INF; // Coût réel du chemin depuis la source jusqu’au nœud i
        f[i] = INF; /*  Coût total estimé pour atteindre le target en passant par i. Il s’agit de la somme de g[i] et de l’heuristique (ici la distance euclidienne entre i et la cible) */
        prev[i] = -1; // Tableau pour constituer le chemin optimal. Initialisée à -1 pour indiquer qu’aucun prédécesseur n’a encore été défini
        closed[i] = 0; //  Tableau binaire pour indiquer si le nœud i a été exploré. Tous sont initialisés à 0
        open[i] = 0; // Tableau binaire indiquant si le nœud i est dans la liste des nœuds à explorer. Initialement, aucun nœud n’est marqué ouvert, sauf la source
    }
    g[source] = 0; // Initialisé à 0 (aucun coût pour partir de la source)
    f[source] = Euclidean_distance(graph->N[source], graph->N[target]); // Initialisé à l’heuristique, ici la distance euclidienne entre la source et la cible
    open[source] = 1; // La source est ajoutée à la liste ouverte (open)

    while(1){
        int current = -1;
        double minF = 1e9;
        for(i = 0; i < n; i++){
            if(open[i] && f[i] < minF){
                minF = f[i];
                current = i;
            }
        }
        if(current == -1)
            break;
        if(current == target){
            printf("Chemin A* de %d vers %d (cout estime: %.2f): ", source, target, g[target]);
            int path[n], count = 0, temp = target;
            while(temp != -1){
                path[count++] = temp;
                temp = prev[temp];
            }
            for(i = count - 1; i >= 0; i--)
                printf("%d ", path[i]);
            printf("\n");
            return;
        }
        open[current] = 0;
        closed[current] = 1;
        for(k = 0; k < graph->nbaretes; k++){
            Arete* a = &graph->A[k];
            if(a->Source == current){
                int neighbor = a->Destination;
                if(closed[neighbor])
                    continue;
                double tentative_g = g[current] + a->Distance;
                if(!open[neighbor])
                    open[neighbor] = 1;
                else if(tentative_g >= g[neighbor])
                    continue;
                prev[neighbor] = current;
                g[neighbor] = tentative_g;
                double h = Euclidean_distance(graph->N[neighbor], graph->N[target]);
                f[neighbor] = g[neighbor] + h;
            }
        }
    }
    printf("A*: Aucun chemin trouve de %d vers %d.\n", source, target);
}

/* ========================================================================= */
/*                   ALGORITHME DE FLOT MAXIMUM (Ford-Fulkerson)           */
/* ========================================================================= */

/* Utilitaire : recherche en largeur (BFS) dans le graphe résiduel */
int BFS_residual(int **residual, int nb_noeuds, int source, int sink, int parent[]) {
    int *visited = (int*)calloc(nb_noeuds, sizeof(int));
    int *queue = (int*)malloc(nb_noeuds * sizeof(int));
    int front = 0, rear = 0;
    queue[rear++] = source;
    visited[source] = 1;
    parent[source] = -1;

    while (front < rear) {
        int u = queue[front++];
        for (int v = 0; v < nb_noeuds; v++) {
            if (!visited[v] && residual[u][v] > 0) {
                queue[rear++] = v;
                parent[v] = u;
                visited[v] = 1;
                if (v == sink) {
                    free(visited);
                    free(queue);
                    return 1;
                }
            }
        }
    }
    free(visited);
    free(queue);
    return 0;
}

/* Implémentation de Ford-Fulkerson (via Edmonds-Karp) */
int Ford_Fulkerson(Graphe* graph, int source, int sink) {
    int nb = graph->nbnoeuds, i;
    /* Allocation et initialisation de la matrice résiduelle */
    int **residual = (int**)malloc(nb * sizeof(int*));
    for(i = 0; i < nb; i++){
        residual[i] = (int*)calloc(nb, sizeof(int));
    }
    for(i = 0; i < graph->nbaretes; i++){
        Arete* a = &graph->A[i];
        residual[a->Source][a->Destination] = a->capacite;
    }

    int max_flow = 0;
    int *parent = (int*)malloc(nb * sizeof(int));
    while(BFS_residual(residual, nb, source, sink, parent)){
        int path_flow = INF;
        int v = sink;
        while(v != source){
            int u = parent[v];
            if(residual[u][v] < path_flow)
                path_flow = residual[u][v];
            v = u;
        }
        v = sink;
        while(v != source){
            int u = parent[v];
            residual[u][v] -= path_flow;
            residual[v][u] += path_flow;
            v = u;
        }
        max_flow += path_flow;
    }
    for(i = 0; i < nb; i++){
        free(residual[i]);
    }
    free(residual);
    free(parent);
    return max_flow;
}

/* ========================================================================= */
/*                SIMULATION DE ROUTAGE DYNAMIQUE                        */
/* ========================================================================= */

/* Pour simuler une perturbation, on modifie le poids d'une arete puis on recalcule l'itineraire */
void recalculer_itineraire(Graphe* graph, int source, int target, int affectedEdgeIndex, double congestionFactor) {
    if (affectedEdgeIndex < 0 || affectedEdgeIndex >= graph->nbaretes) {
        printf("Indice d'arete invalide.\n");
        return;
    }
    graph->A[affectedEdgeIndex].Distance *= congestionFactor;
    printf("Perturbation simulee sur l'arete %d, nouveau poids: %.2f\n",
           affectedEdgeIndex, graph->A[affectedEdgeIndex].Distance);
    Dijkstra(graph, source, target);
}

/* ========================================================================= */
/*                Fonctions de Gestion des Vehicules et Passagers        */
/* ========================================================================= */

/* Ajoute le vehicule en attente dans la file generale */
void ajouterVehiculeEnAttente(Graphe* graph, Vehicule v) {
    printf("%s ID %d en attente a l'arret %d\n", v.type, v.ID, v.positionNoeud);
    enfiler(&graph->fileTrafic, v);
}

/* Gere le passage des vehicules en attente */
void gererVehiculesEnAttente(Graphe* graph) {
    if (graph->fileTrafic.tete) {
        Vehicule v = defiler(&graph->fileTrafic);
        printf("%s ID %d avance depuis l'arret %d\n", v.type, v.ID, v.positionNoeud);
        empiler(&graph->historiqueDeplacements, v);
    }
}

/* Ajoute un vehicule dans la file d'un feu rouge */
void ajouterVehiculeFileFeu(Graphe* graph, int feuID, Vehicule v) {
    if (feuID >= graph->nbFeux) return;
    File* file = &graph->filesFeux[feuID];
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return;
    newNode->vehicule = v;
    newNode->suivant = NULL;
    if (file->queue)
        file->queue->suivant = newNode;
    file->queue = newNode;
    if (!file->tete)
        file->tete = newNode;
    printf("%s ID %d ajoute dans la file du feu rouge %d\n", v.type, v.ID, feuID);
}

/* Libere les vehicules dans la file d'un feu rouge */
void libererVehiculesFeu(Graphe* graph, int feuID) {
    if (feuID >= graph->nbFeux) return;
    File* file = &graph->filesFeux[feuID];
    while (file->tete) {
        Node* temp = file->tete;
        Vehicule v = temp->vehicule;
        printf("%s ID %d avance depuis le feu rouge %d\n", v.type, v.ID, feuID);
        sleep(1);
        file->tete = temp->suivant;
        if (!file->tete)
            file->queue = NULL;
        free(temp);
    }
}

/* Retourne le nombre de vehicules a un arret */
int nombreVehiculesArret(Graphe* graph, int arretID) {
    int count = 0;
    if (graph->principal.positionNoeud == arretID)
        count++;
    for (int i = 0; i < graph->nbFeux; i++) {
        if (graph->F[i].PositionNoeud == arretID) {
            Node* current = graph->filesFeux[i].tete;
            while (current) {
                count++;
                current = current->suivant;
            }
        }
    }
    return count;
}

/* ========================================================================= */
/*                          Gestion des Passagers                     */
/* ========================================================================= */

/* Embarque les passagers depuis la file d'attente de l'arret dans le vehicule */
void embarquer_passagers(Graphe* graph, Vehicule* vehicule) {
    FilePassagers* file = &graph->filesAttente[vehicule->positionNoeud];
    while (vehicule->Npassager < vehicule->capaciteMax && file->tete) {
        PassagerNode* pnode = file->tete;
        file->tete = file->tete->suivant;
        if (!file->tete)
            file->queue = NULL;
        Passager* newPassager = (Passager*)malloc(sizeof(Passager));
        *newPassager = pnode->passager;
        newPassager->suivant = vehicule->passagers;
        vehicule->passagers = newPassager;
        vehicule->Npassager++;
        printf("Passager ID %d embarque, destination: %d\n", newPassager->ID, newPassager->destination);
        free(pnode);
    }
}

/* Débarque les passagers arrivés à destination */
void debarquer_passagers(Vehicule* vehicule) {
    Passager* prev = NULL;
    Passager* current = vehicule->passagers;
    while (current) {
        if (current->destination == vehicule->positionNoeud) {
            printf("Passager ID %d a debarque\n", current->ID);
            if (prev)
                prev->suivant = current->suivant;
            else
                vehicule->passagers = current->suivant;
            Passager* temp = current;
            current = (prev) ? prev->suivant : vehicule->passagers;
            free(temp);
            vehicule->Npassager--;
        } else {
            prev = current;
            current = current->suivant;
        }
    }
}

void attendre_feu_rouge(Graphe* graph, int feuID) {
    if (feuID >= 0) {
        printf("Feu rouge detecte a l'arret %d ! Attente de %d secondes...\n",
               graph->F[feuID].PositionNoeud, graph->F[feuID].DureeRouge);
        sleep(graph->F[feuID].DureeRouge);
        graph->F[feuID].Etat = 0; // Passage au vert
        graph->F[feuID].TempsRestant = graph->F[feuID].DureeVert;
        printf("Feu vert ! Le vehicule peut continuer.\n");
    }
}

/* ========================================================================= */
/*                   Déplacement du Véhicule Principa                   */
/* ========================================================================= */

void deplacerVehiculePrincipal(Graphe* graph) {
    Vehicule* v = &graph->principal;
    // Si le véhicule est arrivé à destination, on termine la simulation
    if (v->positionNoeud == v->destinationNoeud) {
        printf("\nLe vehicule principal est arrive a destination (noeud %d).\n", v->destinationNoeud);
        exit(0);
    }

    printf("\n>> Deplacement du vehicule principal depuis l'arret %d\n", v->positionNoeud);
    debarquer_passagers(v);
    embarquer_passagers(graph, v);

    // Si un feu rouge bloque le passage
    for (int i = 0; i < graph->nbFeux; i++) {
        if (graph->F[i].PositionNoeud == v->positionNoeud && graph->F[i].Etat == 1) {
            printf(">> Feu rouge detecte ! Ajout du vehicule en file d'attente...\n");
            ajouterVehiculeFileFeu(graph, i, *v);
            attendre_feu_rouge(graph, i);
            return;
        }
    }

    // Vérifier l'embouteillage
    if (nombreVehiculesArret(graph, v->positionNoeud) > 3) {
        printf(">> EMBOUTEILLAGE detecte ! Attente...\n");
        sleep(3);
        return;
    }

    // Trouver la prochaine route
    Arete* prochaine_route = NULL;
    for (int i = 0; i < graph->nbaretes; i++) {
        if (graph->A[i].Source == v->positionNoeud) {
            prochaine_route = &graph->A[i];
            break;
        }
    }

    if (!prochaine_route) {
        printf(">> Aucun chemin disponible.\n");
        return;
    }

    double tempsDeplacement = prochaine_route->Distance / v->vitesse;
    printf(">> Deplacement en cours... Temps estime : %.2f secondes\n", tempsDeplacement);
    sleep((int)tempsDeplacement);

    v->positionNoeud = prochaine_route->Destination;
    printf(">> Le vehicule principal est arrive a l'arret %d\n", v->positionNoeud);
}
