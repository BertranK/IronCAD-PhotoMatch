const $ = id => document.getElementById(id);
const ctx = $('photoCanvas').getContext('2d');
let api, state = null, connected = false, busy = false, photo = null, bitmap = null;
let imagePoints = {}, selected = null, zoom = 1, pan = {x: 0, y: 0}, drag = null, toastTimer;
let sessionKey = '', rowsKey = '', cameraKey = '';
const C = window.PhotoCoordinates;
function toast(message) { $('toast').textContent = message; $('toast').classList.remove('hidden'); clearTimeout(toastTimer); toastTimer = setTimeout(() => $('toast').classList.add('hidden'), 6500); }
window.closeError = error => toast('복원 후 닫을 수 있습니다. ' + error);
window.activationError = error => toast('IronCAD 연결을 전환하지 못했습니다. ' + error);
window.hostActivated = result => accept(result);
function accept(result) {
  if (!result.ok) throw new Error(result.error);
  if (result.state) {
    state = result.state; connected = true;
    const key = state.session + ':' + state.capture_id;
    if (sessionKey !== key) { selected = null; imagePoints = {}; rowsKey = ''; sessionKey = key; }
    imagePoints = result.image_points || {};
  }
  render(); return result;
}
async function action(command, args = {}) {
  if (!api || busy) return;
  busy = true; render();
  try { return accept(await api.call(command, state?.session || '', args)); }
  catch (error) { toast(error.message); }
  finally { busy = false; render(); }
}
function render() {
  $('connection').textContent = connected ? 'IronCAD 연결됨' : 'IronCAD 연결 대기';
  $('connectionDot').classList.toggle('ready', connected);
  $('document').textContent = connected && state?.document ? state.document : '—';
  const captured = connected && state?.captured, points = connected ? state?.points || [] : [];
  $('pick').disabled = busy || !connected || !state?.document;
  $('pick').textContent = state?.picking && connected ? '점 선택 마치기' : '＋ 모델 점 선택';
  $('capture').disabled = busy || !connected || !state?.document;
  ['apply', 'measure', 'restore'].forEach(id => $(id).disabled = busy || !captured);
  $('background').disabled = busy || !captured || !photo;
  $('overlay').disabled = busy || !captured || !photo || !state?.fov_resolved || !state?.test_camera;
  $('save').disabled = busy || !connected || !state?.original_camera;
  $('openImage').disabled = $('openEmpty').disabled = busy;
  $('pairCount').textContent = `${Object.keys(imagePoints).length} / ${points.length}`;
  $('alignmentStatus').textContent = state?.fov_resolved ? '투영 검증 통과' : '검증 대기';
  $('alignmentDot').classList.toggle('ready', !!state?.fov_resolved);
  $('stepImage').classList.toggle('active', !photo);
  $('stepPoints').classList.toggle('active', !!photo && !state?.fov_resolved);
  $('stepResult').classList.toggle('active', !!state?.fov_resolved);
  $('footerState').textContent = !connected ? '연결 대기' : busy ? '작업 중' : state?.picking ? '모델 점 선택 중' : '준비';
  if (selected && !points.some(p => p.id === selected)) selected = null;
  if (!selected && points.length) selected = (points.find(p => !imagePoints[p.id]) || points[0]).id;
  const key = JSON.stringify([points, imagePoints, selected]);
  if (key !== rowsKey) {
    rowsKey = key; $('points').replaceChildren();
    if (!points.length) { const empty = document.createElement('p'); empty.className = 'px-3 pt-8 text-center text-xs text-slate-600'; empty.textContent = '선택한 점이 여기에 표시됩니다'; $('points').append(empty); }
    for (const point of points) {
      const row = document.createElement('button'); row.className = 'point-row' + (selected === point.id ? ' selected' : '');
      const title = document.createElement('div'); title.className = 'flex items-center justify-between mb-2';
      const id = document.createElement('span'); id.className = 'coordinate text-mint'; id.textContent = point.id;
      const dot = document.createElement('span'); dot.className = 'dot' + (imagePoints[point.id] ? ' ready' : ''); title.append(id, dot);
      const name = document.createElement('p'); name.className = 'truncate text-xs text-slate-300 mb-1'; name.textContent = point.object_name;
      const pixel = document.createElement('p'); pixel.className = 'coordinate text-slate-500'; pixel.textContent = imagePoints[point.id] ? imagePoints[point.id].map(v => v.toFixed(1)).join(' , ') : '사진에서 위치 선택';
      row.title = `API: ${point.api_coordinates.join(', ')}\n변환 후보: ${point.transformed_coordinates.join(', ')}\n좌표계 검증 대기`;
      row.append(title, name, pixel); row.onclick = () => { selected = point.id; render(); }; $('points').append(row);
    }
  }
  const camera = state?.camera?.pose_and_field || state?.original_camera?.pose_and_field;
  const ck = JSON.stringify([sessionKey, state?.camera_apply?.length]);
  if (camera && cameraKey !== ck) { cameraKey = ck; for (const key of ['position','direction','up']) $(key).value = camera[key].join(' '); $('field').value = camera.field_sdk; }
  $('activePoint').textContent = selected ? selected + ' · 사진에서 위치 선택' : '';
  $('activePoint').classList.toggle('hidden', !selected || !photo || !captured);
  $('logs').textContent = (state?.logs || []).slice(-15).join('\n');
  const best = state?.fov_candidates?.[0];
  $('metrics').textContent = state ? [`SDK ${state.sdk_version}`, state.viewport ? state.viewport.join(' × ') + ` · DPI ${state.dpi}` : '', best ? `${best.name}\n${best.max_error_px?.toFixed(3) ?? '—'} px` : '투영 미측정'].filter(Boolean).join('\n') : '—';
  draw();
}
function view() { return photo ? C.fit(photo.width, photo.height, $('stage').clientWidth, $('stage').clientHeight, zoom, pan) : null; }
function draw() {
  const canvas = $('photoCanvas'), dpr = window.devicePixelRatio || 1, w = $('stage').clientWidth, h = $('stage').clientHeight;
  if (canvas.width !== Math.round(w*dpr) || canvas.height !== Math.round(h*dpr)) { canvas.width = Math.round(w*dpr); canvas.height = Math.round(h*dpr); }
  ctx.setTransform(dpr,0,0,dpr,0,0); ctx.clearRect(0,0,w,h);
  if (!photo || !bitmap) return;
  const v = view(); ctx.drawImage(bitmap, v.x,v.y,photo.width*v.scale,photo.height*v.scale);
  ctx.strokeStyle = '#54636b'; ctx.lineWidth = 1; ctx.strokeRect(v.x,v.y,photo.width*v.scale,photo.height*v.scale);
  for (const [id, pixel] of Object.entries(imagePoints)) {
    const [x,y] = C.toScreen(...pixel,v); const active = id === selected;
    ctx.beginPath(); ctx.arc(x,y,active?9:7,0,Math.PI*2); ctx.fillStyle = '#101416dd'; ctx.fill(); ctx.strokeStyle = active?'#b8f582':'#ffffff'; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.beginPath(); ctx.moveTo(x-13,y);ctx.lineTo(x+13,y);ctx.moveTo(x,y-13);ctx.lineTo(x,y+13);ctx.stroke();
    ctx.font = '11px Segoe UI'; const tw = ctx.measureText(id).width; ctx.fillStyle = '#101416e6';ctx.fillRect(x+12,y-23,tw+12,20);ctx.fillStyle = '#b8f582';ctx.fillText(id,x+18,y-9);
  }
  $('zoom').textContent = Math.round(v.scale*100)+'%';
}
async function openImage() {
  if (!api || busy) return; busy = true; render();
  try {
    const result = await api.open_image(); if (!result.ok) throw new Error(result.error); if (result.cancelled) return;
    const next = new Image(); next.src = result.image.preview; await next.decode();
    photo = result.image; bitmap = next; imagePoints = {}; zoom = 1; pan = {x:0,y:0};
    $('empty').classList.add('hidden'); $('imageName').textContent = photo.name; $('imageSize').textContent = `${photo.width} × ${photo.height}`;
  } catch(error) { toast(error.message); } finally { busy = false; render(); }
}
$('openImage').onclick = $('openEmpty').onclick = openImage;
$('pick').onclick = async () => { if (!state?.captured && !(await action('capture'))) return; await action(state.picking ? 'stop_pick' : 'pick'); };
$('capture').onclick = () => action('capture'); $('restore').onclick = () => action('restore'); $('measure').onclick = () => action('measure');
$('apply').onclick = () => {
  try { const args = {}; for(const key of ['position','direction','up']) { args[key] = $(key).value.trim().split(/\s+/).map(Number); if(args[key].length!==3 || args[key].some(v=>!Number.isFinite(v))) throw new Error('좌표를 확인하세요.'); } args.field_sdk = Number($('field').value); if(!Number.isFinite(args.field_sdk)||args.field_sdk<=0)throw new Error('화각을 확인하세요.'); action('apply', args); } catch(error){toast(error.message);}
};
$('overlay').onclick = () => action('photo', {focal_px:Number($('focal').value)});
$('background').onclick = () => action('background');
$('save').onclick = async () => { if(busy)return;busy=true;render();try {const r=await api.save_project(state.session,state.capture_id);if(!r.ok)throw new Error(r.error);if(!r.cancelled)toast('결과를 저장했습니다.');}catch(error){toast(error.message);}finally{busy=false;render();} };
$('fit').onclick = () => {zoom=1;pan={x:0,y:0};draw();};
$('zoomIn').onclick = () => {zoom=Math.min(8,zoom*1.25);draw();}; $('zoomOut').onclick = () => {zoom=Math.max(.1,zoom/1.25);draw();};
const canvas = $('photoCanvas');
canvas.oncontextmenu = event => event.preventDefault();
canvas.onpointerdown = event => {if(!photo)return;canvas.setPointerCapture(event.pointerId);drag={x:event.offsetX,y:event.offsetY,startX:event.offsetX,startY:event.offsetY,pan:event.button!==0};};
canvas.onpointermove = event => {
  if(!photo)return;
  if(drag?.pan){pan.x+=event.offsetX-drag.x;pan.y+=event.offsetY-drag.y;drag.x=event.offsetX;drag.y=event.offsetY;draw();}
  const p=C.toImage(event.offsetX,event.offsetY,view(),photo.width,photo.height);$('cursor').textContent=p?p.map(v=>v.toFixed(1)).join(' , ')+' px':'—';
};
canvas.onpointerup = async event => {
  const start=drag;drag=null;if(!start||start.pan||Math.hypot(event.offsetX-start.startX,event.offsetY-start.startY)>4)return;
  if(!selected||!connected||!state?.captured||busy)return;
  const p=C.toImage(event.offsetX,event.offsetY,view(),photo.width,photo.height);if(!p)return;
  busy=true;render();try{const r=await api.set_point(state.session,state.capture_id,selected,...p);if(!r.ok)throw new Error(r.error);imagePoints=r.image_points;selected=(state.points.find(p=>!imagePoints[p.id])||{id:selected}).id;}catch(error){toast(error.message);}finally{busy=false;render();}
};
canvas.onpointercancel=()=>{drag=null;};
canvas.onwheel=event=>{event.preventDefault();if(!photo)return;const before=view(),px=(event.offsetX-before.x)/before.scale,py=(event.offsetY-before.y)/before.scale;zoom=Math.max(.1,Math.min(8,zoom*Math.exp(-event.deltaY*.001)));const after=view();pan.x+=event.offsetX-(after.x+px*after.scale);pan.y+=event.offsetY-(after.y+py*after.scale);draw();};
new ResizeObserver(draw).observe($('stage'));
window.addEventListener('pywebviewready',()=>{api=window.pywebview.api;poll();});
async function poll(){try{if(!busy){const result=await api.call('status');if(result.ok)accept(result);else{connected=false;render();}}}catch(_){connected=false;render();}finally{setTimeout(poll,1000);}}
render();
