const test = require('node:test');
const assert = require('node:assert/strict');
const {fit, toImage, toScreen} = require('../gui/web/coordinates.js');
test('source pixels survive portrait/landscape, pan, zoom and DPI-independent CSS coordinates', () => {
  for (const viewport of [[900,600],[500,900],[1400,700]]) for (const zoom of [.2,1,4]) {
    const view = fit(1600,1000,...viewport,zoom,{x:37,y:-82});
    for(const point of [[0,0],[400.25,300.75],[1599.9,999.9]]) {
      const actual = toImage(...toScreen(...point,view),view,1600,1000);
      assert.ok(actual); assert.ok(Math.hypot(actual[0]-point[0],actual[1]-point[1]) < 1e-9);
    }
  }
});
test('letterbox and image boundary are rejected',()=>{
  const view=fit(1600,1000,900,600);
  assert.equal(toImage(view.x-1,view.y,view,1600,1000),null);
  assert.equal(toImage(view.x,view.y+1000*view.scale,view,1600,1000),null);
});
