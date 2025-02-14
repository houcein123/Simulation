
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // Pour sleep()
#include <time.h>
#include <math.h>

#define INF 1000000000

/* ===================== Structures de Base ===================== */

/* Structure pour repr�senter un n�ud (arr�t) */
typedef struct {
    int ID;
    char Nom[50];
    char Type[50];
    int X;
    int Y;
} Noeud;

/* Structure pour repr�senter une ar�te (trajet) */
typedef struct {
    int Source;
    int Destination;
    double Distance;
    int Prioritaire;
    int feuxRouges;
    int capacite;
    int embouteillage ;
    int passagers;
    int etat; // 0 = Normal, 1 = Accident, 2 = Panne
    int flow;
} Arete;

/* Structure pour repr�senter un feu rouge */
typedef struct {
    int ID;
    int PositionNoeud;
    int Etat;      // 0 = Vert, 1 = Rouge
    int DureeRouge;
    int DureeVert;
    int TempsRestant;
} FeuRouge;

/* Structure pour repr�senter un passager */
typedef struct Passager {
    int ID;
    int destination;
    struct Passager* suivant;
} Passager;

/* Structure pour repr�senter un v�hicule */
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

/* Structure pour repr�senter un n�ud dans la file/pile de v�hicules */
typedef struct Node {
    Vehicule vehicule;
    struct Node* suivant;
} Node;

/* Structure pour repr�senter une file (queue) */
typedef struct {
    Node* tete;
    Node* queue;
} File;

/* Structure pour repr�senter une pile */
typedef struct {
    Node* sommet;
} Pile;

/* Structure pour repr�senter un n�ud dans la file d�attente des passagers */
typedef struct PassagerNode {
    Passager passager;
    struct PassagerNode* suivant;
} PassagerNode;

/* Structure pour repr�senter une file d�attente de passagers */
typedef struct {
    PassagerNode* tete;
    PassagerNode* queue;
} FilePassagers;

/* Structure pour repr�senter un graphe */
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
    FilePassagers* filesAttente; // Une file d'attente de passagers par arr�t
} Graphe;


/* ===================== Fonctions de gestion de file et pile ===================== */

void enfiler(File* file, Vehicule vehicule) {
    // Allocation de m�moire pour un nouveau n�ud
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return; // V�rification de l'allocation
    // Initialisation du nouveau n�ud avec le v�hicule et un pointeur suivant NULL
    newNode->vehicule = vehicule;
    newNode->suivant = NULL;
    // Si la file n'est pas vide, ajouter le nouveau n�ud � la fin
    if (file->queue)
        file->queue->suivant = newNode;
    // Mettre � jour la queue de la file
    file->queue = newNode;
    // Si la file �tait vide, mettre � jour la t�te
    if (!file->tete)
        file->tete = newNode;
}

Vehicule defiler(File* file) {
    // V�rifier si la file est vide
    if (!file->tete) {
        Vehicule v = {-1, "", 0, 0, 0, 0, 0.0, NULL}; // Valeur par d�faut
        return v;
    }
     // Sauvegarder l'�l�ment � retirer
    Node* temp = file->tete;
    Vehicule v = temp->vehicule;
    // Mettre � jour la t�te de la file
    file->tete = temp->suivant;
    // Si la file devient vide, mettre � jour la queue
    if (!file->tete)
        file->queue = NULL; 
    // Lib�rer la m�moire de l'ancien premier �l�ment
    free(temp);
    return v;
}

void empiler(Pile* pile, Vehicule vehicule) {
    // Allocation de m�moire pour un nouveau n�ud
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return; // V�rification de l'allocation
    // Initialisation du nouveau n�ud avec le v�hicule et l'ancien sommet
    newNode->vehicule = vehicule;
    newNode->suivant = pile->sommet;
    // Mise � jour du sommet de la pile
    pile->sommet = newNode;
}

