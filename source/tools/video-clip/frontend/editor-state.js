export function parseTime(value){const s=String(value).trim();if(/^\d+(?:\.\d{1,3})?$/.test(s))return Number(s);const m=s.match(/^(\d{1,3}):([0-5]\d):([0-5]\d)(?:\.(\d{1,3}))?$/);return m?Number(m[1])*3600+Number(m[2])*60+Number(m[3])+Number((m[4]||'').padEnd(3,'0'))/1000:NaN;}
export function clock(seconds){const n=Math.max(0,Math.round((Number(seconds)||0)*1000));return [Math.floor(n/3600000),Math.floor(n/60000)%60,Math.floor(n/1000)%60].map(x=>String(x).padStart(2,'0')).join(':')+'.'+String(n%1000).padStart(3,'0');}
const clone=x=>JSON.parse(JSON.stringify(x));
export class ClipEditor{
 constructor(){this.clips=[this.make(0,0)];this.active=this.clips[0].id;this.undoStack=[];this.redoStack=[];this.before=null;}
 make(start,end,name){return {id:crypto.randomUUID(),name:name||'片段 '+(this.clips?.length+1||1),start,end,selected:true};}
 get current(){return this.clips.find(c=>c.id===this.active)||this.clips[0];}
 snapshot(){return {clips:clone(this.clips),active:this.active};}
 restore(s){if(!Array.isArray(s.clips)||s.clips.length<1||s.clips.length>100)throw Error('片段列表无效');const ids=new Set();for(const c of s.clips){if(typeof c.id!=='string'||ids.has(c.id)||!Number.isFinite(c.start)||!Number.isFinite(c.end)||c.start<0||c.end<c.start)throw Error('片段数据无效，已保留原草稿');ids.add(c.id);}this.clips=clone(s.clips).map(c=>({...c,name:String(c.name||'片段').slice(0,60),selected:c.selected!==false}));this.active=s.active;}
 remember(before){if(JSON.stringify(before)===JSON.stringify(this.snapshot()))return;this.undoStack.push(before);if(this.undoStack.length>100)this.undoStack.shift();this.redoStack=[];}
 edit(fn){const s=this.snapshot();fn();this.remember(s);}
 begin(){this.before=this.snapshot();}
 end(){if(this.before)this.remember(this.before);this.before=null;}
 range(start,end,history=true){const fn=()=>Object.assign(this.current,{start,end});history?this.edit(fn):fn();}
 undo(){if(!this.undoStack.length)return;this.redoStack.push(this.snapshot());this.restore(this.undoStack.pop());}
 redo(){if(!this.redoStack.length)return;this.undoStack.push(this.snapshot());this.restore(this.redoStack.pop());}
 add(){if(this.clips.length>=100)throw Error('最多100个片段');this.edit(()=>{const c=this.make(this.current.start,this.current.end);this.clips.push(c);this.active=c.id;});}
 remove(id){if(this.clips.length===1)throw Error('至少保留一个片段');this.edit(()=>{this.clips=this.clips.filter(c=>c.id!==id);if(this.active===id)this.active=this.clips[0].id;});}
 split(at){const c=this.current;if(!(at>c.start&&at<c.end))throw Error('播放头须位于当前片段内部');if(this.clips.length>=100)throw Error('最多100个片段');this.edit(()=>{const tail=this.make(at,c.end,c.name+' · 后半');c.end=at;this.clips.splice(this.clips.indexOf(c)+1,0,tail);this.active=tail.id;});}
 segment(duration,length){if(!(length>0)||Math.ceil(duration/length)>100)throw Error('分段时长须大于0，片段数不能超过100');this.edit(()=>{this.clips=[];for(let s=0;s<duration;s+=length)this.clips.push(this.make(s,Math.min(duration,s+length)));this.active=this.clips[0].id;});}
}
