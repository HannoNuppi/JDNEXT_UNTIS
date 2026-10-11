import {
  onUser,
  getAccessControl,
  hasAccess,
  requireLogin
} from "https://hannonuppi.github.io/Tobiservices-Account/auth-core.js";

function revealSite() {
  document.body.style.visibility = "visible";
}

function showAccessDenied(message) {
  document.body.innerHTML =
    '<main style="max-width:700px;margin:10vh auto;padding:28px;border:1px solid #334155;border-radius:18px;background:#111827;color:#f8fafc;font:16px/1.6 system-ui,sans-serif">' +
    '<h1 style="margin-top:0">Untis Next</h1><p>' + message + '</p>' +
    '<a style="color:#7dd3fc" href="https://hannonuppi.github.io/Tobiservices-Account/account.html">TobiServices Account</a></main>';
  revealSite();
}

(async () => {
  try {
    const access = await getAccessControl();
    const allowedTags = access.permissions?.["untis-next"] || [];
    if (allowedTags.length === 0) {
      revealSite();
      return;
    }
    onUser((user, profile) => {
      if (!user) {
        requireLogin(location.href);
        return;
      }
      if (!hasAccess(profile, "untis-next", access)) {
        showAccessDenied("Dein TobiServices-Konto besitzt keinen Tag, der für Untis Next freigeschaltet ist.");
        return;
      }
      revealSite();
    });
  } catch (error) {
    console.error("Zentrale Untis-Next-Zugriffsregeln konnten nicht gelesen werden:", error);
    showAccessDenied("Die Zugriffsregeln können gerade nicht geladen werden. Bitte versuche es später erneut.");
  }
})();
