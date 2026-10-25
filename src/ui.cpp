// LVGL user interface.
//
// Layout (320x240, landscape):
//   +----------------------------------------------+
//   | [A1 Mini] [X2D]                         wifi |  36 px header: printer selector
//   | Status | Queue (10)                          |  30 px tab bar
//   |                                              |
//   |  state / job / progress / temps              |  "Status" tab
//   |  [ primary action ]   [ secondary ]          |
//   +----------------------------------------------+
//
// Threading rule: everything in this file runs on the Arduino loop task (LVGL is
// not thread-safe). Data comes in only through net_snapshot() / net_poll_result(),
// and commands go out only through net_send().

#include "ui.h"

#include "model.h"
#include "net.h"
#include "strings.h"

#define UI_W 320
#define UI_H 240

// FontAwesome glyphs baked into the custom fonts that LVGL has no LV_SYMBOL_ for.
#define SYM_CLOCK "\xEF\x80\x97"  // U+F017
#define SYM_FIRE  "\xEF\x81\xAD"  // U+F06D

static AppState S;  // last copy of the network task's state
static uint32_t s_version = UINT32_MAX;
static int s_sel = 0;  // index of the selected printer
static uint32_t s_queue_sig = 0;

static lv_obj_t *printer_bar, *net_icon, *tabview, *tab_status, *tab_queue, *queue_list;
static lv_obj_t *lbl_state, *lbl_job, *bar_progress, *lbl_pct, *lbl_info, *lbl_temp;
static lv_obj_t *btn_a, *btn_b, *lbl_a, *lbl_b, *toast;
static lv_timer_t *toast_timer;

// lv_buttonmatrix keeps a pointer to the map, so it must stay alive.
static char printer_names[MAX_PRINTERS][28];
static const char *printer_map[MAX_PRINTERS + 1];
static int printer_map_count = -1;

struct Action {
    bool visible;
    Command cmd;
    const char *confirm;  // nullptr = no confirmation dialog
};
static Action action_a, action_b;
static Command s_pending;  // sent by the confirmation dialog's OK button

// ---------------------------------------------------------------- helpers

static const char *display_name(const PrinterInfo &p) {
    // Auto-discovered printers are named "Printer at <IP>"; the model is nicer.
    if (strncmp(p.name, "Printer at ", 11) == 0 && p.model[0]) return p.model;
    return p.name;
}

static void fmt_duration(char *out, size_t n, int minutes) {
    if (minutes <= 0)
        snprintf(out, n, STR_DUR_UNDER_1);
    else if (minutes < 60)
        snprintf(out, n, STR_FMT_DUR_MIN, minutes);
    else
        snprintf(out, n, STR_FMT_DUR_H_MIN, minutes / 60, minutes % 60);
}

// Bambu gcode_state values as reported by Bambuddy.
static const char *state_text(const char *s) {
    if (!strcmp(s, "IDLE")) return STR_STATE_IDLE;
    if (!strcmp(s, "PREPARE")) return STR_STATE_PREPARE;
    if (!strcmp(s, "SLICING")) return STR_STATE_SLICING;
    if (!strcmp(s, "RUNNING")) return STR_STATE_RUNNING;
    if (!strcmp(s, "PAUSE")) return STR_STATE_PAUSE;
    if (!strcmp(s, "FINISH")) return STR_STATE_FINISH;
    if (!strcmp(s, "FAILED")) return STR_STATE_FAILED;
    return s;
}

static bool is_busy(const PrinterInfo &p) {
    return !strcmp(p.state, "RUNNING") || !strcmp(p.state, "PREPARE") || !strcmp(p.state, "SLICING") ||
           !strcmp(p.state, "PAUSE");
}

static void toast_hide_cb(lv_timer_t *t) {
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(t);
}

static void show_toast(const char *text, bool error = false) {
    lv_label_set_text(toast, text);
    lv_obj_set_style_bg_color(toast, error ? lv_palette_darken(LV_PALETTE_RED, 2) : lv_color_hex(0x303030), 0);
    lv_obj_remove_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast);
    lv_timer_reset(toast_timer);
    lv_timer_resume(toast_timer);
}

static void send(const Command &cmd) {
    net_send(cmd);
    show_toast(LV_SYMBOL_UPLOAD " " STR_SENDING);
}

// ---------------------------------------------------------------- confirmation dialog

static void msgbox_close_cb(lv_event_t *e) { lv_msgbox_close_async((lv_obj_t *)lv_event_get_user_data(e)); }

static void msgbox_ok_cb(lv_event_t *e) {
    send(s_pending);
    lv_msgbox_close_async((lv_obj_t *)lv_event_get_user_data(e));
}

