// pre.js — runs before Emscripten Module init.
// Mounts IDBFS at $HOME (Emscripten's default HOME is /home/web_user, see
// libwasi.js) so that anything MeshCraft writes via meshcraftConfigDir()
// (prefs.ini, recent.txt, keybindings.ini, macro.mc3macro — all resolved
// relative to $HOME/.config on non-Windows, including Emscripten) and any
// mc3.xml explicitly saved under $HOME persist across page reloads via the
// browser's IndexedDB. Mounting at a *different* path than the real $HOME
// (e.g. the previous "/home/user", missing the "web_" Emscripten actually
// uses) would silently leave all of the above in a non-persistent, in-memory
// FS instead — the bug this fixes (STAB-0565).
//
// Virtual filesystem layout at runtime:
//   /test          - preloaded read-only sample mc3.xml files
//   /home/web_user - IDBFS (persistent; $HOME, matches meshcraftConfigDir())

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
    if (!FS.analyzePath('/home').exists)           FS.mkdir('/home');
    if (!FS.analyzePath('/home/web_user').exists)  FS.mkdir('/home/web_user');
    FS.mount(IDBFS, {}, '/home/web_user');

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
