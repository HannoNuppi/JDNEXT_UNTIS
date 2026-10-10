# JDNEXT UNTIS

A/B-Wochen-Stundenplan mit gemeinsamen Hausaufgaben als PWA.

## Hausaufgaben

Der Stundenplan, Klassenarbeiten und alte Daten bleiben im ursprünglichen Firebase-Projekt **`next-untis-plus`**. Neue Hausaufgaben und Meldungen werden im **TobiServices-Firestore-Projekt `tobiservices`** gespeichert, damit Firestore Rules Post, Report und gemeinsames Guthaben in atomaren Transaktionen prüfen können.

Hausaufgaben sind öffentlich lesbar. Posten und melden geht auch ohne TobiServices-Konto; dafür verwendet JDNEXT eine anonyme Firebase-Identität im TobiServices-Projekt. Ein angemeldeter, nicht gesperrter TobiServices-Account erhält bei einem neuen Eintrag `+10` Goldmünzen. Nach zwei Meldungen von unterschiedlichen Firebase-Identitäten wird der Eintrag entfernt und für einen zugeordneten TobiServices-Autor werden `20` Goldmünzen abgezogen.

Schon vorhandene Einträge im alten Projekt bleiben sichtbar, sind aber aus Kompatibilitätsgründen schreibgeschützt. Neue Einträge werden im TobiServices-Projekt angelegt.

## TobiServices Account

Der Button „TobiServices Account“ öffnet optional das zentrale Account Center:

https://hannonuppi.github.io/Tobiservices-Account/

Ein TobiServices-Konto ist nicht erforderlich, um den Stundenplan oder öffentliche Hausaufgaben anzusehen, zu posten oder zu melden.

## Sicherheit

- Firestore Rules erlauben Bonusbuchungen nur zusammen mit einem neuen Homework-Dokument.
- Pro Firebase-Identität ist höchstens eine Meldung pro Eintrag erlaubt; der Zähler muss atomar zusammen mit einer neuen Meldung steigen.
- Entfernen nach zwei Meldungen und die `−20`-Buchung müssen als derselbe Rules-geprüfte Commit erfolgen.
- Autorzuordnungen und Münzereignisse sind für Browser nicht lesbar, manipulierbar oder löschbar.
- Der TobiServices-Saldo wird nicht frei vom Client beschreibbar; nur der eng geprüfte Post-/Moderationsablauf oder Admins können ihn ändern.

Für neue Hausaufgaben und Münzen müssen die Regeln aus `Tobiservices-Account/firestore.rules` im Firebase-Projekt `tobiservices` veröffentlicht werden. In Authentication → Sign-in method muss dort **Anonym** aktiviert sein. Die bestehenden Regeln aus diesem Repository bleiben für alte Hausaufgaben und Klassenarbeiten im Projekt `next-untis-plus` erforderlich.

## GitHub Pages / PWA

Die App kann direkt über GitHub Pages betrieben und als PWA installiert werden.


## Erstinitialisierung der Firestore-Datenstruktur

Beim ersten Start der JDNEXT-Cloud-Functions-Runtime legt das Backend automatisch die Schema-Referenz `system/schema` und den Integrationshinweis `system/discord` an. Diese Dokumente beschreiben das Eintragformat, legen aber keine leeren Hausaufgaben- oder Klassenarbeits-Collections an: Firestore-Collections entstehen automatisch mit den ersten echten Dokumenten.

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
