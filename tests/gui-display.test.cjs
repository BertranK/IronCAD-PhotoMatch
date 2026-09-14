const {test} = require('node:test');
const assert = require('node:assert/strict');
const {readFileSync,mkdirSync} = require('node:fs');
const {resolve} = require('node:path');
const {pathToFileURL} = require('node:url');
const {chromium} = require('../gui/web/node_modules/playwright');

test('clicking the photo without a model point shows picking guidance', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.pointCalls=[];window.testPoints={};window.testState={session:'a',capture_id:1,captured:true,document:'brick.ics',points:[]};
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'en'}),
        call:async()=>({ok:true,state:structuredClone(testState),image_points:structuredClone(testPoints)}),
        open_image:async()=>({ok:true,image:{preview,width:1600,height:1000,name:'reference.png'}}),
        set_point:async(...args)=>{pointCalls.push(args);testPoints[args[2]]=args.slice(-2);return {ok:true,image_points:structuredClone(testPoints)};}}};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    await page.locator('#openImage').click();await page.waitForFunction(()=>photo!==null);
    const center=await page.evaluate(()=>{const v=view(),r=photoCanvas.getBoundingClientRect();return {x:r.x+v.x+800*v.scale,y:r.y+v.y+500*v.scale};});
    await page.mouse.click(center.x,center.y,{button:'right'});
    assert.equal(await page.locator('#toast').isVisible(),false);
    await page.mouse.click(center.x,center.y);
    assert.equal(await page.locator('#toast').isVisible(),true);
    assert.match(await page.locator('#toast').textContent(),/Pick model points.*model vertex first/);
    assert.deepEqual(await page.evaluate(()=>pointCalls),[]);
    await page.evaluate(()=>{testState.points=[{id:'P1',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}];state=structuredClone(testState);render();});
    await page.mouse.click(center.x,center.y);await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>pointCalls.length),1);
    assert.equal(await page.evaluate(()=>pointCalls[0][2]),'P1');
    await page.evaluate(()=>{
      state.picking=true;testState.picking=true;
      testState.points.push({id:'P7',api_coordinates:[4,5,6],transformed_coordinates:[4,5,6]});
      accept({ok:true,state:structuredClone(testState),image_points:structuredClone(imagePoints)});
    });
    assert.equal(await page.locator('.point-row.selected .coordinate').first().textContent(),'P7');
    assert.equal(await page.evaluate(()=>selected),'P7');
    await page.mouse.click(center.x+50,center.y+30);await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>pointCalls[1][2]),'P7');
    await page.evaluate(()=>{testState.picking=false;accept({ok:true,state:structuredClone(testState),image_points:structuredClone(imagePoints)});});
    assert.equal(await page.evaluate(()=>selected),'P1');
    await page.mouse.click(center.x,center.y);await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>pointCalls[2][2]),'P1');
    assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});

test('finishing additional model picks selects the first unmatched photo card', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.testPoints=Object.fromEntries(Array.from({length:6},(_,i)=>['P'+(i+1),[100+i*150,300]]));
      window.testState={session:'a',capture_id:1,captured:true,document:'brick.ics',picking:false,
        points:Object.keys(testPoints).map(id=>({id,api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}))};
      window.pointCalls=[];
      const result=()=>({ok:true,state:structuredClone(testState),image_points:structuredClone(testPoints)});
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'en'}),
        call:async command=>{if(command==='pick')testState.picking=true;if(command==='stop_pick')testState.picking=false;return result();},
        open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'reference.png'}}),
        set_point:async(s,c,id,x,y)=>{pointCalls.push(id);testPoints[id]=[x,y];return result();}}};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    await page.locator('#openProject').click();await page.waitForFunction(()=>photo!==null);
    await page.locator('#automaticRecalculate').uncheck();
    assert.equal(await page.locator('#fitSummary').textContent(),'Ready to calculate');
    const original=await page.evaluate(()=>structuredClone(imagePoints));
    await page.locator('#pick').click();await page.waitForFunction(()=>!busy&&state.picking);
    await page.evaluate(()=>{
      for(const id of ['P7','P8'])testState.points.push({id,api_coordinates:[4,5,6],transformed_coordinates:[4,5,6]});
      accept({ok:true,state:structuredClone(testState),image_points:structuredClone(testPoints)});
    });
    assert.equal(await page.evaluate(()=>selected),'P8');
    await page.locator('#pick').click();await page.waitForFunction(()=>!busy&&!state.picking);
    assert.equal(await page.locator('.point-row.selected .coordinate').first().textContent(),'P7');
    assert.deepEqual(await page.evaluate(()=>imagePoints),original);
    const position=await page.evaluate(()=>{const v=view(),r=photoCanvas.getBoundingClientRect();return {x:r.x+v.x+1100*v.scale,y:r.y+v.y+600*v.scale};});
    await page.mouse.click(position.x,position.y);await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>selected),'P8');
    assert.deepEqual(await page.evaluate(()=>pointCalls),['P7']);
    assert.deepEqual(await page.evaluate(()=>Object.fromEntries(Object.entries(imagePoints).filter(([id])=>id!=='P7'))),original);
    assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});

