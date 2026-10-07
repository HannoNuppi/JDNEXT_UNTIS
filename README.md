# JDNEXT UNTIS

Ein A/B-Wochen-Stundenplan mit gemeinsamen Hausaufgaben als PWA.

## Zentraler TobiServices-Login

Die App verwendet jetzt das zentrale Konto-System aus
[HannoNuppi/Tobiservices-Account](https://github.com/HannoNuppi/Tobiservices-Account).

- Anmeldung über TobiServices Account Center
- gemeinsame Firebase-Authentifizierung
- Hausaufgaben werden nur für angemeldete und per E-Mail verifizierte Konten gelesen/geschrieben
- Admin-Tags werden aus dem zentralen Accountprofil übernommen

Die konkrete Firebase-Sicherheit steht in `firestore.rules`. Dieselben Regeln müssen im Firebase-Projekt `tobiservices` veröffentlicht werden.

## Wichtiger Datenbank-Hinweis

Die frühere Version verwendete das Firebase-Projekt `next-untis-plus`.
Der aktuelle Stand verwendet `tobiservices`, damit Account-UID und Hausaufgaben dieselbe Authentifizierungsbasis haben.

Alte Daten aus dem alten Firebase-Projekt werden dadurch nicht automatisch migriert.

## GitHub Pages

Die Seite kann direkt über GitHub Pages veröffentlicht werden. Der Service Worker und das Manifest machen sie als PWA installierbar.
