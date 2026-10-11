import {
  onUser,
  getAccessControl,
  hasAccess,
  requireLogin
} from "https://hannonuppi.github.io/Tobiservices-Account/auth-core.js";

function showNotice(title, message) {
  document.body.innerHTML =
    '<main style="max-width:700px;margin:10vh auto;padding:28px;border:1px solid #334155;border-radius:18px;background:#111827;color:#f8fafc;font:16px/1.6 system-ui,sans-serif">' +
    '<h1 style="margin-top:0">' + title + '</h1><p>' + message + '</p>' +
    '<a style="color:#7dd3fc" href="https://hannonuppi.github.io/Tobiservices-Account/account.html">Zurück zu TobiServices</a></main>';
  document.body.style.setProperty("visibility", "visible", "important");
}

function blockCompatWrites() {
  if (!window.firebase || !Array.isArray(window.firebase.apps)) return;
  const blocked = function() {
    return Promise.reject(new Error("Test-Mirror: Schreibaktionen sind deaktiviert, damit keine echten Daten verändert werden."));
  };
  const patched = new WeakSet();
  const patch = (prototype, methods) => {
    if (!prototype || patched.has(prototype)) return;
    patched.add(prototype);
    methods.forEach(method => {
      const original = prototype[method];
      if (typeof original !== "function" || original.__testMirrorBlocked) return;
      const replacement = function(...args) {
        if (method === "set" || method === "update" || method === "delete" || method === "add" || method === "runTransaction" || method === "commit") {
          return blocked();
        }
        return original.apply(this, args);
      };
      replacement.__testMirrorBlocked = true;
      try { prototype[method] = replacement; } catch (error) { console.warn("Read-only-Testschutz konnte nicht gesetzt werden:", method, error); }
    });
  };

  window.firebase.apps.forEach(app => {
    try {
      if (typeof app.firestore !== "function") return;
      const db = app.firestore();
      const docRef = db.collection("__test_mode_guard__").doc("__probe__");
      const collectionRef = db.collection("__test_mode_guard__");
      const batch = db.batch();
      patch(Object.getPrototypeOf(docRef), ["set", "update", "delete"]);
      patch(Object.getPrototypeOf(collectionRef), ["add"]);
      patch(Object.getPrototypeOf(db), ["runTransaction"]);
      patch(Object.getPrototypeOf(batch), ["commit"]);
    } catch (error) {
      console.warn("Firestore-Testschutz konnte nicht vollständig initialisiert werden:", error);
    }
  });
}

document.addEventListener("DOMContentLoaded", () => {
  const banner = document.createElement("div");
  banner.style.cssText = "position:sticky;top:0;z-index:100000;padding:10px 14px;text-align:center;background:#f4c84b;color:#211833;font:700 13px/1.4 system-ui,sans-serif;box-shadow:0 2px 12px #0006";
  banner.textContent = "TEST-MIRROR · Nur Vorschau · Schreibaktionen deaktiviert";
  document.body.prepend(banner);
});

blockCompatWrites();
onUser(async (user, profile) => {
  if (!user) {
    requireLogin(location.href);
    return;
  }
  if (!profile || profile.disabled === true) {
    showNotice("Test Mode nicht verfügbar", "Dein Konto ist nicht verfügbar.");
    return;
  }
  let access = null;
  try { access = await getAccessControl(); }
  catch (error) { console.warn("Test-Berechtigungen konnten nicht geladen werden:", error); }
  if (!hasAccess(profile, "test-mode", access)) {
    showNotice("Kein Testzugriff", "Für dieses Konto wurde der Test Mode nicht freigeschaltet.");
    return;
  }
  document.body.style.setProperty("visibility", "visible", "important");
});
