/*
 * ATM Simulator - GTK 3 GUI version (pure C)
 *
 * Features (same as the original console project):
 *   - PIN login (default PIN 1234, 3 attempts, then locked)
 *   - Check balance
 *   - Deposit money
 *   - Withdraw money
 *   - Change PIN (actually saves the new PIN, unlike the console version)
 *   - Exit
 *
 * Build:
 *   gcc atm_gtk.c -o atm_gtk `pkg-config --cflags --libs gtk+-3.0`
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ATTEMPTS 3
#define DEFAULT_PIN  1234
#define START_BALANCE 5000.0

typedef struct {
    /* data */
    int    pin;
    int    attempts;
    double balance;

    /* widgets */
    GtkWidget *window;
    GtkWidget *stack;

    /* login page */
    GtkWidget *pin_entry;
    GtkWidget *login_btn;
    GtkWidget *login_msg;

    /* main page */
    GtkWidget *balance_label;
    GtkWidget *amount_entry;
    GtkWidget *status_label;
} App;

static App app;

/* ---------- CSS styling ---------- */

static const char *CSS =
    "window { background: #eef2f7; }"
    ".card { background: white; border-radius: 12px; padding: 24px;"
    "        box-shadow: 0 2px 10px rgba(0,0,0,0.15); }"
    ".title { font-size: 24px; font-weight: bold; color: #1a3c6e; }"
    ".subtitle { color: #666666; }"
    ".balance { font-size: 32px; font-weight: bold; color: #1b7f3b; }"
    ".ok { color: #1b7f3b; font-weight: bold; }"
    ".err { color: #c0392b; font-weight: bold; }"
    ".primary { background: #1a3c6e; color: white; border-radius: 6px;"
    "           padding: 8px 14px; background-image: none; }"
    ".primary:hover { background: #24539c; }"
    ".danger { background: #c0392b; color: white; border-radius: 6px;"
    "          padding: 8px 14px; background-image: none; }"
    ".danger:hover { background: #e04b3a; }"
    "entry { padding: 6px; }";

static void load_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* ---------- helpers ---------- */

static void set_msg(GtkWidget *label, const char *text, gboolean ok)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context(label);
    gtk_style_context_remove_class(ctx, "ok");
    gtk_style_context_remove_class(ctx, "err");
    gtk_style_context_add_class(ctx, ok ? "ok" : "err");
    gtk_label_set_text(GTK_LABEL(label), text);
}

static void refresh_balance(void)
{
    char buf[64];
    snprintf(buf, sizeof buf, "Rs. %.2f", app.balance);
    gtk_label_set_text(GTK_LABEL(app.balance_label), buf);
}

/* Parse a positive number from an entry. Returns TRUE on success. */
static gboolean read_amount(GtkWidget *entry, double *out)
{
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry));
    char *end = NULL;
    double v;

    if (!txt || !*txt)
        return FALSE;
    v = strtod(txt, &end);
    while (end && (*end == ' ' || *end == '\t'))
        end++;
    if (end == txt || (end && *end != '\0'))
        return FALSE;
    if (!(v > 0.0) || v > 1e12)
        return FALSE;
    *out = v;
    return TRUE;
}

/* Only allow digits in PIN entries */
static void digits_only(GtkEditable *editable, const gchar *text, gint length,
                        gint *position, gpointer data)
{
    gint i;
    (void)data;
    if (length < 0)
        length = (gint)strlen(text);
    for (i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9') {
            g_signal_stop_emission_by_name(editable, "insert-text");
            return;
        }
    }
    (void)position;
}

static GtkWidget *make_pin_entry(void)
{
    GtkWidget *e = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(e), FALSE);
    gtk_entry_set_max_length(GTK_ENTRY(e), 4);
    gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_DIGITS);
    gtk_entry_set_alignment(GTK_ENTRY(e), 0.5);
    g_signal_connect(e, "insert-text", G_CALLBACK(digits_only), NULL);
    return e;
}

/* ---------- login logic ---------- */

