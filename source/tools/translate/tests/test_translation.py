from pathlib import Path
import sys,unittest,urllib.parse,os,threading,json
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'core'))
from translation import translate,segments,endpoint,request_json

class TranslationTests(unittest.TestCase):
    def test_unicode_segments_and_whitespace(self):
        text='你好世界。'*180+'\n\n'+'hello world '*80
        parts=segments(text,500)
        self.assertEqual(''.join(p for p,_ in parts),text)
        self.assertTrue(all(len(p.encode('utf-8'))<=500 for p,send in parts if send))
    def test_automatic_both_directions(self):
        calls=[]
        def service(url,payload=None):
            pair=urllib.parse.parse_qs(urllib.parse.urlsplit(url).query)['langpair'][0];calls.append(pair)
            return {'responseStatus':200,'responseData':{'translatedText':'你好' if pair.startswith('en|') else 'Hello'}}
        self.assertEqual(translate({'text':'Hello'},transport=service)['translated_text'],'你好')
        self.assertEqual(translate({'text':'你好'},transport=service)['translated_text'],'Hello')
        self.assertEqual(calls,['en|zh-CN','zh-CN|en'])
    def test_libre_key_only_from_environment(self):
        seen=[]
        def service(url,payload):seen.append((url,payload));return {'translatedText':'你好'}
        with patch.dict(os.environ,{'MODYN_TRANSLATE_API_KEY':'private-test-key'}):
            result=translate({'text':'Hello','provider':'libretranslate','endpoint':'http://127.0.0.1:5000'},transport=service)
        self.assertEqual(seen[0][0],'http://127.0.0.1:5000/translate')
        self.assertEqual(seen[0][1]['api_key'],'private-test-key')
        self.assertNotIn('private-test-key',json.dumps(result))
    def test_filtered_environment_uses_only_named_user_setting(self):
        import types,contextlib
        seen=[]
        fake=types.SimpleNamespace(HKEY_CURRENT_USER=1,OpenKey=lambda root,path:contextlib.nullcontext(object()),QueryValueEx=lambda key,name:('user-key',1) if name=='MODYN_TRANSLATE_API_KEY' else self.fail('unrelated credential read'))
        def service(url,payload):seen.append(payload);return {'translatedText':'你好'}
        with patch.dict(os.environ,{'MODYN_TRANSLATE_API_KEY':''}),patch('translation.os.name','nt'),patch.dict(sys.modules,{'winreg':fake}):
            result=translate({'text':'Hello','provider':'libretranslate'},transport=service)
        self.assertEqual(seen[0]['api_key'],'user-key');self.assertNotIn('user-key',json.dumps(result))
    def test_bounds_invalid_provider_and_urls(self):
        for data in [{'text':''},{'text':'x'*8001},{'text':'a','provider':'missing'},{'text':'a','source':'xx'}]:
            with self.assertRaises(ValueError):translate(data)
        for url in ['file:///x','http://example.com','https://user:secret@example.com','https://example.com/?key=a']:
            with self.assertRaises(ValueError):endpoint(url)
    def test_quota_and_invalid_responses(self):
        with self.assertRaises(ValueError):translate({'text':'Hello'},transport=lambda *_:{'responseStatus':429})
        with self.assertRaises(ValueError):translate({'text':'Hello'},transport=lambda *_:{'responseStatus':200})
        with self.assertRaises(ValueError):translate({'text':'Hello','provider':'libretranslate'},transport=lambda *_:{'error':'secret detail'})
    def test_cancel_before_any_request(self):
        class Cancelled(Exception):pass
        class Context:
            def check(self):raise Cancelled()
        with self.assertRaises(Cancelled):translate({'text':'Hello'},Context(),lambda *_:self.fail('network called'))
    def test_local_http_and_redirect(self):
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_POST(self):
                data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                self.send_response(200);self.end_headers();self.wfile.write(json.dumps({'translatedText':'你好','seen':data['source']},ensure_ascii=False).encode())
            def do_GET(self):
                self.send_response(302);self.send_header('Location','http://127.0.0.1:1/blocked');self.end_headers()
        server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            base=f'http://127.0.0.1:{server.server_port}'
            self.assertEqual(translate({'text':'Hello','provider':'libretranslate','endpoint':base})['translated_text'],'你好')
            with self.assertRaises(ValueError):request_json(base+'/redirect')
        finally:server.shutdown();server.server_close();thread.join()

if __name__=='__main__':unittest.main()
