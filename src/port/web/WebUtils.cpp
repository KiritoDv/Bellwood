#ifdef __EMSCRIPTEN__
#include "WebUtils.h"

#include <emscripten.h>
#include <cmath>
#include <filesystem>

// clang-format off
EM_JS(void, js_idbfs_mount, (const char* cpath), {
    var path = UTF8ToString(cpath);
    try {
        FS.mkdir(path);
    } catch (e) {
    }
    FS.mount(IDBFS, {}, path);
});

EM_ASYNC_JS(void, js_idbfs_sync, (int populate), {
    await new Promise(function(resolve) {
        FS.syncfs(populate != 0, function(err) {
            if (err) {
                console.error('[WebCache] sync failed:', err);
            }
            resolve();
        });
    });
});

EM_JS(void, js_idbfs_save_nowait, (), {
    FS.syncfs(false, function(err) {
        if (err) {
            console.error('[WebCache] sync failed:', err);
        }
    });
});

EM_ASYNC_JS(int, js_pick_disc, (const char* cdiscDir, const char* carchive, char* errBuf, int errLen), {
    var discDir = UTF8ToString(cdiscDir);
    var archivePath = UTF8ToString(carchive);

    var overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;inset:0;display:flex;align-items:center;justify-content:center;' +
                            'background:rgba(0,0,0,0.65);z-index:1000;font-family:system-ui,sans-serif;';
    var panel = document.createElement('div');
    panel.style.cssText = 'background:#1c1c24;color:#eee;padding:24px 28px;border-radius:12px;text-align:center;' +
                          'max-width:90vw;box-shadow:0 8px 32px rgba(0,0,0,0.5);';
    var label = document.createElement('p');
    label.textContent = 'Choose your Animal Crossing disc image (.iso, .gcm, .ciso, .gcz), ' +
                        'or an ac.o2r generated earlier.';
    label.style.cssText = 'margin:0 0 16px 0;font-size:1rem;line-height:1.4;';
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = '.iso,.gcm,.ciso,.gcz,.o2r';
    input.style.display = 'none';
    var choose = document.createElement('button');
    choose.textContent = 'Choose File';
    choose.style.cssText = 'font-size:1rem;padding:8px 18px;margin:0 6px;cursor:pointer;';
    var cancel = document.createElement('button');
    cancel.textContent = 'Cancel';
    cancel.style.cssText = choose.style.cssText;
    panel.appendChild(label);
    panel.appendChild(input);
    panel.appendChild(choose);
    panel.appendChild(cancel);
    overlay.appendChild(panel);
    document.body.appendChild(overlay);

    var file = await new Promise(function(resolve) {
        input.addEventListener('change', function(evt) { resolve(evt.target.files[0] || null); });
        choose.addEventListener('click', function() { input.click(); });
        cancel.addEventListener('click', function() { resolve(null); });
    });

    if (!file) {
        overlay.remove();
        return 0;
    }

    choose.style.display = 'none';
    cancel.style.display = 'none';
    label.textContent = 'Reading ' + file.name + '...';

    async function bytes(start, end) {
        return new Uint8Array(await file.slice(start, end).arrayBuffer());
    }

    async function openDisc() {
        var head = await bytes(0, 0x20);
        var hv = new DataView(head.buffer);
        if (head[0] == 0x43 && head[1] == 0x49 && head[2] == 0x53 && head[3] == 0x4F) {
            var header = await bytes(0, 0x8000);
            var blockSize = new DataView(header.buffer).getUint32(4, true);
            var map = [];
            var present = 0;
            for (var i = 8; i < 0x8000; i++) {
                map.push(header[i] ? present++ : -1);
            }
            return async function(offset, length) {
                var out = new Uint8Array(length);
                var done = 0;
                while (done < length) {
                    var pos = offset + done;
                    var block = Math.floor(pos / blockSize);
                    var inner = pos % blockSize;
                    var take = Math.min(blockSize - inner, length - done);
                    if (block < map.length && map[block] >= 0) {
                        var start = 0x8000 + map[block] * blockSize + inner;
                        out.set(await bytes(start, start + take), done);
                    }
                    done += take;
                }
                return out;
            };
        }
        if (hv.getUint32(0, true) == 0xB10BC001) {
            var compressedSize = hv.getUint32(8, true) + hv.getUint32(12, true) * 4294967296;
            var gczBlockSize = hv.getUint32(24, true);
            var blockCount = hv.getUint32(28, true);
            var pointers = new DataView((await bytes(0x20, 0x20 + blockCount * 8)).buffer);
            var dataStart = 0x20 + blockCount * 12;
            var cache = new Map();
            function pointer(i) {
                var hi = pointers.getUint32(i * 8 + 4, true);
                return { raw: (hi & 0x80000000) != 0,
                         offset: (hi & 0x7FFFFFFF) * 4294967296 + pointers.getUint32(i * 8, true) };
            }
            async function block(i) {
                if (cache.has(i)) {
                    return cache.get(i);
                }
                var p = pointer(i);
                var end = i + 1 < blockCount ? pointer(i + 1).offset : compressedSize;
                var data = await bytes(dataStart + p.offset, dataStart + end);
                if (!p.raw) {
                    var stream = new Blob([data]).stream().pipeThrough(new DecompressionStream('deflate'));
                    data = new Uint8Array(await new Response(stream).arrayBuffer());
                }
                cache.set(i, data);
                return data;
            }
            return async function(offset, length) {
                var out = new Uint8Array(length);
                var done = 0;
                while (done < length) {
                    var pos = offset + done;
                    var index = Math.floor(pos / gczBlockSize);
                    var inner = pos % gczBlockSize;
                    var take = Math.min(gczBlockSize - inner, length - done);
                    if (index < blockCount) {
                        out.set((await block(index)).subarray(inner, inner + take), done);
                    }
                    done += take;
                }
                return out;
            };
        }
        return async function(offset, length) {
            return bytes(offset, offset + length);
        };
    }

    function removeTree(path) {
        var info = FS.analyzePath(path);
        if (!info.exists) {
            return;
        }
        if (FS.isDir(info.object.mode)) {
            FS.readdir(path).forEach(function(name) {
                if (name != '.' && name != '..') {
                    removeTree(path + '/' + name);
                }
            });
            FS.rmdir(path);
        } else {
            FS.unlink(path);
        }
    }

    async function unpack(read) {
        var boot = await read(0, 0x440);
        var bv = new DataView(boot.buffer);
        if (bv.getUint32(0x1C) != 0xC2339F3D) {
            throw new Error(file.name + ' is not a GameCube disc image.');
        }
        var dolOffset = bv.getUint32(0x420);
        var fstOffset = bv.getUint32(0x424);
        var fstSize = bv.getUint32(0x428);

        var apploaderHead = new DataView((await read(0x2440, 0x20)).buffer);
        var apploaderSize = 0x20 + apploaderHead.getUint32(0x14) + apploaderHead.getUint32(0x18);

        var dolHead = new DataView((await read(dolOffset, 0x100)).buffer);
        var dolSize = 0x100;
        for (var i = 0; i < 18; i++) {
            var sectionOffset = dolHead.getUint32(i * 4);
            var sectionSize = dolHead.getUint32(0x90 + i * 4);
            if (sectionOffset != 0 && sectionOffset + sectionSize > dolSize) {
                dolSize = sectionOffset + sectionSize;
            }
        }

        var fst = await read(fstOffset, fstSize);
        var fv = new DataView(fst.buffer);

        removeTree(discDir);
        FS.mkdirTree(discDir + '/sys');
        FS.mkdirTree(discDir + '/files');
        FS.writeFile(discDir + '/sys/boot.bin', boot);
        FS.writeFile(discDir + '/sys/bi2.bin', await read(0x440, 0x2000));
        FS.writeFile(discDir + '/sys/apploader.img', await read(0x2440, apploaderSize));
        FS.writeFile(discDir + '/sys/main.dol', await read(dolOffset, dolSize));
        FS.writeFile(discDir + '/sys/fst.bin', fst);

        var count = fv.getUint32(8);
        var names = count * 12;
        var decoder = new TextDecoder();
        function nameAt(offset) {
            var end = names + offset;
            while (end < fst.length && fst[end] != 0) {
                end++;
            }
            return decoder.decode(fst.subarray(names + offset, end));
        }

        var stack = [{ end: count, path: discDir + '/files' }];
        for (var entry = 1; entry < count; entry++) {
            while (stack.length > 1 && entry >= stack[stack.length - 1].end) {
                stack.pop();
            }
            var base = entry * 12;
            var name = nameAt(fv.getUint32(base) & 0xFFFFFF);
            var path = stack[stack.length - 1].path + '/' + name;
            if (fst[base] != 0) {
                FS.mkdirTree(path);
                stack.push({ end: fv.getUint32(base + 8), path: path });
            } else {
                label.textContent = 'Reading ' + name + '...';
                FS.writeFile(path, await read(fv.getUint32(base + 4), fv.getUint32(base + 8)));
            }
        }
    }

    try {
        if (file.name.toLowerCase().endsWith('.o2r')) {
            FS.writeFile(archivePath, await bytes(0, file.size));
            overlay.remove();
            return 2;
        }
        await unpack(await openDisc());
        overlay.remove();
        return 1;
    } catch (e) {
        console.error('[WebFilePicker]', e);
        stringToUTF8(String((e && e.message) || e), errBuf, errLen);
        overlay.remove();
        return 3;
    }
});
EM_JS(void, js_install_file_drop, (), {
    if (Module.bellwoodFiles) {
        return;
    }
    Module.bellwoodFiles = [];
    Module.bellwoodFileCount = 0;
    Module.bellwoodStoreFile = function(file) {
        return file.arrayBuffer().then(function(buffer) {
            var dir = '/tmp/web-files/' + (Module.bellwoodFileCount++);
            FS.mkdirTree(dir);
            var path = dir + '/' + file.name;
            FS.writeFile(path, new Uint8Array(buffer));
            Module.bellwoodFiles.push(path);
        }).catch(function(e) {
            console.error('[WebFiles] could not read ' + file.name, e);
        });
    };
    window.addEventListener('dragover', function(e) {
        e.preventDefault();
        if (e.dataTransfer) {
            e.dataTransfer.dropEffect = 'copy';
        }
    });
    window.addEventListener('drop', function(e) {
        e.preventDefault();
        var files = e.dataTransfer ? e.dataTransfer.files : null;
        if (!files) {
            return;
        }
        for (var i = 0; i < files.length; i++) {
            Module.bellwoodStoreFile(files[i]);
        }
    });
});

