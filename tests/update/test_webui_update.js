// The panel's update check (webui.html, "update begin" .. "update end"): it
// reads latest.json from the ota branch, offers only a newer version, and
// installs only a whole ESP8266 image of the announced size.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync(process.env.WEBUI_HTML||'src/webui.html','utf8');
const i=html.indexOf('// update begin'),k=html.indexOf('// update end');
assert.ok(i>=0&&k>i,'update block not found');
const code=html.slice(i,k);

function page(responses){
 const els={};const get=id=>els[id]||(els[id]={id,hidden:false,textContent:'',href:'',disabled:false,style:{}});
 get('updNew').hidden=true;get('updState').hidden=true;
 ['updNew','updState','updAvail','updTitle','updLink','updBtn','upBtn','upBar','upMsg'].forEach(get);
 const calls=[],sent=[];
 const ctx={$:get,lz:(s,a)=>s.replace(/\{(\d)\}/g,(_,n)=>a[n]),Uint8Array,Blob,Error,String,parseInt,Math,encodeURIComponent,
  updBusy:on=>{get('upBtn').disabled=on;get('updBtn').disabled=on},
  sendFirmware:(b,name)=>sent.push([b,name]),
  fetch:url=>{calls.push(url);const r=responses[url.split('/').pop()];
   if(!r)return Promise.reject(new Error('offline'));
   return Promise.resolve({ok:true,status:200,json:()=>Promise.resolve(r),arrayBuffer:()=>Promise.resolve(r.buffer)})}};
 vm.createContext(ctx);vm.runInContext(code+';this.checkUpdate=checkUpdate;this.updInstall=updInstall;this.verNewer=verNewer;this.OTA_URL=OTA_URL',ctx);
 return {ctx,els,calls,sent};
}
const tick=()=>new Promise(r=>setTimeout(r,0));

(async()=>{
 const p0=page({});
 assert.equal(p0.ctx.verNewer('1.41','1.40'),true);
 assert.equal(p0.ctx.verNewer('1.40','1.40'),false);
 assert.equal(p0.ctx.verNewer('1.39','1.40'),false);
 assert.equal(p0.ctx.verNewer('2.0','1.99'),true);
 assert.match(p0.ctx.OTA_URL,/^https:\/\/raw\.githubusercontent\.com\/[^/]+\/[^/]+\/ota\/$/);

 // Offline or in setup-hotspot mode: no request at all.
 p0.ctx.checkUpdate('1.40',false);assert.equal(p0.calls.length,0);
 // No answer: says so, offers nothing.
 p0.ctx.checkUpdate('1.40',true);await tick();await tick();
 assert.equal(p0.els.updNew.hidden,true);assert.equal(p0.els.updState.textContent,'Could not check for updates');

 // Same version: the latest.
 const latest={version:'1.40',file:'geekmagic-1.40.bin',size:4,notes:'https://example/r'};
 const p1=page({'latest.json':latest});
 p1.ctx.checkUpdate('1.40',true);await tick();await tick();
 assert.equal(p1.els.updNew.hidden,true);assert.equal(p1.els.updState.textContent,'This is the latest version');
 assert.equal(p1.els.updAvail.textContent,'');
 // Asked once per installed version (status refreshes every few seconds).
 p1.ctx.checkUpdate('1.40',true);assert.equal(p1.calls.length,1);

 // Newer: offered, installs a whole image.
 const img=new Uint8Array([0xE9,1,2,3]);
 const p2=page({'latest.json':{...latest,version:'1.41',file:'geekmagic-1.41.bin'},'geekmagic-1.41.bin':img});
 p2.ctx.checkUpdate('1.40',true);await tick();await tick();
 assert.equal(p2.els.updNew.hidden,false);assert.equal(p2.els.updTitle.textContent,'geekmagic 1.41');
 assert.equal(p2.els.updAvail.textContent,' · 1.41 available');assert.equal(p2.els.updLink.href,'https://example/r');
 p2.ctx.updInstall();await tick();await tick();await tick();
 assert.equal(p2.sent.length,1);assert.equal(p2.sent[0][1],'geekmagic-1.41.bin');
 assert.equal(p2.calls[1],p2.ctx.OTA_URL+'geekmagic-1.41.bin');

 // A cut download or something that is not an image: refused.
 for(const bad of [new Uint8Array([0xE9,1,2]),new Uint8Array([0x50,1,2,3])]){
  const p=page({'latest.json':{...latest,version:'1.41',file:'geekmagic-1.41.bin'},'geekmagic-1.41.bin':bad});
  p.ctx.checkUpdate('1.40',true);await tick();await tick();
  p.ctx.updInstall();await tick();await tick();await tick();
  assert.equal(p.sent.length,0);assert.equal(p.els.upMsg.textContent,'Download failed');assert.equal(p.els.updBtn.disabled,false);
 }
 console.log('update tests passed');
})().catch(e=>{console.error(e);process.exit(1)});
