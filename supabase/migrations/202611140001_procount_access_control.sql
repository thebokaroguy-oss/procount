create table if not exists public.procount_allowed_users (
  email text primary key check (email = lower(email)),
  role text not null default 'user' check (role in ('admin', 'user')),
  active boolean not null default true,
  created_at timestamptz not null default now()
);

alter table public.procount_allowed_users enable row level security;
revoke all on public.procount_allowed_users from anon, public;
grant select, insert, update on public.procount_allowed_users to authenticated;

create schema if not exists private;
revoke all on schema private from public, anon, authenticated;
grant usage on schema private to authenticated;
create table if not exists private.procount_admin_emails (
  email text primary key check (email = lower(email))
);
revoke all on private.procount_admin_emails from public, anon, authenticated;
create or replace function private.is_procount_admin(candidate_email text)
returns boolean
language sql
stable
security definer
set search_path = ''
as $$
  select exists (
    select 1 from private.procount_admin_emails
    where email = lower(coalesce(candidate_email, ''))
  );
$$;
revoke all on function private.is_procount_admin(text) from public, anon;
grant execute on function private.is_procount_admin(text) to authenticated;

drop policy if exists "procount users can read their active approval" on public.procount_allowed_users;
create policy "procount users can read their active approval"
  on public.procount_allowed_users for select to authenticated
  using (email = lower(coalesce(auth.jwt() ->> 'email', '')) and active = true);

drop policy if exists "procount admins can read all approvals" on public.procount_allowed_users;
create policy "procount admins can read all approvals"
  on public.procount_allowed_users for select to authenticated
  using (private.is_procount_admin(auth.jwt() ->> 'email'));

drop policy if exists "procount admins can add approvals" on public.procount_allowed_users;
create policy "procount admins can add approvals"
  on public.procount_allowed_users for insert to authenticated
  with check (
    private.is_procount_admin(auth.jwt() ->> 'email')
    and role = 'user'
  );

drop policy if exists "procount admins can update approvals" on public.procount_allowed_users;
create policy "procount admins can update approvals"
  on public.procount_allowed_users for update to authenticated
  using (private.is_procount_admin(auth.jwt() ->> 'email'))
  with check (
    private.is_procount_admin(auth.jwt() ->> 'email')
    and role = 'user'
  );
