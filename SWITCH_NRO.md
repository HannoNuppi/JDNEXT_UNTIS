# Untis Next Native Switch Port

The NRO is a native Nintendo Switch homebrew application written in C with
libnx. It does **not** open a browser, WebKit, or a website applet.

## Native implementation

- libnx handles the application lifecycle, controller input and Switch software keyboard.
- switch-curl sends HTTPS requests using libnx's native TLS backend. Certificate
  verification remains enabled.
- The timetable is rendered locally from the same A/B schedule configuration
  used by the website.
- Lesson details read public homework and classwork documents from the existing
  Firestore projects through the Firestore REST API.
- TobiServices sign-in uses Firebase Authentication REST. The password is
  sent only for the sign-in request, cleared from memory afterwards, and is
  never written to the SD card by this version.
- The signed-in user's profile is read from Firestore. Admin status is derived
  from the server profile's active `admin` tag.
- Admin actions call the existing protected Firebase callable functions:
  `requestSwitchNroBuild`, `setMaintenanceMode`, and `listTobiUsers`.
  Those functions perform their own server-side permission checks.
- No GitHub token, service-account key, or admin secret is embedded in the NRO.

## Controls

### Timetable
- D-pad Left/Right: choose weekday
- D-pad Up/Down: choose lesson period
- L/R: previous/next week
- A: open homework and classwork for the selected lesson
- X: account screen
- Y: refresh maintenance status
- Plus: exit

### Lesson details
- B: back to timetable
- Y: refresh entries

### Account
- A: sign in / open native admin area / sign out
- D-pad Up/Down: choose action
- B: back

### Native admin area
- Start a new Switch NRO build
- Toggle the Untis Next maintenance mode and set its visitor message
- View the first 24 accounts from the admin account list
- Every operation is checked again by the Cloud Function

## Current feature boundary

This is the first native client milestone, not yet a complete 1:1 port of every
website feature. The timetable and existing homework/classwork entries are
read-only at this stage. Creating homework or classwork, reports, image uploads,
full tag/permission management, coin editing, event-theme grants, and live/test
publishing still need native screens and workflows. No WebKit fallback is used.

The timetable cycle and subject list are currently kept in the native source.
When the school schedule changes, update `switch-app/source/main.c` and build
a new release.

## Build

GitHub Actions installs the Switch curl portlib and compiles with the official
devkitPro Switch development container. A successful main-branch build uploads
the NRO as an artifact and updates the prerelease:

https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/tag/switch-nro

## Firebase requirements

The NRO uses the public Firebase Web API key already used by the website. A
Firebase API key is not an admin credential; Firestore Security Rules and the
callable Cloud Functions remain the authorization boundary.

The native application requires an internet connection. Email/password
authentication uses the existing TobiServices Firebase Authentication project.
For admin functions, the callable functions in `Tobiservices-Account` must be
deployed and the server-side GitHub secret for NRO builds must be configured.
