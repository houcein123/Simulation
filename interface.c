#include <gtk/gtk.h>
#include "Graphe.h"

// Rayon pour détecter un clic sur un nœud (en pixels)
#define NODE_CLICK_RADIUS 5

// Vitesses pour la simulation (unités arbitraires)
#define SPEED_CAR 60.0
#define SPEED_BUS 40.0
#define SPEED_TRUCK 30.0

// Durées d'arrêt (en secondes)
#define PAUSE_RED_LIGHT 3.0
#define PAUSE_BUS 2.0
#define PAUSE_TRAFFIC_JAM 3.0
#define PAUSE_PASSENGERS 2.0

//    ajouterarete
/* ===================== Structure et Variables de l'Application ===================== */
typedef struct {
    /* Widgets principaux */
    GtkWidget *window;
    GtkWidget *stack;

    /* Page d'entrée */
    GtkWidget *page_input;
    GtkWidget *drawing_area;      // Affiche la carte et les nœuds
    GtkWidget *combo_vehicule;    // Choix du type de véhicule
    GtkWidget *btn_valider;       // Bouton "Valider"

    /* Page des résultats */
    GtkWidget *page_result;
    GtkWidget *header_box;        // Contiendra "Retour" et "Quitter"
    GtkWidget *btn_back;
    GtkWidget *btn_quit;
    GtkWidget *result_drawing_area;  // Zone de dessin pour l'animation et le chemin
    GtkWidget *label_resultats;      // Informations générales finales
    GtkWidget *temp_message_label;   // Message temporaire (événement en cours)

    /* Le graphe */
    Graphe *graphe;

    /* Sélections */
    int selected_source;
    int selected_destination;

    /* Image de fond */
    GdkPixbuf *map_pixbuf;

    /* Chemin calculé par Dijkstra */
    int *chemin;
    int chemin_length;

    /* Variables de simulation pour l'animation */
    int simulation_index;          // Indice du segment courant dans le chemin
    double simulation_progress;    // Progression dans le segment courant [0,1]
    double simulation_pause_remaining; // Temps de pause restant (en secondes)
    double simulation_total_time;  // Temps total de simulation (incluant les pauses)
    double vehicle_speed;          // Vitesse de simulation (selon type)
    char selected_vehicle_type[20]; // Type de véhicule sélectionné ("Voiture", "Bus", "Camion")
    char event_reason[100];        // Raison de l'arrêt ("Feu rouge", "Embouteillages", etc.)
    guint simulation_timer_id;     // ID du timer d'animation
} AppData;

/* ========================================================= */
/* Appliquer un scaling dans le dessin */
static void apply_scaling(cairo_t *cr, GtkWidget *widget, GdkPixbuf *pixbuf) {
    int alloc_width = gtk_widget_get_allocated_width(widget);
    int alloc_height = gtk_widget_get_allocated_height(widget);
    int base_width, base_height;
    if (pixbuf) {
        base_width = gdk_pixbuf_get_width(pixbuf);
        base_height = gdk_pixbuf_get_height(pixbuf);
    } else {
        base_width = 600;
        base_height = 500;
    }
    double scale_x = (double)alloc_width / base_width;
    double scale_y = (double)alloc_height / base_height;
    cairo_scale(cr, scale_x, scale_y);
}

/* ========================================================= */
/* Dessiner une flèche sur un segment */
static void draw_arrow(cairo_t *cr, double x1, double y1, double x2, double y2) {
    double angle = atan2(y2 - y1, x2 - x1);
    double arrow_length = 10;
    double arrow_angle = M_PI / 6;
    double x3 = x2 - arrow_length * cos(angle - arrow_angle);
    double y3 = y2 - arrow_length * sin(angle - arrow_angle);
    double x4 = x2 - arrow_length * cos(angle + arrow_angle);
    double y4 = y2 - arrow_length * sin(angle + arrow_angle);
    cairo_move_to(cr, x2, y2);
    cairo_line_to(cr, x3, y3);
    cairo_move_to(cr, x2, y2);
    cairo_line_to(cr, x4, y4);
    cairo_stroke(cr);
}

/* ===================== Dessin de la page d'entrée ===================== */
static gboolean on_draw_input(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    apply_scaling(cr, GTK_WIDGET(area), data->map_pixbuf);
    if (data->map_pixbuf) {
        gdk_cairo_set_source_pixbuf(cr, data->map_pixbuf, 0, 0);
        cairo_paint(cr);
    }
    for (int i = 0; i < data->graphe->nbnoeuds; i++) {
        Noeud *n = &data->graphe->N[i];
        cairo_arc(cr, n->X, n->Y, NODE_CLICK_RADIUS, 0, 2 * M_PI);
        if (i == data->selected_source)
            cairo_set_source_rgb(cr, 0, 1, 0);
        else if (i == data->selected_destination)
            cairo_set_source_rgb(cr, 1, 0, 0);
        else
            cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_stroke(cr);
    }
    return FALSE;
}

/* ===================== Dessin de la page des résultats ===================== */
static gboolean on_draw_result(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    apply_scaling(cr, GTK_WIDGET(area), data->map_pixbuf);
    if (data->map_pixbuf) {
        gdk_cairo_set_source_pixbuf(cr, data->map_pixbuf, 0, 0);
        cairo_paint(cr);
    }
    for (int i = 0; i < data->graphe->nbnoeuds; i++) {
        Noeud *n = &data->graphe->N[i];
        cairo_arc(cr, n->X, n->Y, NODE_CLICK_RADIUS, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_stroke(cr);
    }
    if (data->chemin_length > 0 && data->chemin != NULL) {
        cairo_set_line_width(cr, 4.0);
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr, data->graphe->N[data->chemin[0]].X, data->graphe->N[data->chemin[0]].Y);
        for (int i = 1; i < data->chemin_length; i++) {
            cairo_line_to(cr, data->graphe->N[data->chemin[i]].X, data->graphe->N[data->chemin[i]].Y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 2.0);
        for (int i = 1; i < data->chemin_length; i++) {
            double x1 = data->graphe->N[data->chemin[i-1]].X;
            double y1 = data->graphe->N[data->chemin[i-1]].Y;
            double x2 = data->graphe->N[data->chemin[i]].X;
            double y2 = data->graphe->N[data->chemin[i]].Y;
            draw_arrow(cr, x1, y1, x2, y2);
        }
    }
    /* Dessiner le véhicule animé */
    if (data->chemin && data->simulation_index < data->chemin_length - 1) {
        int cur = data->chemin[data->simulation_index];
        int nxt = data->chemin[data->simulation_index + 1];
        Noeud start = data->graphe->N[cur];
        Noeud end = data->graphe->N[nxt];
        double t = data->simulation_progress;
        double x = start.X + t * (end.X - start.X);
        double y = start.Y + t * (end.Y - start.Y);
        cairo_arc(cr, x, y, NODE_CLICK_RADIUS + 3, 0, 2*M_PI);
        if (g_strcmp0(data->selected_vehicle_type, "Bus") == 0)
            cairo_set_source_rgb(cr, 0, 0, 1);
        else if (g_strcmp0(data->selected_vehicle_type, "Camion") == 0)
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        else
            cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        const char *label = (g_strcmp0(data->selected_vehicle_type, "Bus") == 0) ? "B" :
                            (g_strcmp0(data->selected_vehicle_type, "Camion") == 0) ? "C" : "V";
        cairo_move_to(cr, x - 4, y + 4);
        cairo_show_text(cr, label);
    }
    return FALSE;
}

/* ===================== Gestion du clic sur la page d'entrée ===================== */
static gboolean on_map_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    AppData *data = user_data;
    g_print("Clic détecté aux coordonnées (%.2f, %.2f)\n", x, y);
    int alloc_width = gtk_widget_get_allocated_width(GTK_WIDGET(data->drawing_area));
    int alloc_height = gtk_widget_get_allocated_height(data->drawing_area);
    int base_width = gdk_pixbuf_get_width(data->map_pixbuf);
    int base_height = gdk_pixbuf_get_height(data->map_pixbuf);
    double scale_x = (double)alloc_width / base_width;
    double scale_y = (double)alloc_height / base_height;
    double orig_x = x / scale_x;
    double orig_y = y / scale_y;
    g_print("Coordonnées converties : (%.2f, %.2f)\n", orig_x, orig_y);
    for (int i = 0; i < data->graphe->nbnoeuds; i++) {
        Noeud *n = &data->graphe->N[i];
        double dx = orig_x - n->X;
        double dy = orig_y - n->Y;
        if (sqrt(dx * dx + dy * dy) <= NODE_CLICK_RADIUS) {
            if (data->selected_source == -1) {
                data->selected_source = i;
                g_print("Source sélectionnée : nœud %d\n", i);
            } else if (data->selected_destination == -1) {
                data->selected_destination = i;
                g_print("Destination sélectionnée : nœud %d\n", i);
            } else {
                data->selected_source = i;
                data->selected_destination = -1;
                g_print("Nouvelle source sélectionnée : nœud %d\n", i);
            }
            gtk_widget_queue_draw(data->drawing_area);
            break;
        }
    }
    return TRUE;
}

/* ===================== Simulation d'animation ===================== */
gboolean simulation_update(gpointer user_data) {
    AppData *data = user_data;
    double dt = 0.03; // 30 ms
    data->simulation_total_time += dt; // Cumuler le temps total

    /* Si une pause est active, on décrémente le compte à rebours */
    if (data->simulation_pause_remaining > 0) {
        data->simulation_pause_remaining -= dt;
        char msg[100];
        snprintf(msg, sizeof(msg), "%s : %.1f s restants", data->event_reason, data->simulation_pause_remaining);
        gtk_label_set_text(GTK_LABEL(data->temp_message_label), msg);
        gtk_widget_queue_draw(data->result_drawing_area);
        return TRUE;
    } else {
        gtk_label_set_text(GTK_LABEL(data->temp_message_label), "");
    }

    /* Fin de simulation */
    if (data->simulation_index >= data->chemin_length - 1) {
        char final_msg[256];
        snprintf(final_msg, sizeof(final_msg),
                 "Parcours terminé.\nChemin :  ");
        for (int i = 0; i < data->chemin_length; i++) {
            char tmp[10];
            sprintf(tmp, "%d ", data->chemin[i]);
            strcat(final_msg, tmp);
        }
        char summary[128];
        snprintf(summary, sizeof(summary),
                 "\nTemps total de parcours : %.1f s", data->simulation_total_time);
        strcat(final_msg, summary);
        gtk_label_set_text(GTK_LABEL(data->label_resultats), final_msg);
        return FALSE;
    }

    int cur = data->chemin[data->simulation_index];
    int nxt = data->chemin[data->simulation_index + 1];
    Noeud start = data->graphe->N[cur];
    Noeud end = data->graphe->N[nxt];
    double distance = Euclidean_distance(start, end);
    double speed = data->vehicle_speed;
    Arete *edge = NULL;
    for (int i = 0; i < data->graphe->nbaretes; i++){
        if (data->graphe->A[i].Source == cur && data->graphe->A[i].Destination == nxt) {
            edge = &data->graphe->A[i];
            break;
        }
    }
    if(edge && edge->embouteillages)
        speed *= 0.5;
    double travel_time = distance / speed;
    data->simulation_progress += dt / travel_time;
    if (data->simulation_progress >= 1.0) {
        data->simulation_progress = 0.0;
        data->simulation_index++;

        /* Réinitialisation de la pause pour le nouveau segment */
        data->simulation_pause_remaining = 0.0;

        if(edge) {
            if(edge->feuxRouges) {
                data->simulation_pause_remaining = PAUSE_RED_LIGHT;
                strcpy(data->event_reason, "Feu rouge");
            } else if(edge->embouteillages) {
                data->simulation_pause_remaining = PAUSE_TRAFFIC_JAM;
                strcpy(data->event_reason, "Embouteillages");
            } else if(edge->passagers) {
                data->simulation_pause_remaining = PAUSE_PASSENGERS;
                strcpy(data->event_reason, "Passagers");
            }
            /* Pour les bus, on n'applique l'arrêt d'embarquement/débarquement
               que si le nœud de destination figure dans la liste des arrêts prévus */
            else if(g_strcmp0(data->selected_vehicle_type, "Bus") == 0) {
                int busStopNodes[] = { 2, 32, 46, 178, 47, 173, 200, 140, 125, 137, 51, 88, 111 };  //
                int numBusStops = sizeof(busStopNodes) / sizeof(busStopNodes[0]);
                int isBusStop = 0;
                for(int i = 0; i < numBusStops; i++){
                    if(nxt == busStopNodes[i]){
                        isBusStop = 1;
                        break;
                    }
                }
                if(isBusStop){
                    data->simulation_pause_remaining = PAUSE_BUS;
                    strcpy(data->event_reason, "Embarquement/débarquement");
                }
            }
        }
    }
    gtk_widget_queue_draw(data->result_drawing_area);
    return TRUE;
}