test('delete removes only its pair and disables fitting below six matches', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.testPoints=Object.fromEntries(Array.from({length:6},(_,i)=>['P'+(i+1),[100+i*150,300]]));
      window.deleted=[];window.fitCount=0;window.opacityCalls=[];
      window.testState={session:'a',capture_id:1,captured:true,document:'brick.ics',point_delete_supported:true,photo_opacity:150/255,
        points:Object.keys(testPoints).map(id=>({id,object_name:'Brick',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}))};
      const result=()=>({ok:true,state:structuredClone(testState),image_points:structuredClone(testPoints),fit:null});
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'en'}),call:async(command,session,args)=>{if(command==='photo_opacity'){opacityCalls.push([session,args.opacity]);testState.photo_opacity=args.opacity;}return result();},
        open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'reference.png'}}),
        delete_point:async(session,capture,id)=>{deleted.push([session,capture,id]);testState.points=testState.points.filter(p=>p.id!==id);delete testPoints[id];return result();},
        clear_points:async(session,capture)=>{deleted.push([session,capture,'photo']);testPoints={};return result();},
        fit_points:async()=>{fitCount++;return result();}}};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    await page.locator('#openProject').click();await page.waitForFunction(()=>photo!==null);
    const originalPoints=await page.evaluate(()=>structuredClone(imagePoints));
    for(const value of [0,25,100]){
      await page.locator('#photoOpacity').evaluate((el,value)=>{el.value=value;el.dispatchEvent(new Event('input'));},value);
      await page.waitForFunction(value=>!busy&&state.photo_opacity===value/100,value);
      assert.equal(await page.locator('#photoOpacityValue').textContent(),value+'%');
    }
    assert.deepEqual(await page.evaluate(()=>opacityCalls),[['a',0],['a',.25],['a',1]]);
    assert.deepEqual(await page.evaluate(()=>imagePoints),originalPoints);
    await page.getByRole('button',{name:'P3 · Delete point pair',exact:true}).click();
    await page.waitForFunction(()=>!busy);
    assert.deepEqual(await page.evaluate(()=>deleted),[['a',1,'P3']]);
    assert.deepEqual(await page.evaluate(()=>Object.keys(imagePoints)),['P1','P2','P4','P5','P6']);
    assert.deepEqual(await page.evaluate(()=>imagePoints.P4),[550,300]);
    assert.equal(await page.locator('.delete-point').count(),5);
    assert.equal(await page.locator('#fitCamera').isDisabled(),true);
    assert.equal(await page.evaluate(()=>fitCount),0);
    await page.evaluate(()=>{state.adjusting_camera=true;render();});
    assert.equal(await page.locator('.delete-point:not(:disabled)').count(),0);
    assert.equal(await page.locator('#clearPoints').isDisabled(),true);
    await page.evaluate(()=>{state.adjusting_camera=false;imagePoints={};render();});
    assert.equal(await page.locator('#clearPoints').isDisabled(),true);
    await page.evaluate(()=>{imagePoints=structuredClone(testPoints);selected='P6';render();});
    assert.equal(await page.locator('#clearPoints').isDisabled(),false);
    await page.locator('#clearPoints').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('.point-row').count(),5);
    assert.equal(await page.locator('#clearPoints').isDisabled(),true);
    assert.equal(await page.evaluate(()=>photo.name),'reference.png');
    assert.equal(await page.evaluate(()=>state.capture_id),1);
    assert.deepEqual(await page.evaluate(()=>deleted.at(-1)),['a',1,'photo']);
    assert.equal(await page.evaluate(()=>selected),'P1');
    assert.deepEqual(await page.evaluate(()=>state.points.map(p=>p.id)),['P1','P2','P4','P5','P6']);
    assert.deepEqual(await page.evaluate(()=>imagePoints),{});
    assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});

