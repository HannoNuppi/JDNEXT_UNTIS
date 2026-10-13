const CACHE_NAME = "untis-next-shell-v21";
const APP_SHELL = [
  "./",
  "./index.html",
  "./manifest.webmanifest",
  "./icons/icon-192.png",
  "./icons/icon-512.png"
];

self.addEventListener("install", event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(APP_SHELL))
  );
  self.skipWaiting();
});

self.addEventListener("activate", event => {
  event.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(key => key !== CACHE_NAME).map(key => caches.delete(key)))
    )
  );
  self.clients.claim();
});

self.addEventListener("fetch", event => {
  if (event.request.method !== "GET") return;

  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return; // Firebase und CDN-Daten bleiben live.

  if (event.request.mode === "navigate") {
    // Stale-while-revalidate: open a cached page immediately, then refresh its cache.
    // On first install (or a cache miss), wait for the network and keep offline fallback.
    const update = fetch(event.request).then(response => {
      if (response && response.ok && response.type === "basic") {
        const copy = response.clone();
        // Keep the network response even if writing it to Cache Storage fails.
        return caches.open(CACHE_NAME)
          .then(cache => cache.put(event.request, copy))
          .catch(error => console.warn("Navigation cache update failed:", error))
          .then(() => response);
      }
      return response;
    }).catch(() => null);

    event.waitUntil(update.then(() => undefined));
    event.respondWith(
      caches.match(event.request).then(cached =>
        cached || update.then(response =>
          response || caches.match("./index.html")
        )
      )
    );
    return;
  }

  // App-shell assets are immutable for this cache version and load cache-first.
  event.respondWith(
    caches.match(event.request).then(cached =>
      cached || fetch(event.request)
    )
  );
});
