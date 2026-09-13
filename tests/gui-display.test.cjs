const {test} = require('node:test');
const assert = require('node:assert/strict');
const {readFileSync,mkdirSync} = require('node:fs');
const {resolve} = require('node:path');
const {pathToFileURL} = require('node:url');
const {chromium} = require('../gui/web/node_modules/playwright');

test('display scaling, live theme/language changes and resize preserve original photo coordinates', async () => {
  const browser = await chromium.launch({channel:'msedge', headless:true});
  const preview = 'data:image/png;base64,' + readFileSync(resolve(__dirname,'fixture/reference.png')).toString('base64');
  try {
    for (const dpr of [1, 1.25, 1.5, 1.75, 2]) {
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
      assert.equal(await page.evaluate(()=>state.captured),false);assert.deepEqual(errors,[]);
      await context.close();
    }
  }finally{await browser.close();}
});
