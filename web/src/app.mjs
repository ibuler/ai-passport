import {convertImage} from './image.mjs';
import {pack, rgb565} from './pack.mjs';
import {PLAYLIST_URL, PLAYLIST_MAX, cities, directUrl, playlistTracks, cityCoordinates} from './settings.mjs';
const $ = id => document.getElementById(id);
let token = '', frames = [], timer, busy = false;
let savedProfile = {};
function message(text, error = false) { $('status').textContent = text; $('status').className = `status ${error ? 'error' : 'success'}`; }
async function api(path, body, binary = false) {
  const result = await fetch(path, {method:'POST', headers:{'Content-Type': binary ? 'application/octet-stream':'application/json', 'X-Pocket-Token':token}, body:binary ? body : JSON.stringify(body)});
  const text = await result.text(); if (!result.ok) throw new Error(text); return text;
}
async function action(fn) {
  if (busy) return;
  busy = true; $('editor').disabled = true;
  try { await fn(); } catch (e) { message(e.message || '设备连接中断，请检查热点后刷新页面', true); }
  finally { busy = false; $('editor').disabled = false; }
}
function addTrack(track = {title:'',url:''}) {
  if ($('tracks').children.length >= 8) return;
  const row = document.createElement('div'); row.className = 'track';
  const head = document.createElement('div'); head.className = 'track-head';
  const heading = document.createElement('label'); heading.textContent = `音乐 ${$('tracks').children.length + 1}`;
  const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'remove'; remove.textContent = '移除'; remove.addEventListener('click', () => row.remove());
  head.append(heading,remove);
  const title = document.createElement('input'); title.value = track.title; title.maxLength = 31; title.placeholder = '曲目名称'; title.setAttribute('aria-label', '曲目名称'); title.dataset.field = 'title';
  const url = document.createElement('input'); url.value = track.url; url.maxLength = 511; url.placeholder = 'https://example.com/music.mp3'; url.type = 'url'; url.setAttribute('aria-label', 'MP3 直链'); url.dataset.field = 'url';
  row.append(head,title,url); $('tracks').append(row);
}
function preview() {
  clearTimeout(timer); let index = 0;
  function draw() {
    if (!frames.length) return;
    const frame = frames[index], pixels = rgb565(frame.rgba, $('color').checked), rgba = new Uint8ClampedArray(96*96*4);
    for (let i = 0; i < 96*96; i++) { const p = pixels[i*2] | pixels[i*2+1]<<8; rgba.set([(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31,255], i*4); }
    $('preview').getContext('2d').putImageData(new ImageData(rgba,96,96),0,0);
    if ($('animate').checked && frames.length > 1) { index = (index+1)%frames.length; timer=setTimeout(draw,Math.min(2000,Math.max(100,frame.delay))); }
  }
  draw();
}
async function convert() {
  clearTimeout(timer); frames=[]; $('upload').disabled=true;
  if (!$('image').files[0]) return;
  await action(async () => {
    frames = await convertImage($('image').files[0], $('animate').checked);
    $('image-info').textContent = `${frames.length} 帧 · ${(pack(frames,$('color').checked,$('animate').checked).length/1024).toFixed(0)} KB · 图片仅在本地转换，上传到你的设备。`;
    $('upload').disabled=false; preview();
  });
}
$('image').addEventListener('change',convert);
$('animate').addEventListener('change',convert);
$('color').addEventListener('change',preview);
$('volume').addEventListener('input',()=> $('volume-value').textContent=`${$('volume').value}%`);
$('add').addEventListener('click',()=>addTrack());
function importPlaylist(value) {
  const tracks = playlistTracks(value);
  $('tracks').replaceChildren(); tracks.forEach(addTrack);
  message(`已导入 ${tracks.length} 首音乐，替换了当前编辑列表；点击“保存名字与音乐”写入设备。`);
}
$('playlist-url').value = PLAYLIST_URL;
$('import-playlist').addEventListener('click',()=>action(async()=> {
  const url = $('playlist-url').value.trim();
  if (!directUrl(url)) throw new Error('请输入有效的 HTTP / HTTPS 歌单 JSON 地址');
  message('设备正在读取歌单，请保持热点连接…');
  importPlaylist(JSON.parse(await api('/api/playlist',{url})));
}));
$('playlist-file').addEventListener('change',()=>action(async()=> {
  const file = $('playlist-file').files[0];
  try {
    if (!file) return;
    if (file.size > PLAYLIST_MAX) throw new Error('歌单文件最大 7 KB');
    importPlaylist(JSON.parse(await file.text()));
  } finally { $('playlist-file').value = ''; }
}));
$('upload').addEventListener('click',()=>action(async()=> {message('正在上传，请保持热点连接…'); message(await api('/api/image',pack(frames,$('color').checked,$('animate').checked),true));}));
$('save').addEventListener('click',()=>action(async()=> {
  const tracks = [...$('tracks').children].map(row=>({title:row.querySelector('[data-field=title]').value.trim(),url:row.querySelector('[data-field=url]').value.trim()}));
  if (tracks.some(t=>!t.title || !/^https?:\/\/[^\s]+$/.test(t.url))) throw new Error('请填写每首音乐的名称和有效直链');
  message(await api('/api/config',{name:$('name').value.trim(),volume:Number($('volume').value),tracks}));
}));
$('save-github').addEventListener('click',()=>action(async()=> {
  const username=$('github').value.trim();
  if(username && !/^[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*$/.test(username)) throw new Error('GitHub ID 仅支持字母、数字和单连字符');
  message(await api('/api/github',{username}));
}));
$('connect').addEventListener('click',()=>action(async()=> {message('正在验证 Wi-Fi，最多等待 15 秒…'); message(await api('/api/wifi',{ssid:$('ssid').value,password:$('password').value})); $('password').value='';}));
try {
  const response=await fetch('/api/config'); if(!response.ok) throw new Error('无法读取设备设置');
  const config=await response.json(); token=config.token;
  $('github').value=config.github_username ?? 'ibuler';
  $('name').value=config.name; loadProfile(config.profile); $('volume').value=config.volume; $('volume-value').textContent=`${config.volume}%`;
  config.tracks.forEach(addTrack); $('connection').textContent=config.connected ? '设备已联网':'设备已连接 · 待配置网络'; $('editor').disabled=false;
} catch(e) { message('无法连接设备。请连接屏幕显示的 Wi-Fi 后，重新打开 192.168.4.1。',true); }

for (const [name] of cities) { const option=document.createElement('option'); option.value=name; option.textContent=name; $('city-preset').append(option); }
$('city-preset').addEventListener('change',()=> { const city=cities.find(c=>c[0]===$('city-preset').value); if(city) { $('city').value=city[0]; $('timezone').value='480'; } });
function addRadio(station={city:$('city').value.trim(),title:'',url:''}) {
  if ($('radios').children.length>=6) return;
  const row=document.createElement('div'); row.className='track';
  for (const [field,label,max] of [['city','城市名称，须与上方一致',20],['title','电台名称',31],['url','MP3 直播直链',511]]) {
    const input=document.createElement('input'); input.dataset.field=field; input.value=station[field]; input.maxLength=max; input.placeholder=label; input.setAttribute('aria-label',label); if(field==='url') input.type='url'; row.append(input);
  }
  const remove=document.createElement('button'); remove.type='button'; remove.className='remove'; remove.textContent='移除此电台'; remove.addEventListener('click',()=>row.remove()); row.append(remove); $('radios').append(row);
}
function loadProfile(profile={company:'',city:'',latitude_e4:0,longitude_e4:0,utc_minutes:480,radios:[]}) {
  savedProfile = profile;
  $('company').value=profile.company; $('city').value=profile.city;
  if (![...$('timezone').options].some(o=>Number(o.value)===profile.utc_minutes)) { const o=document.createElement('option'); o.value=profile.utc_minutes; o.textContent=`UTC 偏移 ${profile.utc_minutes} 分钟`; $('timezone').append(o); }
  $('timezone').value=String(profile.utc_minutes); profile.radios.forEach(addRadio);
}
$('add-radio').addEventListener('click',()=>addRadio());
$('save-profile').addEventListener('click',()=>action(async()=> {
  const city=$('city').value.trim(), coordinates=cityCoordinates(city,savedProfile);
  const radios=[...$('radios').children].map(row=>Object.fromEntries(['city','title','url'].map(f=>[f,row.querySelector(`[data-field=${f}]`).value.trim()])));
  if(radios.some(r=>!r.city||!r.title||!/^https?:\/\/[^\s]+$/.test(r.url))) throw new Error('请填写电台城市、名称及 MP3 直链');
  message(await api('/api/profile',{company:$('company').value.trim(),city,...coordinates,utc_minutes:Number($('timezone').value),radios}));
  savedProfile={city,...coordinates};
}));
