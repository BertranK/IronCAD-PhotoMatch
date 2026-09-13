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
          await page.locator('#language').selectOption(theme==='light'?'en':'system');
          await page.waitForFunction(theme=>document.documentElement.dataset.theme===theme && document.documentElement.lang===(theme==='light'?'en':'ko'),theme);
          const metrics=await page.evaluate(()=>{
            const canvas=document.getElementById('photoCanvas'),stage=document.getElementById('stage');
            const v=view(),rect=canvas.getBoundingClientRect();
            return {cw:canvas.width,ch:canvas.height,w:stage.clientWidth,h:stage.clientHeight,
              x:rect.x+v.x+640*v.scale,y:rect.y+v.y+350*v.scale,
              overflow:document.documentElement.scrollWidth>innerWidth,
              name:document.getElementById('imageName').textContent,doc:document.getElementById('document').textContent,
              scheme:getComputedStyle(document.documentElement).colorScheme,
              controls:['language','save','openImage'].map(id=>{const r=document.getElementById(id).getBoundingClientRect();return r.x>=0&&r.right<=innerWidth&&r.bottom<=innerHeight;})};
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
