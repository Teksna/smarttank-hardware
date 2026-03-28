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