test('photo close and manual camera adjustment preserve points until explicit close', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.testPoints=Object.fromEntries(Array.from({length:6},(_,i)=>['P'+(i+1),[200+i*150,300]]));window.actions=[];window.testFit=null;
      const state={session:'a',capture_id:1,captured:true,document:'brick.ics',photo_workflow_version:1,
        photo_rectangle_physical:[0,0,750,750],points:Object.keys(testPoints).map(id=>({id,object_name:'Brick',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}))};
      const result=()=>({ok:true,state,image_points:structuredClone(testPoints),fit:testFit});
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'ko'}),call:async()=>result(),
        open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'reference.png'}}),
        adjust_camera:async action=>{actions.push(action);state.adjusting_camera=action==='begin';
          if(action==='save'){state.manual_camera={};testFit={source:'manual',stable:true,precision_passed:false,ids:['P1'],max_error_px:null,point_errors_px:[],predicted_image_px:[]};}return result();},
        close_image:async()=>{actions.push('close');testPoints={};testFit=null;delete state.photo_rectangle_physical;return {...result(),image:null};},
        set_principal_estimation:async enabled=>{actions.push(enabled);return result();}}};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    assert.equal(await page.locator('#openEmptyProject').isVisible(),true);
    const photoButton=await page.locator('#openEmpty').boundingBox(),projectButton=await page.locator('#openEmptyProject').boundingBox();
    assert.ok(projectButton.x>photoButton.x+photoButton.width);
    await page.locator('#openEmptyProject').click();await page.waitForFunction(()=>photo!==null);
    await page.locator('#estimatePrincipal').check();await page.waitForFunction(()=>!busy);
    await page.locator('#adjustCamera').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('#saveCamera').isVisible(),true);assert.equal(await page.locator('#overlay').isDisabled(),true);
    await page.locator('#cancelCamera').click();await page.waitForFunction(()=>!busy);
    assert.deepEqual(await page.evaluate(()=>testPoints.P1),[200,300]);
    await page.locator('#adjustCamera').click();await page.waitForFunction(()=>!busy);
    await page.locator('#saveCamera').click();await page.waitForFunction(()=>!busy);
    assert.match(await page.locator('#fitSummary').textContent(),/수동 카메라.*재측정 필요/);
    await page.locator('#closeImage').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('#empty').isVisible(),true);assert.deepEqual(await page.evaluate(()=>imagePoints),{});
    assert.deepEqual(await page.evaluate(()=>actions),[true,'begin','cancel','begin','save','close']);assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});

