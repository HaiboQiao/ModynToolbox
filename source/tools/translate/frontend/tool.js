import {modyn} from './modyn.js';
const $=id=>document.getElementById(id);let busy=false,translatedInput='',partial=false,saving;
const controls=['source','target','provider','endpoint'];
function input(){return {text:$('input').value,...Object.fromEntries(controls.map(id=>[id,$(id).value]))};}
function status(message,error=false){$('status').textContent=message;$('status').classList.toggle('error',error);}
function render(){$('engineLabel').textContent=$('provider').value==='mymemory'?'公共在线 · MyMemory · 中英互译':'自建服务 · LibreTranslate · 中英互译';const has=!!$('output').value;$('count').textContent=$('input').value.length+' / 8000';$('copy').disabled=!has;$('export').disabled=!has;$('translate').disabled=busy||!$('input').value.trim();$('endpointField').hidden=$('provider').value!=='libretranslate';$('outputHint').textContent=has&&translatedInput!==JSON.stringify(input())?'原文或设置已更新':partial?'部分结果':'';}
function snapshot(){const v=input();return {state:v,input:v};}
function save(){clearTimeout(saving);saving=setTimeout(()=>{const v=snapshot();modyn.checkpoint(v.state,v.input).catch(e=>status(e.message,true));},250);render();}
modyn.setCheckpointProvider(snapshot);
function theme(t){if(t.fontSize)document.documentElement.style.fontSize=t.fontSize+'px';}
function job(j){busy=['preparing','queued','running','cancelling'].includes(j.state);for(const id of ['input',...controls,'swap','clear'])$(id).disabled=busy;$('cancel').disabled=!busy||!j.cancellable;status(j.message||j.state,j.state==='failed');render();}
function result(r){partial=!!r.partial;if(typeof r.translated_text==='string'){$('output').value=r.translated_text;translatedInput=JSON.stringify(input());$('outputHint').textContent=r.partial?'部分结果':'';}status(r.message||'处理完成',r.status==='failed');render();}
modyn.on('bootstrap',b=>{theme(b.theme||{});const v={...(b.input||{}),...(b.state||{})};$('input').value=v.text||'';for(const id of controls)if(v[id]!==undefined)$(id).value=v[id];if(b.result)result(b.result);if(b.job?.state)job(b.job);render();});modyn.on('theme',theme);modyn.on('job',job);modyn.on('result',result);
$('translate').onclick=async()=>{try{status('正在提交…');await modyn.submit('translate',input());}catch(e){status(e.message,true)}};
$('cancel').onclick=()=>modyn.cancel().catch(e=>status(e.message,true));$('clear').onclick=()=>{$('input').value='';save()};
$('copy').onclick=()=>modyn.call('clipboard.write',{text:$('output').value}).catch(e=>status(e.message,true));$('export').onclick=()=>modyn.call('artifacts.save',{text:$('output').value}).catch(e=>status(e.message,true));
$('swap').onclick=()=>{let source=$('source').value==='auto'?(/[\u3400-\u9fff]/.test($('input').value)?'zh':'en'):$('source').value;let target=$('target').value==='auto'?(source==='zh'?'en':'zh'):$('target').value;if($('output').value&&translatedInput===JSON.stringify(input()))$('input').value=$('output').value;$('source').value=target;$('target').value=source;save();};
$('serviceToggle').onclick=()=>{const on=$('service').hidden;$('service').hidden=!on;$('serviceToggle').setAttribute('aria-expanded',String(on));};
document.addEventListener('input',save);document.addEventListener('change',save);document.addEventListener('keydown',e=>{if(e.ctrlKey&&e.key==='Enter'&&!$('translate').disabled){e.preventDefault();$('translate').click();}});
document.addEventListener('wheel',e=>{if(e.target.closest('select')){e.preventDefault();const unit=e.deltaMode===1?20:e.deltaMode===2?innerHeight:1;document.scrollingElement.scrollBy(0,e.deltaY*unit);}},{passive:false,capture:true});
window.translationTool={input,run:()=>$('translate').click()};render();modyn.ready().catch(e=>status(e.message,true));
