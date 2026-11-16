import './interactions.js';
import {busy,$,modyn,start,run,table,status,context,fileRows} from './helpers.js';
let files=[],planValid=false,lastAction='';
function invalidate(){planValid=false;$('apply').disabled=true;}
function invoke(action){lastAction=action;if(action!=='preview'&&!planValid&&action==='apply')return;if(action!=='apply')invalidate();run(action,input());}const fields=['prefix','suffix','numbering','start','digits'];
function params(){return {prefix:$('prefix').value,suffix:$('suffix').value,numbering:$('numbering').checked,start:Number($('start').value),digits:Number($('digits').value)}}
function input(){return {files:files.map(f=>f.path),params:params(),context};}
function showFiles(){$('files').className=files.length?'scroll':'empty';$('files').replaceChildren(files.length?fileRows(files):document.createTextNode('添加文件后可调整顺序'));}
const save=start((s,i)=>{files=s.files||i.files?.map(path=>({path}))||[];for(const id of fields){let v=s.params?.[id]??i.params?.[id];if(v!==undefined)$(id)[$(id).type==='checkbox'?'checked':'value']=v;}planValid=!!s.planValid;showFiles();},()=>({state:{files,params:params(),planValid},input:input()}),(r,restoring)=>{table(r);planValid=!!r.context?.plan_id&&(restoring?planValid:lastAction==='preview')&&r.status!=='failed';$('apply').disabled=busy||!planValid;});
document.addEventListener('filesChanged',()=>{invalidate();showFiles();save();});
$('add').onclick=async()=>{try{files.push(...(await modyn.pick()).files);invalidate();showFiles();save();}catch(e){status(e.message,true)}};
$('clear').onclick=()=>{files=[];invalidate();showFiles();save()};$('preview').onclick=()=>invoke('preview');$('apply').onclick=()=>invoke('apply');$('undo').onclick=()=>invoke('undo');$('recover').onclick=()=>invoke('recover');
document.querySelector('.settings').addEventListener('input',()=>{invalidate();save();});

document.addEventListener('dragover',e=>{e.preventDefault()});document.addEventListener('drop',async e=>{e.preventDefault();if(busy)return;try{files.push(...(await modyn.drop(e.dataTransfer.files)).files);invalidate();showFiles();save()}catch(err){status(err.message,true)}});

document.addEventListener('toolBusy',()=>{$('apply').disabled=busy||!planValid;});
