# Untis Next – Android APK

## Automatische Updates

Die Android-App prüft beim Start (höchstens einmal alle sechs Stunden), ob auf GitHub eine neuere APK-Version vorhanden ist. Bei einem Update fragt sie nach, lädt die APK über HTTPS herunter, prüft deren SHA-256-Prüfsumme und Signatur und übergibt sie an den Android-Paketinstaller. Android zeigt immer seine eigene Installationsbestätigung; die App installiert sich nicht heimlich.

- **APK-Download:** [UntisNext.apk](https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/download/android-apk/UntisNext.apk)
- **Updater-Metadaten:** [update.json](https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/download/android-apk/update.json)
- **Builds und Status:** [Build Untis Next Android APK](https://github.com/HannoNuppi/JDNEXT_UNTIS/actions/workflows/android-apk.yml)

Die Datei `update.json` wird zusammen mit der APK veröffentlicht. Sie enthält Versionsnummer, APK-URL, SHA-256-Prüfsumme und Fingerabdruck des Signaturzertifikats.

## Wichtig: dauerhaften Signaturschlüssel einrichten

Android erlaubt ein direktes APK-Update nur dann, wenn die neue Version mit demselben Signaturschlüssel wie die installierte App signiert wurde. GitHub Actions erstellt ohne feste Signierungsschlüssel Test-Builds; ihr automatisch generierter Debug-Schlüssel ist nicht für zuverlässige App-Updates geeignet.

Richte daher einmalig einen privaten, dauerhaften Upload-Schlüssel ein. **Veröffentliche die JKS-Datei oder ihre Passwörter niemals im Repository.** Bewahre eine private Sicherung der JKS-Datei auf: Ohne genau diesen Schlüssel lassen sich spätere Versionen nicht als Update installieren.

1. Erstelle lokal auf einem Rechner mit JDK 17 eine Upload-Keystore-Datei. Zum Beispiel in PowerShell:

   ```powershell
   keytool -genkeypair -v -keystore untis-next-upload.jks -alias untisnext -keyalg RSA -keysize 2048 -validity 10000
   ```

   Wähle ein sicheres Passwort und sichere die Datei an einem privaten Ort.

2. Erzeuge in PowerShell einen Base64-Text zum Eintragen als Secret:

   ```powershell
   [Convert]::ToBase64String([IO.File]::ReadAllBytes(".\\untis-next-upload.jks")) | Set-Clipboard
   ```

3. Öffne im GitHub-Repository **Settings → Secrets and variables → Actions** und lege diese Repository-Secrets an:
   - `ANDROID_KEYSTORE_BASE64`: der kopierte Base64-Inhalt
   - `ANDROID_KEYSTORE_PASSWORD`: Passwort des Keystores
   - `ANDROID_KEY_ALIAS`: `untisnext`
   - `ANDROID_KEY_PASSWORD`: Passwort des Schlüssels (oft dasselbe wie das Keystore-Passwort)

4. Starte danach den Workflow [Build Untis Next Android APK](https://github.com/HannoNuppi/JDNEXT_UNTIS/actions/workflows/android-apk.yml) über **Run workflow**. Wenn alle Secrets vorhanden sind, veröffentlicht er eine mit diesem dauerhaften Schlüssel signierte APK samt Update-Metadaten.

### Einmalige Umstellung von der bisherigen Test-APK

Die bereits verteilte APK wurde ohne dauerhaft gespeicherten privaten Schlüssel gebaut. Falls ihr Signaturzertifikat vom neuen Schlüssel abweicht, kann Android die stabile Version nicht darüber installieren. In diesem Fall muss die bisherige App einmal deinstalliert und die neu signierte APK danach manuell installiert werden. Deine serverseitigen Konten und gespeicherten Kontodaten bleiben in Firebase; möglicherweise musst du dich erneut anmelden. **Nach dieser Umstellung** können weitere APK-Versionen mit demselben geheim gehaltenen Schlüssel als normale Updates installiert werden.

## Was die Android-App kann

- Lädt die aktuelle Untis-Next-Web-App unter `hannonuppi.github.io/JDNEXT_UNTIS`.
- Bestehende Firebase-Konten, Hausaufgaben und Theme-Inventare bleiben dieselbe Online-Datenbasis.
- Der Bildauswahldialog im Android-WebView kann bis zu drei Hausaufgabenbilder an die Website zurückgeben.
- Launcher-Icons sind für alle Themes enthalten. Ein Theme-Wechsel wird gespeichert und das Launcher-Icon erst beim Wechsel der App in den Hintergrund umgestellt, um das Schließen der laufenden App zu vermeiden.

## Lokales Bauen (optional)

Öffne `android-app` in Android Studio mit Android SDK 35 und JDK 17 oder führe auf einer Umgebung mit Gradle 8.9 aus:

```bash
gradle --no-daemon --project-dir android-app assembleDebug
```

Für die Veröffentlichung mit Versionsnummern kann Gradle beispielsweise mit `-PappVersionCode=12 -PappVersionName=1.0.12` aufgerufen werden. Die resultierende Test-APK liegt hier:

```
android-app/app/build/outputs/apk/debug/app-debug.apk
```

Die Android-Hülle lädt die Website live über HTTPS und braucht eine Internetverbindung. Firebase-Konten und Hausaufgaben sind nicht als lokale Datenbank in die APK eingebettet.