static void on_login(GtkWidget *w, gpointer data)
{
    const char *txt;
    char buf[96];
    (void)w; (void)data;

    if (app.attempts >= MAX_ATTEMPTS)
        return;

    txt = gtk_entry_get_text(GTK_ENTRY(app.pin_entry));
    if (strlen(txt) != 4) {
        set_msg(app.login_msg, "Please enter your 4-digit PIN.", FALSE);
        return;
    }

    if (atoi(txt) == app.pin) {
        app.attempts = 0;
        gtk_entry_set_text(GTK_ENTRY(app.pin_entry), "");
        set_msg(app.login_msg, "", TRUE);
        refresh_balance();
        set_msg(app.status_label, "Login Successful!", TRUE);
        gtk_entry_set_text(GTK_ENTRY(app.amount_entry), "");
        gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "main");
        return;
    }

    app.attempts++;
    gtk_entry_set_text(GTK_ENTRY(app.pin_entry), "");

    if (app.attempts >= MAX_ATTEMPTS) {
        set_msg(app.login_msg,
                "ATM access blocked. Too many incorrect attempts.", FALSE);
        gtk_widget_set_sensitive(app.pin_entry, FALSE);
        gtk_widget_set_sensitive(app.login_btn, FALSE);
    } else {
        snprintf(buf, sizeof buf, "Incorrect PIN! Attempts left: %d",
                 MAX_ATTEMPTS - app.attempts);
        set_msg(app.login_msg, buf, FALSE);
    }
}

/* ---------- main menu actions ---------- */

static void on_check_balance(GtkWidget *w, gpointer data)
{
    char buf[80];
    (void)w; (void)data;
    snprintf(buf, sizeof buf, "Balance: Rs. %.2f", app.balance);
    refresh_balance();
    set_msg(app.status_label, buf, TRUE);
}

static void on_deposit(GtkWidget *w, gpointer data)
{
    double amt;
    (void)w; (void)data;

    if (!read_amount(app.amount_entry, &amt)) {
        set_msg(app.status_label, "Invalid amount!", FALSE);
        return;
    }
    app.balance += amt;
    refresh_balance();
    gtk_entry_set_text(GTK_ENTRY(app.amount_entry), "");
    set_msg(app.status_label, "Deposit successful!", TRUE);
}

static void on_withdraw(GtkWidget *w, gpointer data)
{
    double amt;
    (void)w; (void)data;

    if (!read_amount(app.amount_entry, &amt)) {
        set_msg(app.status_label, "Invalid amount!", FALSE);
        return;
    }
    if (amt > app.balance) {
        set_msg(app.status_label, "Insufficient balance!", FALSE);
        return;
    }
    app.balance -= amt;
    refresh_balance();
    gtk_entry_set_text(GTK_ENTRY(app.amount_entry), "");
    set_msg(app.status_label, "Withdrawal successful!", TRUE);
}

static void on_change_pin(GtkWidget *w, gpointer data)
{
    GtkWidget *dlg, *area, *grid, *cur, *neu, *con, *msg;
    gint resp;
    (void)w; (void)data;

    dlg = gtk_dialog_new_with_buttons("Change PIN",
            GTK_WINDOW(app.window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Change", GTK_RESPONSE_OK,
            NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
    gtk_container_add(GTK_CONTAINER(area), grid);

    cur = make_pin_entry();
    neu = make_pin_entry();
    con = make_pin_entry();
    gtk_entry_set_activates_default(GTK_ENTRY(con), TRUE);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Current PIN:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), cur, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("New 4-digit PIN:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), neu, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Confirm new PIN:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), con, 1, 2, 1, 1);

    msg = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), msg, 0, 3, 2, 1);
    gtk_widget_show_all(dlg);

    /* keep the dialog open until success or cancel */
    for (;;) {
        resp = gtk_dialog_run(GTK_DIALOG(dlg));
        if (resp != GTK_RESPONSE_OK)
            break;

        const char *c = gtk_entry_get_text(GTK_ENTRY(cur));
        const char *n = gtk_entry_get_text(GTK_ENTRY(neu));
        const char *f = gtk_entry_get_text(GTK_ENTRY(con));

        if (strlen(c) != 4 || atoi(c) != app.pin) {
            set_msg(msg, "Incorrect current PIN!", FALSE);
        } else if (strlen(n) != 4 || atoi(n) < 1000) {
            set_msg(msg, "PIN must be 4 digits (1000-9999).", FALSE);
        } else if (strcmp(n, f) != 0) {
            set_msg(msg, "New PIN and confirmation do not match.", FALSE);
        } else {
            app.pin = atoi(n);
            set_msg(app.status_label, "PIN changed successfully!", TRUE);
            break;
        }
    }
    gtk_widget_destroy(dlg);
}