Vehicule depiler(Pile* pile) {
    // V�rifier si la pile est vide
    if (!pile->sommet) {
        Vehicule v = {-1, "", 0, 0, 0, 0, 0.0, NULL}; // Valeur par d�faut
        return v;
    }
    // Sauvegarder l'�l�ment � retirer
    Node* temp = pile->sommet;
    Vehicule v = temp->vehicule;
    // Mettre � jour le sommet de la pile
    pile->sommet = temp->suivant;
    // Lib�rer la m�moire de l'ancien sommet
    free(temp);
    return v;
}
/* ===================== Fonctions d'ajout ===================== */
/* Ajouter un noeud */
void ajouternoeud(Graphe* graph, int id, char* name, char* type, int x, int y) {
    // V�rification si l'ID du n�ud est valide
    if (id >= graph->nbnoeuds) {
        printf("ID du noeud hors limite\n");
        return;
    }
    // R�cup�ration du n�ud et initialisation de ses attributs
    Noeud* noeud = &graph->N[id];
    noeud->ID = id;
    strcpy(noeud->Nom, name);
    strcpy(noeud->Type, type);
    noeud->X = x;
    noeud->Y = y;
}

/* Ajouter une ar�te (sans capacit�, on pourra par la suite la d�finir) */
void ajouterarete(Graphe* graph, int indice, int source, int destination, double distance, int prioritaire) {
    // V�rification si l'indice de l'ar�te est valide
    if (indice >= graph->nbaretes) {
        printf("ID d'arete hors limite\n");
        return;
    }
    // Initialisation de l'ar�te avec les valeurs fournies
    Arete* arete = &graph->A[indice];
    arete->Source = source;
    arete->Destination = destination;
    arete->Distance = distance;
    arete->Prioritaire = prioritaire;
    arete->capacite = 0; // Capacit� initialis�e � 0
    arete->flow = 0; // Flux initialis� � 0
    arete->etat = 0; // �tat initialis� � 0
}

/* Ajouter une ar�te avec capacit� (pour la gestion des flux) */
void ajouter_arete_flux(Graphe* graph, int indice, int source, int destination, double distance, int prioritaire, int capacity) {
    // V�rification si l'indice de l'ar�te est valide
    if (indice >= graph->nbaretes) {
        printf("ID d'arete hors limite\n");
        return;
    }
    // Initialisation de l'ar�te avec les valeurs fournies
    Arete* arete = &graph->A[indice];
    arete->Source = source;
    arete->Destination = destination;
    arete->Distance = distance;
    arete->Prioritaire = prioritaire;
    arete->capacite = capacity; // D�finition de la capacit�
    arete->flow = 0; // Flux initialis� � 0
}

/*fonction pour ajouter les feux rouges*/
void ajouterFeuRouge(Graphe* graph, int id, int position, int etat, int dureeRouge, int dureeVert) {
    // V�rification si l'ID du feu rouge est valide
    if (id >= graph->nbFeux) {
        printf("ID de feu rouge hors limite\n");
        return;
    }
    // Initialisation du feu rouge avec les valeurs fournies
    FeuRouge* feu = &graph->F[id];
    feu->ID = id;
    feu->PositionNoeud = position;
    feu->Etat = etat;
    feu->DureeRouge = dureeRouge;
    feu->DureeVert = dureeVert;
    feu->TempsRestant = dureeRouge; // Temps restant initialis� � la dur�e rouge
}
/* ========================================================================= */
/*                Fonctions de Gestion des Vehicules                     */
/* ========================================================================= */

/* Ajoute le v�hicule en attente dans la file g�n�rale */
void ajouterVehiculeEnAttente(Graphe* graph, Vehicule v) {
    // Affichage du v�hicule ajout� � la file d'attente
    printf("%s ID %d en attente a l'arret %d\n", v.type, v.ID, v.positionNoeud);
    // Ajout du v�hicule � la file de trafic
    enfiler(&graph->fileTrafic, v);
}

/* G�re le passage des v�hicules en attente */
void gererVehiculesEnAttente(Graphe* graph) {
    // V�rifie s'il y a des v�hicules en attente
    if (graph->fileTrafic.tete) {
        Vehicule v = defiler(&graph->fileTrafic);
        printf("%s ID %d avance depuis l'arret %d\n", v.type, v.ID, v.positionNoeud);
        // Ajoute le v�hicule dans l'historique des d�placements
        empiler(&graph->historiqueDeplacements, v);
    }
}

