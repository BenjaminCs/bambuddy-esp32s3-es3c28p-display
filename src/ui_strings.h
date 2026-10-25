// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

#pragma once

// All user-visible text lives here.
//
// English is the default. To get Hungarian, add this line to include/secrets.h:
//     #define UI_LANGUAGE_HU
//
// To add another language, copy one block and translate it. The bundled fonts
// (src/fonts) cover ASCII, Latin-1 (U+00A0-U+00FF) and Hungarian Őő/Űű. Any other
// characters (e.g. Polish, Czech, Cyrillic) need the fonts regenerated with
// tools/gen_fonts.sh and a wider glyph range, otherwise they render as boxes.

#include "secrets.h"

#if defined(UI_LANGUAGE_HU)

#define STR_LOADING            "Betöltés…"
#define STR_NO_PRINTER         "Nincs nyomtató"
#define STR_PRINTER            "Nyomtató"
#define STR_TAB_STATUS         "Állapot"
#define STR_TAB_QUEUE          "Sor"
#define STR_FMT_TAB_QUEUE_N    "Sor (%d)"
#define STR_QUEUE_LIST_EMPTY   "Nincs várakozó nyomtatás."
#define STR_UNNAMED            "névtelen"

#define STR_WIFI_CONNECTING    "Csatlakozás a Wi-Fi-hez…"
#define STR_CONNECTING_API     "Bambuddy…"
#define STR_NO_ACTIVE_PRINTER  "Nincs aktív nyomtató"
#define STR_OFFLINE            "Nincs kapcsolat"
#define STR_PRINTER_UNREACHABLE "A Bambuddy nem éri el a nyomtatót."
#define STR_STATUS_UNAVAILABLE "Az állapot nem kérhető le."
#define STR_CLEAR_PLATE_HINT   "ürítsd a tálcát"
#define STR_QUEUE_EMPTY        "A sor üres"

#define STR_ERR_API_KEY        "Hibás vagy kevés jogú API-kulcs"
#define STR_ERR_UNREACHABLE    "A Bambuddy nem érhető el"
#define STR_ERR_NETWORK        "Nem sikerült elérni a Bambuddyt"

#define STR_STATE_IDLE         "Készenlét"
#define STR_STATE_PREPARE      "Előkészítés"
#define STR_STATE_SLICING      "Szeletelés"
#define STR_STATE_RUNNING      "Nyomtat"
#define STR_STATE_PAUSE        "Szünetel"
#define STR_STATE_FINISH       "Kész"
#define STR_STATE_FAILED       "Sikertelen"

#define STR_DUR_UNDER_1        "<1 p"
#define STR_FMT_DUR_MIN        "%d p"
#define STR_FMT_DUR_H_MIN      "%d ó %d p"
#define STR_FMT_REMAINING      "hátra %s"
#define STR_FMT_REMAINING_LAYER "hátra %s   réteg %d/%d"
#define STR_FMT_QUEUE_COUNT    "%d tétel a sorban"
#define STR_NOZZLE             "Fej"
#define STR_BED                "Asztal"
#define STR_AUTO               "automatikus"

#define STR_BTN_PAUSE          "Szünet"
#define STR_BTN_RESUME         "Folytatás"
#define STR_BTN_STOP           "Leállítás"
#define STR_BTN_START_NEXT     "Következő indítása"
#define STR_BTN_PLATE_NEXT     "Tálca üres, jöhet a következő"
#define STR_BTN_PLATE_CLEARED  "Tálca kiürítve"
#define STR_BTN_START          "Indítás"
#define STR_BTN_CANCEL         "Mégse"
#define STR_BTN_GO             "Mehet"
#define STR_BTN_START_ANYWAY   "Mégis indítom"

#define STR_TITLE_CONFIRM      "Megerősítés"
#define STR_TITLE_STOP         "Leállítás"
#define STR_TITLE_LOW_FILAMENT "Kevés filament"
#define STR_STOP_QUESTION      "Biztosan leállítod a nyomtatást?"
#define STR_FMT_START_QUESTION "%s indítása?"
#define STR_FMT_PLATE_START_QUESTION "A tálca üres, indulhat:\n%s?"
#define STR_LOW_FILAMENT_QUESTION "A hozzárendelt tekercsen lehet, hogy nincs elég filament. Indítod mégis?"
#define STR_FILAMENT_MAY_RUN_OUT "Kevés lehet a filament."
#define STR_STARTS_AFTER_CURRENT "A mostani nyomtatás után indul."
#define STR_MARKS_PLATE_CLEAR  "A tálcát üresnek jelöli, és indul."

