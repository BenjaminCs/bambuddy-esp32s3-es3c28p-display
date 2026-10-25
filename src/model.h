#pragma once

// Data shared between the network task and the UI. Plain structs with fixed-size
// strings, so the whole state can be copied under a mutex without heap allocation.

#include <Arduino.h>

#define MAX_PRINTERS 4   // header buttons get too narrow beyond this on 320 px
#define MAX_QUEUE    12  // pending jobs kept per printer

struct PrinterInfo {
    int id;  // Bambuddy printer id
    char name[40];
    char model[24];
    bool status_ok;  // last /status request succeeded
    bool connected;  // Bambuddy is connected to the printer (MQTT)
    char state[16];  // Bambu gcode_state: IDLE, PREPARE, RUNNING, PAUSE, FINISH, FAILED…
    char job[64];
    float progress;  // 0-100
    int remaining_min;
    int layer;
    int total_layers;
    float nozzle, nozzle_target;
    float bed, bed_target;
    bool awaiting_plate_clear;
    int hms_count;  // active HMS (printer health) errors
};

struct QueueItem {
    int id;  // Bambuddy queue item id
    int position;
    char name[64];
    int print_s;  // estimated print time, seconds
    float grams;
    char filament[12];
    bool manual_start;    // waits for someone to press Start
    bool filament_short;  // Bambuddy thinks the assigned spool is too short
    char waiting[64];     // Bambuddy's reason why it has not started yet
};

struct AppState {
    bool wifi;
    bool api_ok;
    char error[96];
    int printer_count;
    PrinterInfo printers[MAX_PRINTERS];
    int queue_count[MAX_PRINTERS];
    QueueItem queue[MAX_PRINTERS][MAX_QUEUE];
    uint32_t version;  // incremented on every change; the UI redraws when it moves
};

enum CmdType {
    CMD_PAUSE,
    CMD_RESUME,
    CMD_STOP,
    CMD_CLEAR_PLATE,
    CMD_START_ITEM,
    CMD_CLEAR_AND_START,  // acknowledge an empty plate, then start a queue item
};

struct Command {
    CmdType type;
    int printer_id;
    int item_id;               // queue item id (start commands only)
    bool skip_filament_check;  // set when the user confirmed "start anyway" after a 409
};

struct CommandResult {
    Command cmd;
    int http_code;  // 0 = network/TLS failure
    char message[128];  // Bambuddy's error detail, if any
};