/* Ajoute un v�hicule dans la file d'un feu rouge */
void ajouterVehiculeFileFeu(Graphe* graph, int feuID, Vehicule v) {
    // V�rifie si l'ID du feu est valide
    if (feuID >= graph->nbFeux) return;
    // R�cup�re la file du feu rouge correspondant
    File* file = &graph->filesFeux[feuID];
    // Cr�ation d'un nouveau n�ud pour stocker le v�hicule
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) return;
    newNode->vehicule = v;
    newNode->suivant = NULL;
    // Ajoute le n�ud � la fin de la file
    if (file->queue)
        file->queue->suivant = newNode;
    file->queue = newNode;
    // Si la file �tait vide, mettre � jour la t�te
    if (!file->tete)
        file->tete = newNode;
    printf("%s ID %d ajoute dans la file du feu rouge %d\n", v.type, v.ID, feuID);
}

/* Lib�re les v�hicules dans la file d'un feu rouge */
void libererVehiculesFeu(Graphe* graph, int feuID) {
    // V�rifie si l'ID du feu est valide
    if (feuID >= graph->nbFeux) return;
    // R�cup�re la file du feu rouge correspondant
    File* file = &graph->filesFeux[feuID];
    // Lib�re tous les v�hicules en attente au feu rouge
    while (file->tete) {
        Node* temp = file->tete;
        Vehicule v = temp->vehicule;
        printf("%s ID %d avance depuis le feu rouge %d\n", v.type, v.ID, feuID);
        sleep(1); // Simulation du temps d'attente
        // Mise � jour de la t�te de la file
        file->tete = temp->suivant;
        // Si la file devient vide, mettre � jour la queue
        if (!file->tete)
            file->queue = NULL;
        // Lib�ration de la m�moire du n�ud
        free(temp);
    }
}

