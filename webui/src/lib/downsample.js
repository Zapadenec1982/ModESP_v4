/**
 * Min/Max bucket downsampling — зберігає форму кривої та піки.
 * @param {Array} points - [[timestamp, v1, v2, ...], ...]
 * @param {number} maxPoints - максимум точок у результаті
 * @param {number} channelIdx - індекс каналу для порівняння (default 1 = air)
 * @returns {Array} downsampled points
 */
export function downsample(points, maxPoints, channelIdx = 1) {
  if (points.length <= maxPoints) return points;
  const bucket = Math.ceil(points.length / maxPoints);
  const result = [];
  for (let i = 0; i < points.length; i += bucket) {
    const slice = points.slice(i, i + bucket);
    // Фільтруємо точки де канал має дані (не null)
    const valid = slice.filter(p => p[channelIdx] != null);
    if (valid.length === 0) {
      // Якщо всі null — беремо першу точку з бакету
      result.push(slice[0]);
      continue;
    }
    const min = valid.reduce((a, b) => a[channelIdx] < b[channelIdx] ? a : b);
    const max = valid.reduce((a, b) => a[channelIdx] > b[channelIdx] ? a : b);
    if (min[0] < max[0]) { result.push(min, max); }
    else { result.push(max, min); }
  }
  return result;
}
