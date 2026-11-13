// Renders one theme frame with the web UI's preview renderer (the "preview"
// block of src/webui.html) and writes it as raw RGB888, for
// test_webui_preview.py to compare with the native renderer.
// node webui_preview.js FONT PACKAGE OUT Y M D h m s wd LANG DATA_JSON
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(process.env.WEBUI_HTML || 'src/webui.html', 'utf8');
const begin = html.indexOf('// preview begin'), end = html.indexOf('// preview end');
if (begin < 0 || end < begin) throw new Error('preview block not found in webui.html');
const ctx = {TextDecoder, Uint8Array, Uint16Array, Uint8ClampedArray, JSON, Math, String, Date, Error, parseInt, parseFloat, isFinite};
vm.createContext(ctx);
vm.runInContext(html.slice(begin, end) + '\nthis.PV = PV;', ctx);
const [fontPath, pkgPath, outPath, Y, M, D, h, m, s, wd, lang, data] = process.argv.slice(2);
const toBuf = p => { const b = fs.readFileSync(p); return b.buffer.slice(b.byteOffset, b.byteOffset + b.length); };
const font = ctx.PV.font(toBuf(fontPath));
const pkg = ctx.PV.pkg(toBuf(pkgPath));
const t = {Y: +Y, M: +M, D: +D, h: +h, m: +m, s: +s, wd: +wd};
const px = ctx.PV.render(pkg, font, t, lang, JSON.parse(data));
const rgba = ctx.PV.rgba(px), out = Buffer.alloc(240 * 240 * 3);
for (let i = 0; i < 240 * 240; i++) { out[i * 3] = rgba[i * 4]; out[i * 3 + 1] = rgba[i * 4 + 1]; out[i * 3 + 2] = rgba[i * 4 + 2]; }
fs.writeFileSync(outPath, out);
