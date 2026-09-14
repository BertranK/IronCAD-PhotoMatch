const $ = id => document.getElementById(id);
const ctx = $('photoCanvas').getContext('2d');
let api, state = null, connected = false, busy = false, photo = null, bitmap = null;
let imagePoints = {}, selected = null, zoom = 1, pan = {x: 0, y: 0}, drag = null, toastTimer;
let sessionKey = '', rowsKey = '', cameraKey = '';
const C = window.PhotoCoordinates;
const L = window.PhotoLanguage, t = L.t;
let review = null, fitResult = null;
let autoPreviewPending = false, calculating = false;
let preferencesKey = '', connectionError = '';
let opacityTimer = null;
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
  let addedPoint = null;
  const finishedPicking = state?.picking && result.state?.session === state?.session && !result.state?.picking;
  if (result.state) {
    if(!result.review && state?.session === result.state.session && (state.picking || result.state.picking)) {
      const previousIds = new Set((state.points || []).map(point=>point.id));
      addedPoint = (result.state.points || []).filter(point=>!previousIds.has(point.id)).at(-1);
    }
    state = result.state; review = result.review || null; fitResult = result.fit || null; connected = true;
    const key = state.session + ':' + state.capture_id;
    if (sessionKey !== key) { if (drag?.id) { drag = null; busy = false; } selected = null; imagePoints = {}; rowsKey = ''; sessionKey = key; }
    imagePoints = result.image_points || {};
    if(finishedPicking){selected=((state.points||[]).find(p=>!imagePoints[p.id])||(state.points||[])[0])?.id || null;addedPoint=null;}
    if(addedPoint)selected = addedPoint.id;
  }
  render();
  if(addedPoint||finishedPicking)$('points').querySelector('.point-row.selected')?.scrollIntoView({block:'nearest'});
  return result;
}
async function action(command, args = {}) {
  if (!api || busy) return;
  busy = true; render();
  try {
    const result = accept(await api.call(command, state?.session || '', args));
    if (['pick','stop_pick','rebind_point'].includes(command)) autoPreviewPending = true;
    return result;
  }
  catch (error) { toast(t(error.message)); }
  finally { busy = false; render(); }
}
function render() {
  $('connection').textContent = connected ? t('IronCAD 연결됨') : t('IronCAD 연결 대기');
  $('connection').title = connected ? '' : connectionError;
  $('connectionDot').classList.toggle('ready', connected);
  $('document').textContent = connected && (review || state)?.document ? (review || state).document : '—';
  const captured = connected && !review && state?.captured, points = connected ? (review || state)?.points || [] : [];
  const pending = points.some(p=>p.binding_status==='needs_reconnection');
  $('clearPoints').disabled = busy || !connected || !Object.keys(imagePoints).length || !!state?.adjusting_camera;
  $('pick').disabled = !!review || busy || !connected || !state?.document || (pending && !state?.picking);
  $('pick').textContent = state?.picking && connected ? t('점 선택 마치기') : t('＋ 모델 점 선택');
  $('capture').disabled = !!review || busy || !connected || !state?.document;
  $('measure').disabled = busy || !captured;
  $('restore').disabled = busy || !connected || !state?.captured;
  $('apply').disabled = !!review || busy || !connected || !(captured || state?.restored);
  $('background').disabled = busy || !captured || !photo;
  $('calculateSpinner').classList.toggle('hidden', !calculating);
  $('calculateLabel').textContent = calculating ? t('계산 중…') : t('카메라 계산');
  $('fitCamera').setAttribute('aria-busy', String(calculating));
  $('fitCamera').disabled = busy || !connected || !photo || Object.keys(imagePoints).length < 6 || pending;
  $('overlay').disabled = busy || !!review || !connected || !photo || !fitResult?.stable;
  $('photoOpacity').disabled = busy || !!review || !connected || !photo || state?.photo_opacity == null;
  if(!opacityTimer){$('photoOpacity').value=Math.round((state?.photo_opacity ?? 150/255)*100);$('photoOpacityValue').textContent=$('photoOpacity').value+'%';}
  $('save').disabled = busy || !connected || !(review || state)?.original_camera;
  $('openImage').disabled = $('openEmpty').disabled = busy;
  $('openProject').disabled = $('openEmptyProject').disabled = busy || !connected;
  const adjusting = !!state?.adjusting_camera;
  const pointsComplete = !review && !pending && !state?.picking && points.length >= 6 && points.every(p => imagePoints[p.id]);
  const showAlignment = !!photo && (pointsComplete || !!fitResult || adjusting);
  $('alignmentPanel').classList.toggle('hidden', !showAlignment);
  $('workspace').style.gridTemplateColumns = showAlignment ? '220px minmax(0,1fr) 280px' : '220px minmax(0,1fr)';
  $('closeImage').disabled = busy || !photo;
  $('estimatePrincipal').disabled = busy || adjusting;
  $('automaticRecalculate').disabled = busy || adjusting;
  $('principalHint').classList.toggle('hidden', !$('estimatePrincipal').checked);
  $('reconnectModel').classList.toggle('hidden', !review);
  $('reconnectModel').disabled = busy || !connected;
  $('replaceModel').classList.toggle('hidden', !photo || !Object.keys(imagePoints).length);
  $('replaceModel').disabled = busy || !connected || !state?.document || adjusting;
  $('replaceHint').classList.toggle('hidden', !review && !pending);
  $('replaceHint').textContent = pending && !review ? t('행을 선택하고 모델 점을 다시 연결하세요. 사진 위치는 유지됩니다.') : t('새 모델을 열고 연결하세요. 사진과 점 위치는 유지됩니다.');
  $('adjustCamera').disabled = busy || !!review || !state?.photo_workflow_version || !state?.photo_rectangle_physical || adjusting;
  for(const id of ['saveCamera','cancelCamera','adjustHint'])$(id).classList.toggle('hidden', !adjusting);
  $('saveCamera').disabled = $('cancelCamera').disabled = busy;
  $('useCamera').classList.toggle('hidden', (!state?.manual_camera&&fitResult?.source!=='manual') || adjusting);
  $('useCamera').disabled = busy || !!review;
  if(adjusting){$('fitCamera').disabled=true;$('overlay').disabled=true;$('pick').disabled=true;$('apply').disabled=true;}
  if(fitResult?.source==='manual')$('overlay').disabled=true;
  $('pairCount').textContent = `${Object.keys(imagePoints).length} / ${points.length}`;
  $('alignmentStatus').textContent = review || pending ? t('모델 연결을 확인하세요.') : !fitResult ? t('계산 대기') : !fitResult.stable ? t('깊이가 다른 점을 추가하세요.') : fitResult.precision_passed ? t('대응점 일치') : t('대응점을 확인하세요.');
  $('fitSummary').textContent = fitResult ? `${fitResult.source==='manual'?t('수동 카메라')+' · ':''}${t('최대 오차')} · ${fitResult.max_error_px?.toFixed(2) ?? t('재측정 필요')} ${t('원본 사진 px')}` : points.filter(p=>imagePoints[p.id]).length >= 6 ? t('계산 대기') : t('대응점 6개 이상');
  if(adjusting)$('alignmentStatus').textContent=t('카메라 조정 중');
  else if(fitResult?.source==='manual')$('alignmentStatus').textContent=t('수동 카메라 저장됨');
  $('alignmentDot').classList.toggle('ready', !review && !!fitResult?.precision_passed);
  $('stepImage').classList.toggle('active', !photo);
  $('stepPoints').classList.toggle('active', !!photo && !showAlignment);
  $('stepResult').classList.toggle('active', showAlignment);
  $('footerState').textContent = !connected ? t('연결 대기') : busy ? t('작업 중') : state?.picking ? t('모델 점 선택 중') : t('준비');
  if (selected && !points.some(p => p.id === selected)) selected = null;
  if (!selected && points.length) selected = (points.find(p => !imagePoints[p.id]) || points[0]).id;
  const key = JSON.stringify([points, imagePoints, selected, fitResult, !!review, busy, adjusting, connected, state?.point_delete_supported]);
  if (key !== rowsKey) {
    rowsKey = key; $('points').replaceChildren();
    if (!points.length) { const empty = document.createElement('p'); empty.className = 'px-3 pt-8 text-center text-xs text-slate-600'; empty.textContent = t('선택한 점이 여기에 표시됩니다'); $('points').append(empty); }
    for (const point of points) {
      const row = document.createElement('div'); row.className = 'point-row' + (selected === point.id ? ' selected' : '');
      const title = document.createElement('div'); title.className = 'flex items-center justify-between mb-2';
      const id = document.createElement('button'); id.className = 'coordinate text-mint'; id.textContent = point.id; id.onclick = () => { selected = point.id; render(); };
      const dot = document.createElement('span'); dot.className = 'dot' + (imagePoints[point.id] ? ' ready' : ''); title.append(id, dot);
      const name = document.createElement('p'); name.className = 'truncate text-xs text-slate-300 mb-1'; name.textContent = point.object_name || t('연결되지 않음');
      const pixel = document.createElement('p'); pixel.className = 'coordinate text-slate-500'; pixel.textContent = imagePoints[point.id] ? imagePoints[point.id].map(v => v.toFixed(1)).join(' , ') : t('사진에서 위치 선택');
      const fitIndex=fitResult?.ids.indexOf(point.id) ?? -1; if(fitIndex>=0&&Number.isFinite(fitResult.point_errors_px[fitIndex]))pixel.textContent+=` · ${fitResult.point_errors_px[fitIndex].toFixed(2)} px`;
      if(point.object_name && point.binding_status==='needs_reconnection')pixel.textContent+=' · '+t('모델 재연결 필요');
      row.title = `API: ${point.api_coordinates.join(', ')}\n${t('변환 후보')}: ${point.transformed_coordinates.join(', ')}\n${t('좌표계 검증 대기')}`;
      const select = document.createElement('button'); select.className = 'point-select w-full text-left';
      select.append(name, pixel); select.onclick = () => { selected = point.id; render(); }; row.append(title, select);
      const reconnect = document.createElement('button'); reconnect.className = 'mini reconnect-point';
      const reconnectIcon = document.createElement('span'); reconnectIcon.textContent = '↻'; reconnectIcon.setAttribute('aria-hidden','true');
      reconnect.append(reconnectIcon, ' ' + t('다시 연결')); reconnect.title = t('모델 점 다시 연결'); reconnect.setAttribute('aria-label', point.id + ' · ' + t('모델 점 다시 연결'));
      reconnect.disabled = !!review || busy || adjusting || !connected;
      reconnect.onclick = async () => { selected = point.id; const result = await action('rebind_point', {id:point.id}); if(result?.state.picking)toast(point.id + ' · ' + t('IronCAD에서 대응하는 꼭짓점을 선택하세요.')); };
      const remove = document.createElement('button'); remove.className = 'mini delete-point'; remove.textContent = '×';
      remove.title = t('대응점 삭제'); remove.setAttribute('aria-label', point.id + ' · ' + t('대응점 삭제'));
      remove.disabled = busy || adjusting || !connected || (!review && !state?.point_delete_supported);
      if(!review && !state?.point_delete_supported)remove.title = t('새 IronCAD 연결 모듈이 필요합니다.');
      remove.onclick = async () => {
        if(busy)return;busy=true;autoPreviewPending=false;render();
        try {accept(await api.delete_point(state.session,state.capture_id,point.id));autoPreviewPending=true;}
        catch(error){toast(t(error.message));}finally{busy=false;render();}
      };
      title.insertBefore(reconnect, dot); title.insertBefore(remove, dot); $('points').append(row);
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
  if (autoPreviewPending && $('automaticRecalculate').checked && photo && connected && pointsComplete && !busy && !adjusting) {
    autoPreviewPending = false;
    autoPreview();
  }
}
async function autoPreview() {
  busy = true; calculating = true; render();
  try {
    accept(await api.fit_points($('estimatePrincipal').checked));
    calculating = false; render();
    if (fitResult?.stable) accept(await api.preview_fit());
  } catch (error) { toast(t(error.message)); }
  finally { busy = false; calculating = false; render(); }
}
function view() { return photo ? C.fit(photo.width, photo.height, $('stage').clientWidth, $('stage').clientHeight, zoom, pan) : null; }
function draw() {
  const canvas = $('photoCanvas'), dpr = window.devicePixelRatio || 1, w = $('stage').clientWidth, h = $('stage').clientHeight;
  if (canvas.width !== Math.round(w*dpr) || canvas.height !== Math.round(h*dpr)) { canvas.width = Math.round(w*dpr); canvas.height = Math.round(h*dpr); }
  ctx.setTransform(w ? canvas.width/w : 1,0,0,h ? canvas.height/h : 1,0,0); ctx.clearRect(0,0,w,h);
  if (!photo || !bitmap) return;
  const v = view(); ctx.drawImage(bitmap, v.x,v.y,photo.width*v.scale,photo.height*v.scale);
  ctx.strokeStyle = '#54636b'; ctx.lineWidth = 1; ctx.strokeRect(v.x,v.y,photo.width*v.scale,photo.height*v.scale);
  ctx.save(); ctx.globalAlpha = 0.7;
  for (const [id, pixel] of Object.entries(imagePoints)) {
    const [x,y] = C.toScreen(...pixel,v); const active = id === selected;
    ctx.beginPath(); ctx.arc(x,y,active?9:7,0,Math.PI*2); ctx.fillStyle = '#101416dd'; ctx.fill(); ctx.strokeStyle = active?'#b8f582':'#ffffff'; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.beginPath(); ctx.moveTo(x-13,y);ctx.lineTo(x+13,y);ctx.moveTo(x,y-13);ctx.lineTo(x,y+13);ctx.stroke();
    ctx.font = '11px Segoe UI'; const tw = ctx.measureText(id).width; ctx.fillStyle = '#101416e6';ctx.fillRect(x+12,y-23,tw+12,20);ctx.fillStyle = '#b8f582';ctx.fillText(id,x+18,y-9);
  }
  if(fitResult)for(let i=0;i<fitResult.predicted_image_px.length;i++){
    const observed=imagePoints[fitResult.ids[i]];if(!observed)continue;
    const a=C.toScreen(...observed,v),b=C.toScreen(...fitResult.predicted_image_px[i],v);
    ctx.strokeStyle='#f7ba68';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(...a);ctx.lineTo(...b);ctx.stroke();
    ctx.strokeRect(b[0]-3,b[1]-3,6,6);
  }
  ctx.restore();
  $('zoom').textContent = Math.round(v.scale*100)+'%';
}
async function openImage(project = false) {
  if (!api || busy) return; busy = true; render();
  try {
    const result = project ? await api.open_project() : await api.open_image(); if (!result.ok) throw new Error(result.error); if (result.cancelled) return;
    const next = new Image(); next.src = result.image.preview; await next.decode();
    photo = result.image; bitmap = next; imagePoints = {}; fitResult = null; autoPreviewPending = false; zoom = 1; pan = {x:0,y:0};
    if (project) accept(result); else review = null;
    $('empty').classList.add('hidden'); $('imageName').textContent = photo.name; $('imageSize').textContent = `${photo.width} × ${photo.height}`;
  } catch(error) { toast(t(error.message)); } finally { busy = false; render(); }
}
$('openImage').onclick = $('openEmpty').onclick = () => openImage();
$('openProject').onclick = $('openEmptyProject').onclick = () => openImage(true);
$('closeImage').onclick = async () => {
  if(!api||busy)return;busy=true;render();
  try {autoPreviewPending=false;accept(await api.close_image());photo=bitmap=null;imagePoints={};fitResult=null;selected=null;
    $('empty').classList.remove('hidden');$('imageName').textContent=t('사진');$('imageSize').textContent='';}
  catch(error){toast(t(error.message));}finally{busy=false;render();}
};
$('reconnectModel').onclick=()=>fitAction('reconnect_model');
$('clearPoints').onclick=async()=>{
  if(!api||busy)return;busy=true;autoPreviewPending=false;render();
  try{const result=await api.clear_points(state.session,state.capture_id);if(result.ok)selected=null;accept(result);}
  catch(error){toast(t(error.message));}finally{busy=false;render();}
};
$('replaceModel').onclick=async()=>{
  if(!api||busy)return;busy=true;render();
  try{accept(await api.replace_model(state.session,state.capture_id));toast(t('행을 선택하고 모델 점을 다시 연결하세요. 사진 위치는 유지됩니다.'));}
  catch(error){toast(t(error.message));}finally{busy=false;render();}
};
$('automaticRecalculate').onchange=()=>{autoPreviewPending=false;};
$('estimatePrincipal').onchange=()=>fitAction('set_principal_estimation',$('estimatePrincipal').checked);
$('pick').onclick = async () => { if (!state?.captured && !state?.restored && !(await action('capture'))) return; await action(state.picking ? 'stop_pick' : 'pick'); };
$('capture').onclick = () => action('capture'); $('restore').onclick = () => action('restore'); $('measure').onclick = () => action('measure');
$('apply').onclick = () => {
  try { const args = {}; for(const key of ['position','direction','up']) { args[key] = $(key).value.trim().split(/\s+/).map(Number); if(args[key].length!==3 || args[key].some(v=>!Number.isFinite(v))) throw new Error(t('좌표를 확인하세요.')); } args.field_sdk = Number($('field').value); if(!Number.isFinite(args.field_sdk)||args.field_sdk<=0)throw new Error(t('화각을 확인하세요.')); action('apply', args); } catch(error){toast(t(error.message));}
};
async function fitAction(method,...args) {if(!api||busy)return;busy=true;calculating=method==='fit_points';render();try{accept(await api[method](...args));}catch(error){toast(t(error.message));}finally{busy=false;calculating=false;render();}}
$('fitCamera').onclick=()=>fitAction('fit_points',$('estimatePrincipal').checked);
$('overlay').onclick=()=>fitAction('preview_fit');
$('photoOpacity').oninput=()=>{
  clearTimeout(opacityTimer);$('photoOpacityValue').textContent=$('photoOpacity').value+'%';
  const value=Number($('photoOpacity').value)/100;
  const session=state?.session;
  opacityTimer=setTimeout(async()=>{
    try{if(session===state?.session)await action('photo_opacity',{opacity:value});}
    finally{opacityTimer=null;render();}
  },120);
};
$('adjustCamera').onclick=()=>fitAction('adjust_camera','begin');
$('saveCamera').onclick=()=>fitAction('adjust_camera','save');
$('cancelCamera').onclick=()=>fitAction('adjust_camera','cancel');
$('useCamera').onclick=()=>fitAction('adjust_camera','use');
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
  if(id){fitResult=null;selected=id;Object.assign(drag,{id,original:imagePoints[id].slice(),scale:view().scale,session:state.session,capture:state.capture_id});busy=true;render();canvas.style.cursor='grabbing';}
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
    try{const r=await api.set_point(start.session,start.capture,start.id,...pixel);if(!r.ok)throw new Error(r.error);if(state.session===start.session&&state.capture_id===start.capture){imagePoints=r.image_points;autoPreviewPending=true;}}
    catch(error){if(state.session===start.session&&state.capture_id===start.capture)imagePoints[start.id]=start.original;toast(t(error.message));}
    finally{busy=false;render();}return;
  }
  if(!start||start.pan||Math.hypot(event.offsetX-start.startX,event.offsetY-start.startY)>4)return;
  if(!connected||busy)return;
  const p=C.toImage(event.offsetX,event.offsetY,view(),photo.width,photo.height);if(!p)return;
  if(!selected){toast(t('먼저 모델 점 선택을 누르고 모델의 꼭짓점을 선택하세요.'));return;}
  fitResult=null;busy=true;render();try{const r=await api.set_point(state.session,state.capture_id,selected,...p);if(!r.ok)throw new Error(r.error);imagePoints=r.image_points;autoPreviewPending=true;selected=((review || state).points.find(p=>!imagePoints[p.id])||{id:selected}).id;}catch(error){toast(t(error.message));}finally{busy=false;render();}
};
canvas.onpointercancel=()=>{if(drag?.id){if(state.session===drag.session&&state.capture_id===drag.capture)imagePoints[drag.id]=drag.original;busy=false;}drag=null;render();};
canvas.onwheel=event=>{event.preventDefault();if(!photo)return;const before=view(),px=(event.offsetX-before.x)/before.scale,py=(event.offsetY-before.y)/before.scale;zoom=Math.max(.1,Math.min(8,zoom*Math.exp(-event.deltaY*.001)));const after=view();pan.x+=event.offsetX-(after.x+px*after.scale);pan.y+=event.offsetY-(after.y+py*after.scale);draw();};
new ResizeObserver(draw).observe($('stage'));
window.addEventListener('resize', draw);
function watchDpi() { const query = matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`); query.addEventListener('change', () => { draw(); watchDpi(); }, {once:true}); }
watchDpi();
window.addEventListener('pywebviewready',()=>{api=window.pywebview.api;poll();});
async function poll(){try{updatePreferences(await api.preferences());if(!busy){const result=await api.call('status');if(result.ok)accept(result);else{connectionError=t(result.error);connected=false;render();}}}catch(error){connectionError=t(error.message);connected=false;render();}finally{setTimeout(poll,1000);}}
render();
