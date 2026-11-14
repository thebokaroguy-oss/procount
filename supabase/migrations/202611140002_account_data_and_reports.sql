create table if not exists public.procount_profiles (
  user_id uuid primary key references auth.users(id) on delete cascade,
  profile_data jsonb not null default '{}'::jsonb,
  updated_at timestamptz not null default now()
);
alter table public.procount_profiles enable row level security;
revoke all on public.procount_profiles from anon, public;
grant select, insert, update, delete on public.procount_profiles to authenticated;
drop policy if exists "users manage only their own ProCount profile" on public.procount_profiles;
create policy "users manage only their own ProCount profile"
  on public.procount_profiles for all to authenticated
  using (user_id = (select auth.uid()))
  with check (user_id = (select auth.uid()));

create table if not exists public.procount_reports (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  file_path text not null unique,
  file_name text not null,
  mime_type text not null,
  extracted_text text not null default '',
  extracted_values jsonb not null default '[]'::jsonb,
  created_at timestamptz not null default now(),
  check (split_part(file_path, '/', 1) = user_id::text)
);
create index if not exists procount_reports_user_created_idx
  on public.procount_reports (user_id, created_at desc);
alter table public.procount_reports enable row level security;
revoke all on public.procount_reports from anon, public;
grant select, insert, delete on public.procount_reports to authenticated;
drop policy if exists "users read their own ProCount reports" on public.procount_reports;
create policy "users read their own ProCount reports"
  on public.procount_reports for select to authenticated
  using (user_id = (select auth.uid()));
drop policy if exists "users add their own ProCount reports" on public.procount_reports;
create policy "users add their own ProCount reports"
  on public.procount_reports for insert to authenticated
  with check (user_id = (select auth.uid()));
drop policy if exists "users delete their own ProCount reports" on public.procount_reports;
create policy "users delete their own ProCount reports"
  on public.procount_reports for delete to authenticated
  using (user_id = (select auth.uid()));

create table if not exists public.procount_privacy_consents (
  user_id uuid primary key references auth.users(id) on delete cascade,
  policy_version text not null,
  accepted_at timestamptz not null default now(),
  withdrawn_at timestamptz
);
alter table public.procount_privacy_consents add column if not exists withdrawn_at timestamptz;
alter table public.procount_privacy_consents enable row level security;
revoke all on public.procount_privacy_consents from anon, public;
grant select, insert, update on public.procount_privacy_consents to authenticated;
drop policy if exists "users manage only their own ProCount privacy consent" on public.procount_privacy_consents;
create policy "users manage only their own ProCount privacy consent"
  on public.procount_privacy_consents for all to authenticated
  using (user_id = (select auth.uid()))
  with check (user_id = (select auth.uid()));

insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values ('procount-reports', 'procount-reports', false, 20971520,
  array['application/pdf', 'application/vnd.openxmlformats-officedocument.wordprocessingml.document'])
on conflict (id) do update set public = false, file_size_limit = 20971520,
  allowed_mime_types = excluded.allowed_mime_types;

drop policy if exists "ProCount users read their own report files" on storage.objects;
create policy "ProCount users read their own report files"
  on storage.objects for select to authenticated
  using (bucket_id = 'procount-reports' and (storage.foldername(name))[1] = (select auth.uid())::text);
drop policy if exists "ProCount users upload their own report files" on storage.objects;
create policy "ProCount users upload their own report files"
  on storage.objects for insert to authenticated
  with check (bucket_id = 'procount-reports' and (storage.foldername(name))[1] = (select auth.uid())::text);
drop policy if exists "ProCount users delete their own report files" on storage.objects;
create policy "ProCount users delete their own report files"
  on storage.objects for delete to authenticated
  using (bucket_id = 'procount-reports' and (storage.foldername(name))[1] = (select auth.uid())::text);
