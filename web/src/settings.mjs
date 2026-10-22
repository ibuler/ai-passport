export const PLAYLIST_URL = 'https://jumpserver.oss-cn-hangzhou.aliyuncs.com/media/mayday-playlist.json';
export const PLAYLIST_MAX = 7168;
export const cities = [['北京',39.9042,116.4074],['天津',39.0842,117.2009],['上海',31.2304,121.4737],['广州',23.1291,113.2644],['深圳',22.5431,114.0579],['珠海',22.2707,113.5767],['杭州',30.2741,120.1551],['南京',32.0603,118.7969],['成都',30.5728,104.0668],['武汉',30.5928,114.3055],['重庆',29.563,106.5516],['西安',34.3416,108.9398]];
const bytes = value => new TextEncoder().encode(value).length;
export function directUrl(value) {
  if (typeof value !== 'string' || bytes(value) > 511 || !/^https?:\/\/[^\s\\\x00-\x20\x7f-\uffff]+$/.test(value)) return false;
  try { const url = new URL(value); return Boolean(url.hostname) && !url.username && !url.password; } catch { return false; }
}
export function playlistTracks(value) {
  if (!value || value.version !== 1 || !Array.isArray(value.tracks) || !value.tracks.length || value.tracks.length > 8) throw new Error('歌单需要 version: 1 和 1–8 首 tracks，超出上限不会截断');
  return value.tracks.map((track, i) => {
    if (typeof track?.title !== 'string' || typeof track?.url !== 'string') throw new Error(`第 ${i+1} 首缺少名称或 MP3 直链`);
    const title = track.title.trim(), url = track.url.trim();
    if (!title || bytes(title) > 95 || /[\x00-\x1f\x7f]/.test(title) || !directUrl(url)) throw new Error(`第 ${i+1} 首名称或直链无效，名称最多 95 字节，地址最多 511 字节`);
    return {title, url};
  });
}
export function cityCoordinates(city, previous = {}) {
  const preset = cities.find(([name]) => name === city);
  if (preset) return {latitude_e4:Math.round(preset[1]*10000), longitude_e4:Math.round(preset[2]*10000)};
  if (city && city === previous.city) return {latitude_e4:previous.latitude_e4 ?? 0, longitude_e4:previous.longitude_e4 ?? 0};
  return {latitude_e4:0, longitude_e4:0};
}
