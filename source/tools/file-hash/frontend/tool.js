import './interactions.js';
import {busy,$,modyn,start,run,table,status,lastResult,fileRows} from './helpers.js';let files=[];
const input=()=>({files:files.map(f=>f.path),params:{algorithm:$('algorithm').value}});
function show(){$('files').replaceChildren(files.length?fileRows(files):document.createTextNode('支持任意普通文件'));$('files').className=files.length?'scroll':'empty';}
const save=start((s,i)=>{files=s.files||i.files?.map(path=>({path}))||[];$('algorithm').value=s.algorithm||i.params?.algorithm||'SHA-256';$('expected').value=s.expected||'';show()},()=>({state:{files,algorithm:$('algorithm').value,expected:$('expected').value},input:input()}),r=>table(r));
document.addEventListener('filesChanged',()=>{show();save()});$('add').onclick=async()=>{try{files.push(...(await modyn.pick()).files);show();save()}catch(e){status(e.message,true)}};$('calculate').onclick=()=>run('hash',input());
$('compare').onclick=()=>{const row=lastResult.rows?.[0]||{};const expected=$('expected').value.trim().toLowerCase();const values=Object.values(row).map(x=>String(x).trim().toLowerCase());$('comparison').textContent=!expected?'请先填写预期校验值':values.includes(expected)?'校验值一致':'校验值不一致，或尚未完成计算';};
function resultText(){return JSON.stringify(lastResult,null,2)}$('copy').onclick=()=>modyn.call('clipboard.write',{text:resultText()});$('save').onclick=()=>modyn.call('artifacts.save',{text:resultText()});

document.addEventListener('dragover',e=>{e.preventDefault()});document.addEventListener('drop',async e=>{e.preventDefault();if(busy)return;try{files.push(...(await modyn.drop(e.dataTransfer.files)).files);show();save()}catch(err){status(err.message,true)}});