test('calculate exposes residuals and editing automatically refreshes camera preview', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.testPoints=Object.fromEntries(Array.from({length:6},(_,i)=>['P'+(i+1),[200+i*150,300]]));
      window.testFit=null;window.previewCount=0;
      const state={session:'test',capture_id:1,captured:true,document:'sample.ics',points:Object.keys(testPoints).map(id=>({id,object_name:'Brick',api_coordinates:[0,0,0],transformed_coordinates:[0,0,0]}))};
      const result=()=>({ok:true,state,image_points:structuredClone(testPoints),fit:testFit});
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'en',selection:'en'}),call:async()=>result(),
        open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'sample.png'}}),
        fit_points:async()=>{await new Promise(resolve=>setTimeout(resolve,400));testFit={ids:Object.keys(testPoints),stable:true,precision_passed:false,max_error_px:3.4,point_errors_px:Array(6).fill(3.4),predicted_image_px:Object.values(testPoints).map(p=>[p[0]+3.4,p[1]])};return result();},
        preview_fit:async()=>{previewCount++;return result();},
        set_point:async(s,c,id,x,y)=>{testPoints[id]=[x,y];testFit=null;return result();}}};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    await page.locator('#openProject').click();await page.waitForFunction(()=>photo!==null);
    assert.equal(await page.locator('#overlay').isDisabled(),true);
    await page.evaluate(()=>{delete imagePoints.P6;render();});
    assert.equal(await page.locator('#alignmentPanel').isVisible(),false);
    await page.evaluate(()=>{imagePoints.P6=testPoints.P6;state.picking=true;render();});
    assert.equal(await page.locator('#alignmentPanel').isVisible(),false);
    await page.evaluate(()=>{state.picking=false;render();});
    assert.equal(await page.locator('#alignmentPanel').isVisible(),true);
    await page.locator('#fitCamera').click();
    assert.equal(await page.locator('#calculateSpinner').isVisible(),true);
    assert.equal(await page.locator('#calculateLabel').textContent(),'Calculating…');
    await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('#calculateSpinner').isVisible(),false);
    assert.match(await page.locator('#fitSummary').textContent(),/3.40/);
    assert.equal(await page.locator('#alignmentDot').evaluate(e=>e.classList.contains('ready')),false);
    await page.locator('#overlay').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>previewCount),1);
    const p=await page.evaluate(()=>{const v=view(),r=photoCanvas.getBoundingClientRect();return {x:r.x+v.x+200*v.scale,y:r.y+v.y+300*v.scale};});
    await page.mouse.move(p.x,p.y);await page.mouse.down();await page.mouse.move(p.x+15,p.y+5);await page.mouse.up();await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>previewCount),2);
    assert.equal(await page.locator('#calculateSpinner').isVisible(),false);
    await page.locator('#automaticRecalculate').uncheck();
    const q=await page.evaluate(()=>{const v=view(),r=photoCanvas.getBoundingClientRect(),pt=testPoints.P1;return {x:r.x+v.x+pt[0]*v.scale,y:r.y+v.y+pt[1]*v.scale};});
    await page.mouse.move(q.x,q.y);await page.mouse.down();await page.mouse.move(q.x+10,q.y+5);await page.mouse.up();await page.waitForFunction(()=>!busy);
    assert.equal(await page.evaluate(()=>previewCount),2);
    assert.equal(await page.evaluate(()=>fitResult),null);
    await page.locator('#fitCamera').click();await page.waitForFunction(()=>!busy);
    assert.notEqual(await page.evaluate(()=>fitResult),null);
    await page.locator('#automaticRecalculate').check();
    assert.equal(await page.evaluate(()=>previewCount),2);
    await page.evaluate(()=>{api.fit_points=async()=>{throw new Error('Calculation failed');};});
    await page.locator('#fitCamera').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('#calculateSpinner').isVisible(),false);
    assert.equal(await page.locator('#calculateLabel').textContent(),'Calculate camera');
    assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});

