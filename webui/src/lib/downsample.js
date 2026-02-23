/**
 * Min/Max bucket downsampling — зберігає форму кривої та піки.
 * @param {Array} points - [[timestamp, value], ...]
 * @param {number} maxPoints - максимум точок у результаті
 * @returns {Array} downsampled points
 */
export function downsample(points, maxPoints) {
  if (points.length <= maxPoints) return points;
  const bucket = Math.ceil(points.length / maxPoints);
  const result = [];
  for (let i = 0; i < points.length; i += bucket) {
    const slice = points.slice(i, i + bucket);
    const min = slice.reduce((a, b) => a[1] < b[1] ? a : b);
    const max = slice.reduce((a, b) => a[1] > b[1] ? a : b);
    if (min[0] < max[0]) { result.push(min, max); }
    else { result.push(max, min); }
  }
  return result;
}
