const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { onDocumentCreated } = require("firebase-functions/v2/firestore");
const { defineSecret } = require("firebase-functions/params");
const { onInit } = require("firebase-functions/v2/core");
const { setGlobalOptions } = require("firebase-functions/v2/options");
const { logger } = require("firebase-functions");
const { initializeApp } = require("firebase-admin/app");
const { getFirestore, FieldValue } = require("firebase-admin/firestore");

initializeApp();
const db = getFirestore();

setGlobalOptions({
  region: "europe-west1",
  maxInstances: 10
});

const DISCORD_BOT_TOKEN = defineSecret("DISCORD_BOT_TOKEN");
const TOBI_REWARD_SECRET = defineSecret("TOBI_REWARD_SECRET");
const TOBI_REWARD_URL = "https://europe-west1-tobiservices.cloudfunctions.net/jdnextCoinEvent";
const DISCORD_CHANNEL_ID = "1557736296795865108";
const MAX_IMAGE_BYTES = 7 * 1024 * 1024;
const ALLOWED_MIME = new Set([
  "image/jpeg",
  "image/png",
  "image/webp",
  "image/gif"
]);

// Erstellt beim ersten Backend-Aufruf die Schema-Hinweise in Firestore.
// Der Bot-Token wird bewusst NICHT in Firestore abgelegt.
let setupDocumentsPromise = null;
async function ensureJdnextSetupDocuments() {
  if (setupDocumentsPromise) return setupDocumentsPromise;

  setupDocumentsPromise = (async () => {
    const schemaRef = db.doc("system/schema");
    const discordRef = db.doc("system/discord");

    await db.runTransaction(async transaction => {
      const schemaSnap = await transaction.get(schemaRef);
      const discordSnap = await transaction.get(discordRef);

      if (!schemaSnap.exists) {
        transaction.set(schemaRef, {
          version: 1,
          purpose: "JDNEXT Firestore schema reference",
          createdAt: FieldValue.serverTimestamp(),
          collections: {
            homeworkEntries: {
              documentPath: "homework/{homeworkKey}/entries/{entryId}",
              fields: {
                text: "string, 3-1000 characters",
                author: "string, up to 80 characters",
                images: "array of up to 3 Discord attachment references",
                "images[]": {
                  messageId: "string (Discord message snowflake)",
                  attachmentId: "string (Discord attachment snowflake)",
                  filename: "string"
                },
                createdAt: "timestamp"
              }
            },
            classworkEntries: {
              documentPath: "classwork/{homeworkKey}/entries/{entryId}",
              fields: {
                title: "string, 2-120 characters",
                createdBy: "string (Firebase anonymous UID)",
                createdAt: "timestamp"
              }
            }
          }
        });
      }

      if (!discordSnap.exists) {
        transaction.set(discordRef, {
          provider: "discord",
          channelId: DISCORD_CHANNEL_ID,
          tokenSecretName: "DISCORD_BOT_TOKEN",
          tokenStorage: "Firebase/Google Cloud Secret Manager",
          tokenStoredInFirestore: false,
          instructions: "Set DISCORD_BOT_TOKEN as a Firebase Functions secret; never paste the token into this document.",
          createdAt: FieldValue.serverTimestamp()
        });
      }
    });
  })().catch(error => {
    setupDocumentsPromise = null;
    throw error;
  });

  return setupDocumentsPromise;
}

// Erstinitialisierung nach dem Start der Cloud-Functions-Runtime.
onInit(async () => {
  await ensureJdnextSetupDocuments();
});

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
  await ensureJdnextSetupDocuments();

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
  await ensureJdnextSetupDocuments();

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


// Server-to-server bridge into the shared TobiServices wallet. The shared secret is
// stored only in Firebase Secret Manager, never in client JavaScript or Firestore.
async function callTobiCoinService(payload) {
  const response = await fetch(TOBI_REWARD_URL, {
    method: "POST",
    headers: {
      "content-type": "application/json",
      "x-jdnext-secret": TOBI_REWARD_SECRET.value()
    },
    body: JSON.stringify(payload)
  });
  const result = await response.json().catch(() => ({}));
  if (!response.ok || result.ok !== true) {
    throw new Error(result.error || `TobiServices coin endpoint failed (${response.status})`);
  }
  return result;
}

function validateHomeworkImages(images) {
  if (!Array.isArray(images) || images.length > 3) {
    throw new HttpsError("invalid-argument", "Es sind höchstens drei Bilder erlaubt.");
  }
  for (const image of images) {
    if (!image || typeof image !== "object" ||
        !/^\d{15,25}$/.test(String(image.messageId || "")) ||
        !/^\d{15,25}$/.test(String(image.attachmentId || "")) ||
        typeof image.filename !== "string" || image.filename.length > 120) {
      throw new HttpsError("invalid-argument", "Ein Bildverweis ist ungültig.");
    }
  }
}

