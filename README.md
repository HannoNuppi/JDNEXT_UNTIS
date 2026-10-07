# JDNEXT UNTIS

A/B-Wochen-Stundenplan mit gemeinsamen Hausaufgaben als PWA.

## Hausaufgaben

JDNEXT verwendet weiterhin das ursprüngliche Firebase-Projekt **next-untis-plus**.

Hausaufgaben sind **öffentlich lesbar**. Dafür ist kein TobiServices-Konto notwendig.

Neue Einträge werden als einzelne, unveränderliche Firestore-Dokumente gespeichert. Die Website verwendet dafür automatisch eine anonyme Firebase-Authentifizierung im Hintergrund. Ein TobiServices-Konto ist dafür nicht erforderlich.

## TobiServices Account

Der Button „TobiServices Account“ öffnet optional das zentrale Account Center:

https://hannonuppi.github.io/Tobiservices-Account/

Wichtig: Die beiden Firebase-Projekte bleiben getrennt. Der TobiServices-Account wird nicht benötigt, um den Stundenplan oder öffentliche Hausaufgaben zu benutzen.

## Sicherheit

- öffentliche Leserechte für Hausaufgaben
- keine öffentlichen Update-/Delete-Rechte
- serverseitige Feld- und Längenvalidierung
- Ersteller-UID wird durch Firestore Rules geprüft
- einzelne Einträge statt gemeinsam überschreibbarer Arrays
- Reports können pro Firebase-Identität nur einmal angelegt werden
- sensible Report-Daten sind nicht öffentlich lesbar
- alles außerhalb der Hausaufgaben ist standardmäßig geschlossen

Die Rules aus `firestore.rules` müssen im Firebase-Projekt `next-untis-plus` veröffentlicht werden.

## GitHub Pages / PWA

Die App kann direkt über GitHub Pages betrieben und als PWA installiert werden.
