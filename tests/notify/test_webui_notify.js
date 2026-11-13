// The Display tab's notification hold time: setNotifySec() selects the stored
// value, adding an option for one the list does not offer (set over the API),
// and collect() sends it as display.notifySec.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync(process.env.WEBUI_HTML||'src/webui.html','utf8');
const between=(a,b)=>{const i=html.indexOf(a),k=html.indexOf(b,i);assert.ok(i>=0&&k>i,'page text not found: '+a);return html.slice(i,k)};
const setCode=between('function setNotifySec(','function resetColors(){');
const display=between('display:{colorOrder:',"notifySec:parseInt(gv('notifySec'))||0}")+"notifySec:parseInt(gv('notifySec'))||0}";

// A <select>: a value with no matching option selects nothing.
function makeSelect(values){
 const options=values.map(v=>({value:v,textContent:v}));
 let cur=values[0];
 return {options,
  appendChild(o){options.push(o)},
  get value(){return options.some(o=>o.value===cur)?cur:''},
  set value(v){cur=options.some(o=>o.value===v)?v:''}};
}
const sel=makeSelect(['0','5','10','15','20','30','45','60','90','120']);
const ctx={lz:(s,a)=>a?s.replace(/\{(\d)\}/g,(_,i)=>a[i]):s,$:id=>id==='notifySec'?sel:null,document:{createElement:()=>({value:'',textContent:''})},String};
vm.createContext(ctx);vm.runInContext(setCode,ctx);

ctx.setNotifySec(30);assert.equal(sel.value,'30');
ctx.setNotifySec(undefined);assert.equal(sel.value,'0');   // an older firmware sends nothing
ctx.setNotifySec(0);assert.equal(sel.value,'0');
const before=sel.options.length;
ctx.setNotifySec(25);assert.equal(sel.value,'25');assert.equal(sel.options.length,before+1);
assert.equal(sel.options.at(-1).textContent,'25 seconds');
ctx.setNotifySec(25);assert.equal(sel.options.length,before+1);   // added once

// collect(): the display object carries the selection as a number.
const values={colorOrder:'auto',rGain:'100',gGain:'100',bGain:'100',notifySec:'45'};
const c2={gv:id=>values[id]||'',gc:()=>false,parseInt};
vm.createContext(c2);
assert.equal(vm.runInContext('({'+display+'})',c2).display.notifySec,45);
values.notifySec='0';
assert.equal(vm.runInContext('({'+display+'})',c2).display.notifySec,0);
console.log('notify web UI tests passed');
