# Untis Next Native Switch Port

The NRO is a native Nintendo Switch homebrew application written in C with
libnx. It does **not** open a browser, WebKit, or a website applet.

## Native implementation

- libnx handles the application lifecycle, controller input and Switch software keyboard.
- switch-curl sends HTTPS requests using libnx's native TLS backend. Certificate
  verification remains enabled.
- The timetable is drawn directly into the libnx native 1280x720 framebuffer; it does not use WebKit or SDL.
- The timetable and lesson screens use the same A/B cycle and Firestore date keys as Untis Next.
- Lesson details read shared homework, legacy homework and classwork from the existing Firestore projects through the Firestore REST API.
- New homework entries are written atomically to TobiServices Firestore, including the private author map and the existing +10 Goldmünzen reward when the signed-in account qualifies.
- New classwork entries are written to the legacy Untis Firestore project with an anonymous Firebase identity, matching the website's public posting rules.
- Homework reports use the protected reportJdnextHomeworkEntryNative callable. The second report removes the entry and private map and applies the existing -20 Goldmünzen penalty when the author earned a posting reward.
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

The native port now includes the timetable, lesson detail screens, native homework and classwork creation, homework reporting, TobiServices sign-in, the account screen, and the first admin tools. It is not yet a complete 1:1 port of every website feature.

Still outstanding are homework image uploads and image viewing, a full profile/theme shop and profile customization, leaderboard, Advent/event screens, complete admin account/tag/permission/coin management, and the Test Mode/live publisher. The native schedule is currently compiled into the client and must be updated in source when the timetable changes. No WebKit fallback is used.

## Build

GitHub Actions installs the Switch curl portlib and compiles with the official
devkitPro Switch development container. A successful main-branch build uploads
the NRO as an artifact and updates the prerelease:

https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/tag/switch-nro

## Firebase requirements

The NRO uses the public Firebase Web API key already used by the website. A
Firebase API key is not an admin credential; Firestore Security Rules and the
callable Cloud Functions remain the authorization boundary.

The native report action also requires the TobiServices callable function in the account repository to be deployed:

```sh
firebase deploy --only functions:reportJdnextHomeworkEntryNative
```

The native application requires an internet connection. Email/password
authentication uses the existing TobiServices Firebase Authentication project.
For admin functions, the callable functions in `Tobiservices-Account` must be
deployed and the server-side GitHub secret for NRO builds must be configured.
