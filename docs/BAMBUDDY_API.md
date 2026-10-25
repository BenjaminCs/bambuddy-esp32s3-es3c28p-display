# Bambuddy API as used by the display

Checked against Bambuddy **1.2.5.7** (`/openapi.json` on your instance lists everything).
All requests go to `https://<BAMBUDDY_HOST>` with the header `X-API-Key: bb_...`
(`Authorization: Bearer bb_...` works too).

## API key permissions

API keys are scoped by permission flags. A request whose permission is not on the key gets
`403`. The display needs:

| Key permission | Grants (relevant part) |
|---|---|
| `can_read_status` | every `*_READ` route: printers, status, queue |
| `can_queue` | queue write operations, including `POST /queue/{id}/start` |
| `can_control_printer` | pause / resume / stop, `clear-plate` |

`printer_ids` on the key (optional) limits it to specific printers. Admin-only routes reject
API keys entirely.

## Reads (polled)

| Request | Interval | Fields used |
|---|---|---|
| `GET /api/v1/printers/` | 60 s | `id`, `name`, `model`, `is_active` |
| `GET /api/v1/printers/{id}/status` | 3 s | `connected`, `state`, `subtask_name`, `gcode_file`, `progress` (0–100), `remaining_time` (**minutes**), `layer_num`, `total_layers`, `temperatures.{nozzle,nozzle_target,bed,bed_target}`, `awaiting_plate_clear`, `hms_errors[]` |
| `GET /api/v1/queue/?status=pending&printer_id={id}` | 6 s | `id`, `position`, `manual_start`, `filament_short`, `waiting_reason`, `archive_name` / `library_file_name`, `print_time_seconds`, `filament_used_grams`, `filament_type` |

`state` is Bambu's `gcode_state`: `IDLE`, `PREPARE`, `SLICING`, `RUNNING`, `PAUSE`, `FINISH`,
`FAILED`. Auto-discovered printers are named `Printer at <IP>`; the display shows the model
instead. Better: rename the printer in Bambuddy.

## Commands

| Action | Request | Notes |
|---|---|---|
| Pause | `POST /api/v1/printers/{id}/print/pause` | needs the printer online |
| Resume | `POST /api/v1/printers/{id}/print/resume` | |
| Stop | `POST /api/v1/printers/{id}/print/stop` | |
| Plate cleared | `POST /api/v1/printers/{id}/clear-plate` | Only sets a flag in Bambuddy; nothing is sent to the printer. `400` if the printer was not waiting for it |
| Start queue item | `POST /api/v1/queue/{item_id}/start?skip_filament_check=false` | see below |

### How starting a job works

- Queue items with **`manual_start = true`** wait for someone to press start. `/start` clears
  that flag. **Bambuddy's scheduler** then sends the job to the printer as soon as the printer
  is idle **and** the plate has been acknowledged. The display never talks to the printer
  directly.
- After a finished or failed print, Bambuddy sets **`awaiting_plate_clear`**. Until
  `clear-plate` is called, the next job does not start. The display's
  "Plate clear, start next" button calls `clear-plate` and then `/start`.
- Pressing start while the printer is busy is fine: the job runs after the current one.
- `/start` returns **`409`** when the spool assigned to the job looks too short (Bambuddy's
  filament-deficit check). The display asks, then retries with `skip_filament_check=true`.
- Only `pending` items can be started (`400` otherwise).

### Webhook alternative

`/api/v1/webhook/...` offers simpler endpoints (`POST /webhook/printer/{id}/start` releases
the next manual-start item, `GET /webhook/queue`, `GET /webhook/printer/{id}/status`). The
display uses the regular API because it needs per-item start, the filament check, the plate
flag and job names.

## Testing safely

Only use **GET** requests when testing against a real printer: pause, stop and start act on
the hardware. Example:

```bash
curl -s -H "X-API-Key: $KEY" "https://$HOST/api/v1/printers/1/status" | python3 -m json.tool
```

If the host name only resolves internally, test from inside the LAN or with
`curl --resolve $HOST:443:<LAN IP> ...`.
