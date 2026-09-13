(function (root) {
  const fit = (iw, ih, vw, vh, zoom = 1, pan = {x: 0, y: 0}) => {
    const scale = Math.min(Math.max(1, vw - 64) / iw, Math.max(1, vh - 64) / ih) * zoom;
    return {x: (vw - iw * scale) / 2 + pan.x, y: (vh - ih * scale) / 2 + pan.y, scale};
  };
  const toImage = (x, y, view, iw, ih) => {
    const px = (x - view.x) / view.scale, py = (y - view.y) / view.scale;
    return px >= 0 && py >= 0 && px < iw && py < ih ? [px, py] : null;
  };
  const toScreen = (x, y, view) => [view.x + x * view.scale, view.y + y * view.scale];
  const api = {fit, toImage, toScreen};
  if (typeof module !== 'undefined') module.exports = api;
  else root.PhotoCoordinates = api;
})(typeof window === 'undefined' ? globalThis : window);
