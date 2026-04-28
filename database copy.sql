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

⚙️ Trigger

create table motor_logs (
  id bigint generated always as identity primary key,
  device_id uuid references devices(id) on delete cascade,
  state text,
  recorded_at timestamp default now()
);

------------------------------------ Trigger for motor state changes ------------------------------------
create or replace function log_device_changes()
returns trigger as $$
begin

  -- ⚙️ MOTOR
  if new.motor_state is distinct from old.motor_state then
    insert into motor_logs (device_id, state, recorded_at)
    values (new.id, new.motor_state, now());
  end if;

  -- 🛢️ TANK
  if new.tank_level_percent is distinct from old.tank_level_percent
     and abs(coalesce(new.tank_level_percent,0) - coalesce(old.tank_level_percent,0)) > 2 then
    insert into tank_logs (device_id, tank_level_percent, recorded_at)
    values (new.id, new.tank_level_percent, now());
  end if;

  -- 🔋 BATTERY
  if new.battery_level is distinct from old.battery_level
     and abs(coalesce(new.battery_level,0) - coalesce(old.battery_level,0)) > 1 then
    insert into battery_logs (device_id, battery_level, recorded_at)
    values (new.id, new.battery_level, now());
  end if;

  return new;
end;
$$ language plpgsql;

create trigger trigger_device_logs
after update of motor_state, tank_level_percent, battery_level
on devices
for each row
execute function log_device_changes();

------------------------------------------------------------------
create or replace function update_timestamp()
returns trigger as $$
begin
  new.updated_at = now();
  return new;
end;
$$ language plpgsql;

create trigger set_updated_at
before update on devices
for each row
execute function update_timestamp();

-----------------------------


insert into supply_stations (
  station_name,
  region,
  town,
  city,
  state,
  supply_state
)
values (
  'Test Station',
  'Zone A',
  'Test Town',
  'Agra',
  'UP',
  true
)
returning id;

insert into devices (
  device_uid,
  supply_station_id,
  motor_state,
  tank_level_percent
)
values (
  'tank1',
  'your-station-id',  -- from supply_stations
  false,
  0
);

--
create or replace function log_battery_change()
returns trigger as $$
begin
  if new.battery_level is distinct from old.battery_level then
    insert into battery_logs (device_id, battery_level)
    values (new.id, new.battery_level);
  end if;
  return new;
end;
$$ language plpgsql;
---

create trigger trigger_battery_log
after update on devices
for each row
execute function log_battery_change();

==========NEW UPDATES FOR OTA FUNCTIONALITY============

create or replace function OTA_update_device_and_get_supply(
  p_device_uid text,
  p_tank_level int,
  p_motor_state boolean,
  p_battery_level numeric
)
returns table (
  supply_state boolean,
  ota_status boolean
)
language plpgsql
as $$
declare
  v_ota boolean;
begin

  -- 🔵 Update device + get values
  update devices d
  set 
    tank_level_percent = p_tank_level,
    motor_state = case when p_motor_state then 'ON' else 'OFF' end,
    battery_level = p_battery_level,
    updated_at = now()
  where d.device_uid = p_device_uid
  returning d.ota_status
  into v_ota;

  -- 🔵 Return supply + ota
  return query
  select 
    coalesce((
      select s.supply_state
      from supply_stations s
      join devices d2 on d2.supply_station_id = s.id
      where d2.device_uid = p_device_uid
    ), false),
    v_ota;

  -- 🔥 ONLY reset if true
  if v_ota = true then
    update devices
    set ota_status = false
    where device_uid = p_device_uid;
  end if;

end;
$$;

==================
=====ADDING USER TABLE AND RELATION TO DEVICES====

create function public.handle_new_user()
returns trigger as $$
begin
  insert into public.users (id)
  values (new.id);
  return new;
end;
$$ language plpgsql security definer;

=============TRIGGER TO CREATE USER RECORD ON NEW AUTH USER CREATION
create trigger on_auth_user_created
after insert on auth.users
for each row execute procedure public.handle_new_user();

====RLS FOR USER TABLE
alter table users enable row level security;

create policy "Allow insert for auth trigger"
on users
for insert
with check (true);



===== Adding motor automation column to devices
alter table devices
add column motor_automation boolean default true

alter table devices
add column rssi integer;