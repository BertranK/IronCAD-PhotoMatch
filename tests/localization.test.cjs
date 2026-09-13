const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const L = require('../gui/web/localization.js');
test('every marked and dynamic UI label has an English translation', () => {
  const html = fs.readFileSync(path.join(__dirname, '../gui/web/index.html'), 'utf8');
  const script = fs.readFileSync(path.join(__dirname, '../gui/web/app.js'), 'utf8');
  const keys = [...html.matchAll(/data-i18n(?:-title|-aria-label)?="([^"]+)"/g), ...script.matchAll(/\bt\('([^']+)'\)/g)].map(m => m[1]);
  for (const key of keys) assert.ok(Object.hasOwn(L.en, key), key);
  L.setLanguage('en'); assert.equal(L.t('결과 저장'), 'Save results');
  assert.equal(L.t('User part name'), 'User part name');
  L.setLanguage('ko'); assert.equal(L.t('결과 저장'), '결과 저장');
});
test('repeated language switches preserve original labels and accessibility', () => {
  const element = {dataset:{i18n:'사진 열기'}, textContent:''};
  const attr = {getAttribute:()=> '언어', setAttribute:(key, value)=>{attr[key]=value;}};
  const doc = {documentElement:{}, querySelectorAll:selector=>selector==='[data-i18n]'?[element]:selector==='[data-i18n-aria-label]'?[attr]:[]};
  for (const language of ['en','ko','en']) { L.setLanguage(language); L.localize(doc); assert.equal(doc.documentElement.lang, language); }
  assert.equal(element.textContent, 'Open photo'); assert.equal(attr['aria-label'], 'Language');
});
