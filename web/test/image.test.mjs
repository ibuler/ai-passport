import {test} from 'node:test';
import assert from 'node:assert/strict';
import {convertImage} from '../src/image.mjs';

test('rejects images above 20 KiB before reading or decoding, with animation on or off', async () => {
  for (const animate of [true, false]) {
    for (const size of [20481, 5 * 1024 * 1024]) {
      let read = false;
      const file = {size, arrayBuffer() { read = true; throw new Error('must not read'); }};
      await assert.rejects(convertImage(file, animate), /20 KB（20,480 字节）/);
      assert.equal(read, false);
    }
  }
});

test('allows files at or below the 20 KiB boundary to reach decoding', async () => {
  for (const size of [20479, 20480]) {
    const reachedRead = new Error('file read reached');
    await assert.rejects(convertImage({size, arrayBuffer() { throw reachedRead; }}, true),
      error => error === reachedRead);
  }
});
