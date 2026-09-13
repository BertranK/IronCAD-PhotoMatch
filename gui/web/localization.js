(function (root) {
  const en = {
    '연결 확인 중':'Connecting', 'IronCAD 연결됨':'Connected to IronCAD', 'IronCAD 연결 대기':'Waiting for IronCAD',
    '결과 저장':'Save results', '결과 열기':'Open results', '사진':'Photo', '대응점':'Points', '정합':'Alignment', '＋ 사진 열기':'＋ Open photo',
    'PhotoMatch 결과 파일을 선택하세요.':'Choose a PhotoMatch result file.', '저장한 사진과 현재 파일이 다릅니다.':'The photo differs from the saved file.',
    '＋ 모델 점 선택':'＋ Pick model points', '점 선택 마치기':'Finish picking', '선택한 점이 여기에 표시됩니다':'Your points appear here',
    'DOCUMENT':'Document', 'ALIGNMENT':'Alignment', '축소':'Zoom out', '확대':'Zoom in', '맞춤':'Fit',
    '사진에서 시작하세요':'Start with a photo', '사진 열기':'Open photo', '스크롤 확대 · 우클릭 이동':'Scroll to zoom · Right-drag to pan',
    '점 끌어서 수정 · 스크롤 확대 · 우클릭 이동':'Drag points to edit · Scroll to zoom · Right-drag to pan',
    '카메라 정합':'Camera alignment', '검증 대기':'Awaiting validation', '투영 검증 통과':'Projection verified',
    'IronCAD에서 확인 ↗':'Preview in IronCAD ↗', '원래 보기로':'Restore view', '카메라 조정':'Adjust camera',
    '위치 · X Y Z':'Position · X Y Z', '방향 · X Y Z':'Direction · X Y Z', '위쪽 · X Y Z':'Up · X Y Z',
    '화각 · SDK 값':'Field of view · SDK', '카메라 위치':'Camera position', '카메라 방향':'Camera direction',
    '카메라 위쪽':'Camera up', '카메라 화각':'Camera field of view', '카메라 적용':'Apply camera', '진단':'Diagnostics',
    '상태 보관':'Capture state', '투영 측정':'Measure projection', '초점거리 · 원본 px':'Focal length · Source px',
    '초점거리':'Focal length', '배경 경로 시험':'Test scene background', '연결 대기':'Waiting for connection',
    '작업 중':'Working', '모델 점 선택 중':'Picking model points', '준비':'Ready', '투영 미측정':'Projection not measured',
    '사진에서 위치 선택':'Select a location in the photo', '변환 후보':'Transform candidate', '좌표계 검증 대기':'Coordinate convention unverified',
    '좌표를 확인하세요.':'Check the coordinates.', '화각을 확인하세요.':'Check the field of view.', '결과를 저장했습니다.':'Results saved.',
    '복원 후 닫을 수 있습니다.':'Restore the view before closing.', 'IronCAD 연결을 전환하지 못했습니다.':'Could not switch the IronCAD connection.',
    '원래 보기 복원을 확인하지 못했습니다.':'Could not verify that the original view was restored.',
    '언어':'Language', '시스템 설정':'System', '지원하지 않는 작업입니다.':'Unsupported operation.',
    '사진을 먼저 여세요.':'Open a photo first.', '문서가 바뀌었습니다. 모델 점을 다시 선택하세요.':'The document changed. Pick the model points again.',
    '모델 점을 먼저 선택하세요.':'Pick a model point first.', '문서가 바뀌었습니다. 상태를 확인한 뒤 저장하세요.':'The document changed. Refresh the state before saving.',
    '100 MB 이하의 사진을 선택하세요.':'Choose a photo no larger than 100 MB.', 'PNG, JPG, BMP, AVIF 사진을 선택하세요.':'Choose a PNG, JPG, BMP or AVIF photo.',
    '4천만 픽셀 이하의 사진을 선택하세요.':'Choose a photo no larger than 40 megapixels.', '올바른 사진 위치를 선택하세요.':'Choose a valid photo location.',
    '사진 안의 점을 선택하세요.':'Choose a point inside the photo.', '모델의 대응점이 바뀌었습니다.':'The model points changed.',
    'IronCAD 응답 대기 시간이 지났습니다. 상태를 확인한 뒤 다시 시도하세요.':'IronCAD timed out. Check its state before retrying.',
    'IronCAD에서 PhotoMatch를 켜세요.':'Open PhotoMatch from IronCAD.',
    '여러 IronCAD가 열려 있습니다. 해당 IronCAD의 PhotoMatch 버튼으로 실행하세요.':'Multiple IronCAD instances are open. Use PhotoMatch from the intended instance.',
    '명령 크기가 너무 큽니다.':'The request is too large.', 'IronCAD 연결을 확인하세요.':'Check the IronCAD connection.',
    'IronCAD에 연결할 수 없습니다.':'Could not connect to IronCAD.', '응답을 확인할 수 없습니다. 상태를 다시 읽으세요.':'The response could not be verified. Refresh the state.',
    'IronCAD 작업에 실패했습니다.':'The IronCAD operation failed.', '설정을 저장하지 못했습니다.':'Could not save the setting.'
  };
  let language = 'ko';
  const t = text => language === 'en' ? (en[text] || text) : text;
  function setLanguage(value) { language = value === 'ko' ? 'ko' : 'en'; }
  function localize(doc) {
    doc.documentElement.lang = language;
    for (const element of doc.querySelectorAll('[data-i18n]')) element.textContent = t(element.dataset.i18n);
    for (const attribute of ['title', 'aria-label'])
      for (const element of doc.querySelectorAll('[data-i18n-' + attribute + ']'))
        element.setAttribute(attribute, t(element.getAttribute('data-i18n-' + attribute)));
  }
  const api = {t, setLanguage, localize, en};
  if (typeof module !== 'undefined') module.exports = api;
  else root.PhotoLanguage = api;
})(typeof window === 'undefined' ? globalThis : window);