static void confirm(const char *title, const char *text, const char *ok_label, const Command &cmd, bool danger) {
    s_pending = cmd;
    lv_obj_t *mb = lv_msgbox_create(NULL);  // NULL = modal on the top layer
    lv_obj_set_width(mb, 290);
    lv_msgbox_add_title(mb, title);
    lv_msgbox_add_text(mb, text);
    lv_obj_t *no = lv_msgbox_add_footer_button(mb, STR_BTN_CANCEL);
    lv_obj_add_event_cb(no, msgbox_close_cb, LV_EVENT_CLICKED, mb);
    lv_obj_t *ok = lv_msgbox_add_footer_button(mb, ok_label);
    lv_obj_add_event_cb(ok, msgbox_ok_cb, LV_EVENT_CLICKED, mb);
    lv_obj_set_style_bg_color(ok, lv_palette_main(danger ? LV_PALETTE_RED : LV_PALETTE_GREEN), 0);
    lv_obj_set_style_bg_color(no, lv_color_hex(0x404040), 0);
}

// ---------------------------------------------------------------- events

static void printer_bar_cb(lv_event_t *e) {
    uint32_t idx = lv_buttonmatrix_get_selected_button(printer_bar);
    if (idx == LV_BUTTONMATRIX_BUTTON_NONE || (int)idx >= S.printer_count) return;
    s_sel = idx;
    s_version = UINT32_MAX;  // force a full redraw on the next tick
    s_queue_sig = 0;
}

static void action_cb(lv_event_t *e) {
    Action *a = (Action *)lv_event_get_user_data(e);
    if (!a->visible) return;
    if (a->confirm) {
        bool danger = a->cmd.type == CMD_STOP;
        confirm(danger ? STR_TITLE_STOP : STR_TITLE_CONFIRM, a->confirm, danger ? STR_BTN_STOP : STR_BTN_GO, a->cmd,
                danger);
    } else {
        send(a->cmd);
    }
}

static void queue_item_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_sel >= S.printer_count || i >= S.queue_count[s_sel]) return;
    const PrinterInfo &p = S.printers[s_sel];
    const QueueItem &q = S.queue[s_sel][i];

    static char text[256];
    char dur[24];
    fmt_duration(dur, sizeof(dur), q.print_s / 60);
    int len = snprintf(text, sizeof(text), "%s · %.0f g %s", dur, q.grams, q.filament);
    if (q.filament_short)
        len += snprintf(text + len, sizeof(text) - len, "\n" LV_SYMBOL_WARNING " " STR_FILAMENT_MAY_RUN_OUT);
    if (q.waiting[0]) len += snprintf(text + len, sizeof(text) - len, "\n%s", q.waiting);
    if (is_busy(p))
        snprintf(text + len, sizeof(text) - len, "\n" STR_STARTS_AFTER_CURRENT);
    else if (p.awaiting_plate_clear)
        snprintf(text + len, sizeof(text) - len, "\n" STR_MARKS_PLATE_CLEAR);

    Command cmd = {p.awaiting_plate_clear ? CMD_CLEAR_AND_START : CMD_START_ITEM, p.id, q.id, false};
    confirm(q.name, text, LV_SYMBOL_PLAY " " STR_BTN_START, cmd, false);
}

// ---------------------------------------------------------------- refresh

static void update_printer_bar() {
    bool changed = printer_map_count != S.printer_count;
    for (int i = 0; i < S.printer_count; i++) {
        const char *n = display_name(S.printers[i]);
        if (strncmp(printer_names[i], n, sizeof(printer_names[i]) - 1) != 0) changed = true;
        strlcpy(printer_names[i], n, sizeof(printer_names[i]));
        printer_map[i] = printer_names[i];
    }
    if (!changed) return;
    printer_map[S.printer_count] = "";  // map terminator
    printer_map_count = S.printer_count;
    if (S.printer_count == 0) {
        static const char *empty[] = {STR_NO_PRINTER, ""};
        lv_buttonmatrix_set_map(printer_bar, empty);
        return;
    }
    if (s_sel >= S.printer_count) s_sel = 0;
    lv_buttonmatrix_set_map(printer_bar, printer_map);
    lv_buttonmatrix_set_button_ctrl_all(printer_bar, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl(printer_bar, s_sel, LV_BUTTONMATRIX_CTRL_CHECKED);
}

static void set_action(lv_obj_t *btn, lv_obj_t *lbl, Action &a, bool visible, const char *text = "",
                       Command cmd = {}, const char *confirm_text = nullptr, bool danger = false) {
    a.visible = visible;
    a.cmd = cmd;
    a.confirm = confirm_text;
    if (!visible) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_bg_color(btn, danger ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN), 0);
}