/* ===================== Callback du bouton Valider ===================== */
static void on_valider(GtkButton *button, gpointer user_data) {
    AppData *data = user_data;
    if (data->selected_source == -1) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Veuillez choisir la source.");
        gtk_window_present(GTK_WINDOW(dialog));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), dialog);
        return;
    }
    if (data->selected_destination == -1) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Veuillez choisir la destination.");
        gtk_window_present(GTK_WINDOW(dialog));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), dialog);
        return;
    }
    const gchar *vehicule = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->combo_vehicule));
    if (!vehicule) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Veuillez choisir un type de véhicule.");
        gtk_window_present(GTK_WINDOW(dialog));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), dialog);
        return;
    }
    g_print("Véhicule sélectionné : %s\n", vehicule);
    Noeud *n_source = &data->graphe->N[data->selected_source];
    Noeud *n_dest = &data->graphe->N[data->selected_destination];
    double distance = Euclidean_distance(*n_source, *n_dest);
    double speed = SPEED_CAR;
    if (g_strcmp0(vehicule, "Bus") == 0)
        speed = SPEED_BUS;
    else if (g_strcmp0(vehicule, "Camion") == 0)
        speed = SPEED_TRUCK;
    double duration = distance / speed;
    CheminResult res = dijkstra(data->graphe, data->selected_source, data->selected_destination);
    if (res.longueur == 0) {
        gtk_label_set_text(GTK_LABEL(data->label_resultats), "Aucun Chemin trouvé");
        g_print("Aucun Chemin trouvé.\n");
    } else {
        data->chemin = res.chemin;
        data->chemin_length = res.longueur;
        char chemin_str[100] = "";
        for (int i = 0; i < res.longueur; i++){
            char temp[10];
            sprintf(temp, "%d ", data->chemin[i]);
            strcat(chemin_str, temp);
        }
        char info[256];
        if (g_strcmp0(vehicule, "Bus") == 0)
            strcpy(info, "Info : Le Bus s'arrêtera au feu rouge et pour l'embarquement/débarquement.");
        else if (g_strcmp0(vehicule, "Camion") == 0)
            strcpy(info, "Info : Le Camion roulera plus lentement en cas de travaux sur la route.");
        else
            strcpy(info, "Info : La Voiture ne subira pas de ralentissement particulier.");
        char resultText[512];
        snprintf(resultText, sizeof(resultText),
                 "Le plus court chemin: %s\nDistance: %.2f \nDurée de base: %.2f s\n(Véhicule: %s)\n%s",
                 chemin_str, distance, duration, vehicule, info);
        gtk_label_set_text(GTK_LABEL(data->label_resultats), resultText);
        g_print("Chemin Dijkstra: %s\n", chemin_str);
    }
    /* Initialiser la simulation */
    data->simulation_index = 0;
    data->simulation_progress = 0.0;
    data->simulation_pause_remaining = 0.0;
    data->simulation_total_time = 0.0;
    strcpy(data->selected_vehicle_type, vehicule);
    data->vehicle_speed = (g_strcmp0(vehicule, "Bus") == 0) ? SPEED_BUS :
                           (g_strcmp0(vehicule, "Camion") == 0) ? SPEED_TRUCK : SPEED_CAR;
    data->simulation_timer_id = g_timeout_add(30, simulation_update, data);
    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "page_result");
}

/* ===================== Callback du bouton Retour ===================== */
static void on_back(GtkButton *button, gpointer user_data) {
    AppData *data = user_data;
    data->selected_source = -1;
    data->selected_destination = -1;
    if (data->chemin) {
        free(data->chemin);
        data->chemin = NULL;
        data->chemin_length = 0;
    }
    gtk_label_set_text(GTK_LABEL(data->label_resultats), "");
    gtk_widget_queue_draw(data->drawing_area);
    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "page_input");
}

/* ===================== Callback du bouton Quitter ===================== */
static void on_quit(GtkButton *button, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(((AppData *)user_data)->window));
}

