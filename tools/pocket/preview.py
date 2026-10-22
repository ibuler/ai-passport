#!/usr/bin/env python3
"""Serve the real settings HTML with an isolated in-memory API for browser QA."""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.request import Request, build_opener, HTTPRedirectHandler
from urllib.parse import urlsplit
ROOT = Path(__file__).resolve().parents[2]
CONFIG = {'token':'local-preview-only', 'name':'你的名字', 'volume':35, 'connected':False, 'github_username':'ibuler',
          'profile':{'company':'你的公司','city':'珠海','latitude_e4':222707,'longitude_e4':1135767,'utc_minutes':480,'radios':[]},
          'tracks':[{'title':'79: Corticyte','url':'https://datashat.net/music_for_programming_79-corticyte.mp3'}]}
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/api/config':
            data=json.dumps(CONFIG,ensure_ascii=False).encode(); mime='application/json'
        elif self.path == '/':
            data=(ROOT/'build/settings-preview.html').read_bytes().replace(b'<h1>', '<p class="small">本地界面预览 · 不会修改实体设备</p><h1>'.encode()); mime='text/html; charset=utf-8'
        elif self.path == '/test-animation.gif':
            data=(ROOT/'build/test-animation.gif').read_bytes(); mime='image/gif'
        else:
            self.send_error(404); return
        self.send_response(200); self.send_header('Content-Type',mime); self.send_header('Content-Length',str(len(data))); self.end_headers(); self.wfile.write(data)
    def do_POST(self):
        if self.headers.get('X-Pocket-Token') != CONFIG['token']: self.send_error(403); return
        n=int(self.headers.get('Content-Length','0'))
        if n<1 or n>900000: self.send_error(413); return
        body=self.rfile.read(n)
        if self.path == '/api/playlist':
            try:
                url=json.loads(body)['url']; parsed=urlsplit(url)
                if parsed.scheme not in ('http','https') or not parsed.hostname or parsed.username or parsed.password: raise ValueError('无效地址')
                class DirectOnly(HTTPRedirectHandler):
                    def redirect_request(self, req, fp, code, msg, headers, newurl): return None
                with build_opener(DirectOnly()).open(Request(url,headers={'User-Agent':'PocketPassport-preview'}),timeout=12) as response:
                    data=response.read(7169)
                if len(data)>7168: raise ValueError('歌单文件最大 7 KB')
                json.loads(data)
            except Exception:
                self.send_error(400,'Playlist unavailable'); return
            self.send_response(200); self.send_header('Content-Type','application/json'); self.send_header('Content-Length',str(len(data))); self.end_headers(); self.wfile.write(data); return
        elif self.path == '/api/config': CONFIG.update(json.loads(body)); result='本地预览：设置已保存到临时会话'
        elif self.path == '/api/github': CONFIG['github_username']=json.loads(body)['username']; result='本地预览：GitHub ID 已保存到临时会话'
        elif self.path == '/api/profile': CONFIG['profile']=json.loads(body); result='本地预览：公司、城市和电台已保存到临时会话'
        elif self.path == '/api/image':
            (ROOT/'build/browser-upload.pfi').write_bytes(body); result='本地预览：帧包已保存用于格式验证'
        elif self.path == '/api/wifi': result='本地预览不连接真实 Wi-Fi，请在设备上验证'
        else: self.send_error(404); return
        data=result.encode(); self.send_response(200); self.send_header('Content-Type','text/plain; charset=utf-8'); self.send_header('Content-Length',str(len(data))); self.end_headers(); self.wfile.write(data)
if __name__=='__main__':
    print('Local settings preview: http://127.0.0.1:4191',flush=True)
    ThreadingHTTPServer(('127.0.0.1',4191),Handler).serve_forever()