test('display scaling, live theme/language changes and resize preserve original photo coordinates', async () => {
  const browser = await chromium.launch({channel:'msedge', headless:true});
  const preview = 'data:image/png;base64,' + readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64');
  try {
    for (const dpr of [1, 1.25, 1.37, 1.5, 1.75, 2]) {
      const context = await browser.newContext({viewport:{width:1320,height:860},deviceScaleFactor:dpr});
      const page = await context.newPage();
      const errors=[]; page.on('pageerror', error => errors.push(error.message));
      await page.addInitScript(({preview}) => {
        window.testPrefs={theme:'dark',selection:'system',language:'ko',system_language:'ko-KR'};
        window.testPoints={};
        const state={session:'test',capture_id:1,captured:true,document:'사용자 모델.ics',sdk_version:'test',
          points:[{id:'P1',object_name:'사용자 부품',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}]};
        window.pywebview={api:{
          preferences:async()=>window.testPrefs,
          set_language:async selection=>Object.assign(window.testPrefs,{selection,language:selection==='system'?'ko':selection}),
          call:async()=>({ok:true,state,image_points:window.testPoints}),
          open_image:async()=>({ok:true,image:{preview,width:1600,height:1000,name:'사진 원본.png'}}),
          set_point:async(session,capture,id,x,y)=>{window.testPoints[id]=[x,y];return {ok:true,image_points:window.testPoints};}
        }};
      }, {preview});
      await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
      await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
      await page.locator('#openImage').click();
      await page.waitForFunction(()=>document.getElementById('empty').classList.contains('hidden'));
      for (const viewport of [{width:1320,height:860},{width:960,height:640},{width:600,height:480},{width:600,height:320}]) {
        await page.setViewportSize(viewport);
        for (const theme of ['light','dark']) {
          await page.evaluate(theme=>{window.testPrefs.theme=theme;updatePreferences(window.testPrefs);},theme);
          await page.locator('#language').selectOption(theme==='light'?'en':'ko');
          await page.waitForFunction(theme=>document.documentElement.dataset.theme===theme && document.documentElement.lang===(theme==='light'?'en':'ko'),theme);
          const metrics=await page.evaluate(()=>{
            const canvas=document.getElementById('photoCanvas'),stage=document.getElementById('stage');
            const v=view(),rect=canvas.getBoundingClientRect();
            return {cw:canvas.width,ch:canvas.height,w:stage.clientWidth,h:stage.clientHeight,
              x:rect.x+v.x+640*v.scale,y:rect.y+v.y+350*v.scale,
              overflow:document.documentElement.scrollWidth>innerWidth,
              name:document.getElementById('imageName').textContent,doc:document.getElementById('document').textContent,
              scheme:getComputedStyle(document.documentElement).colorScheme,
              controls:['language','save','openImage','openProject'].map(id=>{const r=document.getElementById(id).getBoundingClientRect();return r.x>=0&&r.right<=innerWidth&&r.bottom<=innerHeight;})};
          });
          assert.equal(metrics.cw,Math.round(metrics.w*dpr)); assert.equal(metrics.ch,Math.round(metrics.h*dpr));
          assert.ok(metrics.w>0&&metrics.h>0); assert.equal(metrics.overflow,false); assert.ok(metrics.controls.every(Boolean));
          assert.equal(metrics.scheme,theme); assert.equal(metrics.name,'사진 원본.png'); assert.equal(metrics.doc,'사용자 모델.ics');
          if (dpr===1.5 && (viewport.width===1320 || viewport.height===320)) {
            const directory=resolve(__dirname,'../evidence');mkdirSync(directory,{recursive:true});
            await page.screenshot({path:resolve(directory,`browser-ui-${theme}-150-${viewport.width}x${viewport.height}.png`)});
          }
          await page.mouse.click(metrics.x,metrics.y);
          const point=await page.evaluate(()=>window.testPoints.P1);
          // Browser input quantizes CSS positions; preserve the same source pixel within that input precision.
          const scale=await page.evaluate(()=>view().scale);
          assert.ok(Math.hypot(point[0]-640,point[1]-350)<=1.5/scale);
          const before=JSON.stringify(point);
          await page.locator('#zoomIn').click();
          await page.evaluate(()=>{testPrefs.theme=testPrefs.theme==='dark'?'light':'dark';updatePreferences(testPrefs);});
          assert.equal(await page.evaluate(()=>JSON.stringify(window.testPoints.P1)),before);
          await page.locator('#fit').click();
        }
      }
      // A running WebView can change DPI without being recreated.
      const session=await context.newCDPSession(page);
      await session.send('Emulation.setDeviceMetricsOverride',{width:960,height:640,deviceScaleFactor:2, mobile:false});
      await page.waitForFunction(()=>photoCanvas.width===Math.round(stage.clientWidth*2));
      assert.deepEqual(errors,[]);
      await context.close();
    }
  } finally { await browser.close(); }
});

