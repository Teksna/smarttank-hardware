👤 1. USERS
create table users (
  id uuid primary key references auth.users(id) on delete cascade,
  name text,
  created_at timestamp default now()
);
🚰 2. SUPPLY STATIONS (MASTER TABLE)
create table supply_stations (
  id uuid primary key default gen_random_uuid(),

  station_name text,        -- A (Police Line)
  region text,
  town text,
  city text,
  state text,

  created_at timestamp default now()
);

🚰 2. SUPPLY STATIONS (MASTER TABLE)
create table supply_stations (
  id uuid primary key default gen_random_uuid(),

  station_name text,        -- A (Police Line)
  region text,
  town text,
  city text,
  state text,

  created_at timestamp default now()
);

📡 3. DEVICES (🔥 MAIN TABLE - LIVE STATE)
create table devices (
  id uuid primary key default gen_random_uuid(),
  device_uid text unique not null,

  owner_id uuid references users(id) on delete cascade,
  supply_station_id uuid references supply_stations(id),

  -- 🔋 Battery
  battery_level numeric,

  -- ⚙️ Motor
  motor_state text default 'OFF',

  -- 🛢️ Tank
  tank_level_percent numeric,

  -- 🚰 Supply at home
  supply_state text, -- ON / OFF

  -- ⏱️ Daily usage (optional cache)
  today_consumption_minutes numeric default 0,

  -- 🔄 OTA
  firmware_version text,
  target_firmware text,
  firmware_url text,
  ota_status text default 'idle',

  created_at timestamp default now(),
  updated_at timestamp default now()
);


📊 4. LOG TABLES (ALL DEVICE-BASED)
🔋 Battery Logs
create table battery_logs (
  id bigint generated always as identity primary key,
  device_id uuid references devices(id) on delete cascade,
  battery_level numeric,
  recorded_at timestamp default now()
);

⚙️ Motor Logs
create table motor_logs (
  id bigint generated always as identity primary key,
  device_id uuid references devices(id) on delete cascade,
  state text,
  recorded_at timestamp default now()
);