/* Based on https://github.com/gzuidhof/coi-serviceworker (MIT license). */
if (typeof window === 'undefined') {
    self.addEventListener('install', () => self.skipWaiting());
    self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));

    self.addEventListener('fetch', function (event) {
        if (event.request.cache === 'only-if-cached' &&
            event.request.mode !== 'same-origin') {
            return;
        }

        event.respondWith(
            fetch(event.request)
                .then(function (response) {
                    if (response.status === 0) return response;

                    const headers = new Headers(response.headers);
                    headers.set('Cross-Origin-Opener-Policy',   'same-origin');
                    headers.set('Cross-Origin-Embedder-Policy', 'require-corp');

                    return new Response(response.body, {
                        status:     response.status,
                        statusText: response.statusText,
                        headers:    headers,
                    });
                })
                .catch((e) => console.error('[coi-sw] fetch error:', e))
        );
    });
} else {
    if (!self.crossOriginIsolated) {
        const swSrc = document.currentScript
            ? document.currentScript.src
            : 'coi-serviceworker.js';

        navigator.serviceWorker.register(swSrc)
            .then(() => {
                if (navigator.serviceWorker.controller) {
                    location.reload();
                } else {
                    navigator.serviceWorker.addEventListener('controllerchange', () => {
                        location.reload();
                    });
                }
            })
            .catch((err) => {
                console.error('[coi-sw] Registration failed:', err);
            });
    }
}