EM_JS(void, js_pick_files, (const char* ctitle, const char* caccept), {
    var title = UTF8ToString(ctitle);
    var accept = UTF8ToString(caccept);
    if (document.getElementById('bellwood-file-picker')) {
        return;
    }
    var overlay = document.createElement('div');
    overlay.id = 'bellwood-file-picker';
    overlay.style.cssText = 'position:fixed;inset:0;display:flex;align-items:center;justify-content:center;' +
                            'background:rgba(0,0,0,0.65);z-index:1000;font-family:system-ui,sans-serif;';
    var panel = document.createElement('div');
    panel.style.cssText = 'background:#1c1c24;color:#eee;padding:24px 28px;border-radius:12px;text-align:center;' +
                          'max-width:90vw;box-shadow:0 8px 32px rgba(0,0,0,0.5);';
    var label = document.createElement('p');
    label.textContent = title;
    label.style.cssText = 'margin:0 0 16px 0;font-size:1rem;line-height:1.4;';
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = accept;
    input.style.display = 'none';
    var choose = document.createElement('button');
    choose.textContent = 'Choose File';
    choose.style.cssText = 'font-size:1rem;padding:8px 18px;margin:0 6px;cursor:pointer;';
    var cancel = document.createElement('button');
    cancel.textContent = 'Cancel';
    cancel.style.cssText = choose.style.cssText;
    input.addEventListener('change', function(evt) {
        var file = evt.target.files[0];
        overlay.remove();
        if (file) {
            Module.bellwoodStoreFile(file);
        }
    });
    choose.addEventListener('click', function() { input.click(); });
    cancel.addEventListener('click', function() { overlay.remove(); });
    panel.appendChild(label);
    panel.appendChild(input);
    panel.appendChild(choose);
    panel.appendChild(cancel);
    overlay.appendChild(panel);
    document.body.appendChild(overlay);
});