static void clear_status_details() {
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_pct, "");
    lv_label_set_text(lbl_info, "");
    lv_label_set_text(lbl_temp, "");
    set_action(btn_a, lbl_a, action_a, false);
    set_action(btn_b, lbl_b, action_b, false);
}

static void update_status() {
    char buf[128], dur[24];

    if (!S.wifi || !S.api_ok || S.printer_count == 0) {
        lv_label_set_text(lbl_state, !S.wifi ? STR_WIFI_CONNECTING : STR_CONNECTING_API);
        lv_label_set_text(lbl_job, S.error[0] ? S.error : (S.wifi && S.api_ok ? STR_NO_ACTIVE_PRINTER : ""));
        clear_status_details();
        return;
    }

    const PrinterInfo &p = S.printers[s_sel];
    int qn = S.queue_count[s_sel];

    if (!p.status_ok || !p.connected) {
        lv_label_set_text(lbl_state, LV_SYMBOL_CLOSE " " STR_OFFLINE);
        lv_label_set_text(lbl_job, p.status_ok ? STR_PRINTER_UNREACHABLE : STR_STATUS_UNAVAILABLE);
        clear_status_details();
        return;
    }

    bool busy = is_busy(p);
    if (p.awaiting_plate_clear && !busy)
        snprintf(buf, sizeof(buf), "%s " LV_SYMBOL_RIGHT " " STR_CLEAR_PLATE_HINT, state_text(p.state));
    else
        snprintf(buf, sizeof(buf), "%s%s", p.hms_count ? LV_SYMBOL_WARNING " " : "", state_text(p.state));
    lv_label_set_text(lbl_state, buf);
    lv_label_set_text(lbl_job, p.job[0] ? p.job : (qn ? "" : STR_QUEUE_EMPTY));

    if (busy) {
        lv_obj_remove_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar_progress, (int)p.progress, LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%d%%", (int)p.progress);
        lv_label_set_text(lbl_pct, buf);
        fmt_duration(dur, sizeof(dur), p.remaining_min);
        if (p.total_layers > 0)
            snprintf(buf, sizeof(buf), SYM_CLOCK " " STR_FMT_REMAINING_LAYER, dur, p.layer, p.total_layers);
        else
            snprintf(buf, sizeof(buf), SYM_CLOCK " " STR_FMT_REMAINING, dur);
        lv_label_set_text(lbl_info, buf);
    } else {
        lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_pct, "");
        if (qn)
            snprintf(buf, sizeof(buf), LV_SYMBOL_LIST " " STR_FMT_QUEUE_COUNT, qn);
        else
            buf[0] = 0;
        lv_label_set_text(lbl_info, buf);
    }
    snprintf(buf, sizeof(buf), SYM_FIRE " " STR_FMT_TEMPS, p.nozzle, p.nozzle_target, p.bed, p.bed_target);
    lv_label_set_text(lbl_temp, buf);

    // Buttons depend on the printer state.
    lv_obj_set_width(btn_a, 180);
    if (!strcmp(p.state, "PAUSE")) {
        set_action(btn_a, lbl_a, action_a, true, LV_SYMBOL_PLAY " " STR_BTN_RESUME, {CMD_RESUME, p.id});
        set_action(btn_b, lbl_b, action_b, true, LV_SYMBOL_STOP " " STR_BTN_STOP, {CMD_STOP, p.id},
                   STR_STOP_QUESTION, true);
    } else if (busy) {
        set_action(btn_a, lbl_a, action_a, true, LV_SYMBOL_PAUSE " " STR_BTN_PAUSE, {CMD_PAUSE, p.id});
        set_action(btn_b, lbl_b, action_b, true, LV_SYMBOL_STOP " " STR_BTN_STOP, {CMD_STOP, p.id},
                   STR_STOP_QUESTION, true);
    } else {
        // Idle / finished / failed: offer the next queued job. "Next" is the first
        // item (by queue position) that waits for a manual start; items without
        // manual_start are started by Bambuddy's scheduler on its own anyway.
        set_action(btn_b, lbl_b, action_b, false);
        lv_obj_set_width(btn_a, 304);
        const QueueItem *next = nullptr;
        for (int i = 0; i < qn && !next; i++)
            if (S.queue[s_sel][i].manual_start) next = &S.queue[s_sel][i];
        if (!next && qn) next = &S.queue[s_sel][0];

        if (next) {
            static char confirm_text[128];
            snprintf(confirm_text, sizeof(confirm_text),
                     p.awaiting_plate_clear ? STR_FMT_PLATE_START_QUESTION : STR_FMT_START_QUESTION, next->name);
            Command cmd = {p.awaiting_plate_clear ? CMD_CLEAR_AND_START : CMD_START_ITEM, p.id, next->id, false};
            set_action(btn_a, lbl_a, action_a, true,
                       p.awaiting_plate_clear ? LV_SYMBOL_PLAY " " STR_BTN_PLATE_NEXT
                                              : LV_SYMBOL_PLAY " " STR_BTN_START_NEXT,
                       cmd, confirm_text);
        } else if (p.awaiting_plate_clear) {
            set_action(btn_a, lbl_a, action_a, true, LV_SYMBOL_OK " " STR_BTN_PLATE_CLEARED, {CMD_CLEAR_PLATE, p.id});
        } else {
            set_action(btn_a, lbl_a, action_a, false);
        }
    }
}

