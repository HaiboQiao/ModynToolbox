import './interactions.js';
import {busy,$,modyn,start,run,table,status,pickDir,fileRows} from './helpers.js';let files=[];
const params=()=>({format:$('format').value,quality:Number($('quality').value),longest_edge:Number($('longest_edge').value),output_dir:$('output_dir').value});const input=()=>({files:files.map(f=>f.path),params:params()});
async function show(){$('files').replaceChildren(files.length?fileRows(files):document.createTextNode('添加图片后显示预览'));$('files').className=files.length?'scroll':'empty';if(files[0]){try{$('before').src=await modyn.url(files[0].path)}catch{}}}
const save=start((s,i)=>{files=s.files||i.files?.map(path=>({path}))||[];for(const [id,v] of Object.entries(s.params||i.params||{}))if($(id))$(id).value=v;show()},()=>({state:{files,params:params()},input:input()}),async r=>{table(r);const p=r.artifacts?.[0]?.path||r.rows?.find(x=>x.output)?.output;if(p)try{$('after').src=await modyn.url(p)}catch(e){status(e.message,true)}});
document.addEventListener('filesChanged',()=>{show();save()});$('add').onclick=async()=>{try{files.push(...(await modyn.pick({filter:'图片 (*.jpg *.jpeg *.png *.webp)'})).files);show();save()}catch(e){status(e.message,true)}};$('quality').oninput=()=>$('qvalue').textContent=$('quality').value;$('format').onchange=()=>$('quality').disabled=$('format').value==='PNG';$('directory').onclick=()=>pickDir('output_dir');$('compress').onclick=()=>run('compress',input());

document.addEventListener('dragover',e=>{e.preventDefault()});document.addEventListener('drop',async e=>{e.preventDefault();if(busy)return;try{files.push(...(await modyn.drop(e.dataTransfer.files)).files);show();save()}catch(err){status(err.message,true)}});

document.addEventListener('toolBusy',()=>{$('quality').disabled=busy||$('format').value==='PNG';});
