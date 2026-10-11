package de.tobiservices.untisnext;

import android.app.Activity;
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
    private static final Set<String> THEME_IDS = new HashSet<>(Arrays.asList(
        "Next", "Ocean", "Sunset", "Forest", "Candy", "Midnight",
        "Aurora", "Retro", "RoyalRebels", "Bronze", "Silver", "Gold"
    ));
    private WebView webView;
    private ValueCallback<Uri[]> fileCallback;

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