static void on_quit(GtkWidget *w, gpointer data)
{
    GtkWidget *dlg;
    (void)w; (void)data;

    dlg = gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
            "Thank you for using ATM!");
    gtk_dialog_add_buttons(GTK_DIALOG(dlg),
            "_Stay", GTK_RESPONSE_CANCEL,
            "_Exit", GTK_RESPONSE_OK, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        gtk_widget_destroy(dlg);
        gtk_widget_destroy(app.window);   /* triggers "destroy" -> gtk_main_quit */
        return;
    }
    gtk_widget_destroy(dlg);
}

/* ---------- page builders ---------- */

static GtkWidget *wrap_card(GtkWidget *inner)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *card  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");
    gtk_box_pack_start(GTK_BOX(card), inner, TRUE, TRUE, 0);

    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 380, -1);
    gtk_box_pack_start(GTK_BOX(outer), card, TRUE, TRUE, 0);
    return outer;
}

static GtkWidget *add_class(GtkWidget *w, const char *cls)
{
    gtk_style_context_add_class(gtk_widget_get_style_context(w), cls);
    return w;
}

static GtkWidget *build_login_page(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *title = gtk_label_new("ATM SIMULATOR");
    GtkWidget *sub = gtk_label_new("Enter your 4-digit ATM PIN");

    add_class(title, "title");
    add_class(sub, "subtitle");

    app.pin_entry = make_pin_entry();
    g_signal_connect(app.pin_entry, "activate", G_CALLBACK(on_login), NULL);

    app.login_btn = add_class(gtk_button_new_with_label("Login"), "primary");
    g_signal_connect(app.login_btn, "clicked", G_CALLBACK(on_login), NULL);

    app.login_msg = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.login_msg), TRUE);

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.pin_entry, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(box), app.login_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.login_msg, FALSE, FALSE, 0);

    return wrap_card(box);
}

static GtkWidget *build_main_page(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title = gtk_label_new("ATM SIMULATOR");
    GtkWidget *bal_caption = gtk_label_new("Available Balance");
    GtkWidget *amt_caption = gtk_label_new("Amount (Rs.)");
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *b_dep, *b_wd, *b_bal, *b_pin, *b_exit;

    add_class(title, "title");
    add_class(bal_caption, "subtitle");
    add_class(amt_caption, "subtitle");

    app.balance_label = add_class(gtk_label_new("Rs. 0.00"), "balance");

    app.amount_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.amount_entry), "Enter amount");
    gtk_entry_set_input_purpose(GTK_ENTRY(app.amount_entry),
                                GTK_INPUT_PURPOSE_NUMBER);
    gtk_entry_set_alignment(GTK_ENTRY(app.amount_entry), 0.5);

    b_dep  = add_class(gtk_button_new_with_label("Deposit"), "primary");
    b_wd   = add_class(gtk_button_new_with_label("Withdraw"), "primary");
    b_bal  = add_class(gtk_button_new_with_label("Check Balance"), "primary");
    b_pin  = add_class(gtk_button_new_with_label("Change PIN"), "primary");
    b_exit = add_class(gtk_button_new_with_label("Exit"), "danger");

    g_signal_connect(b_dep,  "clicked", G_CALLBACK(on_deposit), NULL);
    g_signal_connect(b_wd,   "clicked", G_CALLBACK(on_withdraw), NULL);
    g_signal_connect(b_bal,  "clicked", G_CALLBACK(on_check_balance), NULL);
    g_signal_connect(b_pin,  "clicked", G_CALLBACK(on_change_pin), NULL);
    g_signal_connect(b_exit, "clicked", G_CALLBACK(on_quit), NULL);

    gtk_box_set_homogeneous(GTK_BOX(row1), TRUE);
    gtk_box_set_homogeneous(GTK_BOX(row2), TRUE);
    gtk_box_pack_start(GTK_BOX(row1), b_dep, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row1), b_wd, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row2), b_bal, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row2), b_pin, TRUE, TRUE, 0);

    app.status_label = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.status_label), TRUE);

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), bal_caption, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.balance_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), amt_caption, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.amount_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), b_exit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.status_label, FALSE, FALSE, 4);

    return wrap_card(box);
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    app.pin = DEFAULT_PIN;
    app.attempts = 0;
    app.balance = START_BALANCE;

    load_css();

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "ATM Simulator");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 480, 520);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_stack_add_named(GTK_STACK(app.stack), build_login_page(), "login");
    gtk_stack_add_named(GTK_STACK(app.stack), build_main_page(), "main");
    gtk_container_add(GTK_CONTAINER(app.window), app.stack);

    gtk_widget_show_all(app.window);
    gtk_main();
    return 0;
}