exports.createJdnextHomework = onCall({ secrets: [TOBI_REWARD_SECRET] }, async (request) => {
  validateOrigin(request);
  await ensureJdnextSetupDocuments();
  if (!request.auth?.uid) {
    throw new HttpsError("unauthenticated", "Die anonyme JDNEXT-Sitzung ist nicht verfügbar.");
  }

  const hwKey = String(request.data?.hwKey || "");
  const entryId = String(request.data?.entryId || "");
  const text = String(request.data?.text || "").trim();
  const author = String(request.data?.author || "").trim().slice(0, 80);
  const images = Array.isArray(request.data?.images) ? request.data.images : [];

  if (!/^\d{4}-\d{2}-\d{2}_\d{1,2}$/.test(hwKey) ||
      !/^[A-Za-z0-9_-]{8,80}$/.test(entryId)) {
    throw new HttpsError("invalid-argument", "Stundenplan-Slot oder Eintrags-ID ist ungültig.");
  }
  if (text.length < 3 || text.length > 1000) {
    throw new HttpsError("invalid-argument", "Die Hausaufgabe muss 3 bis 1000 Zeichen lang sein.");
  }
  validateHomeworkImages(images);

  let authorTobiUid = "";
  const tobiIdToken = typeof request.data?.tobiIdToken === "string"
    ? request.data.tobiIdToken.slice(0, 10000)
    : "";

  // Invalid/missing Tobi login must never block a normal homework post.
  if (tobiIdToken) {
    try {
      const verified = await callTobiCoinService({ action: "verify", idToken: tobiIdToken });
      authorTobiUid = String(verified.uid || "");
    } catch (error) {
      logger.warn("TobiServices session could not be verified for the optional homework reward.", {
        message: String(error?.message || error)
      });
    }
  }

  const entryRef = db.doc(`homework/${hwKey}/entries/${entryId}`);
  try {
    await entryRef.create({
      text,
      author,
      images: images.slice(0, 3),
      createdAt: FieldValue.serverTimestamp()
    });
  } catch (error) {
    if (String(error.code || "").includes("already-exists") || String(error.code || "") === "6") {
      throw new HttpsError("already-exists", "Dieser Hausaufgabeneintrag existiert bereits.");
    }
    throw error;
  }

  if (authorTobiUid) {
    await db.doc(`homeworkPrivate/${hwKey}/entries/${entryId}`).create({
      authorTobiUid,
      rewardStatus: "pending",
      createdAt: FieldValue.serverTimestamp()
    });
  }

  return { ok: true, rewardQueued: Boolean(authorTobiUid) };
});

exports.rewardJdnextHomeworkAuthor = onDocumentCreated({
  document: "homeworkPrivate/{hwKey}/entries/{entryId}",
  retry: true,
  secrets: [TOBI_REWARD_SECRET]
}, async (event) => {
  const data = event.data?.data();
  if (!data?.authorTobiUid || data.rewardStatus === "done") return;
  const { hwKey, entryId } = event.params;

  await callTobiCoinService({
    action: "reward-post",
    uid: String(data.authorTobiUid),
    eventId: `post-${hwKey}-${entryId}`
  });
  await event.data.ref.set({
    rewardStatus: "done",
    rewardedAt: FieldValue.serverTimestamp()
  }, { merge: true });
});

exports.removeReportedJdnextHomework = onDocumentCreated({
  document: "homework/{hwKey}/entries/{entryId}/reports/{reportUid}",
  retry: true,
  secrets: [TOBI_REWARD_SECRET]
}, async (event) => {
  const { hwKey, entryId } = event.params;
  const entryRef = db.doc(`homework/${hwKey}/entries/${entryId}`);
  const entrySnap = await entryRef.get();
  if (!entrySnap.exists) return;

  const reportsSnap = await entryRef.collection("reports").get();
  if (reportsSnap.size < 2) return;

  const privateRef = db.doc(`homeworkPrivate/${hwKey}/entries/${entryId}`);
  const privateSnap = await privateRef.get();
  const privateData = privateSnap.exists ? (privateSnap.data() || {}) : {};
  const authorTobiUid = String(privateData.authorTobiUid || "");

  if (authorTobiUid && privateData.penaltyApplied !== true) {
    await callTobiCoinService({
      action: "penalty",
      uid: authorTobiUid,
      eventId: `penalty-${hwKey}-${entryId}`
    });
    await privateRef.set({
      penaltyApplied: true,
      penaltyAt: FieldValue.serverTimestamp()
    }, { merge: true });
  }

  await entryRef.delete();
});