test('restored photo points can be reopened, dragged, and replaced without changing other points', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  const preview='data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64');
  try {
    for(const dpr of [1,1.5]){
      const context=await browser.newContext({viewport:{width:1320,height:860},deviceScaleFactor:dpr});
      const page=await context.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
      await page.addInitScript(({preview})=>{
        window.testPoints={P1:[200,200],P2:[1000,200],P3:[1000,700],P5:[200,700]};
        window.testState={session:'test',capture_id:1,captured:false,restored:true,document:'sample.ics',sdk_version:'test',points:Object.keys(testPoints).map(id=>({id,object_name:'Brick',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}))};
        window.failPoint=false;
        const result=()=>({ok:true,state:structuredClone(testState),image_points:structuredClone(testPoints)});
        window.pywebview={api:{preferences:async()=>({theme:'dark',selection:'en',language:'en'}),call:async()=>result(),
          open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'sample.png'}}),
          set_point:async(session,capture,id,x,y)=>{if(failPoint)return {ok:false,error:'Rejected edit'};testPoints[id]=[x,y];return {ok:true,image_points:structuredClone(testPoints)};}}};
      },{preview});
      await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
      await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
      await page.locator('#openProject').click();await page.waitForFunction(()=>photo!==null);
      const screen=async id=>page.evaluate(id=>{const v=view(),r=photoCanvas.getBoundingClientRect(),p=PhotoCoordinates.toScreen(...imagePoints[id],v);return {x:r.x+p[0],y:r.y+p[1],scale:v.scale};},id);
      for(const id of ['P1','P5']){
        const old=await page.evaluate(()=>structuredClone(testPoints)),p=await screen(id);
        await page.mouse.move(p.x,p.y);await page.mouse.down();await page.mouse.move(p.x+24,p.y-12,{steps:5});await page.mouse.up();await page.waitForFunction(()=>!busy);
        const points=await page.evaluate(()=>testPoints);
        assert.ok(Math.hypot(points[id][0]-old[id][0]-24/p.scale,points[id][1]-old[id][1]+12/p.scale)<2/p.scale);
        for(const other of Object.keys(old).filter(k=>k!==id))assert.deepEqual(points[other],old[other]);
      }
      await page.locator('.point-row').filter({hasText:'P2'}).click();
      const target=await page.evaluate(()=>{const v=view(),r=photoCanvas.getBoundingClientRect(),p=PhotoCoordinates.toScreen(800,450,v);return {x:r.x+p[0],y:r.y+p[1],scale:v.scale};});
      await page.mouse.click(target.x,target.y);await page.waitForFunction(()=>!busy);
      const p2=await page.evaluate(()=>testPoints.P2);assert.ok(Math.hypot(p2[0]-800,p2[1]-450)<2/target.scale);
      const previous=await page.evaluate(()=>testPoints.P3),p3=await screen('P3');await page.evaluate(()=>{failPoint=true;});
      await page.mouse.move(p3.x,p3.y);await page.mouse.down();await page.mouse.move(p3.x+25,p3.y+15);await page.mouse.up();await page.waitForFunction(()=>!busy);
      assert.deepEqual(await page.evaluate(()=>imagePoints.P3),previous);
      await page.evaluate(()=>{failPoint=false;accept({ok:true,state:{session:'new',capture_id:0,captured:false,points:[]},review:structuredClone(testState),image_points:structuredClone(testPoints)});});
      assert.equal(await page.locator('.point-row').count(),4);
      for(const id of ['apply','capture','pick','overlay','measure'])assert.equal(await page.locator('#'+id).isDisabled(),true);
      assert.equal(await page.locator('#alignmentStatus').textContent(),'Model connection required');
      const reviewed=await screen('P1');await page.mouse.move(reviewed.x,reviewed.y);await page.mouse.down();await page.mouse.move(reviewed.x+10,reviewed.y+10);await page.mouse.up();await page.waitForFunction(()=>!busy);
      assert.equal(await page.evaluate(()=>state.points.length),0);
      assert.equal(await page.evaluate(()=>state.captured),false);assert.deepEqual(errors,[]);
      await context.close();
    }
  }finally{await browser.close();}
});

