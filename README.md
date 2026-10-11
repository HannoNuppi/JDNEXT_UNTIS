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

- Firestore Rules erzwingen Feldtypen, Längen und gültige Upload-Metadaten.
- Bonusbuchungen (`+10`) können nur zusammen mit einem neuen Homework-Eintrag und der privaten Autorzuordnung erfolgen.
- Pro Firebase-Identität ist eine Meldung pro Eintrag erlaubt; die zweite eindeutige Meldung ermöglicht das Entfernen.
- Moderationsabzug (`−20`), Profilsperre und Löschen müssen als gemeinsamer Rules-geprüfter Commit erfolgen.
- Private Autorzuordnungen und Münzereignisse sind nicht öffentlich lesbar oder frei änderbar.
- Der Themenbestand startet mit ausschließlich `Next`.

Für neue Posts, Bildanhänge, Reports und Münzen muss `Tobiservices-Account/firestore.rules` im Projekt `tobiservices` veröffentlicht und **Authentication → Sign-in method → Anonym** aktiviert sein. Die Regeln aus diesem Repository bleiben für Klassenarbeiten und bereits vorhandene Legacy-Hausaufgaben im Projekt `next-untis-plus` erforderlich.

## GitHub Pages / PWA

Die App kann direkt über GitHub Pages betrieben und als PWA installiert werden.


## Erstinitialisierung der Firestore-Datenstruktur

Für die optionale Legacy-Discord-Bildfunktion legt das JDNEXT-Backend Schema-Hinweise an. Neue Hausaufgaben, Reports und Münzereignisse entstehen dagegen direkt im TobiServices-Firestore, sobald ein echter Eintrag geschrieben wird.

Siehe [FIREBASE_SCHEMA.md](FIREBASE_SCHEMA.md) für konkrete Feldformate.

## Hausaufgabenbilder

Neue Hausaufgabenbilder werden lokal im Browser auf maximal etwa 560 KiB komprimiert und als eigene Firestore-Dokumente im TobiServices-Projekt gespeichert. Damit bleiben sie unter der Firestore-Grenze pro Dokument; bis zu drei Bilder pro Aufgabe sind möglich. Für diese neuen Uploads sind weder Discord noch Cloud Functions oder ein Blaze-Tarif nötig.

Bereits vorhandene Einträge aus `next-untis-plus` können noch Discord-Attachment-IDs enthalten. Nur diese alten Bilder benötigen zum Auflösen der Discord-URLs weiterhin die bisherige JDNEXT-Cloud Function; neue Uploads verwenden den direkten Firestore-Weg.

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
