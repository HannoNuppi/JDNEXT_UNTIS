# Stundenplan

Ein Klassen-Stundenplan mit A/B-Wochen-Rhythmus und gemeinsamen Hausaufgaben,
komplett als eine HTML-Datei — läuft direkt über GitHub Pages, ohne eigenen Server.

## Live-Version einrichten

1. **Vor dem Hochladen:** Öffne `index.html` in einem Texteditor und passe im Block
   `const CONFIG = { ... }` deinen Stundenplan an (Fächer, Uhrzeiten, A/B-Woche).
2. Für **geteilte Hausaufgaben** (alle sehen dieselben Einträge): kostenlosen Bin auf
   [jsonbin.io](https://jsonbin.io) anlegen (Inhalt: `{"homework":{}}`) und Bin-ID +
   API-Key bei `CONFIG.jsonbin` eintragen. Ohne das läuft die Seite im lokalen Modus
   (jede Person sieht nur ihre eigenen Einträge).

## Auf GitHub veröffentlichen

1. Auf [github.com](https://github.com) einloggen (Account kostenlos erstellen, falls nötig).
2. Oben rechts **+** → **New repository** → Namen vergeben (z. B. `stundenplan`) → **Public** → **Create repository**.
3. Im leeren Repository auf **uploading an existing file** klicken, `index.html` per Drag & Drop hochladen, unten **Commit changes**.
4. Oben im Repo auf **Settings** → links **Pages** → bei **Source** `main` und Ordner `/ (root)` wählen → **Save**.
5. Nach ca. 1 Minute erscheint der Link oben auf der Pages-Einstellungsseite:
   `https://DEIN-BENUTZERNAME.github.io/stundenplan/`
6. Diesen Link an alle in der Klasse weitergeben.

## Später aktualisieren

Datei im Repo öffnen → Stift-Symbol (**Edit**) → Änderungen einfügen → **Commit changes**.
GitHub Pages aktualisiert die Seite automatisch nach kurzer Zeit.

## Enthaltene Dateien

- `index.html` — die App selbst (Stundenplan, Hausaufgaben, Melde-Funktion, Login-Platzhalter)
- `README.md` — diese Anleitung