/* ===================== Initialisation du graphe ===================== */
static void init_graph(AppData *data) {

    data->graphe = creergraphe(400, 1000, 10);

    data->graphe->nbnoeuds = 400;
    ajouternoeud(data->graphe, 0, "N0", "Station", 7, 33);
    ajouternoeud(data->graphe, 1, "N1", "Station", 69, 70);
    ajouternoeud(data->graphe, 2, "N2", "Station", 150, 64);
    ajouternoeud(data->graphe, 3, "N3", "Station", 211, 57);
    ajouternoeud(data->graphe, 4, "N4", "Station", 278, 54);
    ajouternoeud(data->graphe, 5, "N5", "Station", 291, 52);
    ajouternoeud(data->graphe, 6, "N6", "Station", 390,44);
    ajouternoeud(data->graphe, 7, "N7", "Station", 439, 40);
    ajouternoeud(data->graphe, 8, "N8", "Station", 499, 6);
    ajouternoeud(data->graphe, 9, "N9", "Station", 501, 37);
    ajouternoeud(data->graphe, 10, "N10", "Station", 539, 33);
    ajouternoeud(data->graphe, 11, "N11", "Station", 580, 2);
    ajouternoeud(data->graphe, 12, "N12", "Station", 615, 51);
    ajouternoeud(data->graphe, 13, "N13", "Station", 641, 31);
    ajouternoeud(data->graphe, 14, "N14", "Station", 662, 56);
    ajouternoeud(data->graphe, 16, "N15", "Station", 745, 3);
    ajouternoeud(data->graphe, 15, "N16", "Station", 747,29);
    ajouternoeud(data->graphe, 17, "N17", "Station", 906, 33);
    ajouternoeud(data->graphe, 18, "N18", "Station", 988, 36);
        //
    ajouternoeud(data->graphe, 19, "N19", "Station", 388, 76);
    ajouternoeud(data->graphe, 206, "N19.7", "Station", 435, 69); // noeud 19.7
    ajouternoeud(data->graphe, 20, "N20", "Station", 566, 90);
    ajouternoeud(data->graphe, 21, "N21", "Station", 576, 81);
    ajouternoeud(data->graphe, 22, "N22", "Station", 678, 80);
    ajouternoeud(data->graphe, 23, "N23", "Station", 692, 97);
    ajouternoeud(data->graphe, 24, "N24", "Station", 782, 99);
    ajouternoeud(data->graphe, 27, "N25", "Station", 807, 73);
	ajouternoeud(data->graphe, 26, "N26", "Station", 805, 95);
    ajouternoeud(data->graphe, 25, "N27", "Station", 804, 114);
    ajouternoeud(data->graphe, 28, "N28", "Station", 844, 70);
    ajouternoeud(data->graphe, 29, "N29", "Station", 923, 98);
    ajouternoeud(data->graphe, 30, "N30", "Station", 928, 104);
    ajouternoeud(data->graphe, 207, "N30.17", "Station", 936, 83); // noeud 30.17
        //
    ajouternoeud(data->graphe, 31, "N31", "Station", 133, 158);
    ajouternoeud(data->graphe, 32, "N32", "Station", 284, 125);
    ajouternoeud(data->graphe, 33, "N33", "Station", 296, 124);
    ajouternoeud(data->graphe, 34, "N34", "Station", 509, 133);
    ajouternoeud(data->graphe, 35, "N35", "Station", 558, 137);
    ajouternoeud(data->graphe, 36, "N36", "Station", 585, 117);
    ajouternoeud(data->graphe, 37, "N37", "Station", 691, 130);
    ajouternoeud(data->graphe, 38, "N38", "Station", 708, 118);
    ajouternoeud(data->graphe, 39, "N39", "Station", 741, 133);
    ajouternoeud(data->graphe, 40, "N40", "Station", 762, 163);
    ajouternoeud(data->graphe, 41, "N41", "Station", 784, 147);
    ajouternoeud(data->graphe, 42, "N42", "Station", 843, 162);
    ajouternoeud(data->graphe, 43, "N43", "Station", 877, 135);
        //
    ajouternoeud(data->graphe, 44, "N44", "Station", 206, 220);
    ajouternoeud(data->graphe, 45, "N45", "Station", 225, 217);
    ajouternoeud(data->graphe, 46, "N46", "Station", 293, 214);
    ajouternoeud(data->graphe, 47, "N47", "Station", 399, 214);
    ajouternoeud(data->graphe, 48, "N48", "Station", 419, 199);
    ajouternoeud(data->graphe, 49, "N49", "Station", 442, 230);
    ajouternoeud(data->graphe, 50, "N50", "Station", 494, 189);
    ajouternoeud(data->graphe, 51, "N51", "Station", 515, 217);
    ajouternoeud(data->graphe, 52, "N52", "Station", 525, 166);
    ajouternoeud(data->graphe, 53, "N53", "Station", 538, 246);
    ajouternoeud(data->graphe, 54, "N54", "Station", 548, 238);
    ajouternoeud(data->graphe, 55, "N55", "Station", 569, 222);
    ajouternoeud(data->graphe, 56, "N56", "Station", 601, 198);
    ajouternoeud(data->graphe, 57, "N57", "Station", 614, 156);
    ajouternoeud(data->graphe, 58, "N58", "Station", 628, 176);
    ajouternoeud(data->graphe, 59, "N59", "Station", 628, 230);
    ajouternoeud(data->graphe, 60, "N60", "Station", 650, 262);
    ajouternoeud(data->graphe, 61, "N61", "Station", 660, 153);
    ajouternoeud(data->graphe, 62, "N62", "Station", 687, 188);
    ajouternoeud(data->graphe, 63, "N63", "Station", 689, 240);
    ajouternoeud(data->graphe, 208, "N62.63", "Station", 707, 219); // noeud 62.63
    ajouternoeud(data->graphe, 64, "N64", "Station", 731, 185);
    ajouternoeud(data->graphe, 65, "N65", "Station", 773, 224);
        //
    ajouternoeud(data->graphe, 66, "N66", "Station", 67, 321);
    ajouternoeud(data->graphe, 67, "N67", "Station", 72, 345);
    ajouternoeud(data->graphe, 68, "N68", "Station", 73, 305);
    ajouternoeud(data->graphe, 69, "N69", "Station", 89, 295);
    ajouternoeud(data->graphe, 70, "N70", "Station", 103, 358);
    ajouternoeud(data->graphe, 71, "N71", "Station", 109, 295);
    ajouternoeud(data->graphe, 72, "N72", "Station", 125, 345);
    ajouternoeud(data->graphe, 73, "N73", "Station", 129, 316);
    ajouternoeud(data->graphe, 74, "N74", "Station", 225, 290); //
      // N107
    ajouternoeud(data->graphe, 76, "N76", "Station", 320, 273);
    ajouternoeud(data->graphe, 77, "N77", "Station", 333, 266);
    ajouternoeud(data->graphe, 78, "N78", "Station", 357, 323);
    ajouternoeud(data->graphe, 79, "N79", "Station", 371, 315);
    ajouternoeud(data->graphe, 80, "N80", "Station", 393, 344);
    ajouternoeud(data->graphe, 81, "N81", "Station", 417, 329);
    ajouternoeud(data->graphe, 82, "N82", "Station", 432, 266);
    ajouternoeud(data->graphe, 83, "N83", "Station", 432, 351);
    ajouternoeud(data->graphe, 84, "N84", "Station", 453, 296);
    ajouternoeud(data->graphe, 85, "N85", "Station", 454, 248);
    ajouternoeud(data->graphe, 86, "N86", "Station", 462, 257);
    ajouternoeud(data->graphe, 87, "N87", "Station", 487, 289);
    ajouternoeud(data->graphe, 88, "N88", "Station", 496, 303);
    ajouternoeud(data->graphe, 89, "N89", "Station", 508, 319);
    ajouternoeud(data->graphe, 90, "N90", "Station", 519, 337);
    ajouternoeud(data->graphe, 91, "N91", "Station", 542, 370);
    ajouternoeud(data->graphe, 92, "N92", "Station", 552, 328); //
    ajouternoeud(data->graphe, 93, "N93", "Station", 570, 354);
    ajouternoeud(data->graphe, 94, "N94", "Station", 572, 270);
    ajouternoeud(data->graphe, 95, "N95", "Station", 593, 298);
    ajouternoeud(data->graphe, 96, "N96", "Station", 606, 379); //
    ajouternoeud(data->graphe, 97, "N97", "Station", 614, 283);
    ajouternoeud(data->graphe, 98, "N98", "Station", 635, 309);
    ajouternoeud(data->graphe, 99, "N99", "Station", 659, 340);
    ajouternoeud(data->graphe, 100, "N100", "Station", 695, 367);
    ajouternoeud(data->graphe, 101, "N101", "Station", 727, 355);
    ajouternoeud(data->graphe, 102, "N102", "Station", 750, 344);
    ajouternoeud(data->graphe, 103, "N103", "Station", 758, 337);
    ajouternoeud(data->graphe, 209, "N103.65", "Station", 791, 283);// 103.65
    ajouternoeud(data->graphe, 210, "N210", "Station", 929, 331);//
    ajouternoeud(data->graphe, 211, "N211", "Station", 934, 290);//
        //
    ajouternoeud(data->graphe, 104, "N104", "Station", 23, 474);
    ajouternoeud(data->graphe, 105, "N105", "Station", 73, 487);
    ajouternoeud(data->graphe, 106, "N106", "Station", 184, 429);
    ajouternoeud(data->graphe, 107, "N107", "Station", 232, 374); // ///
    ajouternoeud(data->graphe, 108, "N108", "Station", 295, 437); // 0
    ajouternoeud(data->graphe, 109, "N109", "Station", 344, 491); // 0
    ajouternoeud(data->graphe, 110, "N110", "Station", 359, 420);
    ajouternoeud(data->graphe, 111, "N111", "Station", 368, 460);
    ajouternoeud(data->graphe, 112, "N112", "Station", 377, 408);
    ajouternoeud(data->graphe, 113, "N113", "Station", 405, 388);
    ajouternoeud(data->graphe, 114, "N114", "Station", 410, 509); // 0
    ajouternoeud(data->graphe, 115, "N115", "Station", 419, 379);
    ajouternoeud(data->graphe, 116, "N116", "Station", 423, 409);
    ajouternoeud(data->graphe, 117, "N117", "Station", 435, 400);
    ajouternoeud(data->graphe, 118, "N118", "Station", 435, 490);
    ajouternoeud(data->graphe, 119, "N119", "Station", 458, 383);
    ajouternoeud(data->graphe, 120, "N120", "Station", 468, 468);
    ajouternoeud(data->graphe, 121, "N121", "Station", 488, 495);
    ajouternoeud(data->graphe, 122, "N122", "Station", 491, 408);
    ajouternoeud(data->graphe, 123, "N123", "Station", 514, 511);
    ajouternoeud(data->graphe, 124, "N124", "Station", 520, 459);
    ajouternoeud(data->graphe, 125, "N125", "Station", 540, 427);
    ajouternoeud(data->graphe, 126, "N126", "Station", 570, 469);
    ajouternoeud(data->graphe, 127, "N127", "Station", 590, 495);
    ajouternoeud(data->graphe, 128, "N128", "Station", 599, 441);
    ajouternoeud(data->graphe, 129, "N129", "Station", 619, 468);
    ajouternoeud(data->graphe, 130, "N130", "Station", 630, 417);
    ajouternoeud(data->graphe, 131, "N131", "Station", 639, 494);
    ajouternoeud(data->graphe, 132, "N132", "Station", 674, 509);
    ajouternoeud(data->graphe, 133, "N133", "Station", 704, 404);
    ajouternoeud(data->graphe, 212, "N133.100", "Station", 711, 389); //133.100
    ajouternoeud(data->graphe, 134, "N134", "Station", 722, 432);
    ajouternoeud(data->graphe, 135, "N135", "Station", 726, 491);
    ajouternoeud(data->graphe, 136, "N136", "Station", 750, 472);
    ajouternoeud(data->graphe, 137, "N137", "Station", 755, 543);
    ajouternoeud(data->graphe, 138, "N138", "Station", 813, 425);
    ajouternoeud(data->graphe, 139, "N139", "Station", 826, 414);
    ajouternoeud(data->graphe, 140, "N140", "Station", 859, 464);
    ajouternoeud(data->graphe, 141, "N141", "Station", 872, 384);
    ajouternoeud(data->graphe, 142, "N142", "Station", 895, 514);
    ajouternoeud(data->graphe, 143, "N143", "Station", 959, 394);
    ajouternoeud(data->graphe, 144, "N144", "Station", 986, 347);
    ajouternoeud(data->graphe, 145, "N145", "Station", 996, 440);
    ajouternoeud(data->graphe, 146, "N146", "Station", 997, 531);
    ajouternoeud(data->graphe, 147, "N147", "Station", 1009, 550);
    ajouternoeud(data->graphe, 148, "N148", "Station", 1034, 410);
    ajouternoeud(data->graphe, 149, "N149", "Station", 1044, 500);
        //
    ajouternoeud(data->graphe, 150, "N150", "Station", 248, 582);
    ajouternoeud(data->graphe, 151, "N151", "Station", 253, 531);
    ajouternoeud(data->graphe, 152, "N152", "Station", 270, 554);
    ajouternoeud(data->graphe, 153, "N153", "Station", 292, 497);
    ajouternoeud(data->graphe, 154, "N154", "Station", 314, 504);
    ajouternoeud(data->graphe, 155, "N155", "Station", 315, 611);
    ajouternoeud(data->graphe, 156, "N156", "Station", 365, 514); // 0
    ajouternoeud(data->graphe, 157, "N157", "Station", 380, 558); // 0
    ajouternoeud(data->graphe, 158, "N158", "Station", 389, 571);
    ajouternoeud(data->graphe, 159, "N159", "Station", 399, 564);
    ajouternoeud(data->graphe, 160, "N160", "Station", 408, 597);
    ajouternoeud(data->graphe, 161, "N161", "Station", 449, 565);
    ajouternoeud(data->graphe, 162, "N162", "Station", 476, 546); // 0
    ajouternoeud(data->graphe, 163, "N163", "Station", 520, 594); // 0
    ajouternoeud(data->graphe, 164, "N164", "Station", 537, 539);
    ajouternoeud(data->graphe, 165, "N165", "Station", 545, 574);
    ajouternoeud(data->graphe, 166, "N166", "Station", 565, 516);
    ajouternoeud(data->graphe, 167, "N167", "Station", 575, 590);
    ajouternoeud(data->graphe, 168, "N168", "Station", 577, 614);
    ajouternoeud(data->graphe, 169, "N169", "Station", 587, 606);
    ajouternoeud(data->graphe, 170, "N170", "Station", 602, 568);
    ajouternoeud(data->graphe, 171, "N171", "Station", 629, 544);
    ajouternoeud(data->graphe, 172, "N172", "Station", 758, 618);
    ajouternoeud(data->graphe, 173, "N173", "Station", 784, 596);
    ajouternoeud(data->graphe, 174, "N174", "Station", 880, 628);
    ajouternoeud(data->graphe, 175, "N175", "Station", 947, 573);
    ajouternoeud(data->graphe, 176, "N176", "Station", 987, 628);
        //
    ajouternoeud(data->graphe, 177, "N177", "Station", 9, 761);
    ajouternoeud(data->graphe, 178, "N178", "Station", 12, 751);
    ajouternoeud(data->graphe, 179, "N179", "Station", 34, 653);
    ajouternoeud(data->graphe, 180, "N180", "Station", 42, 714);
    ajouternoeud(data->graphe, 181, "N181", "Station", 54, 658);
    ajouternoeud(data->graphe, 182, "N182", "Station", 63, 618);
    ajouternoeud(data->graphe, 183, "N183", "Station", 414, 733);
    ajouternoeud(data->graphe, 184, "N184", "Station", 415, 674);
    ajouternoeud(data->graphe, 185, "N185", "Station", 445, 648);
    ajouternoeud(data->graphe, 186, "N186", "Station", 468, 692);
    ajouternoeud(data->graphe, 187, "N187", "Station", 475, 685);
    ajouternoeud(data->graphe, 188, "N188", "Station", 502, 665);
    ajouternoeud(data->graphe, 189, "N189", "Station", 523, 649);
    ajouternoeud(data->graphe, 190, "N190", "Station", 544, 638);
    ajouternoeud(data->graphe, 191, "N191", "Station", 573, 755);
    ajouternoeud(data->graphe, 192, "N192", "Station", 607, 731);
    ajouternoeud(data->graphe, 193, "N193", "Station", 613, 725);
    ajouternoeud(data->graphe, 194, "N194", "Station", 643, 704);
    ajouternoeud(data->graphe, 195, "N195", "Station", 650, 775);
    ajouternoeud(data->graphe, 196, "N196", "Station", 654, 695);
    ajouternoeud(data->graphe, 197, "N197", "Station", 688, 741);
    ajouternoeud(data->graphe, 198, "N198", "Station", 735, 708);
    ajouternoeud(data->graphe, 199, "N199", "Station", 765, 760);
    ajouternoeud(data->graphe, 200, "N200", "Station", 788, 676);
    ajouternoeud(data->graphe, 201, "N201", "Station", 815, 733);
    ajouternoeud(data->graphe, 202, "N202", "Station", 837, 771);
    ajouternoeud(data->graphe, 203, "N203", "Station", 940, 737);
    ajouternoeud(data->graphe, 204, "N204", "Station", 1023, 678);
    ajouternoeud(data->graphe, 205, "N205", "Station", 1071, 744);

    // Noeuds supplimentaires
    ajouternoeud(data->graphe, 206, "N19.7", "Station", 435, 69);
    ajouternoeud(data->graphe, 207, "N30.17", "Station", 936, 83);
    ajouternoeud(data->graphe, 208, "N62.63", "Station", 707, 219);

    ajouternoeud(data->graphe, 209, "N209", "Station", 791, 283);
    ajouternoeud(data->graphe, 210, "N210", "Station", 929, 331);
    ajouternoeud(data->graphe, 211, "N211", "Station", 934, 290);
    ajouternoeud(data->graphe, 212, "N212", "Station", 711, 389);

    /* Ajout manuel des aretes :
         - 0 a 1
         - 0 a 2
         - 1 a 2
         - 1 a 3
         - 2 a 4
         - 3 a 4
       Ainsi, pour aller de 1 a  4, il faut passer par 1 -> 3 -> 4 ou 1 -> 2 -> 4.
    */
    int edge_index = 0;
    ajouterarete(data->graphe, edge_index++, 1, 2, Euclidean_distance(data->graphe->N[1], data->graphe->N[2]));
    ajouterarete(data->graphe, edge_index++, 2, 3, Euclidean_distance(data->graphe->N[2], data->graphe->N[3]));
    ajouterarete(data->graphe, edge_index++, 3, 4, Euclidean_distance(data->graphe->N[3], data->graphe->N[4]));
    ajouterarete(data->graphe, edge_index++, 4, 5, Euclidean_distance(data->graphe->N[4], data->graphe->N[5]));
    ajouterarete(data->graphe, edge_index++, 5, 6, Euclidean_distance(data->graphe->N[5], data->graphe->N[6]));
    ajouterarete(data->graphe, edge_index++, 6, 7, Euclidean_distance(data->graphe->N[6], data->graphe->N[7]));
    ajouterarete(data->graphe, edge_index++, 7, 9, Euclidean_distance(data->graphe->N[7], data->graphe->N[9]));
    ajouterarete(data->graphe, edge_index++, 9, 8, Euclidean_distance(data->graphe->N[9], data->graphe->N[8]));
    ajouterarete(data->graphe, edge_index++, 9, 10, Euclidean_distance(data->graphe->N[9], data->graphe->N[10]));
    ajouterarete(data->graphe, edge_index++, 11, 12, Euclidean_distance(data->graphe->N[11], data->graphe->N[12]));
    ajouterarete(data->graphe, edge_index++, 12, 13, Euclidean_distance(data->graphe->N[12], data->graphe->N[13]));
    ajouterarete(data->graphe, edge_index++, 13, 14, Euclidean_distance(data->graphe->N[13], data->graphe->N[14]));
    ajouterarete(data->graphe, edge_index++, 15, 16, Euclidean_distance(data->graphe->N[15], data->graphe->N[16]));
        //
    ajouterarete(data->graphe, edge_index++, 6, 19, Euclidean_distance(data->graphe->N[6], data->graphe->N[19]));
    ajouterarete(data->graphe, edge_index++, 19, 206, Euclidean_distance(data->graphe->N[19], data->graphe->N[206]));
    ajouterarete(data->graphe, edge_index++, 206, 7, Euclidean_distance(data->graphe->N[206], data->graphe->N[7]));
    ajouterarete(data->graphe, edge_index++, 20, 21, Euclidean_distance(data->graphe->N[20], data->graphe->N[21]));
    ajouterarete(data->graphe, edge_index++, 10, 21, Euclidean_distance(data->graphe->N[10], data->graphe->N[21]));
    ajouterarete(data->graphe, edge_index++, 21, 12, Euclidean_distance(data->graphe->N[21], data->graphe->N[12]));
    ajouterarete(data->graphe, edge_index++, 14, 22, Euclidean_distance(data->graphe->N[14], data->graphe->N[22]));
    ajouterarete(data->graphe, edge_index++, 15, 22, Euclidean_distance(data->graphe->N[15], data->graphe->N[22]));
    ajouterarete(data->graphe, edge_index++, 22, 23, Euclidean_distance(data->graphe->N[22], data->graphe->N[23]));
    ajouterarete(data->graphe, edge_index++, 15, 24, Euclidean_distance(data->graphe->N[15], data->graphe->N[24]));
    ajouterarete(data->graphe, edge_index++, 24, 25, Euclidean_distance(data->graphe->N[24], data->graphe->N[25]));
    ajouterarete(data->graphe, edge_index++, 25, 26, Euclidean_distance(data->graphe->N[25], data->graphe->N[26]));
    ajouterarete(data->graphe, edge_index++, 26, 27, Euclidean_distance(data->graphe->N[26], data->graphe->N[27]));
    ajouterarete(data->graphe, edge_index++, 26, 28, Euclidean_distance(data->graphe->N[26], data->graphe->N[28]));
    ajouterarete(data->graphe, edge_index++, 29, 30, Euclidean_distance(data->graphe->N[29], data->graphe->N[30]));
    ajouterarete(data->graphe, edge_index++, 30, 207, Euclidean_distance(data->graphe->N[30], data->graphe->N[207]));
    ajouterarete(data->graphe, edge_index++, 207, 17, Euclidean_distance(data->graphe->N[207], data->graphe->N[17]));
        //
    ajouterarete(data->graphe, edge_index++, 2, 31, Euclidean_distance(data->graphe->N[2], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 4, 32, Euclidean_distance(data->graphe->N[4], data->graphe->N[32]));
    ajouterarete(data->graphe, edge_index++, 32, 33, Euclidean_distance(data->graphe->N[32], data->graphe->N[33]));
    ajouterarete(data->graphe, edge_index++, 9, 34, Euclidean_distance(data->graphe->N[9], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 20, 34, Euclidean_distance(data->graphe->N[20], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 35, 36, Euclidean_distance(data->graphe->N[35], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 20, 36, Euclidean_distance(data->graphe->N[20], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 36, 14, Euclidean_distance(data->graphe->N[36], data->graphe->N[14]));
    ajouterarete(data->graphe, edge_index++, 37, 38, Euclidean_distance(data->graphe->N[37], data->graphe->N[38]));
    ajouterarete(data->graphe, edge_index++, 23, 38, Euclidean_distance(data->graphe->N[23], data->graphe->N[38]));
    ajouterarete(data->graphe, edge_index++, 39, 40, Euclidean_distance(data->graphe->N[39], data->graphe->N[40]));
    ajouterarete(data->graphe, edge_index++, 40, 41, Euclidean_distance(data->graphe->N[40], data->graphe->N[41]));
    ajouterarete(data->graphe, edge_index++, 42, 43, Euclidean_distance(data->graphe->N[42], data->graphe->N[43]));
    ajouterarete(data->graphe, edge_index++, 42, 25, Euclidean_distance(data->graphe->N[42], data->graphe->N[25]));
    ajouterarete(data->graphe, edge_index++, 43, 28, Euclidean_distance(data->graphe->N[43], data->graphe->N[28]));
    ajouterarete(data->graphe, edge_index++, 43, 29, Euclidean_distance(data->graphe->N[43], data->graphe->N[29]));
        //
    ajouterarete(data->graphe, edge_index++, 44, 31, Euclidean_distance(data->graphe->N[44], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 44, 45, Euclidean_distance(data->graphe->N[44], data->graphe->N[45]));
    ajouterarete(data->graphe, edge_index++, 45, 46, Euclidean_distance(data->graphe->N[45], data->graphe->N[46]));
    ajouterarete(data->graphe, edge_index++, 45, 3, Euclidean_distance(data->graphe->N[45], data->graphe->N[3]));
    ajouterarete(data->graphe, edge_index++, 47, 48, Euclidean_distance(data->graphe->N[47], data->graphe->N[48]));
    ajouterarete(data->graphe, edge_index++, 47, 19, Euclidean_distance(data->graphe->N[47], data->graphe->N[19]));
    ajouterarete(data->graphe, edge_index++, 48, 49, Euclidean_distance(data->graphe->N[48], data->graphe->N[49]));
    ajouterarete(data->graphe, edge_index++, 48, 34, Euclidean_distance(data->graphe->N[48], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 49, 50, Euclidean_distance(data->graphe->N[49], data->graphe->N[50]));
    ajouterarete(data->graphe, edge_index++, 50, 51, Euclidean_distance(data->graphe->N[50], data->graphe->N[51]));
    ajouterarete(data->graphe, edge_index++, 50, 52, Euclidean_distance(data->graphe->N[50], data->graphe->N[52]));
    ajouterarete(data->graphe, edge_index++, 51, 53, Euclidean_distance(data->graphe->N[51], data->graphe->N[53]));
    ajouterarete(data->graphe, edge_index++, 52, 34, Euclidean_distance(data->graphe->N[52], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 52, 35, Euclidean_distance(data->graphe->N[52], data->graphe->N[35]));
    ajouterarete(data->graphe, edge_index++, 53, 54, Euclidean_distance(data->graphe->N[53], data->graphe->N[54]));
    ajouterarete(data->graphe, edge_index++, 54, 55, Euclidean_distance(data->graphe->N[54], data->graphe->N[55]));
    ajouterarete(data->graphe, edge_index++, 55, 52, Euclidean_distance(data->graphe->N[55], data->graphe->N[52]));
    ajouterarete(data->graphe, edge_index++, 55, 56, Euclidean_distance(data->graphe->N[55], data->graphe->N[56]));
    ajouterarete(data->graphe, edge_index++, 56, 35, Euclidean_distance(data->graphe->N[56], data->graphe->N[35]));
    ajouterarete(data->graphe, edge_index++, 56, 58, Euclidean_distance(data->graphe->N[56], data->graphe->N[58]));
    ajouterarete(data->graphe, edge_index++, 56, 59, Euclidean_distance(data->graphe->N[56], data->graphe->N[59]));
    ajouterarete(data->graphe, edge_index++, 57, 36, Euclidean_distance(data->graphe->N[57], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 57, 23, Euclidean_distance(data->graphe->N[57], data->graphe->N[23]));
    ajouterarete(data->graphe, edge_index++, 57, 58, Euclidean_distance(data->graphe->N[57], data->graphe->N[58]));
    ajouterarete(data->graphe, edge_index++, 58, 61, Euclidean_distance(data->graphe->N[58], data->graphe->N[61]));
    ajouterarete(data->graphe, edge_index++, 59, 60, Euclidean_distance(data->graphe->N[59], data->graphe->N[60]));
    ajouterarete(data->graphe, edge_index++, 59, 62, Euclidean_distance(data->graphe->N[59], data->graphe->N[62]));
    ajouterarete(data->graphe, edge_index++, 60, 63, Euclidean_distance(data->graphe->N[60], data->graphe->N[63]));
    ajouterarete(data->graphe, edge_index++, 61, 37, Euclidean_distance(data->graphe->N[61], data->graphe->N[37]));
    ajouterarete(data->graphe, edge_index++, 61, 62, Euclidean_distance(data->graphe->N[61], data->graphe->N[62]));
    ajouterarete(data->graphe, edge_index++, 62, 208, Euclidean_distance(data->graphe->N[62], data->graphe->N[208]));
    ajouterarete(data->graphe, edge_index++, 63, 208, Euclidean_distance(data->graphe->N[63], data->graphe->N[208]));
    ajouterarete(data->graphe, edge_index++, 64, 37, Euclidean_distance(data->graphe->N[64], data->graphe->N[37]));
    ajouterarete(data->graphe, edge_index++, 64, 40, Euclidean_distance(data->graphe->N[64], data->graphe->N[40]));
    ajouterarete(data->graphe, edge_index++, 64, 65, Euclidean_distance(data->graphe->N[64], data->graphe->N[65]));
    ajouterarete(data->graphe, edge_index++, 65, 42, Euclidean_distance(data->graphe->N[65], data->graphe->N[42]));
        //
    ajouterarete(data->graphe, edge_index++, 66, 67, Euclidean_distance(data->graphe->N[66], data->graphe->N[67]));
    ajouterarete(data->graphe, edge_index++, 66, 68, Euclidean_distance(data->graphe->N[66], data->graphe->N[68]));
    ajouterarete(data->graphe, edge_index++, 67, 70, Euclidean_distance(data->graphe->N[67], data->graphe->N[70]));
    ajouterarete(data->graphe, edge_index++, 68, 69, Euclidean_distance(data->graphe->N[68], data->graphe->N[69]));
    ajouterarete(data->graphe, edge_index++, 69, 71, Euclidean_distance(data->graphe->N[69], data->graphe->N[71]));
    ajouterarete(data->graphe, edge_index++, 70, 72, Euclidean_distance(data->graphe->N[70], data->graphe->N[72]));
    ajouterarete(data->graphe, edge_index++, 71, 31, Euclidean_distance(data->graphe->N[71], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 71, 73, Euclidean_distance(data->graphe->N[71], data->graphe->N[73]));
    ajouterarete(data->graphe, edge_index++, 72, 73, Euclidean_distance(data->graphe->N[72], data->graphe->N[73]));
    ajouterarete(data->graphe, edge_index++, 73, 74, Euclidean_distance(data->graphe->N[73], data->graphe->N[74]));
    ajouterarete(data->graphe, edge_index++, 74, 44, Euclidean_distance(data->graphe->N[74], data->graphe->N[44]));
    ajouterarete(data->graphe, edge_index++, 74, 76, Euclidean_distance(data->graphe->N[74], data->graphe->N[76]));
    ajouterarete(data->graphe, edge_index++, 76, 77, Euclidean_distance(data->graphe->N[76], data->graphe->N[77]));
    ajouterarete(data->graphe, edge_index++, 76, 78, Euclidean_distance(data->graphe->N[76], data->graphe->N[78]));
    ajouterarete(data->graphe, edge_index++, 77, 33, Euclidean_distance(data->graphe->N[77], data->graphe->N[33]));
    ajouterarete(data->graphe, edge_index++, 77, 47, Euclidean_distance(data->graphe->N[77], data->graphe->N[47]));
    ajouterarete(data->graphe, edge_index++, 78, 79, Euclidean_distance(data->graphe->N[78], data->graphe->N[79]));
    ajouterarete(data->graphe, edge_index++, 80, 81, Euclidean_distance(data->graphe->N[80], data->graphe->N[81]));
    ajouterarete(data->graphe, edge_index++, 81, 83, Euclidean_distance(data->graphe->N[81], data->graphe->N[83]));
    ajouterarete(data->graphe, edge_index++, 82, 84, Euclidean_distance(data->graphe->N[82], data->graphe->N[84]));
    ajouterarete(data->graphe, edge_index++, 82, 85, Euclidean_distance(data->graphe->N[82], data->graphe->N[85]));
    ajouterarete(data->graphe, edge_index++, 83, 88, Euclidean_distance(data->graphe->N[83], data->graphe->N[88]));
    ajouterarete(data->graphe, edge_index++, 81, 84, Euclidean_distance(data->graphe->N[81], data->graphe->N[84]));
    ajouterarete(data->graphe, edge_index++, 85, 49, Euclidean_distance(data->graphe->N[85], data->graphe->N[49]));
    ajouterarete(data->graphe, edge_index++, 85, 86, Euclidean_distance(data->graphe->N[85], data->graphe->N[86]));
    ajouterarete(data->graphe, edge_index++, 86, 51, Euclidean_distance(data->graphe->N[86], data->graphe->N[51]));
    ajouterarete(data->graphe, edge_index++, 86, 87, Euclidean_distance(data->graphe->N[86], data->graphe->N[87]));
    ajouterarete(data->graphe, edge_index++, 87, 88, Euclidean_distance(data->graphe->N[87], data->graphe->N[88]));
    ajouterarete(data->graphe, edge_index++, 87, 53, Euclidean_distance(data->graphe->N[87], data->graphe->N[53]));
    ajouterarete(data->graphe, edge_index++, 88, 89, Euclidean_distance(data->graphe->N[88], data->graphe->N[89]));
    ajouterarete(data->graphe, edge_index++, 89, 90, Euclidean_distance(data->graphe->N[89], data->graphe->N[90]));
    ajouterarete(data->graphe, edge_index++, 89, 94, Euclidean_distance(data->graphe->N[94], data->graphe->N[89]));
    ajouterarete(data->graphe, edge_index++, 90, 91, Euclidean_distance(data->graphe->N[90], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 91, 93, Euclidean_distance(data->graphe->N[91], data->graphe->N[93]));
    ajouterarete(data->graphe, edge_index++, 92, 95, Euclidean_distance(data->graphe->N[92], data->graphe->N[95]));
    ajouterarete(data->graphe, edge_index++, 92, 93, Euclidean_distance(data->graphe->N[92], data->graphe->N[93]));
    ajouterarete(data->graphe, edge_index++, 93, 98, Euclidean_distance(data->graphe->N[93], data->graphe->N[98]));
    ajouterarete(data->graphe, edge_index++, 94, 54, Euclidean_distance(data->graphe->N[94], data->graphe->N[54]));
    ajouterarete(data->graphe, edge_index++, 94, 95, Euclidean_distance(data->graphe->N[94], data->graphe->N[95]));
    ajouterarete(data->graphe, edge_index++, 95, 97, Euclidean_distance(data->graphe->N[95], data->graphe->N[97]));
    ajouterarete(data->graphe, edge_index++, 96, 99, Euclidean_distance(data->graphe->N[96], data->graphe->N[99]));
    ajouterarete(data->graphe, edge_index++, 97, 55, Euclidean_distance(data->graphe->N[97], data->graphe->N[55]));
    ajouterarete(data->graphe, edge_index++, 97, 98, Euclidean_distance(data->graphe->N[97], data->graphe->N[98]));
    ajouterarete(data->graphe, edge_index++, 98, 99, Euclidean_distance(data->graphe->N[98], data->graphe->N[99]));
    ajouterarete(data->graphe, edge_index++, 99, 100, Euclidean_distance(data->graphe->N[99], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 100, 101, Euclidean_distance(data->graphe->N[100], data->graphe->N[101]));
    ajouterarete(data->graphe, edge_index++, 101, 60, Euclidean_distance(data->graphe->N[101], data->graphe->N[60]));
    ajouterarete(data->graphe, edge_index++, 101, 102, Euclidean_distance(data->graphe->N[101], data->graphe->N[102]));
    ajouterarete(data->graphe, edge_index++, 102, 103, Euclidean_distance(data->graphe->N[102], data->graphe->N[103]));
    ajouterarete(data->graphe, edge_index++, 103, 63, Euclidean_distance(data->graphe->N[103], data->graphe->N[63]));
    ajouterarete(data->graphe, edge_index++, 103, 209, Euclidean_distance(data->graphe->N[103], data->graphe->N[209]));
    ajouterarete(data->graphe, edge_index++, 209, 65, Euclidean_distance(data->graphe->N[209], data->graphe->N[65]));
    ajouterarete(data->graphe, edge_index++, 210, 211, Euclidean_distance(data->graphe->N[210], data->graphe->N[211]));
    ajouterarete(data->graphe, edge_index++, 211, 42, Euclidean_distance(data->graphe->N[211], data->graphe->N[42]));
        //
    ajouterarete(data->graphe, edge_index++, 104, 105, Euclidean_distance(data->graphe->N[104], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 105, 70, Euclidean_distance(data->graphe->N[105], data->graphe->N[70]));
    ajouterarete(data->graphe, edge_index++, 106, 72, Euclidean_distance(data->graphe->N[106], data->graphe->N[72]));
    ajouterarete(data->graphe, edge_index++, 106, 107, Euclidean_distance(data->graphe->N[106], data->graphe->N[107]));
    ajouterarete(data->graphe, edge_index++, 107, 74, Euclidean_distance(data->graphe->N[107], data->graphe->N[74]));
    ajouterarete(data->graphe, edge_index++, 108, 110, Euclidean_distance(data->graphe->N[108], data->graphe->N[110]));
    ajouterarete(data->graphe, edge_index++, 109, 111, Euclidean_distance(data->graphe->N[109], data->graphe->N[111]));
    ajouterarete(data->graphe, edge_index++, 110, 111, Euclidean_distance(data->graphe->N[110], data->graphe->N[111]));
    ajouterarete(data->graphe, edge_index++, 110, 112, Euclidean_distance(data->graphe->N[110], data->graphe->N[112]));
    ajouterarete(data->graphe, edge_index++, 111, 114, Euclidean_distance(data->graphe->N[111], data->graphe->N[114]));
    ajouterarete(data->graphe, edge_index++, 112, 113, Euclidean_distance(data->graphe->N[112], data->graphe->N[113]));
    ajouterarete(data->graphe, edge_index++, 112, 118, Euclidean_distance(data->graphe->N[112], data->graphe->N[118]));
    ajouterarete(data->graphe, edge_index++, 113, 115, Euclidean_distance(data->graphe->N[113], data->graphe->N[115]));
    ajouterarete(data->graphe, edge_index++, 114, 118, Euclidean_distance(data->graphe->N[114], data->graphe->N[118]));
    ajouterarete(data->graphe, edge_index++, 113, 116, Euclidean_distance(data->graphe->N[113], data->graphe->N[116]));
    ajouterarete(data->graphe, edge_index++, 115, 80, Euclidean_distance(data->graphe->N[115], data->graphe->N[80]));
    ajouterarete(data->graphe, edge_index++, 116, 117, Euclidean_distance(data->graphe->N[116], data->graphe->N[117]));
    ajouterarete(data->graphe, edge_index++, 116, 120, Euclidean_distance(data->graphe->N[116], data->graphe->N[120]));
    ajouterarete(data->graphe, edge_index++, 117, 119, Euclidean_distance(data->graphe->N[117], data->graphe->N[119]));
    ajouterarete(data->graphe, edge_index++, 118, 120, Euclidean_distance(data->graphe->N[118], data->graphe->N[120]));
    ajouterarete(data->graphe, edge_index++, 119, 122, Euclidean_distance(data->graphe->N[119], data->graphe->N[122]));
    ajouterarete(data->graphe, edge_index++, 119, 83, Euclidean_distance(data->graphe->N[119], data->graphe->N[83]));
    ajouterarete(data->graphe, edge_index++, 119, 90, Euclidean_distance(data->graphe->N[119], data->graphe->N[90]));
    ajouterarete(data->graphe, edge_index++, 120, 121, Euclidean_distance(data->graphe->N[120], data->graphe->N[121]));
    ajouterarete(data->graphe, edge_index++, 122, 91, Euclidean_distance(data->graphe->N[122], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 122, 124, Euclidean_distance(data->graphe->N[122], data->graphe->N[124]));
    ajouterarete(data->graphe, edge_index++, 123, 126, Euclidean_distance(data->graphe->N[123], data->graphe->N[126]));
    ajouterarete(data->graphe, edge_index++, 125, 126, Euclidean_distance(data->graphe->N[125], data->graphe->N[126]));
    ajouterarete(data->graphe, edge_index++, 126, 127, Euclidean_distance(data->graphe->N[126], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 126, 128, Euclidean_distance(data->graphe->N[126], data->graphe->N[128]));
    ajouterarete(data->graphe, edge_index++, 128, 130, Euclidean_distance(data->graphe->N[128], data->graphe->N[130]));
    ajouterarete(data->graphe, edge_index++, 128, 91, Euclidean_distance(data->graphe->N[128], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 128, 129, Euclidean_distance(data->graphe->N[128], data->graphe->N[129]));
    ajouterarete(data->graphe, edge_index++, 129, 131, Euclidean_distance(data->graphe->N[129], data->graphe->N[131]));
    ajouterarete(data->graphe, edge_index++, 129, 133, Euclidean_distance(data->graphe->N[129], data->graphe->N[133]));
    ajouterarete(data->graphe, edge_index++, 130, 96, Euclidean_distance(data->graphe->N[130], data->graphe->N[96]));
    ajouterarete(data->graphe, edge_index++, 130, 100, Euclidean_distance(data->graphe->N[130], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 131, 134, Euclidean_distance(data->graphe->N[131], data->graphe->N[134]));
    ajouterarete(data->graphe, edge_index++, 131, 132, Euclidean_distance(data->graphe->N[131], data->graphe->N[132]));
    ajouterarete(data->graphe, edge_index++, 132, 135, Euclidean_distance(data->graphe->N[132], data->graphe->N[135]));
    ajouterarete(data->graphe, edge_index++, 133, 212, Euclidean_distance(data->graphe->N[133], data->graphe->N[212]));
    ajouterarete(data->graphe, edge_index++, 133, 134, Euclidean_distance(data->graphe->N[133], data->graphe->N[134]));
    ajouterarete(data->graphe, edge_index++, 212, 100, Euclidean_distance(data->graphe->N[212], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 134, 136, Euclidean_distance(data->graphe->N[134], data->graphe->N[136]));
    ajouterarete(data->graphe, edge_index++, 135, 136, Euclidean_distance(data->graphe->N[135], data->graphe->N[136]));
    ajouterarete(data->graphe, edge_index++, 135, 137, Euclidean_distance(data->graphe->N[135], data->graphe->N[137]));
    ajouterarete(data->graphe, edge_index++, 136, 138, Euclidean_distance(data->graphe->N[136], data->graphe->N[138]));
    ajouterarete(data->graphe, edge_index++, 137, 140, Euclidean_distance(data->graphe->N[137], data->graphe->N[140]));
    ajouterarete(data->graphe, edge_index++, 138, 139, Euclidean_distance(data->graphe->N[138], data->graphe->N[139]));
    ajouterarete(data->graphe, edge_index++, 138, 102, Euclidean_distance(data->graphe->N[138], data->graphe->N[102]));
    ajouterarete(data->graphe, edge_index++, 139, 140, Euclidean_distance(data->graphe->N[139], data->graphe->N[140]));
    ajouterarete(data->graphe, edge_index++, 139, 141, Euclidean_distance(data->graphe->N[139], data->graphe->N[141]));
    ajouterarete(data->graphe, edge_index++, 140, 142, Euclidean_distance(data->graphe->N[140], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 141, 143, Euclidean_distance(data->graphe->N[141], data->graphe->N[143]));
    ajouterarete(data->graphe, edge_index++, 141, 105, Euclidean_distance(data->graphe->N[141], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 142, 145, Euclidean_distance(data->graphe->N[142], data->graphe->N[145]));
    ajouterarete(data->graphe, edge_index++, 143, 145, Euclidean_distance(data->graphe->N[143], data->graphe->N[145]));
    ajouterarete(data->graphe, edge_index++, 144, 148, Euclidean_distance(data->graphe->N[144], data->graphe->N[148]));
    ajouterarete(data->graphe, edge_index++, 145, 148, Euclidean_distance(data->graphe->N[145], data->graphe->N[148]));
    ajouterarete(data->graphe, edge_index++, 145, 149, Euclidean_distance(data->graphe->N[145], data->graphe->N[149]));
    ajouterarete(data->graphe, edge_index++, 146, 149, Euclidean_distance(data->graphe->N[146], data->graphe->N[149]));
    ajouterarete(data->graphe, edge_index++, 146, 147, Euclidean_distance(data->graphe->N[146], data->graphe->N[147]));
        //
    ajouterarete(data->graphe, edge_index++, 150, 152, Euclidean_distance(data->graphe->N[150], data->graphe->N[152]));
    ajouterarete(data->graphe, edge_index++, 151, 152, Euclidean_distance(data->graphe->N[151], data->graphe->N[152]));
    ajouterarete(data->graphe, edge_index++, 151, 106, Euclidean_distance(data->graphe->N[151], data->graphe->N[106]));
    ajouterarete(data->graphe, edge_index++, 152, 155, Euclidean_distance(data->graphe->N[152], data->graphe->N[155]));
    ajouterarete(data->graphe, edge_index++, 151, 153, Euclidean_distance(data->graphe->N[151], data->graphe->N[153]));
    ajouterarete(data->graphe, edge_index++, 153, 108, Euclidean_distance(data->graphe->N[153], data->graphe->N[108]));
    ajouterarete(data->graphe, edge_index++, 153, 154, Euclidean_distance(data->graphe->N[153], data->graphe->N[154]));
    ajouterarete(data->graphe, edge_index++, 154, 109, Euclidean_distance(data->graphe->N[154], data->graphe->N[109]));
    ajouterarete(data->graphe, edge_index++, 154, 156, Euclidean_distance(data->graphe->N[154], data->graphe->N[156]));
    ajouterarete(data->graphe, edge_index++, 155, 157, Euclidean_distance(data->graphe->N[155], data->graphe->N[157]));
    ajouterarete(data->graphe, edge_index++, 156, 159, Euclidean_distance(data->graphe->N[156], data->graphe->N[159]));
    ajouterarete(data->graphe, edge_index++, 157, 158, Euclidean_distance(data->graphe->N[157], data->graphe->N[158]));
    ajouterarete(data->graphe, edge_index++, 158, 159, Euclidean_distance(data->graphe->N[158], data->graphe->N[159]));
    ajouterarete(data->graphe, edge_index++, 158, 160, Euclidean_distance(data->graphe->N[158], data->graphe->N[160]));
    ajouterarete(data->graphe, edge_index++, 159, 156, Euclidean_distance(data->graphe->N[159], data->graphe->N[156]));
    ajouterarete(data->graphe, edge_index++, 160, 161, Euclidean_distance(data->graphe->N[160], data->graphe->N[161]));
    ajouterarete(data->graphe, edge_index++, 161, 162, Euclidean_distance(data->graphe->N[161], data->graphe->N[163]));
    ajouterarete(data->graphe, edge_index++, 162, 163, Euclidean_distance(data->graphe->N[162], data->graphe->N[163]));
    ajouterarete(data->graphe, edge_index++, 163, 165, Euclidean_distance(data->graphe->N[163], data->graphe->N[165]));
    ajouterarete(data->graphe, edge_index++, 164, 166, Euclidean_distance(data->graphe->N[164], data->graphe->N[166]));
    ajouterarete(data->graphe, edge_index++, 164, 123, Euclidean_distance(data->graphe->N[164], data->graphe->N[123]));
    ajouterarete(data->graphe, edge_index++, 165, 168, Euclidean_distance(data->graphe->N[165], data->graphe->N[168]));
    ajouterarete(data->graphe, edge_index++, 166, 127, Euclidean_distance(data->graphe->N[166], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 166, 170, Euclidean_distance(data->graphe->N[166], data->graphe->N[170]));
    ajouterarete(data->graphe, edge_index++, 167, 170, Euclidean_distance(data->graphe->N[167], data->graphe->N[170]));
    ajouterarete(data->graphe, edge_index++, 168, 169, Euclidean_distance(data->graphe->N[168], data->graphe->N[169]));
    ajouterarete(data->graphe, edge_index++, 170, 171, Euclidean_distance(data->graphe->N[170], data->graphe->N[171]));
    ajouterarete(data->graphe, edge_index++, 171, 127, Euclidean_distance(data->graphe->N[171], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 171, 132, Euclidean_distance(data->graphe->N[171], data->graphe->N[132]));
    ajouterarete(data->graphe, edge_index++, 172, 173, Euclidean_distance(data->graphe->N[172], data->graphe->N[173]));
    ajouterarete(data->graphe, edge_index++, 173, 137, Euclidean_distance(data->graphe->N[173], data->graphe->N[137]));
    ajouterarete(data->graphe, edge_index++, 173, 142, Euclidean_distance(data->graphe->N[173], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 174, 175, Euclidean_distance(data->graphe->N[174], data->graphe->N[175]));
    ajouterarete(data->graphe, edge_index++, 175, 142, Euclidean_distance(data->graphe->N[175], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 175, 146, Euclidean_distance(data->graphe->N[175], data->graphe->N[146]));
    ajouterarete(data->graphe, edge_index++, 175, 176, Euclidean_distance(data->graphe->N[175], data->graphe->N[176]));
        //
    ajouterarete(data->graphe, edge_index++, 177, 178, Euclidean_distance(data->graphe->N[177], data->graphe->N[178]));
    ajouterarete(data->graphe, edge_index++, 178, 179, Euclidean_distance(data->graphe->N[178], data->graphe->N[179]));
    ajouterarete(data->graphe, edge_index++, 179, 181, Euclidean_distance(data->graphe->N[179], data->graphe->N[181]));
    ajouterarete(data->graphe, edge_index++, 179, 105, Euclidean_distance(data->graphe->N[179], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 180, 181, Euclidean_distance(data->graphe->N[180], data->graphe->N[181]));
    ajouterarete(data->graphe, edge_index++, 181, 182, Euclidean_distance(data->graphe->N[181], data->graphe->N[182]));
    ajouterarete(data->graphe, edge_index++, 184, 185, Euclidean_distance(data->graphe->N[184], data->graphe->N[185]));
    ajouterarete(data->graphe, edge_index++, 183, 186, Euclidean_distance(data->graphe->N[183], data->graphe->N[186]));
    ajouterarete(data->graphe, edge_index++, 183, 155, Euclidean_distance(data->graphe->N[183], data->graphe->N[155]));
    ajouterarete(data->graphe, edge_index++, 185, 160, Euclidean_distance(data->graphe->N[185], data->graphe->N[160]));
    ajouterarete(data->graphe, edge_index++, 185, 187, Euclidean_distance(data->graphe->N[185], data->graphe->N[187]));
    ajouterarete(data->graphe, edge_index++, 186, 187, Euclidean_distance(data->graphe->N[186], data->graphe->N[187]));
    ajouterarete(data->graphe, edge_index++, 187, 188, Euclidean_distance(data->graphe->N[187], data->graphe->N[188]));
    ajouterarete(data->graphe, edge_index++, 188, 189, Euclidean_distance(data->graphe->N[188], data->graphe->N[189]));
    ajouterarete(data->graphe, edge_index++, 188, 191, Euclidean_distance(data->graphe->N[188], data->graphe->N[191]));
    ajouterarete(data->graphe, edge_index++, 189, 190, Euclidean_distance(data->graphe->N[189], data->graphe->N[190]));
    ajouterarete(data->graphe, edge_index++, 189, 161, Euclidean_distance(data->graphe->N[189], data->graphe->N[161]));
    ajouterarete(data->graphe, edge_index++, 190, 192, Euclidean_distance(data->graphe->N[190], data->graphe->N[192]));
    ajouterarete(data->graphe, edge_index++, 190, 168, Euclidean_distance(data->graphe->N[190], data->graphe->N[168]));
    ajouterarete(data->graphe, edge_index++, 191, 192, Euclidean_distance(data->graphe->N[191], data->graphe->N[192]));
    ajouterarete(data->graphe, edge_index++, 192, 193, Euclidean_distance(data->graphe->N[192], data->graphe->N[193]));
    ajouterarete(data->graphe, edge_index++, 193, 194, Euclidean_distance(data->graphe->N[193], data->graphe->N[194]));
    ajouterarete(data->graphe, edge_index++, 193, 195, Euclidean_distance(data->graphe->N[193], data->graphe->N[195]));
    ajouterarete(data->graphe, edge_index++, 194, 196, Euclidean_distance(data->graphe->N[194], data->graphe->N[196]));
    ajouterarete(data->graphe, edge_index++, 196, 169, Euclidean_distance(data->graphe->N[196], data->graphe->N[169]));
    ajouterarete(data->graphe, edge_index++, 196, 172, Euclidean_distance(data->graphe->N[196], data->graphe->N[172]));
    ajouterarete(data->graphe, edge_index++, 195, 197, Euclidean_distance(data->graphe->N[195], data->graphe->N[197]));
    ajouterarete(data->graphe, edge_index++, 197, 198, Euclidean_distance(data->graphe->N[197], data->graphe->N[198]));
    ajouterarete(data->graphe, edge_index++, 198, 199, Euclidean_distance(data->graphe->N[198], data->graphe->N[199]));
    ajouterarete(data->graphe, edge_index++, 198, 200, Euclidean_distance(data->graphe->N[198], data->graphe->N[200]));
    ajouterarete(data->graphe, edge_index++, 199, 201, Euclidean_distance(data->graphe->N[199], data->graphe->N[201]));
    ajouterarete(data->graphe, edge_index++, 200, 201, Euclidean_distance(data->graphe->N[200], data->graphe->N[201]));
    ajouterarete(data->graphe, edge_index++, 200, 172, Euclidean_distance(data->graphe->N[200], data->graphe->N[172]));
    ajouterarete(data->graphe, edge_index++, 201, 202, Euclidean_distance(data->graphe->N[201], data->graphe->N[204]));
    ajouterarete(data->graphe, edge_index++, 203, 204, Euclidean_distance(data->graphe->N[203], data->graphe->N[204]));
    ajouterarete(data->graphe, edge_index++, 204, 205, Euclidean_distance(data->graphe->N[204], data->graphe->N[205]));
    ajouterarete(data->graphe, edge_index++, 204, 176, Euclidean_distance(data->graphe->N[204], data->graphe->N[176]));




    // Les aretes de sens opposee

    ajouterarete(data->graphe, edge_index++, 2, 1, Euclidean_distance(data->graphe->N[1], data->graphe->N[2]));
    ajouterarete(data->graphe, edge_index++, 3, 2, Euclidean_distance(data->graphe->N[2], data->graphe->N[3]));
    ajouterarete(data->graphe, edge_index++, 4, 3, Euclidean_distance(data->graphe->N[3], data->graphe->N[4]));
    ajouterarete(data->graphe, edge_index++, 5, 4, Euclidean_distance(data->graphe->N[4], data->graphe->N[5]));
    ajouterarete(data->graphe, edge_index++, 6, 5, Euclidean_distance(data->graphe->N[5], data->graphe->N[6]));
    ajouterarete(data->graphe, edge_index++, 7, 6, Euclidean_distance(data->graphe->N[6], data->graphe->N[7]));
    ajouterarete(data->graphe, edge_index++, 9, 7, Euclidean_distance(data->graphe->N[7], data->graphe->N[9]));
    ajouterarete(data->graphe, edge_index++, 8, 9, Euclidean_distance(data->graphe->N[9], data->graphe->N[8]));
    ajouterarete(data->graphe, edge_index++, 10, 9, Euclidean_distance(data->graphe->N[9], data->graphe->N[10]));
    ajouterarete(data->graphe, edge_index++, 12, 11, Euclidean_distance(data->graphe->N[11], data->graphe->N[12]));
    ajouterarete(data->graphe, edge_index++, 13, 12, Euclidean_distance(data->graphe->N[12], data->graphe->N[13]));
    ajouterarete(data->graphe, edge_index++, 14, 13, Euclidean_distance(data->graphe->N[13], data->graphe->N[14]));
    ajouterarete(data->graphe, edge_index++, 16, 15, Euclidean_distance(data->graphe->N[15], data->graphe->N[16]));
        //
    ajouterarete(data->graphe, edge_index++, 19, 6, Euclidean_distance(data->graphe->N[6], data->graphe->N[19]));
    ajouterarete(data->graphe, edge_index++, 206, 19, Euclidean_distance(data->graphe->N[19], data->graphe->N[206]));
    ajouterarete(data->graphe, edge_index++, 7, 206, Euclidean_distance(data->graphe->N[206], data->graphe->N[7]));
    ajouterarete(data->graphe, edge_index++, 21, 20, Euclidean_distance(data->graphe->N[20], data->graphe->N[21]));
    ajouterarete(data->graphe, edge_index++, 21, 10, Euclidean_distance(data->graphe->N[10], data->graphe->N[21]));
    ajouterarete(data->graphe, edge_index++, 12, 21, Euclidean_distance(data->graphe->N[21], data->graphe->N[12]));
    ajouterarete(data->graphe, edge_index++, 22, 14, Euclidean_distance(data->graphe->N[14], data->graphe->N[22]));
    ajouterarete(data->graphe, edge_index++, 22, 15, Euclidean_distance(data->graphe->N[15], data->graphe->N[22]));
    ajouterarete(data->graphe, edge_index++, 23, 22, Euclidean_distance(data->graphe->N[22], data->graphe->N[23]));
    ajouterarete(data->graphe, edge_index++, 24, 15, Euclidean_distance(data->graphe->N[15], data->graphe->N[24]));
    ajouterarete(data->graphe, edge_index++, 25, 24, Euclidean_distance(data->graphe->N[24], data->graphe->N[25]));
    ajouterarete(data->graphe, edge_index++, 26, 25, Euclidean_distance(data->graphe->N[25], data->graphe->N[26]));
    ajouterarete(data->graphe, edge_index++, 27, 26, Euclidean_distance(data->graphe->N[26], data->graphe->N[27]));
    ajouterarete(data->graphe, edge_index++, 28, 26, Euclidean_distance(data->graphe->N[26], data->graphe->N[28]));
    ajouterarete(data->graphe, edge_index++, 30, 29, Euclidean_distance(data->graphe->N[29], data->graphe->N[30]));
    ajouterarete(data->graphe, edge_index++, 207, 30, Euclidean_distance(data->graphe->N[30], data->graphe->N[207]));
    ajouterarete(data->graphe, edge_index++, 17, 207, Euclidean_distance(data->graphe->N[207], data->graphe->N[17]));
        //
    ajouterarete(data->graphe, edge_index++, 31, 2, Euclidean_distance(data->graphe->N[2], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 33, 32, Euclidean_distance(data->graphe->N[32], data->graphe->N[33]));
    ajouterarete(data->graphe, edge_index++, 33, 5, Euclidean_distance(data->graphe->N[5], data->graphe->N[33]));
    ajouterarete(data->graphe, edge_index++, 34, 9, Euclidean_distance(data->graphe->N[9], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 34, 20, Euclidean_distance(data->graphe->N[20], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 36, 35, Euclidean_distance(data->graphe->N[35], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 36, 20, Euclidean_distance(data->graphe->N[20], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 14, 36, Euclidean_distance(data->graphe->N[36], data->graphe->N[14]));
    ajouterarete(data->graphe, edge_index++, 38, 37, Euclidean_distance(data->graphe->N[37], data->graphe->N[38]));
    ajouterarete(data->graphe, edge_index++, 38, 23, Euclidean_distance(data->graphe->N[23], data->graphe->N[38]));
    ajouterarete(data->graphe, edge_index++, 40, 39, Euclidean_distance(data->graphe->N[39], data->graphe->N[40]));
    ajouterarete(data->graphe, edge_index++, 41, 40, Euclidean_distance(data->graphe->N[40], data->graphe->N[41]));
    ajouterarete(data->graphe, edge_index++, 43, 42, Euclidean_distance(data->graphe->N[42], data->graphe->N[43]));
    ajouterarete(data->graphe, edge_index++, 25, 42, Euclidean_distance(data->graphe->N[42], data->graphe->N[25]));
    ajouterarete(data->graphe, edge_index++, 28, 43, Euclidean_distance(data->graphe->N[43], data->graphe->N[28]));
    ajouterarete(data->graphe, edge_index++, 29, 43, Euclidean_distance(data->graphe->N[43], data->graphe->N[29]));
        //
    ajouterarete(data->graphe, edge_index++, 31, 44, Euclidean_distance(data->graphe->N[44], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 45, 44, Euclidean_distance(data->graphe->N[44], data->graphe->N[45]));
    ajouterarete(data->graphe, edge_index++, 46, 45, Euclidean_distance(data->graphe->N[45], data->graphe->N[46]));
    ajouterarete(data->graphe, edge_index++, 3, 45, Euclidean_distance(data->graphe->N[45], data->graphe->N[3]));
    ajouterarete(data->graphe, edge_index++, 32, 46, Euclidean_distance(data->graphe->N[46], data->graphe->N[32]));
    ajouterarete(data->graphe, edge_index++, 48, 47, Euclidean_distance(data->graphe->N[47], data->graphe->N[48]));
    ajouterarete(data->graphe, edge_index++, 19, 47, Euclidean_distance(data->graphe->N[47], data->graphe->N[19]));
    ajouterarete(data->graphe, edge_index++, 49, 48, Euclidean_distance(data->graphe->N[48], data->graphe->N[49]));
    ajouterarete(data->graphe, edge_index++, 34, 48, Euclidean_distance(data->graphe->N[48], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 50, 49, Euclidean_distance(data->graphe->N[49], data->graphe->N[50]));
    ajouterarete(data->graphe, edge_index++, 51, 50, Euclidean_distance(data->graphe->N[50], data->graphe->N[51]));
    ajouterarete(data->graphe, edge_index++, 52, 50, Euclidean_distance(data->graphe->N[50], data->graphe->N[52]));
    ajouterarete(data->graphe, edge_index++, 53, 51, Euclidean_distance(data->graphe->N[51], data->graphe->N[53]));
    ajouterarete(data->graphe, edge_index++, 34, 52, Euclidean_distance(data->graphe->N[52], data->graphe->N[34]));
    ajouterarete(data->graphe, edge_index++, 35, 52, Euclidean_distance(data->graphe->N[52], data->graphe->N[35]));
    ajouterarete(data->graphe, edge_index++, 54, 53, Euclidean_distance(data->graphe->N[53], data->graphe->N[54]));
    ajouterarete(data->graphe, edge_index++, 55, 54, Euclidean_distance(data->graphe->N[54], data->graphe->N[55]));
    ajouterarete(data->graphe, edge_index++, 52, 55, Euclidean_distance(data->graphe->N[55], data->graphe->N[52]));
    ajouterarete(data->graphe, edge_index++, 56, 55, Euclidean_distance(data->graphe->N[55], data->graphe->N[56]));
    ajouterarete(data->graphe, edge_index++, 35, 56, Euclidean_distance(data->graphe->N[56], data->graphe->N[35]));
    ajouterarete(data->graphe, edge_index++, 58, 56, Euclidean_distance(data->graphe->N[56], data->graphe->N[58]));
    ajouterarete(data->graphe, edge_index++, 59, 56, Euclidean_distance(data->graphe->N[56], data->graphe->N[59]));
    ajouterarete(data->graphe, edge_index++, 36, 57, Euclidean_distance(data->graphe->N[57], data->graphe->N[36]));
    ajouterarete(data->graphe, edge_index++, 23, 57, Euclidean_distance(data->graphe->N[57], data->graphe->N[23]));
    ajouterarete(data->graphe, edge_index++, 58, 57, Euclidean_distance(data->graphe->N[57], data->graphe->N[58]));
    ajouterarete(data->graphe, edge_index++, 61, 58, Euclidean_distance(data->graphe->N[58], data->graphe->N[61]));
    ajouterarete(data->graphe, edge_index++, 60, 59, Euclidean_distance(data->graphe->N[59], data->graphe->N[60]));
    ajouterarete(data->graphe, edge_index++, 62, 59, Euclidean_distance(data->graphe->N[59], data->graphe->N[62]));
    ajouterarete(data->graphe, edge_index++, 63, 60, Euclidean_distance(data->graphe->N[60], data->graphe->N[63]));
    ajouterarete(data->graphe, edge_index++, 37, 61, Euclidean_distance(data->graphe->N[61], data->graphe->N[37]));
    ajouterarete(data->graphe, edge_index++, 62, 61, Euclidean_distance(data->graphe->N[61], data->graphe->N[62]));
    ajouterarete(data->graphe, edge_index++, 208, 62, Euclidean_distance(data->graphe->N[62], data->graphe->N[208]));
    ajouterarete(data->graphe, edge_index++, 208, 63, Euclidean_distance(data->graphe->N[63], data->graphe->N[208]));
    ajouterarete(data->graphe, edge_index++, 37, 64, Euclidean_distance(data->graphe->N[64], data->graphe->N[37]));
    ajouterarete(data->graphe, edge_index++, 40, 64, Euclidean_distance(data->graphe->N[64], data->graphe->N[40]));
    ajouterarete(data->graphe, edge_index++, 65, 64, Euclidean_distance(data->graphe->N[64], data->graphe->N[65]));
    ajouterarete(data->graphe, edge_index++, 42, 65, Euclidean_distance(data->graphe->N[65], data->graphe->N[42]));
        //
    ajouterarete(data->graphe, edge_index++, 67, 66, Euclidean_distance(data->graphe->N[66], data->graphe->N[67]));
    ajouterarete(data->graphe, edge_index++, 68, 66, Euclidean_distance(data->graphe->N[66], data->graphe->N[68]));
    ajouterarete(data->graphe, edge_index++, 70, 67, Euclidean_distance(data->graphe->N[67], data->graphe->N[70]));
    ajouterarete(data->graphe, edge_index++, 69, 68, Euclidean_distance(data->graphe->N[68], data->graphe->N[69]));
    ajouterarete(data->graphe, edge_index++, 71, 69, Euclidean_distance(data->graphe->N[69], data->graphe->N[71]));
    ajouterarete(data->graphe, edge_index++, 72, 70, Euclidean_distance(data->graphe->N[70], data->graphe->N[72]));
    ajouterarete(data->graphe, edge_index++, 31, 71, Euclidean_distance(data->graphe->N[71], data->graphe->N[31]));
    ajouterarete(data->graphe, edge_index++, 73, 71, Euclidean_distance(data->graphe->N[71], data->graphe->N[73]));
    ajouterarete(data->graphe, edge_index++, 73, 72, Euclidean_distance(data->graphe->N[72], data->graphe->N[73]));
    ajouterarete(data->graphe, edge_index++, 74, 73, Euclidean_distance(data->graphe->N[73], data->graphe->N[74]));
    /** ajouterarete(data->graphe, edge_index++, 44, 74, Euclidean_distance(data->graphe->N[74], data->graphe->N[44]));*/ // La travereé dans cette direction est interdite
    ajouterarete(data->graphe, edge_index++, 76, 74, Euclidean_distance(data->graphe->N[74], data->graphe->N[76]));
    ajouterarete(data->graphe, edge_index++, 77, 76, Euclidean_distance(data->graphe->N[76], data->graphe->N[77]));
    ajouterarete(data->graphe, edge_index++, 46, 76, Euclidean_distance(data->graphe->N[76], data->graphe->N[46]));
    ajouterarete(data->graphe, edge_index++, 47, 77, Euclidean_distance(data->graphe->N[77], data->graphe->N[47]));
    ajouterarete(data->graphe, edge_index++, 79, 77, Euclidean_distance(data->graphe->N[77], data->graphe->N[79]));
    ajouterarete(data->graphe, edge_index++, 79, 78, Euclidean_distance(data->graphe->N[78], data->graphe->N[79]));
    ajouterarete(data->graphe, edge_index++, 80, 79, Euclidean_distance(data->graphe->N[79], data->graphe->N[80]));
    ajouterarete(data->graphe, edge_index++, 81, 80, Euclidean_distance(data->graphe->N[80], data->graphe->N[81]));
    ajouterarete(data->graphe, edge_index++, 83, 81, Euclidean_distance(data->graphe->N[81], data->graphe->N[83]));
    ajouterarete(data->graphe, edge_index++, 84, 82, Euclidean_distance(data->graphe->N[82], data->graphe->N[84]));
    ajouterarete(data->graphe, edge_index++, 85, 82, Euclidean_distance(data->graphe->N[82], data->graphe->N[85]));
    ajouterarete(data->graphe, edge_index++, 88, 83, Euclidean_distance(data->graphe->N[83], data->graphe->N[88]));
    ajouterarete(data->graphe, edge_index++, 84, 81, Euclidean_distance(data->graphe->N[81], data->graphe->N[84]));
    ajouterarete(data->graphe, edge_index++, 49, 85, Euclidean_distance(data->graphe->N[85], data->graphe->N[49]));
    ajouterarete(data->graphe, edge_index++, 86, 85, Euclidean_distance(data->graphe->N[85], data->graphe->N[86]));
    ajouterarete(data->graphe, edge_index++, 51, 86, Euclidean_distance(data->graphe->N[86], data->graphe->N[51]));
    ajouterarete(data->graphe, edge_index++, 87, 86, Euclidean_distance(data->graphe->N[86], data->graphe->N[87]));
    ajouterarete(data->graphe, edge_index++, 88, 87, Euclidean_distance(data->graphe->N[87], data->graphe->N[88]));
    ajouterarete(data->graphe, edge_index++, 53, 87, Euclidean_distance(data->graphe->N[87], data->graphe->N[53]));
    ajouterarete(data->graphe, edge_index++, 89, 88, Euclidean_distance(data->graphe->N[88], data->graphe->N[89]));
    ajouterarete(data->graphe, edge_index++, 90, 89, Euclidean_distance(data->graphe->N[89], data->graphe->N[90]));
    ajouterarete(data->graphe, edge_index++, 94, 89, Euclidean_distance(data->graphe->N[94], data->graphe->N[89]));
    ajouterarete(data->graphe, edge_index++, 91, 90, Euclidean_distance(data->graphe->N[90], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 93, 91, Euclidean_distance(data->graphe->N[91], data->graphe->N[93]));
    ajouterarete(data->graphe, edge_index++, 95, 92, Euclidean_distance(data->graphe->N[92], data->graphe->N[95]));
    ajouterarete(data->graphe, edge_index++, 93, 92, Euclidean_distance(data->graphe->N[92], data->graphe->N[93]));
    ajouterarete(data->graphe, edge_index++, 98, 93, Euclidean_distance(data->graphe->N[93], data->graphe->N[98]));
    ajouterarete(data->graphe, edge_index++, 54, 94, Euclidean_distance(data->graphe->N[94], data->graphe->N[54]));
    ajouterarete(data->graphe, edge_index++, 95, 94, Euclidean_distance(data->graphe->N[94], data->graphe->N[95]));
    ajouterarete(data->graphe, edge_index++, 97, 95, Euclidean_distance(data->graphe->N[95], data->graphe->N[97]));
    ajouterarete(data->graphe, edge_index++, 99, 96, Euclidean_distance(data->graphe->N[96], data->graphe->N[99]));
    ajouterarete(data->graphe, edge_index++, 55, 97, Euclidean_distance(data->graphe->N[97], data->graphe->N[55]));
    ajouterarete(data->graphe, edge_index++, 98, 97, Euclidean_distance(data->graphe->N[97], data->graphe->N[98]));
    ajouterarete(data->graphe, edge_index++, 99, 98, Euclidean_distance(data->graphe->N[98], data->graphe->N[99]));
    ajouterarete(data->graphe, edge_index++, 100, 99, Euclidean_distance(data->graphe->N[99], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 101, 100, Euclidean_distance(data->graphe->N[100], data->graphe->N[101]));
    ajouterarete(data->graphe, edge_index++, 60, 101, Euclidean_distance(data->graphe->N[101], data->graphe->N[60]));
    ajouterarete(data->graphe, edge_index++, 102, 101, Euclidean_distance(data->graphe->N[101], data->graphe->N[102]));
    ajouterarete(data->graphe, edge_index++, 103, 102, Euclidean_distance(data->graphe->N[102], data->graphe->N[103]));
    ajouterarete(data->graphe, edge_index++, 63, 103, Euclidean_distance(data->graphe->N[103], data->graphe->N[63]));
    ajouterarete(data->graphe, edge_index++, 209, 103, Euclidean_distance(data->graphe->N[103], data->graphe->N[209]));
    ajouterarete(data->graphe, edge_index++, 65, 209, Euclidean_distance(data->graphe->N[209], data->graphe->N[65]));
    ajouterarete(data->graphe, edge_index++, 211, 210, Euclidean_distance(data->graphe->N[210], data->graphe->N[211]));
    ajouterarete(data->graphe, edge_index++, 42, 211, Euclidean_distance(data->graphe->N[211], data->graphe->N[42]));
        //
    ajouterarete(data->graphe, edge_index++, 105, 104, Euclidean_distance(data->graphe->N[104], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 70, 105, Euclidean_distance(data->graphe->N[105], data->graphe->N[70]));
    ajouterarete(data->graphe, edge_index++, 72, 106, Euclidean_distance(data->graphe->N[106], data->graphe->N[72]));
    ajouterarete(data->graphe, edge_index++, 107, 106, Euclidean_distance(data->graphe->N[106], data->graphe->N[107]));
    ajouterarete(data->graphe, edge_index++, 74, 107, Euclidean_distance(data->graphe->N[107], data->graphe->N[74]));
    ajouterarete(data->graphe, edge_index++, 110, 108, Euclidean_distance(data->graphe->N[108], data->graphe->N[110]));
    ajouterarete(data->graphe, edge_index++, 111, 109, Euclidean_distance(data->graphe->N[109], data->graphe->N[111]));
    ajouterarete(data->graphe, edge_index++, 111, 110, Euclidean_distance(data->graphe->N[110], data->graphe->N[111]));
    ajouterarete(data->graphe, edge_index++, 112, 110, Euclidean_distance(data->graphe->N[110], data->graphe->N[112]));
    ajouterarete(data->graphe, edge_index++, 114, 111, Euclidean_distance(data->graphe->N[111], data->graphe->N[114]));
    ajouterarete(data->graphe, edge_index++, 113, 112, Euclidean_distance(data->graphe->N[112], data->graphe->N[113]));
    ajouterarete(data->graphe, edge_index++, 118, 112, Euclidean_distance(data->graphe->N[112], data->graphe->N[118]));
    ajouterarete(data->graphe, edge_index++, 115, 113, Euclidean_distance(data->graphe->N[113], data->graphe->N[115]));
    ajouterarete(data->graphe, edge_index++, 78, 113, Euclidean_distance(data->graphe->N[113], data->graphe->N[78]));
    ajouterarete(data->graphe, edge_index++, 118, 114, Euclidean_distance(data->graphe->N[114], data->graphe->N[118]));
    ajouterarete(data->graphe, edge_index++, 117, 115, Euclidean_distance(data->graphe->N[115], data->graphe->N[117]));
    ajouterarete(data->graphe, edge_index++, 117, 116, Euclidean_distance(data->graphe->N[116], data->graphe->N[117]));
    ajouterarete(data->graphe, edge_index++, 119, 117, Euclidean_distance(data->graphe->N[117], data->graphe->N[119]));
    ajouterarete(data->graphe, edge_index++, 123, 117, Euclidean_distance(data->graphe->N[117], data->graphe->N[123]));
    ajouterarete(data->graphe, edge_index++, 120, 118, Euclidean_distance(data->graphe->N[118], data->graphe->N[120]));
    ajouterarete(data->graphe, edge_index++, 122, 119, Euclidean_distance(data->graphe->N[119], data->graphe->N[122]));
    ajouterarete(data->graphe, edge_index++, 83, 119, Euclidean_distance(data->graphe->N[119], data->graphe->N[83]));
    ajouterarete(data->graphe, edge_index++, 90, 119, Euclidean_distance(data->graphe->N[119], data->graphe->N[90]));
    ajouterarete(data->graphe, edge_index++, 91, 122, Euclidean_distance(data->graphe->N[122], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 124, 122, Euclidean_distance(data->graphe->N[122], data->graphe->N[124]));
    ajouterarete(data->graphe, edge_index++, 126, 123, Euclidean_distance(data->graphe->N[123], data->graphe->N[126]));
    ajouterarete(data->graphe, edge_index++, 126, 125, Euclidean_distance(data->graphe->N[125], data->graphe->N[126]));
    ajouterarete(data->graphe, edge_index++, 127, 126, Euclidean_distance(data->graphe->N[126], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 128, 126, Euclidean_distance(data->graphe->N[126], data->graphe->N[128]));
    ajouterarete(data->graphe, edge_index++, 130, 128, Euclidean_distance(data->graphe->N[128], data->graphe->N[130]));
    ajouterarete(data->graphe, edge_index++, 91, 128, Euclidean_distance(data->graphe->N[128], data->graphe->N[91]));
    ajouterarete(data->graphe, edge_index++, 129, 128, Euclidean_distance(data->graphe->N[128], data->graphe->N[129]));
    ajouterarete(data->graphe, edge_index++, 131, 129, Euclidean_distance(data->graphe->N[129], data->graphe->N[131]));
    ajouterarete(data->graphe, edge_index++, 133, 129, Euclidean_distance(data->graphe->N[129], data->graphe->N[133]));
    ajouterarete(data->graphe, edge_index++, 96, 130, Euclidean_distance(data->graphe->N[130], data->graphe->N[96]));
    ajouterarete(data->graphe, edge_index++, 100, 130, Euclidean_distance(data->graphe->N[130], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 134, 131, Euclidean_distance(data->graphe->N[131], data->graphe->N[134]));
    ajouterarete(data->graphe, edge_index++, 132, 131, Euclidean_distance(data->graphe->N[131], data->graphe->N[132]));
    ajouterarete(data->graphe, edge_index++, 135, 132, Euclidean_distance(data->graphe->N[132], data->graphe->N[135]));
    ajouterarete(data->graphe, edge_index++, 212, 133, Euclidean_distance(data->graphe->N[133], data->graphe->N[212]));
    ajouterarete(data->graphe, edge_index++, 134, 133, Euclidean_distance(data->graphe->N[133], data->graphe->N[134]));
    ajouterarete(data->graphe, edge_index++, 100, 212, Euclidean_distance(data->graphe->N[212], data->graphe->N[100]));
    ajouterarete(data->graphe, edge_index++, 136, 134, Euclidean_distance(data->graphe->N[134], data->graphe->N[136]));
    ajouterarete(data->graphe, edge_index++, 136, 135, Euclidean_distance(data->graphe->N[135], data->graphe->N[136]));
    ajouterarete(data->graphe, edge_index++, 137, 135, Euclidean_distance(data->graphe->N[135], data->graphe->N[137]));
    ajouterarete(data->graphe, edge_index++, 138, 136, Euclidean_distance(data->graphe->N[136], data->graphe->N[138]));
    ajouterarete(data->graphe, edge_index++, 140, 137, Euclidean_distance(data->graphe->N[137], data->graphe->N[140]));
    ajouterarete(data->graphe, edge_index++, 139, 138, Euclidean_distance(data->graphe->N[138], data->graphe->N[139]));
    ajouterarete(data->graphe, edge_index++, 102, 138, Euclidean_distance(data->graphe->N[138], data->graphe->N[102]));
    ajouterarete(data->graphe, edge_index++, 140, 139, Euclidean_distance(data->graphe->N[139], data->graphe->N[140]));
    ajouterarete(data->graphe, edge_index++, 141, 139, Euclidean_distance(data->graphe->N[139], data->graphe->N[141]));
    ajouterarete(data->graphe, edge_index++, 142, 140, Euclidean_distance(data->graphe->N[140], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 143, 141, Euclidean_distance(data->graphe->N[141], data->graphe->N[143]));
    ajouterarete(data->graphe, edge_index++, 105, 141, Euclidean_distance(data->graphe->N[141], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 145, 142, Euclidean_distance(data->graphe->N[142], data->graphe->N[145]));
    ajouterarete(data->graphe, edge_index++, 145, 143, Euclidean_distance(data->graphe->N[143], data->graphe->N[145]));
    ajouterarete(data->graphe, edge_index++, 148, 144, Euclidean_distance(data->graphe->N[144], data->graphe->N[148]));
    ajouterarete(data->graphe, edge_index++, 148, 145, Euclidean_distance(data->graphe->N[145], data->graphe->N[148]));
    ajouterarete(data->graphe, edge_index++, 149, 145, Euclidean_distance(data->graphe->N[145], data->graphe->N[149]));
    ajouterarete(data->graphe, edge_index++, 149, 146, Euclidean_distance(data->graphe->N[146], data->graphe->N[149]));
    ajouterarete(data->graphe, edge_index++, 147, 146, Euclidean_distance(data->graphe->N[146], data->graphe->N[147]));
        //
    ajouterarete(data->graphe, edge_index++, 152, 150, Euclidean_distance(data->graphe->N[150], data->graphe->N[152]));
    ajouterarete(data->graphe, edge_index++, 152, 151, Euclidean_distance(data->graphe->N[151], data->graphe->N[152]));
    ajouterarete(data->graphe, edge_index++, 106, 151, Euclidean_distance(data->graphe->N[151], data->graphe->N[106]));
    ajouterarete(data->graphe, edge_index++, 155, 152, Euclidean_distance(data->graphe->N[152], data->graphe->N[155]));
    ajouterarete(data->graphe, edge_index++, 153, 151, Euclidean_distance(data->graphe->N[151], data->graphe->N[153]));
    ajouterarete(data->graphe, edge_index++, 108, 153, Euclidean_distance(data->graphe->N[153], data->graphe->N[108]));
    ajouterarete(data->graphe, edge_index++, 154, 153, Euclidean_distance(data->graphe->N[153], data->graphe->N[154]));
    ajouterarete(data->graphe, edge_index++, 109, 154, Euclidean_distance(data->graphe->N[154], data->graphe->N[109]));
    ajouterarete(data->graphe, edge_index++, 156, 154, Euclidean_distance(data->graphe->N[154], data->graphe->N[156]));
    ajouterarete(data->graphe, edge_index++, 157, 155, Euclidean_distance(data->graphe->N[155], data->graphe->N[157]));
    ajouterarete(data->graphe, edge_index++, 159, 156, Euclidean_distance(data->graphe->N[156], data->graphe->N[159]));
    ajouterarete(data->graphe, edge_index++, 158, 157, Euclidean_distance(data->graphe->N[157], data->graphe->N[158]));
    ajouterarete(data->graphe, edge_index++, 159, 158, Euclidean_distance(data->graphe->N[158], data->graphe->N[159]));
    ajouterarete(data->graphe, edge_index++, 160, 158, Euclidean_distance(data->graphe->N[158], data->graphe->N[160]));
    ajouterarete(data->graphe, edge_index++, 156, 159, Euclidean_distance(data->graphe->N[159], data->graphe->N[156]));
    ajouterarete(data->graphe, edge_index++, 161, 160, Euclidean_distance(data->graphe->N[160], data->graphe->N[161]));
    ajouterarete(data->graphe, edge_index++, 162, 161, Euclidean_distance(data->graphe->N[161], data->graphe->N[163]));
    ajouterarete(data->graphe, edge_index++, 163, 162, Euclidean_distance(data->graphe->N[162], data->graphe->N[163]));
    ajouterarete(data->graphe, edge_index++, 165, 163, Euclidean_distance(data->graphe->N[163], data->graphe->N[165]));
    ajouterarete(data->graphe, edge_index++, 166, 164, Euclidean_distance(data->graphe->N[164], data->graphe->N[166]));
    ajouterarete(data->graphe, edge_index++, 121, 165, Euclidean_distance(data->graphe->N[165], data->graphe->N[121]));
    ajouterarete(data->graphe, edge_index++, 167, 164, Euclidean_distance(data->graphe->N[164], data->graphe->N[167]));
    ajouterarete(data->graphe, edge_index++, 127, 166, Euclidean_distance(data->graphe->N[166], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 170, 166, Euclidean_distance(data->graphe->N[166], data->graphe->N[170]));
    ajouterarete(data->graphe, edge_index++, 169, 167, Euclidean_distance(data->graphe->N[167], data->graphe->N[169]));
    ajouterarete(data->graphe, edge_index++, 170, 167, Euclidean_distance(data->graphe->N[167], data->graphe->N[170]));
    ajouterarete(data->graphe, edge_index++, 169, 168, Euclidean_distance(data->graphe->N[168], data->graphe->N[169]));
    ajouterarete(data->graphe, edge_index++, 171, 170, Euclidean_distance(data->graphe->N[170], data->graphe->N[171]));
    ajouterarete(data->graphe, edge_index++, 127, 171, Euclidean_distance(data->graphe->N[171], data->graphe->N[127]));
    ajouterarete(data->graphe, edge_index++, 132, 171, Euclidean_distance(data->graphe->N[171], data->graphe->N[132]));
    ajouterarete(data->graphe, edge_index++, 173, 172, Euclidean_distance(data->graphe->N[172], data->graphe->N[173]));
    ajouterarete(data->graphe, edge_index++, 137, 173, Euclidean_distance(data->graphe->N[173], data->graphe->N[137]));
    ajouterarete(data->graphe, edge_index++, 142, 173, Euclidean_distance(data->graphe->N[173], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 175, 174, Euclidean_distance(data->graphe->N[174], data->graphe->N[175]));
    ajouterarete(data->graphe, edge_index++, 142, 175, Euclidean_distance(data->graphe->N[175], data->graphe->N[142]));
    ajouterarete(data->graphe, edge_index++, 146, 175, Euclidean_distance(data->graphe->N[175], data->graphe->N[146]));
    ajouterarete(data->graphe, edge_index++, 176, 175, Euclidean_distance(data->graphe->N[175], data->graphe->N[176]));
        //
    ajouterarete(data->graphe, edge_index++, 178, 177, Euclidean_distance(data->graphe->N[177], data->graphe->N[178]));
    ajouterarete(data->graphe, edge_index++, 179, 178, Euclidean_distance(data->graphe->N[178], data->graphe->N[179]));
    ajouterarete(data->graphe, edge_index++, 181, 179, Euclidean_distance(data->graphe->N[179], data->graphe->N[181]));
    ajouterarete(data->graphe, edge_index++, 105, 179, Euclidean_distance(data->graphe->N[179], data->graphe->N[105]));
    ajouterarete(data->graphe, edge_index++, 181, 180, Euclidean_distance(data->graphe->N[180], data->graphe->N[181]));
    ajouterarete(data->graphe, edge_index++, 182, 181, Euclidean_distance(data->graphe->N[181], data->graphe->N[182]));
    ajouterarete(data->graphe, edge_index++, 185, 184, Euclidean_distance(data->graphe->N[184], data->graphe->N[185]));
    ajouterarete(data->graphe, edge_index++, 186, 183, Euclidean_distance(data->graphe->N[183], data->graphe->N[186]));
    ajouterarete(data->graphe, edge_index++, 155, 183, Euclidean_distance(data->graphe->N[183], data->graphe->N[155]));
    ajouterarete(data->graphe, edge_index++, 160, 185, Euclidean_distance(data->graphe->N[185], data->graphe->N[160]));
    ajouterarete(data->graphe, edge_index++, 187, 185, Euclidean_distance(data->graphe->N[185], data->graphe->N[187]));
    ajouterarete(data->graphe, edge_index++, 187, 186, Euclidean_distance(data->graphe->N[186], data->graphe->N[187]));
    ajouterarete(data->graphe, edge_index++, 188, 187, Euclidean_distance(data->graphe->N[187], data->graphe->N[188]));
    ajouterarete(data->graphe, edge_index++, 189, 188, Euclidean_distance(data->graphe->N[188], data->graphe->N[189]));
    ajouterarete(data->graphe, edge_index++, 191, 188, Euclidean_distance(data->graphe->N[188], data->graphe->N[191]));
    ajouterarete(data->graphe, edge_index++, 190, 189, Euclidean_distance(data->graphe->N[189], data->graphe->N[190]));
    ajouterarete(data->graphe, edge_index++, 161, 189, Euclidean_distance(data->graphe->N[189], data->graphe->N[161]));
    ajouterarete(data->graphe, edge_index++, 192, 190, Euclidean_distance(data->graphe->N[190], data->graphe->N[192]));
    ajouterarete(data->graphe, edge_index++, 168, 190, Euclidean_distance(data->graphe->N[190], data->graphe->N[168]));
    ajouterarete(data->graphe, edge_index++, 192, 191, Euclidean_distance(data->graphe->N[191], data->graphe->N[192]));
    ajouterarete(data->graphe, edge_index++, 193, 192, Euclidean_distance(data->graphe->N[192], data->graphe->N[193]));
    ajouterarete(data->graphe, edge_index++, 194, 193, Euclidean_distance(data->graphe->N[193], data->graphe->N[194]));
    ajouterarete(data->graphe, edge_index++, 195, 193, Euclidean_distance(data->graphe->N[193], data->graphe->N[195]));
    ajouterarete(data->graphe, edge_index++, 196, 194, Euclidean_distance(data->graphe->N[194], data->graphe->N[196]));
    ajouterarete(data->graphe, edge_index++, 168, 194, Euclidean_distance(data->graphe->N[194], data->graphe->N[168]));
    ajouterarete(data->graphe, edge_index++, 172, 196, Euclidean_distance(data->graphe->N[196], data->graphe->N[172]));
    ajouterarete(data->graphe, edge_index++, 197, 195, Euclidean_distance(data->graphe->N[195], data->graphe->N[197]));
    ajouterarete(data->graphe, edge_index++, 198, 197, Euclidean_distance(data->graphe->N[197], data->graphe->N[198]));
    ajouterarete(data->graphe, edge_index++, 199, 198, Euclidean_distance(data->graphe->N[198], data->graphe->N[199]));
    ajouterarete(data->graphe, edge_index++, 200, 198, Euclidean_distance(data->graphe->N[198], data->graphe->N[200]));
    ajouterarete(data->graphe, edge_index++, 201, 199, Euclidean_distance(data->graphe->N[199], data->graphe->N[201]));
    ajouterarete(data->graphe, edge_index++, 201, 200, Euclidean_distance(data->graphe->N[200], data->graphe->N[201]));
    ajouterarete(data->graphe, edge_index++, 172, 200, Euclidean_distance(data->graphe->N[200], data->graphe->N[172]));
    ajouterarete(data->graphe, edge_index++, 202, 201, Euclidean_distance(data->graphe->N[201], data->graphe->N[204]));
    ajouterarete(data->graphe, edge_index++, 204, 203, Euclidean_distance(data->graphe->N[203], data->graphe->N[204]));
    ajouterarete(data->graphe, edge_index++, 205, 204, Euclidean_distance(data->graphe->N[204], data->graphe->N[205]));
    ajouterarete(data->graphe, edge_index++, 176, 204, Euclidean_distance(data->graphe->N[204], data->graphe->N[176]));


    data->graphe->A[115].feuxRouges = 1;

    data->graphe->A[88].embouteillages = 1;

    data->graphe->A[3].passagers = 1;

    data->graphe->A[2].feuxRouges = 1;


    data->graphe->A[4].feuxRouges = 1;

    data->graphe->A[5].embouteillages = 1;

    data->graphe->A[6].passagers = 1;

    ///
    data->graphe->A[76].feuxRouges = 1;

    data->graphe->A[11].passagers = 1;

    data->graphe->A[142].feuxRouges = 1;

    data->graphe->A[77].feuxRouges = 1;

    data->graphe->A[194].feuxRouges = 1;

    data->graphe->A[116].embouteillages = 1;

    data->graphe->A[78].passagers = 1;


    data->graphe->nbaretes = edge_index;

    /*ajoute des feux rouges sur les nœuds */
    ajouterFeuRouge(data->graphe, 1, 3, 1, 3, 3); // état rouge, durée 3 s
    ajouterFeuRouge(data->graphe, 2, 77, 1, 3, 3);
    ajouterFeuRouge(data->graphe, 3, 168, 1, 3, 3);



}

/* ===================== Activation de l'application ===================== */
static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = g_malloc(sizeof(AppData));
    data->selected_source = -1;
    data->selected_destination = -1;
    data->chemin = NULL;
    data->chemin_length = 0;
    data->simulation_index = 0;
    data->simulation_progress = 0.0;
    data->simulation_pause_remaining = 0.0;
    data->simulation_total_time = 0.0;
    memset(data->event_reason, 0, sizeof(data->event_reason));

    GError *error = NULL;
    data->map_pixbuf = gdk_pixbuf_new_from_file("map.png", &error);
    if (!data->map_pixbuf) {
        g_print("Erreur lors du chargement de map.png : %s\n", error->message);
        g_error_free(error);
        data->map_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 600, 500);
    }

    init_graph(data);

    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "Simulation de Transport");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 600);

    /* Charger le CSS pour agrandir les messages */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "window { background-color: #f0f0f0; }"
        ".drawing-area { border: 2px solid #34495e; }"
        ".message { font-size: 20px; color: #ff0000; }",
        -1);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
         GTK_STYLE_PROVIDER(css_provider),
         GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);

    data->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(data->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_stack_set_transition_duration(GTK_STACK(data->stack), 500);

    /* ----- Page d'entrée ----- */
    data->page_input = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    data->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(data->drawing_area, TRUE);
    gtk_widget_set_vexpand(data->drawing_area, TRUE);
    gtk_box_append(GTK_BOX(data->page_input), data->drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->drawing_area),
                                   (GtkDrawingAreaDrawFunc) on_draw_input, data, NULL);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_widget_add_controller(data->drawing_area, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(on_map_click), data);
    data->combo_vehicule = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->combo_vehicule), "Voiture");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->combo_vehicule), "Bus");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->combo_vehicule), "Camion");
    gtk_box_append(GTK_BOX(data->page_input), data->combo_vehicule);
    data->btn_valider = gtk_button_new_with_label("Valider");
    gtk_box_append(GTK_BOX(data->page_input), data->btn_valider);
    g_signal_connect(data->btn_valider, "clicked", G_CALLBACK(on_valider), data);
    gtk_stack_add_titled(GTK_STACK(data->stack), data->page_input, "page_input", "Entrée");

    /* ----- Page des résultats ----- */
    data->page_result = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    data->header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    data->btn_back = gtk_button_new_with_label("Retour");
    g_signal_connect(data->btn_back, "clicked", G_CALLBACK(on_back), data);
    data->btn_quit = gtk_button_new_with_label("Quitter");
    g_signal_connect(data->btn_quit, "clicked", G_CALLBACK(on_quit), data);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(data->header_box), data->btn_back);
    gtk_box_append(GTK_BOX(data->header_box), spacer);
    gtk_box_append(GTK_BOX(data->header_box), data->btn_quit);
    gtk_box_append(GTK_BOX(data->page_result), data->header_box);
    data->temp_message_label = gtk_label_new("");
    gtk_widget_add_css_class(data->temp_message_label, "message");
    gtk_box_append(GTK_BOX(data->page_result), data->temp_message_label);
    data->result_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(data->result_drawing_area, TRUE);
    gtk_widget_set_vexpand(data->result_drawing_area, TRUE);
    gtk_box_append(GTK_BOX(data->page_result), data->result_drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->result_drawing_area),
                                   (GtkDrawingAreaDrawFunc) on_draw_result, data, NULL);
    data->label_resultats = gtk_label_new("");
    gtk_box_append(GTK_BOX(data->page_result), data->label_resultats);
    gtk_stack_add_titled(GTK_STACK(data->stack), data->page_result, "page_result", "Résultats");

    gtk_window_set_child(GTK_WINDOW(data->window), data->stack);
    gtk_window_present(GTK_WINDOW(data->window));
}

/* ===================== Fonction principale ===================== */
int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    app = gtk_application_new("org.example.GtkMapSimulation", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
