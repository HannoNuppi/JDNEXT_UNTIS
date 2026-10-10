# Firebase-Datenstruktur – JDNEXT

Beim ersten Start der JDNEXT-Cloud-Functions-Runtime legt das Backend die Schema-Hinweise automatisch an. Firestore ist schemafrei und erstellt Collections erst zusammen mit echten Dokumenten.

## 1. `system/schema`

Schema-Hinweis für Daten, die JDNEXT verwendet. Das Backend erstellt ihn automatisch.

### Hausaufgaben: `homework/{homeworkKey}/entries/{entryId}`

```json
{
  "text": "Aufgaben im Buch bearbeiten",
  "author": "Name (optional)",
  "images": [
    {
      "messageId": "<Discord Message ID>",
      "attachmentId": "<Discord Attachment ID>",
      "filename": "arbeitsblatt.jpg"
    }
  ],
  "createdAt": "<Firestore Timestamp>"
}
```

Ohne Bild ist `images` eine leere Liste: `[]`. Die IDs und der Dateiname werden nach dem Discord-Upload gespeichert; nicht die Bilddatei und nicht die kurzlebige CDN-URL.

### Klassenarbeiten: `classwork/{homeworkKey}/entries/{entryId}`

```json
{
  "title": "Mathe-Klassenarbeit",
  "createdBy": "<anonyme Firebase UID>",
  "createdAt": "<Firestore Timestamp>"
}
```

Diese Einträge werden durch die Website erzeugt; leere Collections müssen nicht manuell angelegt werden.

## 2. `system/discord`

Das Backend erstellt unter `system/discord` einen Metadatensatz mit Kanal-ID und Secret-Namen. **Er enthält absichtlich keinen Bot-Token.** Das Feld `tokenStoredInFirestore` ist `false`.

## 3. Discord-Bot-Token: nicht in Firestore

Der Token gehört in **Firebase/Google Cloud Secret Manager**, nicht in eine Firestore-Collection.

- Firebase-Projekt: `next-untis-plus`
- Secret-Name: `DISCORD_BOT_TOKEN`
- Wert: der echte Bot-Token
- Functions, die ihn benutzen: `uploadJdnextImage` und `resolveJdnextDiscordImage`

Einrichten kannst du ihn im Terminal im lokalen JDNEXT-Projekt:

```bash
firebase use next-untis-plus
firebase functions:secrets:set DISCORD_BOT_TOKEN
firebase deploy --only functions
```

Die CLI fordert dich zur Eingabe des geheimen Wertes auf. Den Token nicht in GitHub, HTML, ein öffentliches Dokument oder eine normale Firestore-Collection schreiben. Nach Änderung des Secrets müssen die referenzierenden Functions erneut deployed werden.


## 4. Private Autorzuordnung, Meldungen und Münzen

Die öffentliche Hausaufgabe liegt unter `homework/{homeworkKey}/entries/{entryId}`. Falls beim Posten ein TobiServices-Login verifiziert wurde, legt die Cloud Function zusätzlich `homeworkPrivate/{homeworkKey}/entries/{entryId}` an. Dieses Dokument enthält die zugehörige TobiServices-UID und den Status der Bonusvergabe. Firestore-Regeln verweigern Browsern den Zugriff darauf.

Die öffentlichen Report-Dokumente liegen unter `homework/{homeworkKey}/entries/{entryId}/reports/{reportUid}`. Der Report-Trigger zählt unterschiedliche Report-UIDs. Bei mindestens zwei Reports wird die öffentliche Hausaufgabe entfernt. Ist ein verifizierter TobiServices-Autor hinterlegt, werden genau einmal 20 Münzen abgezogen. Der Trigger und die Münzbuchung verwenden idempotente Event-IDs, damit erneute Function-Aufrufe nicht doppelt abbuchen.

## 5. Gemeinsamer Schlüssel zu TobiServices

Der Münzbridge-Aufruf benötigt **denselben geheimen Zufallswert** in beiden Firebase-Projekten, aber unter unterschiedlichen Secret-Namen:

- Projekt `next-untis-plus`: `TOBI_REWARD_SECRET`
- Projekt `tobiservices`: `JDNEXT_REWARD_SECRET`

Beispiel für die Konfiguration mit Firebase CLI; die CLI fragt den Wert sicher interaktiv ab:

```bash
# Zuerst in TobiServices denselben langen zufälligen Wert setzen
firebase use tobiservices
firebase functions:secrets:set JDNEXT_REWARD_SECRET
firebase deploy --only functions,firestore:rules

# Dann den identischen Wert im JDNEXT-Projekt setzen
firebase use next-untis-plus
firebase functions:secrets:set TOBI_REWARD_SECRET
firebase deploy --only functions,firestore:rules
```

Dafür wird ein Firebase-Blaze-Tarif mit aktivierter Abrechnung benötigt. Der geheime Wert gehört niemals ins Repository, in HTML/JavaScript oder nach Firestore. Ohne korrekt gesetzte Secrets und deployte Functions bleiben normale Hausaufgaben-Posts und -Meldungen accountlos möglich, aber die Münzautomatik läuft nicht.
