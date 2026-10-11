# Untis Next UNTIS

A/B-Wochen-Stundenplan mit gemeinsamen Hausaufgaben als PWA.

## Hausaufgaben

Der Stundenplan, Klassenarbeiten und alte Daten bleiben im ursprünglichen Firebase-Projekt **`next-untis-plus`**. Neue Hausaufgaben und Meldungen werden im **TobiServices-Firestore-Projekt `tobiservices`** gespeichert, damit Firestore Rules Post, Report und gemeinsames Guthaben in atomaren Transaktionen prüfen können.

Hausaufgaben sind öffentlich lesbar. Posten und melden geht auch ohne TobiServices-Konto; dafür verwendet Untis Next eine anonyme Firebase-Identität im TobiServices-Projekt. Ein angemeldeter, nicht gesperrter TobiServices-Account erhält bei einem neuen Eintrag `+10` Goldmünzen. Nach zwei Meldungen von unterschiedlichen Firebase-Identitäten wird der Eintrag entfernt und für einen zugeordneten TobiServices-Autor werden `20` Goldmünzen abgezogen.

Schon vorhandene Einträge im alten Projekt bleiben sichtbar, sind aber aus Kompatibilitätsgründen schreibgeschützt. Neue Einträge werden im TobiServices-Projekt angelegt.

## TobiServices Account

Der Button „TobiServices Account“ öffnet optional das zentrale Account Center:

https://hannonuppi.github.io/Tobiservices-Account/

Ein TobiServices-Konto ist nicht erforderlich, um den Stundenplan oder öffentliche Hausaufgaben anzusehen, zu posten oder zu melden.

## Themes und Wartungsbypass

Die Theme-Auswahl enthält das kostenlose Grundtheme **Next** und sieben weitere Themes für jeweils **10 Goldmünzen**. Käufe werden atomar im Projekt `tobiservices` verbucht: Der Kontostand wird um 10 reduziert und das Theme-Inventar erweitert. Die eigene Theme-Auswahl wird im TobiServices-Profil gespeichert und zwischen Geräten synchronisiert.

Admins können während des Wartungsmodus in der Wartungsansicht den **Admin-Wartungsbypass** ein- oder ausschalten. Das funktioniert nur für aktive TobiServices-Profile mit dem Tag `admin`; die Umschaltung wird im eigenen Profilfeld `maintenanceBypass` gespeichert.

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

Für die optionale Legacy-Discord-Bildfunktion legt das Untis Next-Backend Schema-Hinweise an. Neue Hausaufgaben, Reports und Münzereignisse entstehen dagegen direkt im TobiServices-Firestore, sobald ein echter Eintrag geschrieben wird.

Siehe [FIREBASE_SCHEMA.md](FIREBASE_SCHEMA.md) für konkrete Feldformate.

## Hausaufgabenbilder

Neue Hausaufgabenbilder werden lokal im Browser auf maximal etwa 560 KiB komprimiert und als eigene Firestore-Dokumente im TobiServices-Projekt gespeichert. Damit bleiben sie unter der Firestore-Grenze pro Dokument; bis zu drei Bilder pro Aufgabe sind möglich. Für diese neuen Uploads sind weder Discord noch Cloud Functions oder ein Blaze-Tarif nötig.

Bereits vorhandene Einträge aus `next-untis-plus` können noch Discord-Attachment-IDs enthalten. Nur diese alten Bilder benötigen zum Auflösen der Discord-URLs weiterhin die bisherige Untis Next-Cloud Function; neue Uploads verwenden den direkten Firestore-Weg.

## Kontosperre

Bei einem Guthaben von `-50` oder weniger wird `users/{uid}.disabled = true` gesetzt und bei neuen Anmeldungen des TobiServices-Account Centers der Login verweigert. Firebase Authentication selbst kann im Spark-Tarif nicht über einen Client-Write deaktiviert werden; verbundene Dienste müssen das Profilfeld `disabled` beachten. Das Konto und die Daten bleiben erhalten.
