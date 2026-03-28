# smarttank-hardware


## Schema commands

create table firmware (
  id bigint generated always as identity primary key,
  version text,
  url text,
  description text,
  created_at timestamp default now()
);

insert into firmware (version, url, description)
values (
  'v1.0.1',
  'https://yljggigahlagdihhycfj.supabase.co/storage/v1/object/public/firmwares/v1.0.1.bin',
  'OTA test update'
);

                        ┌──────────────────────────────┐
                        │        Flutter App           │
                        │  (Supabase Auth - JWT)       │
                        └──────────────┬───────────────┘
                                       │
                                       │ REST / Realtime
                                       ▼
                        ┌──────────────────────────────┐
                        │          Supabase            │
                        │  (Postgres + Auth + RLS)     │
                        └──────────────┬───────────────┘
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        │                              │                              │
        ▼                              ▼                              ▼

┌──────────────────────┐   ┌────────────────────────┐   ┌────────────────────────┐
│      USERS           │   │       DEVICES          │   │     DEVICE_ACCESS      │
│ (auth.users link)    │   │ (ESP32 registered)     │   │ (multi-user mapping)   │
└─────────┬────────────┘   └──────────┬─────────────┘   └──────────┬─────────────┘
          │                           │                            │
          └──────────────┬────────────┴──────────────┬─────────────┘
                         ▼                           ▼

                ┌──────────────────────┐   ┌──────────────────────┐
                │        TANKS         │   │   DEVICE_FIRMWARE     │
                │ (per device config)  │   │ (OTA assignment)      │
                └─────────┬────────────┘   └──────────┬───────────┘
                          │                           │
                          ▼                           ▼

        ┌──────────────────────────────┐   ┌──────────────────────────────┐
        │       TANK_READINGS          │   │      FIRMWARE_UPDATES        │
        │ (time-series water level)    │   │ (version + bin URL)          │
        └──────────────┬───────────────┘   └──────────────────────────────┘
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼

┌────────────────┐ ┌────────────────┐ ┌────────────────┐
│ CONSUMPTION    │ │ FILLING_LOGS   │ │ DEVICE_COMMAND │
│ (usage data)   │ │ (tank refill)  │ │ (motor control)│
└────────────────┘ └────────────────┘ └────────────────┘


────────────────────────────────────────────────────────────────────────


                    🌍 WATER SUPPLY HIERARCHY

┌──────────────┐
│   STATES     │
└──────┬───────┘
       ▼
┌──────────────┐
│   CITIES     │
└──────┬───────┘
       ▼
┌──────────────┐
│    TOWNS     │
└──────┬───────┘
       ▼
┌──────────────────────┐
│  SUPPLY_STATIONS     │
└──────────┬───────────┘
           ▼
┌──────────────────────────────┐
│       SUPPLY_LOGS            │
│ (ON/OFF timing for ML)       │
└──────────────────────────────┘


────────────────────────────────────────────────────────────────────────


                    🔌 ESP32 DEVICE FLOW

┌──────────────────────────────┐
│          ESP32 Device        │
└──────────────┬───────────────┘
               │
               │ HTTPS (API KEY)
               ▼

        ┌──────────────────────────────┐
        │   Supabase Edge Functions    │
        │ (Auth + Validation Layer)    │
        └──────────────┬───────────────┘
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼

  Send Data       Fetch Commands     Check OTA
(TANK_READINGS)   (DEVICE_COMMAND)   (DEVICE_FIRMWARE)


────────────────────────────────────────────────────────────────────────


                    📱 FLUTTER APP FLOW

┌──────────────────────────────┐
│        Flutter App           │
└──────────────┬───────────────┘
               │
               ▼

     Supabase Client (JWT Auth)

               │
   ┌───────────┼────────────┬──────────────┐
   ▼           ▼            ▼              ▼

Fetch Tanks  Graph Data   Control Motor   OTA Status
(TANKS)      (READINGS)   (COMMANDS)      (FIRMWARE)
