# Untis Next – Android APK

## APK automatisch bauen

Die GitHub-Action **Build Untis Next Android APK** baut eine installierbare Android-APK automatisch, wenn relevante Android- oder Website-Dateien nach `main` gepusht werden. Der Build kann auch manuell über die Registerkarte **Actions** gestartet werden.

- **Direkter Download:** [UntisNext.apk](https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/download/android-apk/UntisNext.apk)
- **Builds und Status:** [Build Untis Next Android APK](https://github.com/HannoNuppi/JDNEXT_UNTIS/actions/workflows/android-apk.yml)

Der Build landet zusätzlich als Artifact im jeweiligen GitHub-Actions-Lauf. Der Dateiname und die angezeigte App-Version haben keinen Debug-Zusatz. Die Datei wird derzeit noch mit einer Android-Entwicklungssignatur gebaut und ist kein Play-Store-Release.

## Was die Android-App kann

- Lädt die aktuelle Untis-Next-Web-App unter `hannonuppi.github.io/JDNEXT_UNTIS`.
- Die bestehenden Firebase-Konten, Hausaufgaben und Theme-Inventare bleiben dieselbe Online-Datenbasis.
- Der Bildauswahldialog im Android-WebView kann bis zu drei Hausaufgabenbilder an die Website zurückgeben.
- Die Launcher-Icons sind für alle Themes enthalten. Wenn Untis Next ein Theme aus dem gespeicherten Profil lädt oder ein anderes Theme ausgerüstet wird, übermittelt die Seite dessen ID an eine native Android-Bridge. Diese merkt sich das gewünschte Icon und schaltet die Android-Launcher-Aliase erst dann um, wenn die App natürlich in den Hintergrund geht, damit der Theme-Wechsel die laufende App nicht schließt.
- Im normalen Browser ändert sich das Tab-Favicon nach dem aktuellen Theme. Bei bereits installierten PWAs aktualisieren Browser das Startbildschirm-Icon nicht zuverlässig; für den Theme-abhängigen Launcher ist die APK vorgesehen.

## Lokales Bauen (optional)

Öffne den Ordner `android-app` in Android Studio mit einem installierten Android SDK 35 und JDK 17, oder führe auf einer Umgebung mit Gradle 8.9 aus:

```bash
gradle --no-daemon --project-dir android-app assembleDebug
mkdir -p dist
cp android-app/app/build/outputs/apk/debug/app-debug.apk dist/UntisNext.apk
```

Die resultierende Datei liegt unter:

```
dist/UntisNext.apk
```

Die Android-Hülle lädt die Website live über HTTPS. Daher braucht sie eine Internetverbindung; die APK bettet Firebase-/Web-App-Daten nicht ein und speichert Konten nicht separat in einer zweiten Datenbank.