EM_JS(int, js_take_file, (char* buf, int len), {
    var queue = Module.bellwoodFiles;
    if (!queue || queue.length == 0) {
        return 0;
    }
    stringToUTF8(queue.shift(), buf, len);
    return 1;
});
// clang-format on

void WebFiles_Install() {
    js_install_file_drop();
}

void WebFiles_Pick(const char* title, const char* accept) {
    js_pick_files(title, accept);
}

bool WebFiles_Take(std::string& path) {
    char buf[1024] = {};
    if (!js_take_file(buf, sizeof(buf))) {
        return false;
    }
    path = buf;
    return true;
}

void WebCache_Mount(const char* path) {
    static bool mounted = false;
    if (mounted) {
        return;
    }
    js_idbfs_mount(path);
    mounted = true;
}

void WebCache_Load() {
    js_idbfs_sync(1);
}

void WebCache_Save() {
    js_idbfs_sync(0);
}

void WebCache_SaveNoWait() {
    js_idbfs_save_nowait();
}

void WebFrame_Wait(double milliseconds) {
    emscripten_sleep(milliseconds > 0.0 ? (unsigned int)std::lround(milliseconds) : 0);
}

WebPickResult WebFilePicker_PickDisc(const std::string& discDir, const std::string& archivePath, std::string& error) {
    char message[512] = {};
    switch (js_pick_disc(discDir.c_str(), archivePath.c_str(), message, sizeof(message))) {
        case 1:
            return WebPickResult::Disc;
        case 2:
            return WebPickResult::Archive;
        case 3:
            error = message;
            return WebPickResult::Failed;
        default:
            return WebPickResult::Cancelled;
    }
}

#endif