/* Retourne le nombre de v�hicules � un arr�t */
int nombreVehiculesArret(Graphe* graph, int arretID) {
    int count = 0;
    // V�rifie si le v�hicule principal est � l'arr�t donn�
    if (graph->principal.positionNoeud == arretID)
        count++;
    // Parcours des feux pour compter les v�hicules en attente
    int i;
    for (i = 0; i < graph->nbFeux; i++) {
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
/*                Fonctions de Gestion des Passagers                    */
/* ========================================================================= */

/* Embarque les passagers depuis la file d'attente de l'arr�t dans le v�hicule */
void embarquer_passagers(Graphe* graph, Vehicule* vehicule) {
    // R�cup�ration de la file d'attente des passagers � la position actuelle du v�hicule
    FilePassagers* file = &graph->filesAttente[vehicule->positionNoeud];
    // Tant que le v�hicule n'est pas plein et qu'il reste des passagers en attente
    while (vehicule->Npassager < vehicule->capaciteMax && file->tete) {
        // R�cup�ration du premier passager dans la file d'attente
        PassagerNode* pnode = file->tete;
        file->tete = file->tete->suivant; // Avancement de la file d'attente
        // Si la file devient vide, mettre � jour le pointeur de la queue
        if (!file->tete)
            file->queue = NULL;
        // Allocation d'un nouvel espace m�moire pour le passager embarqu�
        Passager* newPassager = (Passager*)malloc(sizeof(Passager));
        *newPassager = pnode->passager; // Copie des donn�es du passager
        // Ajout du passager en t�te de la liste des passagers du v�hicule
        newPassager->suivant = vehicule->passagers;
        vehicule->passagers = newPassager;
        vehicule->Npassager++; // Incr�mentation du nombre de passagers
        // Affichage de l'embarquement du passager
        printf("Passager ID %d embarqu�, destination: %d\n", newPassager->ID, newPassager->destination);
        // Lib�ration de l'ancienne structure du passager dans la file
        free(pnode);
    }
}

/* D�barque les passagers arriv�s � destination */
void debarquer_passagers(Vehicule* vehicule) {
    Passager* prev = NULL;
    Passager* current = vehicule->passagers;
    // Parcours de la liste des passagers
    while (current) {
        // V�rification si le passager est arriv� � destination
        if (current->destination == vehicule->positionNoeud) {
            printf("Passager ID %d a d�barqu�\n", current->ID);
            // Suppression du passager de la liste
            if (prev)
                prev->suivant = current->suivant;
            else
                vehicule->passagers = current->suivant;
            // Lib�ration de la m�moire allou�e au passager
            Passager* temp = current;
            current = (prev) ? prev->suivant : vehicule->passagers;
            free(temp);
            vehicule->Npassager--; // D�cr�mentation du nombre de passagers
        } else {
            prev = current;
            current = current->suivant;
        }
    }
}

/* Simule l'attente d'un v�hicule au feu rouge */
void attendre_feu_rouge(Graphe* graph, int feuID) {
    if (feuID >= 0) {
        // Affichage d'un message d'attente
        printf("Feu rouge d�tect� � l'arr�t %d ! Attente de %d secondes...\n",
        graph->F[feuID].PositionNoeud, graph->F[feuID].DureeRouge);
        // Pause du programme pendant la dur�e du feu rouge
        sleep(graph->F[feuID].DureeRouge);
        // Passage au feu vert
        graph->F[feuID].Etat = 0; // 0 repr�sente le vert
        graph->F[feuID].TempsRestant = graph->F[feuID].DureeVert;
        printf("Feu vert ! Le v�hicule peut continuer.\n");
    }
}

/* ===================== Cr�ation et initialisation du graphe ===================== */

/* Cr�e un graphe avec le nombre sp�cifi� de n�uds, d'ar�tes et de feux de signalisation */
Graphe* creergraphe(int Nbnoeuds, int Nbaretes, int NbFeux) {
    // Allocation de m�moire pour la structure Graphe
    Graphe* graph = (Graphe*)malloc(sizeof(Graphe));
    if (!graph) {
        printf("Erreur d'allocation m�moire pour le graphe !\n");
        return NULL;
    }
    // Initialisation des attributs du graphe
    graph->nbnoeuds = Nbnoeuds;
    graph->nbaretes = Nbaretes;
    graph->nbFeux = NbFeux;
    graph->nbVehicules = 0;
    graph->fileTrafic.tete = graph->fileTrafic.queue = NULL;
    graph->historiqueDeplacements.sommet = NULL;
    // Allocation de m�moire pour les n�uds, ar�tes et feux
    graph->N = (Noeud*)malloc(sizeof(Noeud) * Nbnoeuds);
    graph->A = (Arete*)malloc(sizeof(Arete) * Nbaretes);
    graph->F = (FeuRouge*)malloc(sizeof(FeuRouge) * NbFeux);
    graph->filesFeux = (File*)malloc(sizeof(File) * NbFeux);
    // V�rification de l'allocation m�moire
    if (!graph->N || !graph->A || !graph->F || !graph->filesFeux) {
        printf("Erreur d'allocation m�moire !\n");
        if (graph->N) free(graph->N);
        if (graph->A) free(graph->A);
        if (graph->F) free(graph->F);
        if (graph->filesFeux) free(graph->filesFeux);
        free(graph);
        return NULL;
    }
    // Initialisation des files associ�es aux feux
    int i;
    for (i = 0; i < NbFeux; i++) {
        graph->filesFeux[i].tete = NULL;
        graph->filesFeux[i].queue = NULL;
    }
    // Allocation de m�moire pour les files d'attente des passagers
    graph->filesAttente = (FilePassagers*)malloc(sizeof(FilePassagers) * Nbnoeuds);
    if (!graph->filesAttente) {
        printf("Erreur d'allocation m�moire pour les files d'attente !\n");
        free(graph->N);
        free(graph->A);
        free(graph->F);
        free(graph->filesFeux);
        free(graph);
        return NULL;
    }
    // Initialisation du v�hicule principal
    Vehicule* v = &graph->principal;
    v->vitesse = 5.0;
    // Initialisation des files d'attente des passagers
    for (i = 0; i < Nbnoeuds; i++) {
        graph->filesAttente[i].tete = NULL;
        graph->filesAttente[i].queue = NULL;
    }
    return graph;
}

/* ========================================================================= */
/*                   D�placement du V�hicule Principal                   */
/* ========================================================================= */

void deplacerVehiculePrincipal(Graphe* graph) {
    Vehicule* v = &graph->principal;
    // Si le v�hicule est arriv� � destination, on termine la simulation
    if (v->positionNoeud == v->destinationNoeud) {
        printf("\nLe vehicule principal est arrive a destination (noeud %d).\n", v->destinationNoeud);
        exit(0);
    }
    printf("\n>> Deplacement du vehicule principal depuis l'arret %d\n", v->positionNoeud);
    int i ;
    // Si un feu rouge bloque le passage
    for (i = 0; i < graph->nbFeux; i++) {
        if (graph->F[i].PositionNoeud == v->positionNoeud && graph->F[i].Etat == 1) {
            printf(">> Feu rouge detecte ! Ajout du vehicule en file d'attente...\n");
            ajouterVehiculeFileFeu(graph, i, *v);
            attendre_feu_rouge(graph, i);
            return;
        }
    }
    // V�rifier l'embouteillage
    if (nombreVehiculesArret(graph, v->positionNoeud) > 3) {
        printf(">> EMBOUTEILLAGE detecte ! Attente...\n");
        sleep(3);
        return;
    }
    // Trouver la prochaine route
    Arete* prochaine_route = NULL;
    for (i = 0; i < graph->nbaretes; i++) {
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


/* ========================================================================= */
/*                   ALGORITHMES DE CHEMIN (Dijkstra & A*)                 */
/* ========================================================================= */

/* --- Dijkstra Standard pour calculer le chemin le plus court --- */
void Dijkstra(Graphe* graph, int source, int target){
    int n = graph->nbnoeuds; // Nombre de noeuds du graphe 
    int dist[n] /* Contient la distance minimale connue depuis la source jusqu'� chaque n�ud */,
	prev[n] /* Tableau des pr�c�dents */, 
	visited[n] /* Tableau pour indiquer la visite d'une ar�te */, 
	i, j, k;
    for(i = 0; i < n; i++){
        dist[i] = INF; // Initialis� tout les ar�tes par l'infini pour signifier que ces distances sont inconnues au d�part
        prev[i] = -1; // Pour chaque n�ud, ce tableau enregistre le n�ud pr�c�dent sur le chemin le plus court (initialis� � -1 pour indiquer l'absence de pr�d�cesseur)  
        visited[i] = 0; // Indique si la distance minimale d'un n�ud est d�termin�e. Tous les n�uds sont initialement non visit�s (valeur 0)
    }
    dist[source] = 0; // Initialis� la distance du n�ud source par 0  
    for(i = 0; i < n; i++){
        int u = -1, minDist = INF; // Initialis� la distance minimale par l'ifini 
        for(j = 0; j < n; j++){
            if(!visited[j] && dist[j] < minDist){ // Pour chaque it�ration, on cherche parmi tous les n�uds non visit�s celui ayant la distance minimale actuelle
                minDist = dist[j];
                u = j;
            }
        }
        if(u == -1) break; // Si aucun n�ud n'est trouv� (cas o� u reste �gal � -1), cela signifie que tous les n�uds accessibles ont �t� trait�s, et on sort de la boucle
        visited[u] = 1; // Marquer la visite de l'ar�te 
        if(u == target) break; // Si u est le n�ud cible, on peut arr�ter car le chemin le plus court a �t� trouv� /* Parcours de tous les n�uds jusqu'� trouver le n�uds cible (target)*/
        // Mise � jour des distances (relaxation)
        /* Parcours de toutes les ar�tes sortant de u */
        for(k = 0; k < graph->nbaretes; k++){ // Pour le n�ud actuellement s�lectionn� u, on parcourt toutes les ar�tes du graphe
            Arete* a = &graph->A[k];
            if(a->Source == u){ // Pour chaque ar�te qui part de u, on identifie le n�ud voisin v (la destination de l'ar�te)
                int v = a->Destination;
                if(!visited[v]){ // Si v n�a pas encore �t� visit�, on calcule une nouvelle distance potentielle alt pour atteindre v en passant par u (le cours)
                    int alt = dist[u] + (int)a->Distance; // (int) indique juste la partie entiere de a->Distance  
                    if(alt < dist[v]){ // Si cette nouvelle distance alt est inf�rieure � la distance actuelle enregistr�e pour v (dist[v]), on met � jour
                        dist[v] = alt; // La nouvelle distance
                        prev[v] = u; // Pour indiquer que le meilleur chemin pour atteindre v passe par u
                    }
                }
            }
        }
    }
    if(dist[target] == INF) // On v�rifie si la distance vers le n�ud cible est rest�e infinie
        printf("Aucun chemin trouve de %d vers %d.\n", source, target); // Si c'est le cas, cela signifie qu'il n'existe aucun chemin entre la source et la cible
    else{ // Sinon, on affiche la distance minimale trouv�e
        printf("Chemin le plus court de %d vers %d (distance: %d): ", source, target, dist[target]);
        int path[n], count = 0, current = target;
		// Pour reconstituer le chemin, on utilise le tableau prev en partant du n�ud cible et en remontant jusqu'� la source
        while(current != -1){
            path[count++] = current;
            current = prev[current];
        }
		// Les n�uds sont stock�s dans le tableau path
        for(i = count - 1; i >= 0; i--) // Enfin, le chemin est affich� dans l'ordre (de la source � la cible) 
            printf("%d ", path[i]);
        printf("\n");
    }
}


/* Dijkstra modifi� pour int�grer les routes prioritaires (r�duction des poids) */
// M�me fonctionnalit� que Dijkstra Standard, mais avant d'ajouter le co�t d'une ar�te, on v�rifie si celle-ci est prioritaire. Si c'est le cas, le co�t est multipli� par un facteur de r�duction pour favoriser ces routes
/* Algorithme de Dijkstra modifi� avec priorit� pour les routes prioritaires */
void Dijkstra_priority(Graphe* graph, int source, int target, double reduction_factor){
    int n = graph->nbnoeuds;
    int dist[n], prev[n], visited[n], i, j, k;
    // Initialisation des tableaux de distances, pr�d�cesseurs et �tat de visite
    for(i = 0; i < n; i++){
        dist[i] = INF;
        prev[i] = -1;
        visited[i] = 0;
    }
    dist[source] = 0;
    // Boucle principale de l'algorithme
    for(i = 0; i < n; i++){
        int u = -1, minDist = INF;
        // S�lection du n�ud avec la plus courte distance non encore visit�
        for(j = 0; j < n; j++){
            if(!visited[j] && dist[j] < minDist){
                minDist = dist[j];
                u = j;
            }
        }
        if(u == -1) break;
        visited[u] = 1;
        if(u == target) break;
        // Mise � jour des distances des voisins du n�ud actuel
        for(k = 0; k < graph->nbaretes; k++){
            Arete* a = &graph->A[k];
            if(a->Source == u){
                int v = a->Destination;
                if(!visited[v]){
                    double weight = a->Distance;
                    if(a->Prioritaire){
                        weight *= reduction_factor;  // R�duction du poids pour les routes prioritaires
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
    // Affichage du r�sultat
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

/* Fonction utilitaire : calcul de la distance euclidienne entre deux n�uds */
double Euclidean_distance(Noeud a, Noeud b) {
    return sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));
}

/* Algorithme A* pour trouver un chemin optimis� � l'aide d'une heuristique (distance euclidienne) */
void A_star(Graphe* graph, int source, int target){
    int n = graph->nbnoeuds;
    double g[n], f[n];
    int prev[n], closed[n], open[n], i, k;
    // Initialisation des tableaux
    for(i = 0; i < n; i++){
        g[i] = INF; // Co�t r�el du chemin depuis la source jusqu�au n�ud i
        f[i] = INF; /* Co�t total estim� pour atteindre la cible en passant par i */
        prev[i] = -1; // Tableau des pr�d�cesseurs pour reconstruire le chemin
        closed[i] = 0; // Indique si le n�ud a �t� explor�
        open[i] = 0; // Indique si le n�ud est dans la liste � explorer
    }
    // Initialisation des valeurs pour la source
    g[source] = 0;
    f[source] = Euclidean_distance(graph->N[source], graph->N[target]);
    open[source] = 1;
    // Boucle principale de l'algorithme
    while(1){
        int current = -1;
        double minF = 1e9;
        // S�lection du n�ud avec le plus petit co�t estim� (f)
        for(i = 0; i < n; i++){
            if(open[i] && f[i] < minF){
                minF = f[i];
                current = i;
            }
        }
        if(current == -1) break;
        if(current == target){
            printf("Chemin A* de %d vers %d (co�t estim�: %.2f): ", source, target, g[target]);
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
        // Marquer le n�ud comme explor�
        open[current] = 0;
        closed[current] = 1;
        // Exploration des voisins
        for(k = 0; k < graph->nbaretes; k++){
            Arete* a = &graph->A[k];
            if(a->Source == current){
                int neighbor = a->Destination;
                if(closed[neighbor]) continue;
                double tentative_g = g[current] + a->Distance;
                if(!open[neighbor]) open[neighbor] = 1;
                else if(tentative_g >= g[neighbor]) continue;
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
/* Utilitaire : recherche en largeur (BFS) dans le graphe r�siduel */
int BFS_residual(int **residual, int nb_noeuds, int source, int sink, int parent[]) {
    int *visited = (int*)calloc(nb_noeuds, sizeof(int));
    int *queue = (int*)malloc(nb_noeuds * sizeof(int));
    int front = 0, rear = 0;
    queue[rear++] = source;
    visited[source] = 1;
    parent[source] = -1;
    
    while (front < rear) {
        int u = queue[front++];
        int v ;
        for (v = 0; v < nb_noeuds; v++) {
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

/* Impl�mentation de Ford-Fulkerson (via Edmonds-Karp) */

int Ford_Fulkerson(Graphe* graph, int source, int sink) {
    int nb = graph->nbnoeuds, i;
    /* Allocation et initialisation de la matrice r�siduelle */
    int **residual = (int**)malloc(nb * sizeof(int*));
    for(i = 0; i < nb; i++){
        residual[i] = (int*)calloc(nb, sizeof(int));
    }
    /* Remplissage de la matrice r�siduelle avec les capacit�s des ar�tes */
    for(i = 0; i < graph->nbaretes; i++){
        Arete* a = &graph->A[i];
        residual[a->Source][a->Destination] = a->capacite;
    }
    int max_flow = 0;
    int *parent = (int*)malloc(nb * sizeof(int));
    /* Boucle principale : Tant qu'un chemin augmentant existe */
    while(BFS_residual(residual, nb, source, sink, parent)){
        /* Trouver la capacit� minimale sur le chemin augmentant */
        int path_flow = INF;
        int v = sink;
        while(v != source){
            int u = parent[v];
            if(residual[u][v] < path_flow)
                path_flow = residual[u][v];
            v = u;
        }
        /* Mettre � jour la capacit� r�siduelle des ar�tes et des ar�tes inverses */
        v = sink;
        while(v != source){
            int u = parent[v];
            residual[u][v] -= path_flow;
            residual[v][u] += path_flow;
            v = u;
        }
        /* Ajouter le flot du chemin augmentant au flot total */
        max_flow += path_flow;
    }
    /* Lib�ration de la m�moire allou�e */
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
/* ===================== Affichage du graphe ===================== */

void afficher_graph(Graphe* graph) {
    printf("\n======== RESEAU DE TRANSPORT ========\n");
    printf("Stations (%d) :\n", graph->nbnoeuds);
    int i;
    for (i = 0; i < graph->nbnoeuds; i++) {
        Noeud* n = &graph->N[i];
        printf(" - Station : ID = %d : '%s' (%s), Coordonnees : (%d, %d)\n",
               n->ID, n->Nom, n->Type, n->X, n->Y);
    }
    printf("\nTrajets (%d) :\n", graph->nbaretes);
    for (i = 0; i < graph->nbaretes; i++) {
        Arete* a = &graph->A[i];
       printf(" - Trajet %d : De l'arret %d a l'arret %d, Distance : %.2f, Prioritaire : %d\n",
               i, a->Source, a->Destination, a->Distance, a->Prioritaire);
    }
}
/* ========================================================================= */
/*                                MAIN                                       */
/* ========================================================================= */
int main() {
    srand(time(NULL));

    /*------------------ Partie Simulation ------------------*/
    /* Cr�ation du graphe de transport (simulation) :
       6 noeuds, 8 aretes */
    Graphe* graph = creergraphe(6, 8, 2);
    if (!graph) return 1;

    /* Ajout des arr�ts */
    ajouternoeud(graph, 0, "Gare Centrale", "Multi", 10, 20);
    ajouternoeud(graph, 1, "Place Ville", "Bus", 15, 30);
    ajouternoeud(graph, 2, "Station Metro", "Metro", 20, 40);
    ajouternoeud(graph, 3, "Parc Principal", "Bus", 25, 35);
    ajouternoeud(graph, 4, "Centre Commercial", "Multi", 30, 25);
    ajouternoeud(graph, 5, "Zone Industrielle", "Bus", 35, 45);

    /* Ajout des trajets */
    ajouterarete(graph, 0, 0, 1, 5.0, 1);
    ajouterarete(graph, 1, 1, 2, 3.5, 0);
    ajouterarete(graph, 2, 2, 3, 4.0, 1);
    ajouterarete(graph, 3, 3, 4, 3.0, 1);
    ajouterarete(graph, 4, 4, 5, 4.5, 0);
    ajouterarete(graph, 5, 0, 2, 8.0, 1);
    ajouterarete(graph, 6, 1, 3, 6.0, 0);
    ajouterarete(graph, 7, 2, 4, 5.5, 1);

    /* Affichage du graphe */
    afficher_graph(graph);

    /* Saisie de la source, de la destination et du type de v�hicule */
    int source, destination;
    char vehicule[20];
    char continuer ;
    do{
    printf("\n=== ESPACE UTILISATEUR : ===\n");
    printf("Source (ID de la station) : ");
    scanf("%d", &source);
    Vehicule* v = &graph->principal;
    v->positionNoeud = source;
    printf("Destination (ID de la station) : ");
    scanf("%d", &destination);
    v->destinationNoeud = destination;
    printf("Type de vehicule (Bus, Metro, Voiture, etc.) : ");
    scanf("%s", vehicule);
    strcpy(v->type,vehicule);

    /* Application des algorithmes pour le v�hicule choisi */
    printf("\n=== Resultats pour le vehicule : %s ===\n", vehicule);

    // 1. Dijkstra standard
    printf("\n=== Calcul du chemin le plus court (Dijkstra standard) ===\n");
    Dijkstra(graph, source, destination);

    // 2. Dijkstra avec priorit�
    double reduction_factor = 0.5; // Facteur de r�duction pour les routes prioritaires
    printf("\n=== Calcul du chemin optimise (Dijkstra avec priorite) ===\n");
    Dijkstra_priority(graph, source, destination, reduction_factor);
    // 3. A* pour un chemin optimis� avec heuristique
    printf("\n=== Calcul du chemin optimise (A*) ===\n");
    A_star(graph, source, destination);
    // 4. Simulation du d�placement
    printf("\n=== Simulation du deplacement du vehicule ===\n");
    while (v->positionNoeud != v->destinationNoeud) {
    deplacerVehiculePrincipal(graph);
    sleep(3);
    }
    
    /* Demander � l'utilisateur s'il souhaite continuer */
    printf("\nVoulez-vous continuer ? (o/n) : ");
    scanf(" %c", &continuer); // Notez l'espace avant %c pour ignorer les espaces blancs
    }while (continuer == 'o' || continuer == 'O');

    /* Lib�ration de la m�moire */
    free(graph->N);
    free(graph->A);
    free(graph->F);
    free(graph);
     printf("\nProgramme termine. Au revoir !\n");

    return 0;
}