#define STR_SENDING            "Küldés…"
#define STR_DONE               "Kész"
#define STR_PAUSED             "Szüneteltetve"
#define STR_RESUMED            "Folytatva"
#define STR_STOPPED            "Leállítva"
#define STR_PLATE_ACKED        "Tálca nyugtázva"
#define STR_STARTED            "Elindítva, a Bambuddy küldi a nyomtatóra"
#define STR_FMT_ERROR          "Hiba (%d)"

#else  // English (default)

#define STR_LOADING            "Loading…"
#define STR_NO_PRINTER         "No printer"
#define STR_PRINTER            "Printer"
#define STR_TAB_STATUS         "Status"
#define STR_TAB_QUEUE          "Queue"
#define STR_FMT_TAB_QUEUE_N    "Queue (%d)"
#define STR_QUEUE_LIST_EMPTY   "No jobs waiting."
#define STR_UNNAMED            "unnamed"

#define STR_WIFI_CONNECTING    "Connecting to Wi-Fi…"
#define STR_CONNECTING_API     "Bambuddy…"
#define STR_NO_ACTIVE_PRINTER  "No active printer"
#define STR_OFFLINE            "Offline"
#define STR_PRINTER_UNREACHABLE "Bambuddy cannot reach the printer."
#define STR_STATUS_UNAVAILABLE "Status unavailable."
#define STR_CLEAR_PLATE_HINT   "clear the plate"
#define STR_QUEUE_EMPTY        "Queue is empty"

#define STR_ERR_API_KEY        "Invalid API key or missing permission"
#define STR_ERR_UNREACHABLE    "Bambuddy unreachable"
#define STR_ERR_NETWORK        "Could not reach Bambuddy"

#define STR_STATE_IDLE         "Idle"
#define STR_STATE_PREPARE      "Preparing"
#define STR_STATE_SLICING      "Slicing"
#define STR_STATE_RUNNING      "Printing"
#define STR_STATE_PAUSE        "Paused"
#define STR_STATE_FINISH       "Finished"
#define STR_STATE_FAILED       "Failed"

#define STR_DUR_UNDER_1        "<1 min"
#define STR_FMT_DUR_MIN        "%d min"
#define STR_FMT_DUR_H_MIN      "%d h %d min"
#define STR_FMT_REMAINING      "%s left"
#define STR_FMT_REMAINING_LAYER "%s left   layer %d/%d"
#define STR_FMT_QUEUE_COUNT    "%d job(s) queued"
#define STR_NOZZLE             "Nozzle"
#define STR_BED                "Bed"
#define STR_AUTO               "auto"

#define STR_BTN_PAUSE          "Pause"
#define STR_BTN_RESUME         "Resume"
#define STR_BTN_STOP           "Stop"
#define STR_BTN_START_NEXT     "Start next"
#define STR_BTN_PLATE_NEXT     "Plate clear, start next"
#define STR_BTN_PLATE_CLEARED  "Plate cleared"
#define STR_BTN_START          "Start"
#define STR_BTN_CANCEL         "Cancel"
#define STR_BTN_GO             "Go"
#define STR_BTN_START_ANYWAY   "Start anyway"

#define STR_TITLE_CONFIRM      "Confirm"
#define STR_TITLE_STOP         "Stop"
#define STR_TITLE_LOW_FILAMENT "Low filament"
#define STR_STOP_QUESTION      "Really stop the print?"
#define STR_FMT_START_QUESTION "Start %s?"
#define STR_FMT_PLATE_START_QUESTION "Mark the plate as clear and start\n%s?"
#define STR_LOW_FILAMENT_QUESTION "The assigned spool may not have enough filament. Start anyway?"
#define STR_FILAMENT_MAY_RUN_OUT "Filament may run short."
#define STR_STARTS_AFTER_CURRENT "Starts after the current print."
#define STR_MARKS_PLATE_CLEAR  "Marks the plate as clear, then starts."

#define STR_SENDING            "Sending…"
#define STR_DONE               "Done"
#define STR_PAUSED             "Paused"
#define STR_RESUMED            "Resumed"
#define STR_STOPPED            "Stopped"
#define STR_PLATE_ACKED        "Plate acknowledged"
#define STR_STARTED            "Started, Bambuddy is sending it to the printer"
#define STR_FMT_ERROR          "Error (%d)"

#endif
