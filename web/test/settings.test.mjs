import {test} from 'node:test';
import assert from 'node:assert/strict';
import {playlistTracks, directUrl, cityCoordinates} from '../src/settings.mjs';
const track = {title:'我心中尚未崩坏的地方',url:'https://cs1.mp3.pm/download/1/token/Mayday_-(mp3.pm).mp3'};
test('imports ordered tracks and ignores optional metadata without rewriting signed URLs',()=> {
  const tracks = [{...track,id:'first',artist:'五月天',duration_sec:325},{...track,title:'OAOA'}];
  assert.deepEqual(playlistTracks({version:1,title:'精选',tracks}),[track,{...track,title:'OAOA'}]);
});
test('rejects invalid or oversized playlists without truncating them',()=> {
  for (const value of [null,{},[],{version:2,tracks:[track]},{version:1,tracks:[]},{version:1,tracks:Array(9).fill(track)},
    {version:1,tracks:[track,null]},{version:1,tracks:[{...track,title:'你'.repeat(32)}]},
    {version:1,tracks:[{...track,title:'歌\0曲'}]},{version:1,tracks:[{...track,url:'https://example.com/'+ 'x'.repeat(512)}]}])
    assert.throws(()=>playlistTracks(value));
  assert.equal(playlistTracks({version:1,tracks:Array(8).fill({...track,title:'你'.repeat(31)})}).length,8);
});
test('validates direct HTTP URLs and preserves parentheses',()=> {
  assert.equal(directUrl(track.url),true);
  for (const url of ['https://','javascript:alert(1)','file:///etc/passwd','https://user:pass@example.com/','https://example.com/a b','https://example.com/歌.mp3','https://example.com/\\song']) assert.equal(directUrl(url),false,url);
});
test('city coordinates follow typed presets, retain existing custom cities and reset changed cities',()=> {
  const old = {city:'自定义',latitude_e4:101234,longitude_e4:1045678};
  assert.deepEqual(cityCoordinates('天津',old),{latitude_e4:390842,longitude_e4:1172009});
  assert.deepEqual(cityCoordinates('自定义',old),{latitude_e4:101234,longitude_e4:1045678});
  assert.deepEqual(cityCoordinates('苏州',old),{latitude_e4:0,longitude_e4:0});
  assert.deepEqual(cityCoordinates('',old),{latitude_e4:0,longitude_e4:0});
});
