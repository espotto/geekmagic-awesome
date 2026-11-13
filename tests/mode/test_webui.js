// A save sends the chosen mode, and no mode when the stored one is not in
// the list (an app this firmware no longer has): the firmware then keeps the
// stored mode. This runs the page's own collect() on a fake page whose mode
// list behaves like a <select>.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync(process.env.WEBUI_HTML||'src/webui.html','utf8');
const between=(a,b)=>{const i=html.indexOf(a),k=html.indexOf(b,i);assert.ok(i>=0&&k>i,'page text not found: '+a);return html.slice(i,k)};
const helpers=between('function $(id)','// sun begin');
const feat=between('var APPPAGE=','var lastApp=');
const collectCode=between('function collect(){','function saveAll(){');

const OPTIONS=['theme','calendar','reminders','weather','timer','carousel'];

// A <select>: setting a value with no matching option leaves nothing selected.
function makeSelect(){
 const opts=OPTIONS.map(v=>({value:v,remove(){opts.splice(opts.indexOf(this),1)}}));
 let cur=opts[0].value;
 return {id:'mode',opts,
  get value(){return opts.some(o=>o.value===cur)?cur:''},
  set value(v){cur=opts.some(o=>o.value===v)?v:''}};
}

function makePage(){
 const els={};
 const add=(id,props)=>{els[id]=Object.assign({id},props)};
 els.mode=makeSelect();
 add('carouselSec',{value:'45'});add('brightness',{value:'80'});add('rotation',{value:'0'});
 add('hostname',{value:'geekmagic-test'});add('apSsid',{value:'GeekMagic-Test'});
 const document={getElementById:id=>els[id]||null,
  querySelector:sel=>{const m=/^#mode option\[value="(\w+)"\]$/.exec(sel);
   return m?els.mode.opts.find(o=>o.value===m[1])||null:null},
  querySelectorAll:()=>[]};
 const ctx={document,C:{},parseInt,parseFloat,Object,collectWifi:()=>[],toast:()=>{}};
 vm.createContext(ctx);
 vm.runInContext(helpers+feat+collectCode,ctx);
 return {ctx,els};
}

// Full build: the chosen mode is sent, whatever it is.
for(const m of OPTIONS){
 const {ctx,els}=makePage();
 els.mode.value=m;
 assert.strictEqual(ctx.collect().mode,m,'full build, mode '+m);
}

// A stored mode the page does not offer (an app this firmware no longer has):
// nothing is selected, and no mode is sent, so the save keeps the stored one.
for(const m of ['stocks','radar','ha','usage']){
 const {ctx,els}=makePage();
 els.mode.value=m;                       // what loadConfig does with the stored mode
 assert.strictEqual(els.mode.value,'','nothing selected for '+m);
 const o=ctx.collect();
 assert.ok(!('mode' in o),'mode must not be sent for '+m+', got '+JSON.stringify(o.mode));
 assert.strictEqual(o.hostname,'geekmagic-test','the rest is still sent');
 assert.strictEqual(o.carouselSec,45);
}
console.log('mode save page tests passed');
