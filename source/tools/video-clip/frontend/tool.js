import './interactions.js';
import {busy,$,modyn,start,run,table,status,context,pickDir} from './helpers.js';let file=null;
const ids=['mode','quality','size','audio_index','remove_audio','filename','output_dir','start_s','end_s'];
function params(){const p={};for(const id of ids)p[id]=$(id).type==='checkbox'?$(id).checked:$(id).value;p.start_ms=Math.round(Number(p.start_s)*1000);p.end_ms=Math.round(Number(p.end_s)*1000);if(p.audio_index==='-1')delete p.audio_index;else p.audio_index=Number(p.audio_index);return p;}
const input=()=>({files:file?[file.path]:[],params:params(),context});
function range(){const media=context.media;if(media){const duration=media.duration_ms/1000;for(const id of ['in-range','out-range'])$(id).max=duration;$('duration').textContent=duration.toFixed(3)+' 秒';const tracks=media.audio_tracks||[];const select=$('audio_index');const current=select.value;select.replaceChildren(new Option('自动选择','-1'));for(const t of tracks)select.add(new Option(t.label||t.language||('音轨 '+t.index),String(t.index)));select.value=current||'-1';}$('in-range').value=$('start_s').value;$('out-range').value=$('end_s').value;}
async function show(){if(!file)return;try{$('player').src=await modyn.url(file.path);$('empty').hidden=true}catch(e){status(e.message,true)}}
const save=start((s,i)=>{file=s.file|| (i.files?.[0]?{path:i.files[0]}:null);for(const id of ids){const v=s.params?.[id];if(v!==undefined)$(id)[$(id).type==='checkbox'?'checked':'value']=v}show();range()},()=>({state:{file,params:params()},input:input()}),r=>{table(r);if(r.context?.media&&Number($('end_s').value)===0)$('end_s').value=(r.context.media.duration_ms/1000).toFixed(3);range();if(r.context?.plan)status(r.context.plan.message||r.message);});
$('add').onclick=async()=>{try{file=(await modyn.pick({filter:'视频 (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mts *.m2ts)'})).files[0]||file;if(!file)return;$('start_s').value=0;$('end_s').value=0;await show();save();run('probe',input())}catch(e){status(e.message,true)}};
$('player').onerror=()=>status('此素材暂不能在浏览器直接预览；仍可通过分析与时间输入裁剪。',true);
$('mark-in').onclick=()=>{$('start_s').value=$('player').currentTime.toFixed(3);range();save()};$('mark-out').onclick=()=>{$('end_s').value=$('player').currentTime.toFixed(3);range();save()};
for(const [a,b] of [['in-range','start_s'],['out-range','end_s']])$(a).oninput=()=>{$(b).value=$(a).value;save()};
$('start_s').onchange=range;$('end_s').onchange=range;$('directory').onclick=()=>pickDir('output_dir');$('prepare').onclick=()=>run('prepare',input());$('export').onclick=()=>run('export',input());
$('mode').onchange=()=>{const fast=$('mode').value==='快速截取';$('quality').disabled=fast;$('size').disabled=fast;};

document.addEventListener('dragover',e=>e.preventDefault());document.addEventListener('drop',async e=>{e.preventDefault();if(busy)return;try{file=(await modyn.drop(e.dataTransfer.files)).files[0]||file;if(!file)return;$('start_s').value=0;$('end_s').value=0;await show();save();run('probe',input())}catch(err){status(err.message,true)}});

document.addEventListener('toolBusy',()=>{const fast=$('mode').value==='快速截取';$('quality').disabled=busy||fast;$('size').disabled=busy||fast;});
