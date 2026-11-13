// A save sends every app's rotation flag as its box says. This runs the
// page's own collect() on a small fake page.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync(process.env.WEBUI_HTML||'src/webui.html','utf8');
const between=(a,b)=>{const i=html.indexOf(a),k=html.indexOf(b,i);assert.ok(i>=0&&k>i,'page text not found: '+a);return html.slice(i,k)};
const helpers=between('function $(id)','// sun begin');
const feat=between('var APPPAGE=','var lastApp=');
const collectCode=between('function collect(){','function saveAll(){');

const FEATS=['theme','calendar','reminders','weather','timer'];
const BOX={theme:'carouselTheme',calendar:'carouselCalendar',reminders:'carouselReminders',weather:'carouselWeather',timer:'carouselTimer'};

// A fake page: elements by id. A feature section holds its carousel box.
function makePage(ticked){
 const els={};
 const add=(id,props)=>{els[id]=Object.assign({id},props)};
 add('mode',{value:'carousel'});add('carouselSec',{value:'45'});
 add('brightness',{value:'80'});add('rotation',{value:'0'});
 add('hostname',{value:'geekmagic-test'});add('apSsid',{value:'GeekMagic-Test'});
 for(const f of FEATS){
  add(f,{remove(){delete els[f]}});
  add(BOX[f],{checked:!!ticked[f],closest(){return {remove(){delete els[BOX[f]]}}}});
 }
 const document={getElementById:id=>els[id]||null,querySelector:()=>null,querySelectorAll:()=>[]};
 const ctx={document,C:{},parseInt,parseFloat,Object,collectWifi:()=>[],toast:()=>{}};
 vm.createContext(ctx);
 vm.runInContext(helpers+feat+collectCode,ctx);
 return {ctx,els};
}

// Build with every feature: each flag is sent as ticked.
{
 const ticked={theme:true,calendar:true,reminders:false,weather:false,timer:true};
 const {ctx}=makePage(ticked);
 const o=ctx.collect();
 for(const f of FEATS)assert.strictEqual(o[BOX[f]],ticked[f],'full build, '+BOX[f]);
 const flipped={theme:false,calendar:false,reminders:true,weather:true,timer:false};
 const p=makePage(flipped);
 const o2=p.ctx.collect();
 for(const f of FEATS)assert.strictEqual(o2[BOX[f]],flipped[f],'full build flipped, '+BOX[f]);
}

console.log('carousel flag tests passed');
