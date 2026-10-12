package de.tobiservices.untisnext;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.os.Build;
import android.provider.Settings;
import android.content.pm.PackageInfo;
import android.content.pm.Signature;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import org.json.JSONObject;
import androidx.core.content.FileProvider;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.webkit.CookieManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.content.ClipData;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public final class MainActivity extends Activity {
    private static final String START_URL = "https://hannonuppi.github.io/JDNEXT_UNTIS/?source=android-apk";
    private static final int FILE_CHOOSER_REQUEST = 7124;
    private static final String ICON_PREFS = "untis_next_launcher_icon";
    private static final String PENDING_ICON_KEY = "pending_theme_icon";
    private static final String ACTIVE_ICON_KEY = "active_theme_icon";
    private static final String UPDATE_METADATA_URL =
        "https://github.com/HannoNuppi/JDNEXT_UNTIS/releases/download/android-apk/update.json";
    private static final String UPDATE_PREFS = "untis_next_updater";
    private static final String LAST_CHECK_KEY = "last_check_millis";
    private static final String LAST_PROMPTED_VERSION_KEY = "last_prompted_version";
    private static final long UPDATE_CHECK_INTERVAL_MS = 6L * 60L * 60L * 1000L;
    private static final int UPDATE_PERMISSION_REQUEST = 7125;
    private static final Set<String> THEME_IDS = new HashSet<>(Arrays.asList(
        "Next", "Ocean", "Sunset", "Forest", "Candy", "Midnight",
        "Aurora", "Retro", "RoyalRebels", "Bronze", "Silver", "Gold",
        "WinterEvent", "BirthdayEvent", "AdminMatrix", "HalloweenEvent"
    ));
    private WebView webView;
    private ValueCallback<Uri[]> fileCallback;
    private File pendingUpdateApk;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window window = getWindow();
        window.setStatusBarColor(Color.rgb(23, 17, 84));
        window.setNavigationBarColor(Color.rgb(10, 12, 42));
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        webView = new WebView(this);
        webView.setLayoutParams(new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
        ));
        webView.setBackgroundColor(Color.rgb(10, 12, 42));
        setContentView(webView);

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(true);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.setAcceptCookie(true);
        cookieManager.setAcceptThirdPartyCookies(webView, true);

        webView.addJavascriptInterface(new UntisNextAndroidBridge(), "UntisNextAndroid");
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (isTrustedFirstParty(uri)) return false;
                openOutsideApp(uri);
                return true;
            }
        });
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback,
                                             FileChooserParams params) {
                if (fileCallback != null) fileCallback.onReceiveValue(null);
                fileCallback = callback;
                Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("image/*");
                intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                try {
                    startActivityForResult(Intent.createChooser(intent, "Bilder für die Hausaufgabe auswählen"),
                        FILE_CHOOSER_REQUEST);
                } catch (ActivityNotFoundException error) {
                    fileCallback = null;
                    Toast.makeText(MainActivity.this, "Kein Bildauswahldialog verfügbar.", Toast.LENGTH_SHORT).show();
                    return false;
                }
                return true;
            }
        });

        webView.loadUrl(START_URL);
        checkForUpdates();
    }

    private boolean isTrustedFirstParty(Uri uri) {
        if (uri == null || !"https".equalsIgnoreCase(uri.getScheme())) return false;
        if (!"hannonuppi.github.io".equalsIgnoreCase(uri.getHost())) return false;
        String path = uri.getPath() == null ? "" : uri.getPath();
        return path.startsWith("/JDNEXT_UNTIS/") || path.startsWith("/Tobiservices-Account/");
    }

    private void openOutsideApp(Uri uri) {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, uri));
        } catch (ActivityNotFoundException ignored) {
            Toast.makeText(this, "Dieser Link kann auf dem Gerät nicht geöffnet werden.", Toast.LENGTH_SHORT).show();
        }
    }


    private void checkForUpdates() {
        final SharedPreferences preferences = getSharedPreferences(UPDATE_PREFS, MODE_PRIVATE);
        final long now = System.currentTimeMillis();
        if (now - preferences.getLong(LAST_CHECK_KEY, 0L) < UPDATE_CHECK_INTERVAL_MS) return;
        new Thread(() -> {
            HttpURLConnection connection = null;
            try {
                connection = (HttpURLConnection) new URL(UPDATE_METADATA_URL).openConnection();
                connection.setRequestMethod("GET");
                connection.setConnectTimeout(10000);
                connection.setReadTimeout(10000);
                connection.setInstanceFollowRedirects(true);
                connection.setRequestProperty("User-Agent", "UntisNext-Android-Updater");
                connection.setRequestProperty("Accept", "application/json");
                if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) return;
                String body = readText(connection.getInputStream(), 65536);
                JSONObject metadata = new JSONObject(body);
                long latestVersionCode = metadata.getLong("versionCode");
                String latestVersionName = metadata.optString("versionName", "neu");
                String apkUrl = metadata.getString("apkUrl");
                String apkSha256 = normalizeDigest(metadata.getString("sha256"));
                String signerSha256 = normalizeDigest(metadata.getString("signerSha256"));
                if (!isTrustedApkUrl(apkUrl) || apkSha256.length() != 64 || signerSha256.length() != 64) {
                    throw new IllegalStateException("Ungültige Update-Metadaten");
                }
                preferences.edit().putLong(LAST_CHECK_KEY, System.currentTimeMillis()).apply();
                if (latestVersionCode <= getInstalledVersionCode()) return;
                if (preferences.getLong(LAST_PROMPTED_VERSION_KEY, -1L) == latestVersionCode) return;
                runOnUiThread(() -> showUpdatePrompt(
                    latestVersionCode, latestVersionName, apkUrl, apkSha256, signerSha256
                ));
            } catch (Exception error) {
                android.util.Log.i("UntisNextUpdater", "Update-Prüfung übersprungen", error);
            } finally {
                if (connection != null) connection.disconnect();
            }
        }, "UntisNext-update-check").start();
    }

    private long getInstalledVersionCode() throws Exception {
        PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
        if (Build.VERSION.SDK_INT >= 28) return info.getLongVersionCode();
        return info.versionCode;
    }

    private boolean isTrustedApkUrl(String value) {
        try {
            Uri uri = Uri.parse(value);
            return "https".equalsIgnoreCase(uri.getScheme())
                && "github.com".equalsIgnoreCase(uri.getHost())
                && "/HannoNuppi/JDNEXT_UNTIS/releases/download/android-apk/UntisNext.apk".equals(uri.getPath());
        } catch (Exception ignored) {
            return false;
        }
    }

    private void showUpdatePrompt(long versionCode, String versionName, String apkUrl,
                                  String apkSha256, String signerSha256) {
        if (isFinishing() || (Build.VERSION.SDK_INT >= 17 && isDestroyed())) return;
        SharedPreferences preferences = getSharedPreferences(UPDATE_PREFS, MODE_PRIVATE);
        if (preferences.getLong(LAST_PROMPTED_VERSION_KEY, -1L) == versionCode) return;
        preferences.edit().putLong(LAST_PROMPTED_VERSION_KEY, versionCode).apply();

        String installedSigner;
        try {
            installedSigner = getInstalledSignerSha256();
        } catch (Exception error) {
            installedSigner = "";
        }
        if (!normalizeDigest(installedSigner).equals(normalizeDigest(signerSha256))) {
            new AlertDialog.Builder(this)
                .setTitle("Untis Next – Neuinstallation nötig")
                .setMessage("Version " + versionName + " ist verfügbar, wurde aber mit einem anderen APK-Signaturschlüssel gebaut. Android kann sie daher nicht über diese Installation aktualisieren. Für automatische Updates muss dauerhaft derselbe private Signaturschlüssel verwendet werden. Zum Wechsel musst du die APK herunterladen, diese App deinstallieren und danach die neue APK installieren. Deine serverseitigen Untis-Next-Kontodaten bleiben erhalten; die lokale Anmeldung kann erneut nötig sein.")
                .setPositiveButton("APK herunterladen", (dialog, which) -> openOutsideApp(Uri.parse(apkUrl)))
                .setNegativeButton("Später", null)
                .show();
            return;
        }
        new AlertDialog.Builder(this)
            .setTitle("Update für Untis Next verfügbar")
            .setMessage("Version " + versionName + " ist verfügbar. Die APK wird geprüft und anschließend vom Android-Installer zur Installation bestätigt.")
            .setPositiveButton("Jetzt aktualisieren", (dialog, which) ->
                downloadAndInstallUpdate(apkUrl, apkSha256, signerSha256))
            .setNegativeButton("Später", null)
            .show();
    }

    private void downloadAndInstallUpdate(String apkUrl, String expectedSha256,
                                          String expectedSignerSha256) {
        ProgressDialog progress = new ProgressDialog(this);
        progress.setTitle("Untis Next wird aktualisiert");
        progress.setMessage("Update wird heruntergeladen und geprüft …");
        progress.setIndeterminate(true);
        progress.setCancelable(false);
        progress.show();
        new Thread(() -> {
            File downloadedFile = null;
            String failure = null;
            boolean signerMatches = false;
            try {
                downloadedFile = downloadApk(apkUrl);
                String actualSha256 = sha256Hex(downloadedFile);
                if (!normalizeDigest(actualSha256).equals(normalizeDigest(expectedSha256))) {
                    throw new IllegalStateException("Die Prüfsumme der heruntergeladenen APK stimmt nicht.");
                }
                int flags = Build.VERSION.SDK_INT >= 28
                    ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
                PackageInfo apkInfo = getPackageManager().getPackageArchiveInfo(downloadedFile.getAbsolutePath(), flags);
                if (apkInfo == null || !getPackageName().equals(apkInfo.packageName)) {
                    throw new IllegalStateException("Die heruntergeladene Datei ist keine passende Untis-Next-APK.");
                }
                String downloadedSigner = getSignerSha256(apkInfo);
                signerMatches = normalizeDigest(downloadedSigner).equals(normalizeDigest(expectedSignerSha256))
                    && normalizeDigest(downloadedSigner).equals(normalizeDigest(getInstalledSignerSha256()));
                if (!signerMatches) {
                    throw new IllegalStateException("Die APK-Signatur stimmt nicht mit der installierten App überein.");
                }
            } catch (Exception error) {
                failure = error.getMessage() == null ? "Update fehlgeschlagen." : error.getMessage();
            }
            final File resultFile = downloadedFile;
            final String resultFailure = failure;
            final boolean resultSignerMatches = signerMatches;
            runOnUiThread(() -> {
                if (progress.isShowing()) progress.dismiss();
                if (resultFailure != null) {
                    if (resultFile != null && !resultSignerMatches && resultFailure.contains("Signatur")) {
                        new AlertDialog.Builder(this)
                            .setTitle("Update erfordert Neuinstallation")
                            .setMessage("Die neue APK ist nicht mit demselben Signaturschlüssel signiert. Android blockiert deshalb ein direktes Update. Lade die APK im Browser herunter und installiere sie erst nach einer Deinstallation der bisherigen App neu. Richte danach einen dauerhaften Signaturschlüssel in GitHub Actions ein.")
                            .setPositiveButton("APK herunterladen", (dialog, which) -> openOutsideApp(Uri.parse(apkUrl)))
                            .setNegativeButton("Abbrechen", null)
                            .show();
                    } else {
                        new AlertDialog.Builder(this)
                            .setTitle("Update nicht möglich")
                            .setMessage(resultFailure)
                            .setPositiveButton("OK", null)
                            .show();
                    }
                    return;
                }
                if (resultFile != null) beginInstall(resultFile);
            });
        }, "UntisNext-update-download").start();
    }

    private File downloadApk(String apkUrl) throws Exception {
        File updateDirectory = new File(getCacheDir(), "updates");
        if (!updateDirectory.exists() && !updateDirectory.mkdirs()) {
            throw new IllegalStateException("Update-Ordner konnte nicht angelegt werden.");
        }
        File target = new File(updateDirectory, "UntisNext-update.apk");
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(apkUrl).openConnection();
            connection.setRequestMethod("GET");
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestProperty("User-Agent", "UntisNext-Android-Updater");
            int status = connection.getResponseCode();
            if (status != HttpURLConnection.HTTP_OK) {
                throw new IllegalStateException("Download fehlgeschlagen (HTTP " + status + ").");
            }
            int contentLength = connection.getContentLength();
            if (contentLength > 100 * 1024 * 1024) {
                throw new IllegalStateException("Die APK ist unerwartet groß.");
            }
            long total = 0L;
            try (InputStream input = connection.getInputStream();
                 FileOutputStream output = new FileOutputStream(target)) {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    total += count;
                    if (total > 100L * 1024L * 1024L) {
                        throw new IllegalStateException("Die APK überschreitet die zulässige Downloadgröße.");
                    }
                    output.write(buffer, 0, count);
                }
                output.flush();
            }
            return target;
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private String getInstalledSignerSha256() throws Exception {
        int flags = Build.VERSION.SDK_INT >= 28
            ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
        PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), flags);
        return getSignerSha256(info);
    }

    private String getSignerSha256(PackageInfo info) throws Exception {
        Signature[] signers;
        if (Build.VERSION.SDK_INT >= 28) {
            if (info.signingInfo == null) return "";
            signers = info.signingInfo.getApkContentsSigners();
        } else {
            signers = info.signatures;
        }
        if (signers == null || signers.length == 0) return "";
        return sha256Hex(signers[0].toByteArray());
    }

    private String sha256Hex(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
        }
        return toHex(digest.digest());
    }

    private String sha256Hex(byte[] bytes) throws Exception {
        return toHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private String toHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) result.append(String.format(Locale.US, "%02x", value & 0xff));
        return result.toString();
    }

    private String normalizeDigest(String value) {
        return value == null ? "" : value.replace(":", "").trim().toLowerCase(Locale.ROOT);
    }

    private String readText(InputStream input, int maxBytes) throws Exception {
        try (InputStream source = input; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int total = 0;
            int count;
            while ((count = source.read(buffer)) != -1) {
                total += count;
                if (total > maxBytes) throw new IllegalStateException("Update-Metadaten sind zu groß.");
                output.write(buffer, 0, count);
            }
            return new String(output.toByteArray(), "UTF-8");
        }
    }

    private void beginInstall(File apkFile) {
        if (Build.VERSION.SDK_INT >= 26 && !getPackageManager().canRequestPackageInstalls()) {
            pendingUpdateApk = apkFile;
            try {
                Intent settingsIntent = new Intent(
                    Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + getPackageName())
                );
                startActivityForResult(settingsIntent, UPDATE_PERMISSION_REQUEST);
            } catch (ActivityNotFoundException error) {
                Toast.makeText(this, "Bitte erlaube dieser App in Android die Installation unbekannter Apps.", Toast.LENGTH_LONG).show();
            }
            return;
        }
        launchApkInstaller(apkFile);
    }

    private void launchApkInstaller(File apkFile) {
        try {
            Uri apkUri = FileProvider.getUriForFile(this, getPackageName() + ".fileprovider", apkFile);
            Intent installIntent = new Intent(Intent.ACTION_VIEW);
            installIntent.setDataAndType(apkUri, "application/vnd.android.package-archive");
            installIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivity(installIntent);
        } catch (ActivityNotFoundException | IllegalArgumentException error) {
            Toast.makeText(this, "Der Android-Paketinstaller konnte nicht geöffnet werden.", Toast.LENGTH_LONG).show();
            android.util.Log.e("UntisNextUpdater", "Paketinstallation konnte nicht gestartet werden", error);
        }
    }

    private final class UntisNextAndroidBridge {
        @JavascriptInterface
        public void setThemeIcon(String themeId) {
            if (themeId == null || !THEME_IDS.contains(themeId)) return;
            // Never toggle launcher aliases while the app is visible. Changing the
            // component used to launch an activity can make some Android launchers
            // remove/recreate its task. Save the latest theme; onStop applies it
            // only after the user naturally leaves the app.
            getSharedPreferences(ICON_PREFS, MODE_PRIVATE)
                .edit()
                .putString(PENDING_ICON_KEY, themeId)
                .apply();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        applyPendingLauncherIcon();
    }

    private void applyPendingLauncherIcon() {
        SharedPreferences prefs = getSharedPreferences(ICON_PREFS, MODE_PRIVATE);
        String themeId = prefs.getString(PENDING_ICON_KEY, null);
        if (themeId == null || !THEME_IDS.contains(themeId)) return;
        if (themeId.equals(prefs.getString(ACTIVE_ICON_KEY, "Next"))) {
            prefs.edit().remove(PENDING_ICON_KEY).apply();
            return;
        }
        if (applyLauncherIcon(themeId)) {
            prefs.edit()
                .putString(ACTIVE_ICON_KEY, themeId)
                .remove(PENDING_ICON_KEY)
                .apply();
        }
    }

    private boolean applyLauncherIcon(String themeId) {
        if (themeId == null || !THEME_IDS.contains(themeId)) return false;
        PackageManager manager = getPackageManager();
        // Manifest aliases are resolved against the installed applicationId.
        // Enable the chosen alias before disabling the others so there is never
        // a period with no launcher icon. DONT_KILL_APP avoids killing the app.
        try {
            ComponentName selected = new ComponentName(
                getPackageName(), getPackageName() + ".ThemeIcon" + themeId
            );
            manager.setComponentEnabledSetting(
                selected,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP
            );
            for (String id : THEME_IDS) {
                if (id.equals(themeId)) continue;
                manager.setComponentEnabledSetting(
                    new ComponentName(getPackageName(), getPackageName() + ".ThemeIcon" + id),
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                    PackageManager.DONT_KILL_APP
                );
            }
            return true;
        } catch (IllegalArgumentException error) {
            android.util.Log.w("UntisNext", "Launcher icon switch failed for " + themeId, error);
            return false;
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == UPDATE_PERMISSION_REQUEST) {
            File updateFile = pendingUpdateApk;
            pendingUpdateApk = null;
            if (updateFile != null &&
                (Build.VERSION.SDK_INT < 26 || getPackageManager().canRequestPackageInstalls())) {
                launchApkInstaller(updateFile);
            } else if (updateFile != null) {
                Toast.makeText(this, "Die Installation wurde nicht freigegeben.", Toast.LENGTH_SHORT).show();
            }
            return;
        }
        if (requestCode != FILE_CHOOSER_REQUEST || fileCallback == null) return;
        Uri[] results = null;
        if (resultCode == RESULT_OK && data != null) {
            if (data.getClipData() != null) {
                ClipData clip = data.getClipData();
                int count = Math.min(3, clip.getItemCount());
                results = new Uri[count];
                for (int i = 0; i < count; i++) results[i] = clip.getItemAt(i).getUri();
            } else if (data.getData() != null) {
                results = new Uri[]{data.getData()};
            }
        }
        fileCallback.onReceiveValue(results);
        fileCallback = null;
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        if (fileCallback != null) {
            fileCallback.onReceiveValue(null);
            fileCallback = null;
        }
        if (webView != null) {
            webView.removeJavascriptInterface("UntisNextAndroid");
            webView.destroy();
        }
        super.onDestroy();
    }
}
