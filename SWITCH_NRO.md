# Untis Next Switch NRO

This folder contains the source for the installable Nintendo Switch homebrew
application `UntisNext.nro`.

## What it does

The NRO is a small controller-friendly launcher with two entries:

- Untis Next: `https://hannonuppi.github.io/JDNEXT_UNTIS/`
- TobiServices Admin Center: `https://hannonuppi.github.io/Tobiservices-Account/admin.html`

It opens the selected URL through libnx's Web applet. The websites remain the
source of truth for accounts, permissions, timetable data, and admin actions.
No Firebase credentials, GitHub tokens, or admin permissions are embedded in
the NRO. Admin Center access still requires a valid TobiServices admin account.

An internet connection is required. The Switch Web applet has its own browser
session, so sign in on the Switch separately when prompted.

## Controls

- D-pad Up/Down: select a website
- A: open the selected website
- Plus: exit to Homebrew Menu

## Build

The GitHub Actions workflow `.github/workflows/switch-nro.yml` compiles this
project in the official devkitPro Switch development container.

A successful main-branch build publishes the latest NRO at:

https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/tag/switch-nro

The same workflow can be started from the Untis Next website or the
TobiServices Admin Center by an active admin. The request is authenticated by
the TobiServices Cloud Function and dispatched to GitHub Actions; the GitHub
token is stored only as a Firebase Functions secret.

## Limitations

This first version is a web-portal launcher, not a native/offline recreation of
the timetable. It depends on the Switch system Web applet and network access.
The CI build verifies compilation, but the NRO still needs a real-console test
for applet availability and website compatibility.
