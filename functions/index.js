const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { defineSecret } = require("firebase-functions/params");
const { setGlobalOptions } = require("firebase-functions/v2/options");
const { logger } = require("firebase-functions");
const { initializeApp } = require("firebase-admin/app");

initializeApp();

setGlobalOptions({
  region: "europe-west1",
  maxInstances: 10
});

const DISCORD_BOT_TOKEN = defineSecret("DISCORD_BOT_TOKEN");
const DISCORD_CHANNEL_ID = "1557736296795865108";
const MAX_IMAGE_BYTES = 7 * 1024 * 1024;
const ALLOWED_MIME = new Set([
  "image/jpeg",
  "image/png",
  "image/webp",
  "image/gif"
]);

function validateOrigin(request) {
  const origin = String(request.rawRequest?.headers?.origin || "");
  if (
    origin &&
    origin !== "https://hannonuppi.github.io" &&
    !origin.startsWith("http://localhost:")
  ) {
    throw new HttpsError("permission-denied", "Dieser Aufruf ist nicht für JDNEXT freigegeben.");
  }
}

function parseDataUrl(dataUrl) {
  const match = /^data:(image\/(?:jpeg|png|webp|gif));base64,([A-Za-z0-9+/=]+)$/.exec(String(dataUrl || ""));
  if (!match) {
    throw new HttpsError("invalid-argument", "Ungültiges Bildformat.");
  }

  const mime = match[1];
  const base64 = match[2];

  if (!ALLOWED_MIME.has(mime)) {
    throw new HttpsError("invalid-argument", "Dieses Bildformat wird nicht unterstützt.");
  }

  const buffer = Buffer.from(base64, "base64");
  if (!buffer.length || buffer.length > MAX_IMAGE_BYTES) {
    throw new HttpsError("invalid-argument", "Das Bild ist zu groß.");
  }

  return { mime, buffer };
}

function safeFilename(filename, mime) {
  const extension = mime === "image/png"
    ? ".png"
    : mime === "image/webp"
      ? ".webp"
      : mime === "image/gif"
        ? ".gif"
        : ".jpg";

  const base = String(filename || "jdnext-image")
    .replace(/\\/g, "/")
    .split("/").pop()
    .replace(/\.[a-zA-Z0-9]{1,8}$/, "")
    .replace(/[^a-zA-Z0-9_-]+/g, "_")
    .slice(0, 90);

  return (base || "jdnext-image") + extension;
}

async function discordFetch(path, options = {}) {
  const token = DISCORD_BOT_TOKEN.value();
  const response = await fetch("https://discord.com/api/v10" + path, {
    ...options,
    headers: {
      ...(options.headers || {}),
      Authorization: "Bot " + token
    }
  });

  const text = await response.text();
  let data = null;
  try {
    data = text ? JSON.parse(text) : null;
  } catch {}

  if (!response.ok) {
    logger.error("Discord API error", {
      status: response.status,
      path,
      body: text.slice(0, 500)
    });
    throw new HttpsError("unavailable", "Discord konnte die Datei momentan nicht verarbeiten.");
  }

  return data;
}

exports.uploadJdnextImage = onCall({ secrets: [DISCORD_BOT_TOKEN] }, async (request) => {
  validateOrigin(request);

  if (!request.auth?.uid) {
    throw new HttpsError("unauthenticated", "Eine anonyme JDNEXT-Sitzung ist für Bild-Uploads erforderlich.");
  }

  const { mime, buffer } = parseDataUrl(request.data?.dataUrl);
  const filename = safeFilename(request.data?.filename, mime);

  const form = new FormData();
  form.append(
    "payload_json",
    JSON.stringify({
      content: "JDNEXT-Bild",
      allowed_mentions: { parse: [] }
    })
  );
  form.append(
    "files[0]",
    new Blob([buffer], { type: mime }),
    filename
  );

  const message = await discordFetch(
    "/channels/" + DISCORD_CHANNEL_ID + "/messages",
    {
      method: "POST",
      body: form
    }
  );

  const attachment = Array.isArray(message?.attachments)
    ? message.attachments[0]
    : null;

  if (!attachment?.id || !attachment?.url) {
    throw new HttpsError("internal", "Discord hat keinen Bildanhang zurückgegeben.");
  }

  return {
    ok: true,
    channelId: DISCORD_CHANNEL_ID,
    messageId: String(message.id),
    attachmentId: String(attachment.id),
    filename: String(attachment.filename || filename).slice(0, 120)
  };
});

exports.resolveJdnextDiscordImage = onCall({ secrets: [DISCORD_BOT_TOKEN] }, async (request) => {
  validateOrigin(request);

  const messageId = String(request.data?.messageId || "");
  const attachmentId = String(request.data?.attachmentId || "");

  if (!/^\d{15,25}$/.test(messageId) || !/^\d{15,25}$/.test(attachmentId)) {
    throw new HttpsError("invalid-argument", "Ungültige Discord-ID.");
  }

  const message = await discordFetch(
    "/channels/" + DISCORD_CHANNEL_ID + "/messages/" + messageId,
    { method: "GET" }
  );

  if (String(message?.channel_id || "") !== DISCORD_CHANNEL_ID) {
    throw new HttpsError("not-found", "Discord-Bild nicht gefunden.");
  }

  const attachment = Array.isArray(message?.attachments)
    ? message.attachments.find(a => String(a.id) === attachmentId)
    : null;

  if (!attachment?.url ||
      !String(attachment.url).startsWith("https://cdn.discordapp.com/") ||
      !String(attachment.content_type || "").startsWith("image/")) {
    throw new HttpsError("not-found", "Discord-Bild nicht gefunden.");
  }

  return {
    ok: true,
    url: attachment.url
  };
});
