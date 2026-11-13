// Exercise the real theme UI functions with a small DOM adapter, no browser dependency.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync('src/webui.html','utf8');
const code=html.slice(html.indexOf('function themeRequest('),html.indexOf('function upload(){'));
assert.ok(html.includes('id="faceList"')&&html.includes('id="faceSheet"'),'gallery markup present');
const jsonHelper=html.slice(html.indexOf('function j('),html.indexOf('// tabs'));
// A minimal DOM: elements with children, text, classes, ids and handlers.
// Canvases have no getContext, so previews are skipped (the renderer has its
// own test, test_webui_preview.py).
function element(tag){const e={tagName:tag,children:[],className:'',id:'',disabled:false,style:{},attrs:{},
 appendChild(child){this.children.push(child);return child},setAttribute(k,v){this.attrs[k]=v},
 get textContent(){return this._text||''},set textContent(v){this._text=v;this.children=[]}};return e}
function walk(e,f){if(!e)return;f(e);(e.children||[]).forEach(c=>walk(c,f))}
function find(root,test){let hit=null;walk(root,e=>{if(!hit&&test(e))hit=e});return hit}
const nodes={};
for(const id of ['faceList','faceDetail','themeStatus','themeFile','mode'])nodes[id]=element('div');
let response,requests=[],toast='',confirmAnswer=true;
const context={lz:(s,a)=>a?s.replace(/\{(\d)\}/g,(_,i)=>a[i]):s,
 $:id=>nodes[id],C:{mode:'theme',clock:{lang:'en'}},Promise,JSON,Math,FormData:class {},
 document:{createElement:element},j:async()=>response,confirm:()=>confirmAnswer,
 fetch:async(path,options)=>{if(!options)return {ok:true,arrayBuffer:async()=>new ArrayBuffer(0)};
  requests.push([path,JSON.parse(options.body)]);return {ok:true,json:async()=>({id:'broken'})}},
 toast:message=>toast=message,sv:(id,value)=>nodes[id].value=value,modeChanged:()=>{},renderApps:()=>{},
 PV:{font:()=>[],pkg:()=>({theme:{}}),render:()=>[],rgba:()=>[],sample:()=>({})},encodeURIComponent
};
vm.createContext(context);vm.runInContext(code,context);
const tick=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 const helperContext={lz:(s,a)=>a?s.replace(/\{(\d)\}/g,(_,i)=>a[i]):s,fetch:async()=>({ok:false,status:500,json:async()=>({error:'could not save settings'})})};
 vm.createContext(helperContext);vm.runInContext(jsonHelper,helperContext);
 await assert.rejects(helperContext.j('/api/config'),/could not save settings/);
 response={selected:'broken',freeBytes:65536,themes:[
  {id:'healthy',name:'Clock',author:'Me',version:'1',bytes:100,valid:true},
  {id:'broken',name:'broken',bytes:1234,valid:false,error:'Invalid or truncated package'}]};
 await context.loadThemes();
 // One tile per face plus the Add tile; the selected face is the one in focus.
 assert.equal(nodes.faceList.children.length,3);
 assert.ok(find(nodes.faceList.children[1],e=>e.textContent==='Invalid package'));
 assert.ok(find(nodes.faceList.children[2],e=>e.textContent==='Add face'));
 assert.match(nodes.themeStatus.textContent,/2 installed · 64 KB free/);
 let use=find(nodes.faceDetail,e=>e.id==='themeUseBtn');
 assert.equal(use.disabled,true);
 assert.ok(find(nodes.faceDetail,e=>/Invalid or truncated package/.test(e.textContent)));
 await context.selectTheme('broken');assert.equal(requests.length,0);assert.match(toast,/valid installed theme/);
 // Removing asks first; declining sends nothing.
 confirmAnswer=false;context.deleteTheme('broken');await tick();assert.equal(requests.length,0);
 confirmAnswer=true;context.deleteTheme('broken');await tick();
 assert.equal(requests[0][0],'/api/themes/delete');assert.equal(requests[0][1].id,'broken');
 // Tapping a tile focuses it; a valid face that is not on the device can be used.
 nodes.faceList.children[0].onclick();
 use=find(nodes.faceDetail,e=>e.id==='themeUseBtn');assert.equal(use.disabled,false);assert.equal(use.textContent,'Use on device');
 assert.ok(find(nodes.faceList.children[0],e=>/sel/.test(e.className))||/sel/.test(nodes.faceList.children[0].className));
 response={selected:'',freeBytes:65536,themes:[]};await context.loadThemes();
 assert.equal(nodes.faceList.children.length,1);   // only the Add tile
 assert.ok(find(nodes.faceDetail,e=>/No watch faces yet/.test(e.textContent)));
 // A save the device refuses (or never answers) must say so: the Save
 // buttons used to show nothing when the request failed.
 const saveCode=html.slice(html.indexOf('function saveAll('),html.indexOf('// wifi networks'));
 let saveToast='';
 const saveContext={lz:(s,a)=>a?s.replace(/\{(\d)\}/g,(_,i)=>a[i]):s,j:async()=>{throw new Error('not enough memory')},collect:()=>({}),collectWifi:()=>[],
  toast:message=>saveToast=message,loadStatus:()=>{},setTimeout,JSON,
  authProblem:()=>'',gv:()=>''};   // saveAll() checks the password fields first
 vm.createContext(saveContext);vm.runInContext(saveCode,saveContext);
 for(const save of ['saveAll','saveWifi']){
  saveToast='';saveContext[save]();await new Promise(resolve=>setImmediate(resolve));
  assert.equal(saveToast,'Save failed: not enough memory',save);
 }
 console.log('theme web UI tests passed');
})().catch(error=>{console.error(error);process.exitCode=1});
