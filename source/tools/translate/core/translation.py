"""Independent provider adapters. No host/UI dependency, no automatic text transmission."""
import html
import json
import os
import re
import urllib.request
import urllib.parse
import urllib.error

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError('服务重定向已停止，请填写最终服务地址')

def endpoint(value):
    url=urllib.parse.urlsplit(value.strip())
    if url.scheme not in ('http','https') or not url.hostname or url.username or url.password or url.query or url.fragment:
        raise ValueError('服务地址需为不含账号、查询参数或片段的 HTTP(S) 地址')
    if url.scheme=='http' and url.hostname not in ('localhost','127.0.0.1','::1'):
        raise ValueError('远程服务请使用 HTTPS；HTTP 仅用于本机服务')
    return urllib.parse.urlunsplit(url).rstrip('/')

def segments(text, budget):
    """Preserve newline/whitespace and enforce the provider's UTF-8 byte limit."""
    result=[]
    for part in re.split(r'(\n+)',text):
        if not part:continue
        if part.isspace():result.append((part,False));continue
        while part:
            size=0;end=0;boundary=0
            for i,ch in enumerate(part):
                n=len(ch.encode('utf-8'))
                if size+n>budget:break
                size+=n;end=i+1
                if ch in ' 。！？.!?;；，,':boundary=end
            if not end:raise ValueError('字符超出服务限制')
            if end<len(part) and boundary>end//2:end=boundary
            result.append((part[:end],True));part=part[end:]
    return result

def api_key():
    value=os.environ.get('MODYN_TRANSLATE_API_KEY','')
    if value:return value
    # Worker environments may omit custom variables. Read only this explicitly named user setting.
    if os.name=='nt':
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,'Environment') as key:
                value,kind=winreg.QueryValueEx(key,'MODYN_TRANSLATE_API_KEY')
            return value if isinstance(value,str) else ''
        except OSError:pass
    return ''

def request_json(url, payload=None):
    body=None if payload is None else json.dumps(payload,ensure_ascii=False).encode('utf-8')
    req=urllib.request.Request(url,data=body,headers={'Accept':'application/json','Content-Type':'application/json','User-Agent':'ModynToolbox-Translate/1.0'})
    try:
        with urllib.request.build_opener(NoRedirect).open(req,timeout=20) as response:
            raw=response.read(1024*1024+1)
        if len(raw)>1024*1024:raise ValueError('服务返回内容过大')
        value=json.loads(raw.decode('utf-8'))
        if not isinstance(value,dict):raise ValueError('翻译服务返回格式不正确')
        return value
    except urllib.error.HTTPError as error:
        messages={400:'服务不接受此请求，请检查语言与服务配置',401:'服务需要有效 API Key',403:'服务拒绝请求，请检查密钥或配额',429:'翻译服务额度不足或请求过于频繁'}
        raise ValueError(messages.get(error.code,f'翻译服务暂不可用（HTTP {error.code}）')) from None
    except (urllib.error.URLError,TimeoutError,OSError):
        raise ValueError('无法连接翻译服务或连接超时，请检查地址与网络') from None
    except (UnicodeError,json.JSONDecodeError):
        raise ValueError('服务没有返回有效的 UTF-8 JSON') from None

def translate(data,context=None,transport=request_json):
    text=data.get('text','')
    if not isinstance(text,str) or not text.strip():raise ValueError('请输入需要翻译的文本')
    if len(text)>8000:raise ValueError('单次最多 8000 字符，请分批翻译')
    source=data.get('source','auto');target=data.get('target','auto');provider=data.get('provider','mymemory')
    if source not in ('auto','zh','en') or target not in ('auto','zh','en'):raise ValueError('第一版支持中文与英文')
    if source=='auto':source='zh' if re.search(r'[\u3400-\u9fff]',text) else 'en'
    if target=='auto':target='en' if source=='zh' else 'zh'
    if provider not in ('mymemory','libretranslate'):raise ValueError('未知翻译服务')
    if source==target:return {'translated_text':text,'source':source,'target':target,'provider':provider,'message':'源语言与目标语言相同，已保留原文'}
    base=endpoint(data.get('endpoint','http://127.0.0.1:5000')) if provider=='libretranslate' else ''
    parts=segments(text,500 if provider=='mymemory' else 4000)
    count=sum(send for _,send in parts)
    if count>40:raise ValueError('分段数量超过单次限制，请缩小文本范围')
    output=[];completed=0
    for part,send in parts:
        if context:context.check()
        if not send:output.append(part);continue
        lead=re.match(r'^\s*',part).group();tail=re.search(r'\s*$',part).group();content=part[len(lead):len(part)-len(tail) if tail else None]
        if provider=='mymemory':
            pair=('zh-CN' if source=='zh' else 'en')+'|'+('zh-CN' if target=='zh' else 'en')
            reply=transport('https://api.mymemory.translated.net/get?'+urllib.parse.urlencode({'q':content,'langpair':pair}))
            if str(reply.get('responseStatus',200))!='200' or reply.get('quotaFinished'):
                raise ValueError('公共翻译服务额度不足或拒绝请求，请稍后重试或切换自建服务')
            value=reply.get('responseData',{}).get('translatedText')
            if not isinstance(value,str):raise ValueError('公共翻译服务返回内容不完整')
            value=html.unescape(value)
        else:
            payload={'q':content,'source':source,'target':target,'format':'text'}
            key=api_key()
            if key:payload['api_key']=key
            reply=transport(base+'/translate',payload)
            if 'error' in reply:raise ValueError('自建服务返回错误，请检查服务状态、语言模型或密钥')
            value=reply.get('translatedText')
            if not isinstance(value,str):raise ValueError('自建翻译服务返回内容不完整')
        if len(value)>100000 or sum(map(len,output))+len(value)>100000:raise ValueError('服务返回译文过长，已停止处理')
        if context:context.check()
        output.append(lead+value+tail);completed+=1
        if context:
            context.progress(completed,count,f'已翻译 {completed}/{count} 段')
            context.checkpoint({'translated_text':''.join(output),'source':source,'target':target,'provider':provider,'partial':True})
    return {'translated_text':''.join(output),'source':source,'target':target,'provider':provider,'partial':False,'message':'翻译完成'}