static void update_queue() {
    int qn = S.printer_count ? S.queue_count[s_sel] : 0;

    // Rebuild the list only when its content really changed; rebuilding on every
    // poll would make the list jump back to the top while the user scrolls.
    uint32_t sig = 2166136261u ^ (uint32_t)(s_sel + 1) ^ ((uint32_t)qn << 24);  // FNV-1a style
    for (int i = 0; i < qn; i++) {
        const QueueItem &q = S.queue[s_sel][i];
        sig = (sig ^ (uint32_t)q.id) * 16777619u;
        sig = (sig ^ (q.manual_start | q.filament_short << 1)) * 16777619u;
    }
    if (sig == s_queue_sig) return;
    s_queue_sig = sig;

    static char tab_name[24];
    if (qn)
        snprintf(tab_name, sizeof(tab_name), STR_FMT_TAB_QUEUE_N, qn);
    else
        strlcpy(tab_name, STR_TAB_QUEUE, sizeof(tab_name));
    lv_tabview_rename_tab(tabview, 1, tab_name);

    lv_obj_clean(queue_list);
    if (!qn) {
        lv_list_add_text(queue_list, STR_QUEUE_LIST_EMPTY);
        return;
    }
    for (int i = 0; i < qn; i++) {
        const QueueItem &q = S.queue[s_sel][i];
        char dur[24], line[160];
        fmt_duration(dur, sizeof(dur), q.print_s / 60);
        snprintf(line, sizeof(line), "%s\n%s · %.0f g %s%s", q.name, dur, q.grams, q.filament,
                 q.manual_start ? "" : " · " STR_AUTO);
        lv_obj_t *b = lv_list_add_button(queue_list, q.filament_short ? LV_SYMBOL_WARNING : LV_SYMBOL_FILE, line);
        lv_obj_add_event_cb(b, queue_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

static void handle_result(const CommandResult &r) {
    static char msg[160];
    if (r.http_code >= 200 && r.http_code < 300) {
        const char *ok = STR_DONE;
        switch (r.cmd.type) {
            case CMD_PAUSE: ok = LV_SYMBOL_PAUSE " " STR_PAUSED; break;
            case CMD_RESUME: ok = LV_SYMBOL_PLAY " " STR_RESUMED; break;
            case CMD_STOP: ok = LV_SYMBOL_STOP " " STR_STOPPED; break;
            case CMD_CLEAR_PLATE: ok = LV_SYMBOL_OK " " STR_PLATE_ACKED; break;
            case CMD_START_ITEM:
            case CMD_CLEAR_AND_START: ok = LV_SYMBOL_OK " " STR_STARTED; break;
        }
        show_toast(ok);
        return;
    }
    // 409 from /queue/{id}/start = Bambuddy's filament-deficit check. Ask, then
    // retry with skip_filament_check=true.
    if (r.http_code == 409 && (r.cmd.type == CMD_START_ITEM || r.cmd.type == CMD_CLEAR_AND_START)) {
        Command retry = r.cmd;
        retry.skip_filament_check = true;
        retry.type = CMD_START_ITEM;  // the plate was already acknowledged by the first request
        confirm(LV_SYMBOL_WARNING " " STR_TITLE_LOW_FILAMENT, STR_LOW_FILAMENT_QUESTION, STR_BTN_START_ANYWAY, retry,
                false);
        return;
    }
    int len;
    if (r.http_code == 0)
        len = snprintf(msg, sizeof(msg), LV_SYMBOL_WARNING " %s", STR_ERR_NETWORK);
    else
        len = snprintf(msg, sizeof(msg), LV_SYMBOL_WARNING " " STR_FMT_ERROR, r.http_code);
    if (r.message[0]) snprintf(msg + len, sizeof(msg) - len, ": %s", r.message);
    show_toast(msg, true);
}

void ui_tick() {
    CommandResult r;
    while (net_poll_result(r)) handle_result(r);

    if (!net_snapshot(S, s_version)) return;
    s_version = S.version;

    if (s_sel >= S.printer_count) s_sel = 0;
    update_printer_bar();
    // Header icon: green = OK, orange = Wi-Fi OK but Bambuddy failing, red = no Wi-Fi.
    lv_obj_set_style_text_color(net_icon,
                                !S.wifi ? lv_palette_main(LV_PALETTE_RED)
                                : !S.api_ok ? lv_palette_main(LV_PALETTE_ORANGE)
                                            : lv_palette_main(LV_PALETTE_GREEN),
                                0);
    update_status();
    update_queue();
}

// ---------------------------------------------------------------- construction

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, int x, int y, int w) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *make_button(lv_obj_t *parent, int x, int w, Action *a, lv_obj_t **lbl) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, 112);
    lv_obj_set_size(b, w, 46);  // big enough to hit reliably with a finger
    lv_obj_add_event_cb(b, action_cb, LV_EVENT_CLICKED, a);
    *lbl = lv_label_create(b);
    lv_obj_center(*lbl);
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    return b;
}

