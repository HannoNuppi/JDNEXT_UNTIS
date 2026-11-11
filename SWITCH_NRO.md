# Untis Next Native Switch Port

The NRO is a native Nintendo Switch homebrew application written in C with
libnx. It does **not** open a browser, WebKit, SDL, or a website applet.

## Native implementation

- libnx manages the application lifecycle, controller input, system keyboard,
  SD-card access, HTTPS services and a 1280x720 native framebuffer.
- The dashboard, timetable, lesson details, account, theme shop, Advent
  calendar and admin tools are drawn directly by the native framebuffer UI.
- HTTPS requests use switch-curl with libnx TLS and certificate verification.
- The timetable uses the current A/B cycle and date keys from Untis Next.
  The schedule is compiled into the client and must be updated in
  `switch-app/source/main.c` when the school timetable changes.
- Homework and classwork are loaded from their existing Firestore projects.
  New homework and classwork are posted through Firestore REST, respecting the
  current Firestore Rules and server timestamps.
- Homework can include one JPEG attachment selected from
  `sdmc:/switch/UntisNext/uploads/`. Use a JPEG smaller than 580 KB. The native
  client writes the image as a Firestore image subdocument and the public entry
  stores the same image metadata as the website.
- Homework reports use the protected
  `reportJdnextHomeworkEntryNative` callable. The second report removes the
  entry, its private author mapping and attached image, and applies the existing
  -20 Goldmünzen penalty when the author received the posting reward.
- TobiServices supports native email/password sign-in and account creation.
  Passwords are only sent to Firebase Authentication for the request and are
  cleared from the temporary buffers; they are not written to the SD card.
- The profile screen reads and saves profile title, profile icon, theme
  ownership, equipped theme and Goldmünzen.
- The native theme shop uses the website's existing prices and Firestore
  purchase rules. Admin Matrix is a local palette enabled by the active admin
  tag; event themes appear when the account owns them.
- The Advent calendar shows all 24 doors. The client only attempts to open the
  current Berlin-calendar day; Firestore Rules validate the year, day and
  rewards against server time and grant WinterEvent / WinterGoldEvent where
  eligible.
- The admin interface can start an NRO build, toggle maintenance, inspect
  accounts, edit tags, disable/enable accounts, set Goldmünzen, grant/revoke
  event themes, view the Gold leaderboard, edit resource access tags, and
  request a live publication.
- Admin writes use the signed-in Firebase account and are checked by existing
  Firestore Rules or protected callable Cloud Functions. No GitHub token,
  service-account key or admin secret is embedded in the NRO.

## Controls

### Timetable

- D-pad Left/Right: choose weekday
- D-pad Up/Down: choose lesson period
- L/R: previous/next week
- A: open the selected lesson
- X: account
- Y: refresh maintenance status
- Plus: exit

### Lesson details

- D-pad Up/Down: select an entry
- A: create a homework entry (the SD JPEG picker is optional; press B to skip)
- X: create a classwork entry
- R: report the selected new homework entry
- Y: refresh entries
- B: back to timetable

### Account, themes and profile

- D-pad Up/Down: move through account actions or themes
- A: confirm, equip or buy
- X on the theme screen: open profile design
- Profile design: Up/Down selects title; Left/Right selects icon; A saves
- Guest accounts can browse themes and create/sign into a TobiServices account
- Signed-in accounts can open the Advent calendar

### Advent calendar

- D-pad: choose one of the 24 doors
- A: attempt to open the selected door (only today's door is accepted)
- Y: refresh profile and door state
- B: back

### Native admin area

- Start a new Switch NRO build
- Toggle Untis Next maintenance mode and set its visitor message
- Open the account manager:
  - A: enable/disable the selected account
  - X: edit the selected account's tags
  - L: edit the selected account's Goldmünzen balance
  - R: grant/revoke an event theme using `+WinterGoldEvent` or `-WinterGoldEvent`
  - Y: refresh the account list
- Open the Gold leaderboard
- Open the access manager:
  - Up/Down: choose a resource
  - A: edit its allowed tags (empty means public)
  - X: edit the allowed tag catalog (the admin tag must remain)
  - Y: save changes
- Request a live publication with an optional note
- Every privileged operation is guarded by the current account and the existing
  Firestore Rules / Cloud Functions.

## Current feature boundary

This is now a substantial native port, but it is not yet a perfect 1:1 copy of
every website feature.

Still outstanding:

- Native JPEG image preview/viewing inside a homework detail.
- Selecting and attaching more than one image to a homework entry (the web
  version supports up to three; the Switch version currently attaches one).
- Full Test Mode mirror editing and the complete pre-publication review flow.
- Editing the timetable itself from the Switch (the current schedule is
  compiled into the NRO).
- Some smaller web-only account, cosmetic and moderation details.

No WebKit fallback is used.

## Build

GitHub Actions installs the host tools, builds the Switch curl portlib, compiles
the native libnx application and publishes the NRO as an artifact and prerelease:

https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/tag/switch-nro

## Firebase requirements

The NRO uses the public Firebase Web API keys already used by the website. A
Firebase API key is not an admin credential; Firestore Security Rules and
callable Cloud Functions remain the authorization boundary.

Deploy the native report callable from the `Tobiservices-Account` directory:

```sh
firebase deploy --only functions:reportJdnextHomeworkEntryNative
```

For the native and website NRO-build buttons, configure the
`SWITCH_NRO_GITHUB_TOKEN` Firebase Functions secret and deploy the
`requestSwitchNroBuild` callable as described in the repository's Firebase
setup instructions. The token must remain in Firebase Secret Manager and
must never be included in the NRO.

The application requires an internet connection. Firebase email/password and
anonymous authentication methods must be enabled in the corresponding Firebase
projects. The SD JPEG picker reads files from
`sdmc:/switch/UntisNext/uploads/`.
