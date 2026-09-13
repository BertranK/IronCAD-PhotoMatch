const $ = id => document.getElementById(id);
const ctx = $('photoCanvas').getContext('2d');
let api, state = null, connected = false, busy = false, photo = null, bitmap = null;
let imagePoints = {}, selected = null, zoom = 1, pan = {x: 0, y: 0}, drag = null, toastTimer;
let sessionKey = '', rowsKey = '', cameraKey = '';
const C = window.PhotoCoordinates;
const L = window.PhotoLanguage, t = L.t;
let preferencesKey = '';
function updatePreferences(value) {
  const key = JSON.stringify(value); if (key === preferencesKey) return; preferencesKey = key;
  document.documentElement.dataset.theme = value.theme;
  L.setLanguage(value.language); L.localize(document); $('language').value = value.language; rowsKey = ''; render();
}
$('language').onchange = async () => {
  try { updatePreferences(await api.set_language($('language').value)); }
  catch (_) { toast(t('설정을 저장하지 못했습니다.')); $('language').value = JSON.parse(preferencesKey || '{}').language || 'en'; }
};
function toast(message) { $('toast').textContent = message; $('toast').classList.remove('hidden'); clearTimeout(toastTimer); toastTimer = setTimeout(() => $('toast').classList.add('hidden'), 6500); }
window.closeError = error => toast(t('복원 후 닫을 수 있습니다.') + ' ' + t(error));
window.activationError = error => toast(t('IronCAD 연결을 전환하지 못했습니다.') + ' ' + t(error));
window.hostActivated = result => accept(result);
function accept(result) {
  if (!result.ok) throw new Error(result.error);
  if (result.state) {
    state = result.state; connected = true;
    const key = state.session + ':' + state.capture_id;
    if (sessionKey !== key) { if (drag?.id) { drag = null; busy = false; } selected = null; imagePoints = {}; rowsKey = ''; sessionKey = key; }
    imagePoints = result.image_points || {};
  }
  render(); return result;
}
async function action(command, args = {}) {
  if (!api || busy) return;
  busy = true; render();
  try { return accept(await api.call(command, state?.session || '', args)); }
  catch (error) { toast(t(error.message)); }
  finally { busy = false; render(); }
}
function render() {
  $('connection').textContent = connected ? t('IronCAD 연결됨') : t('IronCAD 연결 대기');
  $('connectionDot').classList.toggle('ready', connected);
  $('document').textContent = connected && state?.document ? state.document : '—';
  const captured = connected && state?.captured, points = connected ? state?.points || [] : [];
  $('pick').disabled = busy || !connected || !state?.document;
  $('pick').textContent = state?.picking && connected ? t('점 선택 마치기') : t('＋ 모델 점 선택');
  $('capture').disabled = busy || !connected || !state?.document;
  ['measure', 'restore'].forEach(id => $(id).disabled = busy || !captured);
  $('apply').disabled = busy || !connected || !(captured || state?.restored);
  $('background').disabled = busy || !captured || !photo;
  $('overlay').disabled = busy || !captured || !photo || !state?.fov_resolved || !state?.test_camera;
  $('save').disabled = busy || !connected || !state?.original_camera;
  $('openImage').disabled = $('openEmpty').disabled = busy;
  $('openProject').disabled = busy || !connected;
  $('pairCount').textContent = `${Object.keys(imagePoints).length} / ${points.length}`;
  $('alignmentStatus').textContent = state?.fov_resolved ? t('투영 검증 통과') : t('검증 대기');
  $('alignmentDot').classList.toggle('ready', !!state?.fov_resolved);
  $('stepImage').classList.toggle('active', !photo);
  $('stepPoints').classList.toggle('active', !!photo && !state?.fov_resolved);
  $('stepResult').classList.toggle('active', !!state?.fov_resolved);
  $('footerState').textContent = !connected ? t('연결 대기') : busy ? t('작업 중') : state?.picking ? t('모델 점 선택 중') : t('준비');
  if (selected && !points.some(p => p.id === selected)) selected = null;
  if (!selected && points.length) selected = (points.find(p => !imagePoints[p.id]) || points[0]).id;
  const key = JSON.stringify([points, imagePoints, selected]);
  if (key !== rowsKey) {
    rowsKey = key; $('points').replaceChildren();
    if (!points.length) { const empty = document.createElement('p'); empty.className = 'px-3 pt-8 text-center text-xs text-slate-600'; empty.textContent = t('선택한 점이 여기에 표시됩니다'); $('points').append(empty); }
    for (const point of points) {
      const row = document.createElement('button'); row.className = 'point-row' + (selected === point.id ? ' selected' : '');
      const title = document.createElement('div'); title.className = 'flex items-center justify-between mb-2';
      const id = document.createElement('span'); id.className = 'coordinate text-mint'; id.textContent = point.id;
      const dot = document.createElement('span'); dot.className = 'dot' + (imagePoints[point.id] ? ' ready' : ''); title.append(id, dot);
      const name = document.createElement('p'); name.className = 'truncate text-xs text-slate-300 mb-1'; name.textContent = point.object_name;
      const pixel = document.createElement('p'); pixel.className = 'coordinate text-slate-500'; pixel.textContent = imagePoints[point.id] ? imagePoints[point.id].map(v => v.toFixed(1)).join(' , ') : t('사진에서 위치 선택');
      row.title = `API: ${point.api_coordinates.join(', ')}\n${t('변환 후보')}: ${point.transformed_coordinates.join(', ')}\n${t('좌표계 검증 대기')}`;
      row.append(title, name, pixel); row.onclick = () => { selected = point.id; render(); }; $('points').append(row);
    }
  }
  const camera = state?.camera?.pose_and_field || state?.original_camera?.pose_and_field;
  const ck = JSON.stringify([sessionKey, state?.camera_apply?.length]);
  if (camera && cameraKey !== ck) { cameraKey = ck; for (const key of ['position','direction','up']) $(key).value = camera[key].join(' '); $('field').value = camera.field_sdk; }
  $('activePoint').textContent = selected ? selected + ' · ' + t('사진에서 위치 선택') : '';
  $('activePoint').classList.toggle('hidden', !selected || !photo || !connected);
  $('logs').textContent = (state?.logs || []).slice(-15).join('\n');
  const best = state?.fov_candidates?.[0];
  $('metrics').textContent = state ? [`SDK ${state.sdk_version}`, state.viewport ? state.viewport.join(' × ') + ` · DPI ${state.dpi}` : '', best ? `${best.name}\n${best.max_error_px?.toFixed(3) ?? '—'} px` : t('투영 미측정')].filter(Boolean).join('\n') : '—';
  draw();
}
function view() { return photo ? C.fit(photo.width, photo.height, $('stage').clientWidth, $('stage').clientHeight, zoom, pan) : null; }
function draw() {
  const canvas = $('photoCanvas'), dpr = window.devicePixelRatio || 1, w = $('stage').clientWidth, h = $('stage').clientHeight;
  if (canvas.width !== Math.round(w*dpr) || canvas.height !== Math.round(h*dpr)) { canvas.width = Math.round(w*dpr); canvas.height = Math.round(h*dpr); }
  ctx.setTransform(w ? canvas.width/w : 1,0,0,h ? canvas.height/h : 1,0,0); ctx.clearRect(0,0,w,h);
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
async function openImage(project = false) {
  if (!api || busy) return; busy = true; render();
  try {
    const result = project ? await api.open_project() : await api.open_image(); if (!result.ok) throw new Error(result.error); if (result.cancelled) return;
    const next = new Image(); next.src = result.image.preview; await next.decode();
    photo = result.image; bitmap = next; imagePoints = {}; zoom = 1; pan = {x:0,y:0};
    if (project) accept(result);
    $('empty').classList.add('hidden'); $('imageName').textContent = photo.name; $('imageSize').textContent = `${photo.width} × ${photo.height}`;
  } catch(error) { toast(t(error.message)); } finally { busy = false; render(); }
}
$('openImage').onclick = $('openEmpty').onclick = () => openImage();
$('openProject').onclick = () => openImage(true);
$('pick').onclick = async () => { if (!state?.captured && !(await action('capture'))) return; await action(state.picking ? 'stop_pick' : 'pick'); };
$('capture').onclick = () => action('capture'); $('restore').onclick = () => action('restore'); $('measure').onclick = () => action('measure');
$('apply').onclick = () => {
  try { const args = {}; for(const key of ['position','direction','up']) { args[key] = $(key).value.trim().split(/\s+/).map(Number); if(args[key].length!==3 || args[key].some(v=>!Number.isFinite(v))) throw new Error(t('좌표를 확인하세요.')); } args.field_sdk = Number($('field').value); if(!Number.isFinite(args.field_sdk)||args.field_sdk<=0)throw new Error(t('화각을 확인하세요.')); action('apply', args); } catch(error){toast(t(error.message));}
};
$('overlay').onclick = () => action('photo', {focal_px:Number($('focal').value)});
$('background').onclick = () => action('background');
$('save').onclick = async () => { if(busy)return;busy=true;render();try {const r=await api.save_project(state.session,state.capture_id);if(!r.ok)throw new Error(r.error);if(!r.cancelled)toast(t('결과를 저장했습니다.'));}catch(error){toast(t(error.message));}finally{busy=false;render();} };
$('fit').onclick = () => {zoom=1;pan={x:0,y:0};draw();};
$('zoomIn').onclick = () => {zoom=Math.min(8,zoom*1.25);draw();}; $('zoomOut').onclick = () => {zoom=Math.max(.1,zoom/1.25);draw();};
const canvas = $('photoCanvas');
canvas.oncontextmenu = event => event.preventDefault();
function pointAt(x,y) {
  if(!photo)return null;
  return Object.entries(imagePoints).reverse().find(([,pixel]) => {const p=C.toScreen(...pixel,view());return Math.hypot(p[0]-x,p[1]-y)<=14;})?.[0] || null;
}
canvas.onpointerdown = event => {
  if(!photo||busy)return;canvas.setPointerCapture(event.pointerId);
  drag={x:event.offsetX,y:event.offsetY,startX:event.offsetX,startY:event.offsetY,pan:event.button!==0};
  const id=connected&&event.button===0&&pointAt(event.offsetX,event.offsetY);
  if(id){selected=id;Object.assign(drag,{id,original:imagePoints[id].slice(),scale:view().scale,session:state.session,capture:state.capture_id});busy=true;render();canvas.style.cursor='grabbing';}
};
canvas.onpointermove = event => {
  if(!photo)return;
  if(drag?.pan){pan.x+=event.offsetX-drag.x;pan.y+=event.offsetY-drag.y;drag.x=event.offsetX;drag.y=event.offsetY;draw();}
  else if(drag?.id){
    imagePoints[drag.id]=[Math.max(0,Math.min(photo.width-1,drag.original[0]+(event.offsetX-drag.startX)/drag.scale)),Math.max(0,Math.min(photo.height-1,drag.original[1]+(event.offsetY-drag.startY)/drag.scale))];draw();
  }else canvas.style.cursor=pointAt(event.offsetX,event.offsetY)?'grab':'crosshair';
  const p=C.toImage(event.offsetX,event.offsetY,view(),photo.width,photo.height);$('cursor').textContent=p?p.map(v=>v.toFixed(1)).join(' , ')+' px':'—';
};
canvas.onpointerup = async event => {
  const start=drag;drag=null;canvas.style.cursor='crosshair';
  if(start?.id){
    const pixel=imagePoints[start.id];
    try{const r=await api.set_point(start.session,start.capture,start.id,...pixel);if(!r.ok)throw new Error(r.error);if(state.session===start.session&&state.capture_id===start.capture)imagePoints=r.image_points;}
    catch(error){if(state.session===start.session&&state.capture_id===start.capture)imagePoints[start.id]=start.original;toast(t(error.message));}
    finally{busy=false;render();}return;
  }
  if(!start||start.pan||Math.hypot(event.offsetX-start.startX,event.offsetY-start.startY)>4)return;
  if(!selected||!connected||busy)return;
  const p=C.toImage(event.offsetX,event.offsetY,view(),photo.width,photo.height);if(!p)return;
  busy=true;render();try{const r=await api.set_point(state.session,state.capture_id,selected,...p);if(!r.ok)throw new Error(r.error);imagePoints=r.image_points;selected=(state.points.find(p=>!imagePoints[p.id])||{id:selected}).id;}catch(error){toast(t(error.message));}finally{busy=false;render();}
};
canvas.onpointercancel=()=>{if(drag?.id){if(state.session===drag.session&&state.capture_id===drag.capture)imagePoints[drag.id]=drag.original;busy=false;}drag=null;render();};
canvas.onwheel=event=>{event.preventDefault();if(!photo)return;const before=view(),px=(event.offsetX-before.x)/before.scale,py=(event.offsetY-before.y)/before.scale;zoom=Math.max(.1,Math.min(8,zoom*Math.exp(-event.deltaY*.001)));const after=view();pan.x+=event.offsetX-(after.x+px*after.scale);pan.y+=event.offsetY-(after.y+py*after.scale);draw();};
new ResizeObserver(draw).observe($('stage'));
window.addEventListener('resize', draw);
function watchDpi() { const query = matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`); query.addEventListener('change', () => { draw(); watchDpi(); }, {once:true}); }
watchDpi();
window.addEventListener('pywebviewready',()=>{api=window.pywebview.api;poll();});
async function poll(){try{updatePreferences(await api.preferences());if(!busy){const result=await api.call('status');if(result.ok)accept(result);else{connected=false;render();}}}catch(_){connected=false;render();}finally{setTimeout(poll,1000);}}
render();
