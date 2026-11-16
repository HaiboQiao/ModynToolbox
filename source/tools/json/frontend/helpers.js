import {modyn} from './modyn.js';
export {modyn};export const $=id=>document.getElementById(id);
export let context={};export let lastResult={};export let bootState={};
export function status(text,error=false){if($('status')){$('status').textContent=text;$('status').classList.toggle('error',error);}}
export function table(result,target='results'){const root=$(target);if(!root)return;root.replaceChildren();if(!result.rows?.length){root.textContent=result.message||'暂无结果';return;}const t=document.createElement('table'),head=t.createTHead().insertRow();for(const c of result.columns||[]){const th=document.createElement('th');th.textContent=c.label;head.append(th);}const body=t.createTBody();for(const row of result.rows){const tr=body.insertRow();for(const c of result.columns||[]){tr.insertCell().textContent=String(row[c.key]??'');}}root.append(t);}
export function start(restore,snapshot,renderResult){
 modyn.on('bootstrap',b=>{bootState=b;context=b.result?.context||{};lastResult=b.result||{};restore(b.state||{},b.input||{});if(Object.keys(lastResult).length)renderResult(lastResult);status(b.job?.message||'就绪');});
 modyn.on('result',r=>{lastResult=r;if(r.context)context=r.context;renderResult(r);status(r.message||'处理完成',r.status==='failed');save();});
 modyn.on('job',j=>{status(j.message||j.state,j.state==='failed');const busy=['preparing','queued','running','cancelling'].includes(j.state);document.querySelectorAll('[data-action]').forEach(b=>b.disabled=busy);if($('cancel'))$('cancel').disabled=!busy||!j.cancellable;if($('progress')){$('progress').value=j.done||0;$('progress').max=j.total||1;}});
 let timer;function save(){clearTimeout(timer);timer=setTimeout(()=>{const s=snapshot();modyn.checkpoint(s.state,s.input).catch(e=>status('当前编辑尚未保存：'+e.message,true));},250);}document.addEventListener('input',save);document.addEventListener('change',save);
 if($('cancel'))$('cancel').onclick=()=>modyn.cancel().catch(e=>status(e.message,true));
 modyn.ready().catch(e=>status(e.message,true));return save;
}
export async function run(action,input){try{status('正在提交…');await modyn.submit(action,input);}catch(e){status(e.message,true);}}
export async function pickDir(id){try{const r=await modyn.pick({directory:true});if(r.files[0]){$(id).value=r.files[0].path;$(id).dispatchEvent(new Event('input',{bubbles:true}));}}catch(e){status(e.message,true);}}
export function fileRows(files){const t=document.createElement('table');const h=t.createTHead().insertRow();['文件','操作'].forEach(x=>{const th=document.createElement('th');th.textContent=x;h.append(th)});const b=t.createTBody();files.forEach((f,i)=>{const row=b.insertRow();row.insertCell().textContent=f.name||f.path.split(/[\\/]/).pop();const cell=row.insertCell();for(const [name,n] of [['↑',i-1],['↓',i+1],['移除',-1]]){const button=document.createElement('button');button.textContent=name;button.title=name==='↑'?'上移':name==='↓'?'下移':'移除此文件';button.onclick=()=>{if(n===-1)files.splice(i,1);else if(n>=0&&n<files.length)[files[i],files[n]]=[files[n],files[i]];document.dispatchEvent(new Event('filesChanged'));};cell.append(button);}});return t;}
