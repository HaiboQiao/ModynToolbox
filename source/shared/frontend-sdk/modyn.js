// Transport-only SDK. It owns no business controls, styles, or layout.
const pending=new Map(), listeners=new Map();let lastSequence=0,recovering=false;let serial=0,boot=null,checkpointProvider=()=>({state:{},input:{}});
const packets=new Map();
function emit(type,payload){for(const f of listeners.get(type)||[])f(payload);}
function receive(m){
 if(m.packet){let p=packets.get(m.packet);if(!p){p={parts:new Array(m.total),received:0};packets.set(m.packet,p);setTimeout(()=>packets.delete(m.packet),60000);}if(!p.parts[m.index]){p.parts[m.index]=m.data;p.received++;}if(p.received===p.parts.length){packets.delete(m.packet);const value=JSON.parse(p.parts.join(''));receive(value);}return;}

 if(m.requestId){const p=pending.get(m.requestId);if(!p)return;clearTimeout(p.timer);pending.delete(m.requestId);m.error?p.reject(new Error(m.error)):p.resolve(m.result);}
 else if(m.event){const seq=Number(m.sequence||0);if(lastSequence&&seq!==lastSequence+1&&!recovering){recovering=true;modyn.query().then(r=>{emit('job',r.job||{});if(r.result)emit('result',r.result)}).finally(()=>recovering=false);}if(seq)lastSequence=seq;if(m.event==='checkpoint-request'){Promise.resolve(checkpointProvider()).then(p=>modyn.call('session.checkpoint',{...p,nonce:m.payload.nonce})).catch(()=>{});return;}if(m.event==='bootstrap')boot=m.payload;emit(m.event,m.payload);}
}
const channel=globalThis.chrome?.webview;channel?.addEventListener('message',({data})=>receive(data));
export const modyn={
 async call(method,payload={}){
  const text=JSON.stringify(payload),bytes=new TextEncoder().encode(text);
  if(bytes.length>190*1024){const {token}=await this.raw('payload.begin',{bytes:bytes.length});for(let i=0;i<bytes.length;i+=65536){const part=bytes.subarray(i,i+65536);let binary='';for(const byte of part)binary+=String.fromCharCode(byte);await this.raw('payload.append',{token,base64:btoa(binary)});}return this.raw('payload.dispatch',{token,method});}
  return this.raw(method,payload);
 },
 raw(method,payload={},objects=[]){if(!channel)return Promise.reject(new Error('请通过工具箱或开发桥模拟器运行'));
  const requestId=String(++serial);return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{pending.delete(requestId);reject(new Error('宿主请求超时，可查询作业状态'))},120000);pending.set(requestId,{resolve,reject,timer});const message={bridgeVersion:1,requestId,method,payload};if(objects.length)channel.postMessageWithAdditionalObjects(message,objects);else channel.postMessage(message);});},
 on(type,fn){if(!listeners.has(type))listeners.set(type,new Set());listeners.get(type).add(fn);if(type==='bootstrap'&&boot)queueMicrotask(()=>fn(boot));return ()=>listeners.get(type)?.delete(fn);},
 setCheckpointProvider(fn){checkpointProvider=fn;},
 ready(){return this.call('ready');},
 submit(action,input,key=crypto.randomUUID()){return this.call('jobs.submit',{action,input,key});},
 checkpoint(state,input={}){return this.call('session.checkpoint',{state,input});},
 drop(files){return this.raw("files.drop",{},Array.from(files));},
 pick(options={}){return this.call('files.pick',options);},
 url(path){return this.call('files.url',{path}).then(x=>x.url);},
 cancel(){return this.call('jobs.cancel');},
 query(){return this.call('jobs.query');}
};

document.addEventListener('input',()=>{modyn.call('session.dirty').catch(()=>{});},true);
