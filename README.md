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


## Erstinitialisierung der Firestore-Datenstruktur

Beim ersten Start der JDNEXT-Cloud-Functions-Runtime legt das Backend automatisch die Schema-Referenz `system/schema` und den Integrationshinweis `system/integrations/discord` an. Diese Dokumente beschreiben das Eintragformat, legen aber keine leeren Hausaufgaben- oder Klassenarbeits-Collections an: Firestore-Collections entstehen automatisch mit den ersten echten Dokumenten.

Siehe [FIREBASE_SCHEMA.md](FIREBASE_SCHEMA.md) für konkrete Feldformate.

## Discord-Bilder

Bilder für Hausaufgaben werden nicht in GitHub oder Firebase Storage gespeichert. JDNEXT lädt sie über eine geschützte Cloud Function mit einem Discord-Bot in den Kanal `1557736296795865108`. In Firestore werden nur Discord-Nachrichten-/Attachment-IDs gespeichert. Beim Anzeigen wird daraus serverseitig eine aktuelle Discord-CDN-URL geholt.

Einrichtung im lokalen Repository:

1. Firebase CLI installieren und anmelden.
2. Das Projekt wählen:
   `firebase use next-untis-plus`
3. Den Bot-Token als Secret setzen:
   `firebase functions:secrets:set DISCORD_BOT_TOKEN`
   Danach den Token **nur in die CLI-Eingabe** einfügen. Er gehört weder in GitHub noch in den Browsercode.
4. Der Bot muss den Zielkanal sehen und dort Nachrichten mit Dateianhängen senden dürfen.
5. Backend und Rules deployen:
   `firebase deploy --only functions,firestore`

Discord-CDN-URLs für Anhänge sind signiert und können ablaufen. Deshalb speichert JDNEXT absichtlich nicht die temporäre URL, sondern ruft beim Anzeigen eine frische URL ab.

## TobiServices Bootstrap-Schlüssel

Im TobiServices-Account-Client ist mit **Bootstrap-Schlüssel** genau der geheime Wert gemeint, den du vorher mit

`firebase functions:secrets:set TOBI_BOOTSTRAP_KEY`

für das Projekt `tobiservices` gesetzt hast.

Wichtig:
- **Secret-Name:** `TOBI_BOOTSTRAP_KEY`
- **Eingabe im Client:** der tatsächliche geheime Wert
- Nicht eingeben: `TOBI_BOOTSTRAP_KEY` selbst
- Nicht eingeben: den Discord-Bot-Token

Beim ersten Admin wird dieser Wert serverseitig geprüft. Nach erfolgreicher Erstellung eines Admins kann derselbe Bootstrap-Endpunkt keinen zweiten ersten Admin mehr anlegen.

Cloud Functions benötigen für die Bereitstellung aktuell den Firebase-Blaze-Tarif.
