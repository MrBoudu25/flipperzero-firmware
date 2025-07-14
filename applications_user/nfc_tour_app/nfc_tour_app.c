/**
 * Application NFC Tour pour Flipper Zero
 * Version finale et fonctionnelle.
 */

#include <furi.h>
#include <furi_hal.h>

// --- Interfaces graphiques ---
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>

// --- API nécessaires ---
#include <storage/storage.h> // Pour lire les fichiers sur la carte SD
#include <nfc/nfc_worker.h>   // Pour émuler les badges NFC

// --- Constantes de l'application ---
#define TAG "NFCTourApp"
#define NFC_TOUR_FOLDER "/ext/nfc_tour" // Le dossier où stocker les badges .nfc
#define MAX_BADGES 64                   // Nombre maximum de badges gérés

// --- Structure principale de l'application ---
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextBox* textbox;
    FuriThread* worker_thread;
    volatile bool stop_requested;
} App;

// --- Vues de l'application ---
typedef enum {
    AppViewSubmenu,
    AppViewTextBox,
} AppView;

// Callback pour gérer le bouton retour et quitter l'application
static bool app_navigation_callback(void* context) {
    App* app = context;
    // Si on est dans la vue de la tournée, on demande l'arrêt du thread
    if(view_dispatcher_get_current_view_index(app->view_dispatcher) == AppViewTextBox) {
        if(!app->stop_requested) {
            app->stop_requested = true;
            furi_thread_join(app->worker_thread); // On attend que le thread se termine proprement
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
        }
        return true; // Indique qu'on a géré l'événement
    }
    // Sinon, on quitte l'application
    return false;
}

/**
 * La fonction qui exécute la tournée.
 * Elle tourne dans un thread séparé pour ne pas geler l'interface.
 */
static int32_t nfc_tour_worker(void* context) {
    App* app = context;
    app->stop_requested = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Dir* dir = storage_dir_alloc(storage);

    char* badge_files[MAX_BADGES];
    size_t badge_count = 0;

    text_box_set_text(app->textbox, "Recherche des badges\ndans " NFC_TOUR_FOLDER "...");
    
    if(!storage_dir_exists(storage, NFC_TOUR_FOLDER)) {
        storage_simply_mkdir(storage, NFC_TOUR_FOLDER);
    }

    if(storage_dir_open(dir, NFC_TOUR_FOLDER)) {
        const char* file_name;
        while(storage_dir_read(dir, NULL, &file_name, 1024) && badge_count < MAX_BADGES) {
            if(strstr(file_name, ".nfc")) {
                badge_files[badge_count] = strdup(file_name);
                badge_count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_dir_free(dir);

    if(badge_count == 0) {
        text_box_set_text(app->textbox, "Aucun badge .nfc trouve.\n\nPlacez vos badges dans\n" NFC_TOUR_FOLDER);
        furi_delay_ms(4000);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    size_t current_badge_index = 0;
    while(!app->stop_requested) {
        FuriString* current_path = furi_string_alloc_printf("%s/%s", NFC_TOUR_FOLDER, badge_files[current_badge_index]);
        FuriString* display_text = furi_string_alloc_printf(
            "Badge %zu/%zu\n%s\n\nEmulation (10s)...",
            current_badge_index + 1,
            badge_count,
            badge_files[current_badge_index]);
        text_box_set_text(app->textbox, furi_string_get_cstr(display_text));

        NFCWorker* nfc_worker = nfc_worker_alloc();
        if(nfc_worker_load(nfc_worker, furi_string_get_cstr(current_path))) {
            nfc_worker_start_emulate(nfc_worker);
            for(int i = 0; i < 100 && !app->stop_requested; i++) furi_delay_ms(100);
            nfc_worker_stop(nfc_worker);
        } else {
            furi_string_cat_printf(display_text, "\nErreur chargement !");
            text_box_set_text(app->textbox, furi_string_get_cstr(display_text));
            furi_delay_ms(2000);
        }
        nfc_worker_free(nfc_worker);
        furi_string_free(current_path);
        furi_string_free(display_text);

        if(app->stop_requested) break;

        uint32_t delay_s = 30 + (furi_hal_random_get() % 151);
        display_text = furi_string_alloc_printf("Pause de %lu secondes...", delay_s);
        text_box_set_text(app->textbox, furi_string_get_cstr(display_text));
        furi_string_free(display_text);
        for(uint32_t i = 0; i < delay_s * 10 && !app->stop_requested; i++) furi_delay_ms(100);

        if(app->stop_requested) break;
        current_badge_index = (current_badge_index + 1) % badge_count;
    }

    for(size_t i = 0; i < badge_count; i++) free(badge_files[i]);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    if(index == 0) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextBox);
        furi_thread_set_callback(app->worker_thread, nfc_tour_worker);
        furi_thread_start(app->worker_thread);
    }
}

int32_t nfc_tour_app_main(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->textbox = text_box_alloc();
    app->worker_thread = furi_thread_alloc_ex("NFCTourWorker", 1024, app);

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, AppViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, AppViewTextBox, text_box_get_view(app->textbox));
    submenu_add_item(app->submenu, "Demarrer la tournee", 0, submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    app->stop_requested = true;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewTextBox);
    text_box_free(app->textbox);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