test('replacement flow retains photo points and connects selected rows before calculation', async () => {
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try {
    const page=await browser.newPage({viewport:{width:1320,height:860}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(preview=>{
      window.testPoints=Object.fromEntries(Array.from({length:6},(_,i)=>['P'+(i+1),[200+i*150,300]]));
      window.autoFits=0;window.autoPreviews=0;window.actions=[];const rows=Object.keys(testPoints).map(id=>({id,object_name:'Old brick',api_coordinates:[1,2,3],transformed_coordinates:[1,2,3]}));
      let review={document:'old.ics',original_camera:{},points:rows};
      const state={session:'new',capture_id:1,captured:false,document:'new.ics',replace_model_supported:true,points:[]};
      let fit=null;const result=()=>({ok:true,state,review,fit,image_points:structuredClone(testPoints)});
      window.pywebview={api:{preferences:async()=>({theme:'dark',language:'en'}),
        call:async(command,session,args)=>{if(command==='rebind_point'){actions.push(args.id);const point=state.points.find(p=>p.id===args.id);point.binding_status='connected';point.object_name='New brick';}return result();},
        open_project:async()=>({...result(),image:{preview,width:1600,height:1000,name:'photo.png'}}),
        fit_points:async()=>{autoFits++;fit={stable:true,ids:[],point_errors_px:[],predicted_image_px:[]};return result();},
        preview_fit:async()=>{autoPreviews++;return result();},
        replace_model:async()=>{actions.push('replace');review=null;state.capture_id++;state.captured=true;state.original_camera={};state.points=rows.map(p=>({...p,object_name:'',binding_status:'needs_reconnection'}));return result();}
      }};
    },'data:image/png;base64,'+readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64'));
    await page.goto(pathToFileURL(resolve(__dirname,'../gui/web/index.html')).href);
    await page.evaluate(()=>window.dispatchEvent(new Event('pywebviewready')));
    assert.equal(await page.locator('header #openProject + #save').count(),1);
    await page.locator('#openProject').click();await page.waitForFunction(()=>photo!==null);
    const before=await page.evaluate(()=>({points:structuredClone(imagePoints),path:photo.name}));
    await page.locator('#replaceModel').click();await page.waitForFunction(()=>!busy);
    assert.equal(await page.locator('#fitCamera').isDisabled(),true);
    assert.equal(await page.locator('#alignmentPanel').isVisible(),false);
    const wideStage=await page.locator('#stage').evaluate(e=>e.clientWidth);
    assert.equal(await page.locator('#pick').isDisabled(),true);
    assert.equal(await page.locator('.reconnect-point').first().isVisible(),true);
    for(let i=0;i<6;i++){
      await page.locator('.reconnect-point').nth(i).click();await page.waitForFunction(()=>!busy);
    }
    assert.deepEqual(await page.evaluate(()=>[autoFits,autoPreviews]),[1,1]);
    await page.waitForTimeout(1200);
    assert.deepEqual(await page.evaluate(()=>[autoFits,autoPreviews]),[1,1]);
    assert.equal(await page.locator('#alignmentPanel').isVisible(),true);
    assert.equal(await page.locator('#stage').evaluate(e=>e.clientWidth),wideStage-280);
    assert.equal(await page.locator('#stepResult').evaluate(e=>e.classList.contains('active')),true);
    assert.equal(await page.locator('#fitCamera').isEnabled(),true);
    assert.deepEqual(await page.evaluate(()=>({points:structuredClone(imagePoints),path:photo.name})),before);
    assert.deepEqual(await page.evaluate(()=>actions),['replace','P1','P2','P3','P4','P5','P6']);
    assert.deepEqual(errors,[]);
  } finally {await browser.close();}
});
