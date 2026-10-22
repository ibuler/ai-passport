export const SIZE = 96;
export const MAX_FRAMES = 48;
export function crc32(bytes) {
  let crc = -1;
  for (const value of bytes) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (-(crc & 1) & 0xedb88320);
  }
  return (~crc) >>> 0;
}
export function rgb565(rgba, color = true) {
  if (rgba.length !== SIZE * SIZE * 4) throw new Error('图片尺寸不正确');
  const out = new Uint8Array(SIZE * SIZE * 2);
  for (let i = 0; i < SIZE * SIZE; i++) {
    const alpha = rgba[i * 4 + 3] / 255;
    let r = Math.round(rgba[i * 4] * alpha + 9 * (1 - alpha));
    let g = Math.round(rgba[i * 4 + 1] * alpha + 21 * (1 - alpha));
    let b = Math.round(rgba[i * 4 + 2] * alpha + 28 * (1 - alpha));
    if (!color) r = g = b = Math.round(r * .2126 + g * .7152 + b * .0722);
    const pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    out[i * 2] = pixel & 255; out[i * 2 + 1] = pixel >>> 8;
  }
  return out;
}
export function pack(frames, color = true, animate = true) {
  if (!frames.length || frames.length > MAX_FRAMES) throw new Error('最多 48 帧');
  if (!animate) frames = frames.slice(0, 1);
  const stride = 2 + SIZE * SIZE * 2;
  const bytes = new Uint8Array(16 + stride * frames.length);
  const view = new DataView(bytes.buffer);
  bytes.set([80, 70, 73, 49]);
  view.setUint16(4, SIZE, true); view.setUint16(6, SIZE, true);
  view.setUint16(8, frames.length, true);
  view.setUint16(10, (color ? 1 : 0) | (animate && frames.length > 1 ? 2 : 0), true);
  frames.forEach((frame, i) => {
    view.setUint16(16 + i * stride, Math.min(2000, Math.max(100, Math.round(frame.delay || 100))), true);
    bytes.set(rgb565(frame.rgba, color), 18 + i * stride);
  });
  view.setUint32(12, crc32(bytes.subarray(16)), true);
  return bytes;
}
