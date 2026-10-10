# Firebase-Datenstruktur – JDNEXT

Beim ersten Start der JDNEXT-Cloud-Functions-Runtime legt das Backend die Schema-Hinweise automatisch an. Firestore ist schemafrei und erstellt Collections erst zusammen mit echten Dokumenten.

## 1. `system/schema`

Schema-Hinweis für Daten, die JDNEXT verwendet. Das Backend erstellt ihn automatisch.

### Bestehende Hausaufgaben (Legacy): `homework/{homeworkKey}/entries/{entryId}`

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

Diese Pfade bleiben im Projekt `next-untis-plus` für bereits gespeicherte Einträge lesbar. Neue Hausaufgaben werden jetzt unter `tobiservices/jdnextHomework/{homeworkKey}/entries/{entryId}` abgelegt; Details stehen in Abschnitt 4.

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


## 4. Neue Hausaufgaben und Münzen (Spark-kompatibel)

Neue Hausaufgaben, Reports, private Autorzuordnungen und Münzereignisse werden im Firestore-Projekt `tobiservices` gespeichert. Dadurch können die Firestore Rules zusammengehörige Änderungen mit `getAfter()` und atomaren Client-Transaktionen prüfen; für die Text-Hausaufgaben-Münzlogik ist keine Cloud Function nötig.

### Öffentliche Einträge: `jdnextHomework/{homeworkKey}/entries/{entryId}`

```json
{
  "text": "Aufgaben im Buch bearbeiten",
  "author": "Name (optional)",
  "images": [],
  "reportCount": 0,
  "createdAt": "<Firestore Timestamp>"
}
```

### Private Autorzuordnung: `jdnextHomeworkPrivate/{homeworkKey}/entries/{entryId}`

```json
{
  "authorAuthUid": "<Firebase UID, einschließlich anonymen Sitzungen>",
  "authorTobiUid": "<TobiServices UID oder null>",
  "createdAt": "<Firestore Timestamp>"
}
```

Browser dürfen diese Zuordnung nur per direktem `get` lesen, wenn der öffentliche Eintrag bereits zwei Meldungen hat; Schreiben und Ändern ist über Regeln eingeschränkt.

### Reports: `jdnextHomework/{homeworkKey}/entries/{entryId}/reports/{reportUid}`

Jede Firebase-Identität darf genau einen Report pro Eintrag anlegen. Das Report-Dokument und das Erhöhen von `reportCount` müssen Teil derselben atomaren Transaktion sein.

### Münzereignisse: `jdnextCoinEvents/{eventId}`

Münzereignisse werden für Post-Boni (`+10`) und Entfernung nach zwei Reports (`−20`) im selben Firestore-Projekt protokolliert. Die Rules verlangen, dass ein Bonus zusammen mit einem neuen Eintrag und die Strafe zusammen mit dem Löschen des Eintrags und der Saldoänderung geschrieben werden. Event-Dokumente sind für Browser nicht lesbar, änderbar oder löschbar.

Bei einem Saldo von `-50` oder weniger wird im TobiServices-Profil `disabled: true` gesetzt. Im Spark-Tarif kann dieser Ablauf das Firebase-Authentication-Konto nicht serverseitig deaktivieren; die angebundenen Dienste müssen deshalb das Profilfeld `disabled` beachten.

Aktiviere in Firebase Console → Authentication → Sign-in method im Projekt `tobiservices` **Anonym** und veröffentliche dort die Regeln aus `Tobiservices-Account/firestore.rules`. Die Discord-Bild-Uploads sind weiterhin eine separate Cloud-Function-Funktion und werden über `DISCORD_BOT_TOKEN` eingerichtet.
