import {parseGIF, decompressFrame} from 'gifuct-js';
import {SIZE, MAX_FRAMES} from './pack.mjs';
function canvas(w, h) {
  const c = document.createElement('canvas'); c.width = w; c.height = h; return c;
}
function sample(source) {
  const target = canvas(SIZE, SIZE), ctx = target.getContext('2d', {willReadFrequently:true});
  ctx.fillStyle = '#09151c'; ctx.fillRect(0, 0, SIZE, SIZE);
  const scale = Math.max(SIZE / source.width, SIZE / source.height);
  const w = source.width * scale, h = source.height * scale;
  ctx.drawImage(source, (SIZE-w)/2, (SIZE-h)/2, w, h);
  return ctx.getImageData(0, 0, SIZE, SIZE).data;
}
export async function convertImage(file, animate) {
  if (!file || file.size > 5 * 1024 * 1024) throw new Error('请选择不超过 5 MB 的图片');
  const data = await file.arrayBuffer();
  if (new TextDecoder().decode(data.slice(0, 6)).startsWith('GIF8')) {
    const gif = parseGIF(data), {width, height} = gif.lsd;
    const images = gif.frames.filter(f => f.image);
    if (!width || !height || width > 1024 || height > 1024 || !images.length || images.length > 300) throw new Error('GIF 最多 1024×1024、300 帧，请先压缩');
    for (const image of images) {
      const d = image.image.descriptor;
      if (!d.width || !d.height || d.width > width || d.height > height || d.left + d.width > width || d.top + d.height > height) throw new Error('GIF 帧范围无效');
    }
    const screen = canvas(width, height), ctx = screen.getContext('2d', {willReadFrequently:true});
    const patch = canvas(width, height), patchCtx = patch.getContext('2d');
    const frames = []; let last = null, restore = null;
    const step = animate ? Math.max(1, Math.ceil(images.length / MAX_FRAMES)) : images.length;
    for (let i = 0; i < images.length; i++) {
      if (last?.disposalType === 2) ctx.clearRect(last.dims.left, last.dims.top, last.dims.width, last.dims.height);
      else if (last?.disposalType === 3 && restore) ctx.putImageData(restore, 0, 0);
      const f = decompressFrame(images[i], gif.gct, true);
      if (f.dims.left + f.dims.width > width || f.dims.top + f.dims.height > height) throw new Error('GIF 帧范围无效');
      restore = f.disposalType === 3 ? ctx.getImageData(0, 0, width, height) : null;
      patch.width = f.dims.width; patch.height = f.dims.height;
      patchCtx.putImageData(new ImageData(f.patch, f.dims.width, f.dims.height), 0, 0);
      ctx.drawImage(patch, f.dims.left, f.dims.top);
      if (i % step === 0) frames.push({rgba:sample(screen), delay:Math.max(100, f.delay || 100)});
      else frames[frames.length - 1].delay += Math.max(100, f.delay || 100);
      last = f;
      if (!animate) break;
    }
    return frames;
  }
  const url = URL.createObjectURL(new Blob([data], {type:file.type}));
  try {
    const image = new Image(); image.src = url; await image.decode();
    if (image.naturalWidth > 8192 || image.naturalHeight > 8192) throw new Error('图片尺寸过大');
    const source = canvas(image.naturalWidth, image.naturalHeight);
    source.getContext('2d').drawImage(image, 0, 0);
    return [{rgba:sample(source), delay:1000}];
  } finally { URL.revokeObjectURL(url); }
}
