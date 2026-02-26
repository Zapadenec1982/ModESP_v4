/**
 * Shared chart math utilities — used by ChartWidget and MiniChart.
 */

/** Catmull-Rom → SVG cubic Bezier path */
export function catmullRomPath(points) {
  if (points.length < 2) return '';
  if (points.length === 2) {
    return `M${points[0].x},${points[0].y} L${points[1].x},${points[1].y}`;
  }
  let d = `M${points[0].x},${points[0].y}`;
  for (let i = 0; i < points.length - 1; i++) {
    const p0 = points[Math.max(0, i - 1)];
    const p1 = points[i];
    const p2 = points[i + 1];
    const p3 = points[Math.min(points.length - 1, i + 2)];
    const cp1x = (p1.x + (p2.x - p0.x) / 6).toFixed(1);
    const cp1y = (p1.y + (p2.y - p0.y) / 6).toFixed(1);
    const cp2x = (p2.x - (p3.x - p1.x) / 6).toFixed(1);
    const cp2y = (p2.y - (p3.y - p1.y) / 6).toFixed(1);
    d += ` C${cp1x},${cp1y} ${cp2x},${cp2y} ${p2.x},${p2.y}`;
  }
  return d;
}

/** Build smooth SVG path segments for one channel (handles null gaps) */
export function buildSmoothSegments(pts, chIdx, pad, xFn, yFn) {
  const segments = [];
  let seg = [];
  for (const p of pts) {
    const raw = p[chIdx];
    if (raw == null) {
      if (seg.length > 0) { segments.push(catmullRomPath(seg)); seg = []; }
      continue;
    }
    seg.push({ x: +(pad.left + xFn(p[0])).toFixed(1), y: +(pad.top + yFn(raw / 10)).toFixed(1) });
  }
  if (seg.length > 0) segments.push(catmullRomPath(seg));
  return segments;
}

/** Find min/max temperature from data points across given channel indices */
export function tempRange(pts, channelIndices, defaultRange) {
  let mn = Infinity, mx = -Infinity;
  for (const p of pts) {
    for (const idx of channelIndices) {
      const raw = p[idx];
      if (raw == null) continue;
      const v = raw / 10;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
  }
  if (mn === Infinity) return defaultRange || [-25, -15];
  const margin = Math.max((mx - mn) * 0.1, 1);
  return [Math.floor(mn - margin), Math.ceil(mx + margin)];
}

/** Generate time axis labels (N evenly-spaced) */
export function computeTimeLabels(tMin, tMax, count, padLeft, xFn) {
  if (tMin === tMax) return [];
  const labels = [];
  for (let i = 0; i <= count; i++) {
    const ts = tMin + (tMax - tMin) * i / count;
    const d = new Date(ts * 1000);
    labels.push({
      x: padLeft + xFn(ts),
      label: `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`
    });
  }
  return labels;
}

/** Generate temperature axis labels */
export function computeTempLabels(vMin, vMax, count, padTop, yFn) {
  const labels = [];
  const step = Math.max(1, Math.round((vMax - vMin) / count));
  for (let v = Math.ceil(vMin); v <= Math.floor(vMax); v += step) {
    labels.push({ y: padTop + yFn(v), label: `${v}°` });
  }
  return labels;
}
