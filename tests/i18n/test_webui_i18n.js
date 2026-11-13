// Panel language (webui.html, "i18n begin" .. "i18n end"): every message the
// code builds has a Russian entry with the same {n} slots, lz() fills them,
// and English stays English.
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../../src/webui.html'),'utf8');
const code=html.slice(html.indexOf('// i18n begin'),html.indexOf('// i18n end'));
assert.ok(code.length>1000,'i18n block not found');
function load(lang){const store={'gm.lang':lang};const ctx={localStorage:{getItem:k=>store[k]||null,setItem(){}},navigator:{language:'en-US'}};
 vm.createContext(ctx);vm.runInContext(code+';this.lz=lz;this.RU=RU;this.UI_LANG=UI_LANG',ctx);return ctx}
const ru=load('ru'),en=load('en');
assert.strictEqual(ru.UI_LANG,'ru');assert.strictEqual(en.UI_LANG,'en');
// Every literal handed to lz(), also both sides of a ?: inside it.
const keys=new Set();const lit="'((?:[^'\\\\]|\\\\.)*)'";
for(const m of html.matchAll(new RegExp('lz\\('+lit,'g')))keys.add(m[1]);
for(const m of html.matchAll(new RegExp('lz\\([^()\']*\\?'+lit+':'+lit,'g'))){keys.add(m[1]);keys.add(m[2])}
for(const m of html.matchAll(new RegExp('lz\\([^()\']*\\?'+lit+':[^()\']*\\?'+lit+':'+lit,'g'))){keys.add(m[1]);keys.add(m[2]);keys.add(m[3])}
const missing=[...keys].map(k=>k.replace(/\\'/g,"'")).filter(k=>!(k in ru.RU));
assert.deepStrictEqual(missing,[],'lz() keys without a Russian entry');
assert.ok(keys.size>120,'too few lz() calls found: '+keys.size);
// Same slots in both languages.
for(const [k,v] of Object.entries(ru.RU)){const s=x=>(x.match(/\{\d\}/g)||[]).sort().join();
 assert.strictEqual(s(v),s(k),'slots differ: '+k);assert.ok(v.length>0,'empty: '+k)}
assert.strictEqual(ru.lz('Connected to {0}',['Home']),'Подключено к Home');
assert.strictEqual(en.lz('Connected to {0}',['Home']),'Connected to Home');
assert.strictEqual(ru.lz('Some network name'),'Some network name');   // not in the table: unchanged
assert.strictEqual(ru.lz('{0} installed · {1} KB free',[2,950]),'Установлено: 2 · свободно 950 КБ');
// The code never compares a shown text with an English word.
assert.ok(!/textContent\s*===?\s*'/.test(html),'a translated text is read back');
console.log('i18n tests passed ('+keys.size+' keys)');
