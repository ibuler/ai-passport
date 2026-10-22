import {test} from 'node:test';
import assert from 'node:assert/strict';
import {writeFileSync} from 'node:fs';
import {pack,rgb565,crc32,SIZE} from '../src/pack.mjs';
const rgba=new Uint8ClampedArray(SIZE*SIZE*4);
for(let i=0;i<rgba.length;i+=4) rgba.set([255,0,0,255],i);
test('RGB565 is little endian and preserves red',()=>assert.deepEqual([...rgb565(rgba).slice(0,2)],[0,248]));
test('monochrome option changes pixel brightness',()=>assert.notDeepEqual(rgb565(rgba,false),rgb565(rgba,true)));
test('CRC32 known vector',()=>assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926));
test('frame count, delay bounds and format',()=>{
  const data=pack([{rgba,delay:1},{rgba,delay:6000}]); const view=new DataView(data.buffer);
  assert.equal(data.length,16+2*(2+SIZE*SIZE*2)); assert.equal(view.getUint16(8,true),2);
  assert.equal(view.getUint16(16,true),100); assert.equal(view.getUint16(16+2+SIZE*SIZE*2,true),2000);
  assert.equal(view.getUint32(12,true),crc32(data.subarray(16)));
  writeFileSync('../build/test-image.pfi',data);
});
test('disabled animation sends just the first frame',()=>assert.equal(new DataView(pack([{rgba},{rgba}],true,false).buffer).getUint16(8,true),1));
test('oversized or empty animation is rejected',()=>{assert.throws(()=>pack([])); assert.throws(()=>pack(Array(49).fill({rgba})));});
