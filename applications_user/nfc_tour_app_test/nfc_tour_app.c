/**
 * Application "Hello World" Test pour Flipper Zero
 * Ce code est une version de test simplifiée pour vérifier que la compilation fonctionne.
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>

// Structure de l'application
typedef struct {
    ViewDispatcher* view_dispatcher;
    Widget* widget;
} App;

// Callback pour le bouton retour pour quitter l'app
static bool app_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; 
    }
    return true;
}

// Point d'entrée principal
int32_t nfc_tour_app_main(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));

    app->view_dispatcher = view_dispatcher_alloc();
    app->widget = widget_alloc();

    // Ajoute le texte à afficher
    widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Test de Compilation");
    widget_add_string_element(app->widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "Si vous voyez ca, ca marche !");

    // Configure l'interface
    view_dispatcher_set_input_callback(app->view_dispatcher, app_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, 0, widget_get_view(app->widget));
    
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    // Lance la boucle de l'application
    view_dispatcher_run(app->view_dispatcher);

    // Nettoyage à la sortie
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    widget_free(app->widget);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