void ui_begin(lv_display_t *disp) {
    // Dark theme, Bambu-ish green as the primary colour.
    lv_theme_t *th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_RED),
                                           true, &font_hu_14);
    lv_display_set_theme(disp, th);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: printer selector + network indicator
    printer_bar = lv_buttonmatrix_create(scr);
    lv_obj_set_pos(printer_bar, 0, 0);
    lv_obj_set_size(printer_bar, 284, 36);
    lv_obj_set_style_pad_all(printer_bar, 3, 0);
    lv_obj_set_style_pad_gap(printer_bar, 4, 0);
    lv_obj_set_style_border_width(printer_bar, 0, 0);
    lv_obj_set_style_radius(printer_bar, 0, 0);
    lv_buttonmatrix_set_one_checked(printer_bar, true);
    lv_obj_add_event_cb(printer_bar, printer_bar_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    static const char *loading[] = {STR_LOADING, ""};
    lv_buttonmatrix_set_map(printer_bar, loading);

    net_icon = lv_label_create(scr);
    lv_label_set_text(net_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(net_icon, &font_hu_18, 0);
    lv_obj_set_style_text_color(net_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(net_icon, LV_ALIGN_TOP_RIGHT, -8, 8);

    tabview = lv_tabview_create(scr);
    lv_obj_set_pos(tabview, 0, 36);
    lv_obj_set_size(tabview, UI_W, UI_H - 36);
    lv_tabview_set_tab_bar_size(tabview, 30);
    tab_status = lv_tabview_add_tab(tabview, STR_TAB_STATUS);
    tab_queue = lv_tabview_add_tab(tabview, STR_TAB_QUEUE);
    lv_obj_set_style_pad_all(tab_status, 8, 0);
    lv_obj_remove_flag(tab_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab_queue, 0, 0);

    // Status tab: absolute positions inside a 304x160 content area.
    lbl_state = make_label(tab_status, &font_hu_18, 0, 0, 304);
    lbl_job = make_label(tab_status, &font_hu_14, 0, 24, 304);
    bar_progress = lv_bar_create(tab_status);
    lv_obj_set_pos(bar_progress, 0, 48);
    lv_obj_set_size(bar_progress, 236, 14);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
    lbl_pct = make_label(tab_status, &font_hu_18, 246, 44, 0);
    lbl_info = make_label(tab_status, &font_hu_14, 0, 68, 304);
    lbl_temp = make_label(tab_status, &font_hu_14, 0, 88, 304);
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xA0A0A0), 0);

    btn_a = make_button(tab_status, 0, 180, &action_a, &lbl_a);
    btn_b = make_button(tab_status, 188, 116, &action_b, &lbl_b);

    queue_list = lv_list_create(tab_queue);
    lv_obj_set_size(queue_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(queue_list, 0, 0);
    lv_obj_set_style_radius(queue_list, 0, 0);
    lv_list_add_text(queue_list, STR_LOADING);

    // Toast: short feedback line on the top layer, auto-hides.
    toast = lv_label_create(lv_layer_top());
    lv_obj_set_width(toast, 300);
    lv_label_set_long_mode(toast, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(toast, 10, 0);
    lv_obj_set_style_radius(toast, 8, 0);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    toast_timer = lv_timer_create(toast_hide_cb, 3500, nullptr);
    lv_timer_pause(toast_timer);
}
