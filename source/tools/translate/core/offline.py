"""CPU-only model inference. No network, service process or host dependency."""
from pathlib import Path
import os,re,time
# Keep the tool's native allocator bounded inside its Job Object; do not change the user's environment.
os.environ['CT2_USE_MKL']='0'
os.environ['MKL_DISABLE_FAST_MM']='1'
os.environ['CT2_PACKED_GEMM']='0'
_CACHE={}
def translate_local(text,source,target,context=None):
    if context:context.check();context.progress(0,1,'正在加载本地中英模型…')
    root=Path(__file__).resolve().parent/'models'/(source+'_'+target)
    if not (root/'model/model.bin').is_file() or not (root/'sentencepiece.model').is_file():
        raise ValueError('本地模型不完整，请安装包含中英模型的完整翻译工具包；不会自动发送到在线服务')
    try:
        import ctranslate2,sentencepiece
    except (ImportError,OSError) as e:
        raise ValueError('本地推理运行库未能加载，请使用完整Windows x64工具包并检查CPU兼容性') from e
    key=str(root)
    if key not in _CACHE:
        tokenizer=sentencepiece.SentencePieceProcessor(model_file=str(root/'sentencepiece.model'))
        engine=ctranslate2.Translator(str(root/'model'),device='cpu',compute_type='int8',inter_threads=1,intra_threads=min(4,os.cpu_count() or 2))
        _CACHE[key]=(tokenizer,engine)
    tokenizer,engine=_CACHE[key]
    # Keep paragraph boundaries, split at sentence punctuation, then cap subword length.
    parts=re.split(r'(\n+)',text);jobs=[]
    for part in parts:
        if not part:continue
        if part.isspace():jobs.append((part,None));continue
        for sentence in re.findall(r'.+?(?:[。！？!?]+|[.](?=\s|$)|$)',part):
            lead=re.match(r'^\s*',sentence).group();tail=re.search(r'\s*$',sentence).group()
            content=sentence[len(lead):len(sentence)-len(tail) if tail else None]
            tokens=tokenizer.encode(content,out_type=str)
            chunks=[tokens[i:i+240] for i in range(0,len(tokens),240)]
            for i,chunk in enumerate(chunks):jobs.append((lead if i==0 else '',(chunk,tail if i==len(chunks)-1 else '')))
    total=sum(v is not None for _,v in jobs);done=0;output=[]
    for prefix,job in jobs:
        if context:context.check()
        if job is None:output.append(prefix);continue
        tokens,tail=job
        future=engine.translate_batch([tokens],beam_size=4,length_penalty=0.2,replace_unknowns=True,max_input_length=256,max_decoding_length=512,asynchronous=True)[0]
        while not future.done():
            if context:context.check()
            time.sleep(0.02)
        result=future.result()
        value=tokenizer.decode(result.hypotheses[0]).replace('▁',' ').strip()
        if target=='zh':value=re.sub(r'\s+([。，！？；：])',r'\1',value)
        if target=='en' and output and not prefix and output[-1] and not output[-1][-1].isspace():prefix=' '
        output.append(prefix+value+tail);done+=1
        if context:
            context.check();context.progress(done,total,f'本地翻译 {done}/{total} 段')
            context.checkpoint({'translated_text':''.join(output),'source':source,'target':target,'provider':'local','partial':True})
    return {'translated_text':''.join(output),'source':source,'target':target,'provider':'local','partial':False,'message':'本地翻译完成 · 原文未联网发送'}
