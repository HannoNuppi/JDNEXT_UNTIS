# Firebase-Datenstruktur – JDNEXT

Diese Struktur wird vom JDNEXT-Backend beim ersten Aufruf der Discord-Upload-/CDN-Funktion vorbereitet. Firestore ist schemafrei und erstellt Collections erst zusammen mit echten Dokumenten.

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

## 2. `system/integrations/discord`

Das Backend erstellt einen Metadatensatz mit Kanal-ID und Secret-Namen. **Er enthält absichtlich keinen Bot-Token.** Das Feld `tokenStoredInFirestore` ist `false).

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
