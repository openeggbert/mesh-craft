// pre.js — runs before Emscripten Module init.
// Mounts IDBFS at /home/user so that mc3.xml files saved by the editor
// persist across page reloads via the browser's IndexedDB.
//
// Virtual filesystem layout at runtime:
//   /test   - preloaded read-only sample mc3.xml files
//   /home/user - IDBFS (persistent; user files written here)

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
    if (!FS.analyzePath('/home').exists)      FS.mkdir('/home');
    if (!FS.analyzePath('/home/user').exists) FS.mkdir('/home/user');
    FS.mount(IDBFS, {}, '/home/user');

    FS.syncfs(true, function (err) {
        if (err) {
            console.warn('IDBFS preRun sync failed:', err);
        }
    });
});

window.addEventListener('beforeunload', function () {
    if (typeof FS !== 'undefined' && typeof IDBFS !== 'undefined') {
        FS.syncfs(false, function (err) {
            if (err) {
                console.warn('IDBFS beforeunload sync failed:', err);
            }
        });
    }
});
