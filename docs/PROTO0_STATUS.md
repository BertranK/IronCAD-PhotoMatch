# Proto 0 검증 현황

## 사용자 지정 샘플 — 오른쪽 위 연한 하늘색 블록 (2026-09-14)

사용자가 지정한 오른쪽 위 연한 하늘색 블록 하나로 시험했다. Windows 배율은 150%를
유지했다. 실제 LEGO 모델의 위·아래 모서리 6개를 선택하고 GUI에서 사진의 대응점을
지정했다. vertex ID는 1189, 1191, 1199, 1185, 1186, 1181이며 COM 오류는 없었다.
`evidence/sky-blue-six-pairs.json`은 실제 GUI의 결과 저장으로 생성했다.

원본 750×750 사진에서 초점거리 1000 px, 주점 (375,375)를 가정한 별도 수치 시험으로
카메라 자세를 근사했다. 제품에 PnP/렌즈 보정 기능을 추가한 것은 아니다.
수동 사진 점의 불확실성과 사진/CAD의 형상 동일성은 검증되지 않았다.
해당 자세를 실제 IronCAD 테스트 카메라에 적용하자 모델이 지정 블록 위에 겹쳐 표시됐다.
아래쪽 가장자리의 홈 등 상세 형상은 사진과 모델에서 다르게 보였다.

| 결과 | 측정값 |
|---|---|
| 별도 수치 근사에서 최대 사진 점 오차 | 4.3046 원본 이미지 px |
| 실제 SDK 꼭짓점 투영과 사진 표식의 최대 거리 | 10.8524 물리 화면 px (원본 환산 4.4187 px) |
| 실제 화면 크기 / 배율 | 3341×1844 물리 px / DPI 144 |
| SDK 연결용 합성 기준점 투영 | 최대 0 물리 px |
| 사용자 사진의 1 물리 px 정합 기준 | **미통과** |
| 시험 후 복원 | restored=true, model_transforms_bounds_unchanged=true, errors=[] |

0 px인 SDK 연결 검사와 사용자 사진의 정합 오차를 혼동하지 않는다. 이 결과만으로
남은 오차를 렌즈, 클릭 오차, 형상 차이 중 어느 하나의 원인으로 단정하지 않는다.
증거: `sky-blue-six-pairs.jpg`, `sky-blue-camera-fit.json`, `sky-blue-overlay.json`,
`sky-blue-overlay-maximized.jpg`, `sky-blue-alignment-result.json`,
`sky-blue-tested-project.json`, `sky-blue-restored.json` (모두 로컬 evidence 폴더).
카메라 근사 재현용 일회성 분석은 `evidence/fit-sky-blue.py`에 보관했다.

## 화면 접근 복구 후 추가 검증 (2026-09-14)

사용자가 모니터를 켠 뒤 화면 캡처와 입력이 다시 작동했다. IronCAD를 정상 종료하고
오버레이 수정 DLL `56E49BE5F79950579DCE92C571C7B1024300FEFD6931F4549A125B8CB18CCEC8`을
배포·등록한 다음 재시작했다. PID 36440에서 자동 로드와 시험 문서 연결을 확인했다.
GUI와 IronCAD 모두 DPI 144였다(`evidence/overlay-fix-native-dpi.json`).

세로 창 → 최대화 → 원래 창 크기 복원에서 사진이 재선택 없이 계속 표시되었다.
이전의 최대화 시 사진 소실 문제가 이 실행에서는 재현되지 않았다.
화면 증거는 `overlay-fix-portrait.jpg`, `overlay-fix-maximized.jpg`,
`overlay-fix-restored-window.jpg`다. 가로·세로 측정과 최대화 후 측정에서
`minimum_radians_full` 최대 오차 0 물리 px, errors=[]를 확인했다.
수치 기록은 `overlay-fix-landscape.json`, `overlay-fix-portrait.json`,
`overlay-fix-restored-window.json`이다. 사진의 실제 모델 대응점 정합을 뜻하지는 않는다.

마지막 카메라 복원은 restored=true, model_transforms_bounds_unchanged=true,
errors=[]였다(`overlay-fix-camera-restored.json`). 기존 GUI 창과 사진을 유지한 채
새 호스트에 연결됐고, 언어를 System으로 선택해 실제 한국어 화면을 확인했다
(`overlay-fix-gui-system.jpg`). Windows 다크 테마와 150%는 유지했다.

**전체 Proto 0는 여전히 미완료다.** 새 DLL의 100% 실행 행렬, 알려진 중첩 어셈블리
전역 좌표, 사진 모서리·내부 기준점 정합, 문서 수명/오류 입력의 최종 실행 검증이 남았다.
아래의 교체·재실행 대기 기록은 위 추가 검증으로 갱신한다.

## 최신 실행 검증 — Windows 150% (2026-09-14)

**전체 Proto 0는 미완료다.** 아래는 PID 42232에서 실제 확인한 결과다.
설치 DLL은 `38E0C378331176ECE19163F538F7C61299A7B522624E885C0C6BF333C2AA4E1A`이며
화면 끝점/물리 픽셀 변환 수정이 포함된다. 이후 오버레이 수정 빌드는 아직 설치하지 않았다.

| 검사 | 실제 결과 및 로컬 증거 |
|---|---|
| 실제 150% / GUI DPI 인식 | GUI와 IronCAD 모두 DPI 144. GUI는 Per Monitor V2, IronCAD는 아님. `evidence/final-native-window-dpi.json` |
| 가로/세로 SDK 투영 | 3180×1844, 1379×1035, 425×1035, 3341×1844 물리 화면에서 시험. FOV 0.7/1.1, 두 자세 포함. 합성 기준점의 최대 오차 0 물리 px. `final-dpi150-*.json`, `final-photo-landscape.json` |
| FOV 규약 | 가로·세로 결과를 합쳐 `minimum_radians_full`이 유일하게 통과. 다음 후보 최대 오차 159.201 px. `final-dpi150-portrait.json` |
| 실제 모델 점 선택 | LEGO 꼭짓점 1186과 1140 취득, COM 오류 없음. 이동 변환을 포함한 좌표 기록. 알려진 중첩 회전 모델과의 대조는 미검증. `final-point-selection.json`, `final-overlay-click-through.json` |
| GUI | AVIF 표시, System/English 전환, 기존 창 재사용 확인. `final-gui-english-dpi150.jpg` |
| 배경 경로 | 독립 복원 스냅샷 미지원으로 변경하지 않음. `final-background-probe.json` |
| 사진 오버레이 | 세로 창 표시·이동 추적·클릭 통과 확인. **최대화하면 사진이 사라짐을 재현.** 좌표 JSON만으로 화면 통과 판정하지 않음. `final-photo-portrait.jpg`, `final-photo-moved.jpg` |
| 복원 | restored=true, model_transforms_bounds_unchanged=true, errors=[]. 시험 문서 저장 확인. `final-before-overlay-update-restore.json`, `final-dpi150-stopped-state.json` |

사진 최대화 문제에 대해 캐시한 32비트 이미지를 창 위치·크기와 함께
[`UpdateLayeredWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)로
갱신하도록 수정했다. 크기가 그대로이면 이미지를 다시 그리지 않는다.
v143 빌드(경고 0, 오류 0)와 투영 회귀 시험은 통과했다.
새 DLL SHA-256은 `56E49BE5F79950579DCE92C571C7B1024300FEFD6931F4549A125B8CB18CCEC8`다.
**수정 효과는 실제 IronCAD에서 아직 미검증이다.**

교체를 위한 정상 종료 과정에서 화면 도구가
`IGraphicsCaptureItemInterop.CreateForMonitor ... 0x80070057`과
`wait for accessibility element target: timed out waiting on channel`을 반환했다.
IronCAD와 GUI 모두 캡처가 실패해 화면 입력을 중단했다. PID 42232는 여전히 응답하며
실행 중 DLL을 교체하거나 호스트를 강제 종료하지 않았다. 카메라는 이미 복원되었고
사용자 원본 대신 `evidence/LEGO-before-menu-update.ics` 시험 복사본만 저장했다.
Windows 다크 테마와 150%는 유지했다. 언어 저장값은 System으로 복구했지만
열려 있는 GUI의 English 표시가 즉시 바뀌었는지는 미확인이다.

남은 순서: 화면 접근 복구 → IronCAD 정상 종료 → `scripts/configure-host.ps1` →
재실행 → 오버레이의 세로/최대화/복원 반복 → 새 DLL의 실제 100%/150% 행렬 →
알려진 중첩 모델 좌표 및 사진 모서리·내부 기준점 정합 → 문서 전환·종료·오류 입력 검증.
아래 기록은 이 검증 이전의 이력이며, 이전의 배포 대기/배율 미확인 상태는 위 결과로 갱신한다.

## 이전 구현·검증 이력

2026-09-14. **전체 Proto 0는 미완료**다. 기존 MFC 창을 독립 Python GUI로 전환했다.
이전 버전의 호스트 성공과 새 연결 모듈의 시험 결과를 구분한다.

| 검사 | 확인 결과 |
|---|---|
| v143 / x64 / MFC·ATL 빌드 | 새 DLL 통과, 경고 0·오류 0 |
| 독립 투영 수식 테스트 | 통과 |
| Python / Named Pipe 테스트 | 21개 통과: 연결·동시 접속·창 재사용·복원 판정·언어 설정·초기 창 크기 |
| JavaScript / 브라우저 테스트 | 5개 통과: 사진 좌표·번역·DPI/테마/언어/리사이즈 행렬 |
| Python + Tailwind 실제 창 표시 | 통과: evidence/gui-empty.png |
| 사용자 AVIF 샘플 표시·확대 | 통과: 750×750 원본, evidence/gui-sample-avif.png |
| DLL 배포·시스템 COM 등록 | 메뉴·복원 비교 버전 배포 완료. 이번 DPI/끝점 규약 수정 DLL은 빌드 완료, 교체·호스트 검증 대기 |
| 기본 로드 / 추가 메뉴 | 통과: 수동 체크 없이 재시작, Add-Ins → IronCAD PhotoMatch → PhotoMatch 열기 |
| 기존 GUI 재사용 | 통과: 반복 메뉴 클릭·최소화 복구·호스트 재시작 후 같은 창과 사진 유지 |
| 새 연결 모듈의 IronCAD 내부 초기화 | 통과: 실제 호스트 InitSelf 및 Named Pipe 상태 응답 확인 |
| 실제 LEGO 샘플 상태 보관·점 선택 | 통과: PID 44744, P1 / vertex 1194 좌표 취득, errors=[] |
| 카메라 적용·복원 / GUI 저장 | 호스트 API 적용·복원 수행, GUI 결과 저장 확인. 아래 복원 비교 수정 참고 |
| 꼭짓점 전역 좌표·FOV 규약·사진 정합 | 전체 기준 미검증 |
| 가로/세로 및 Windows 배율 100%/150% 행렬 | 150%에서 가로/세로·3개 화각·2개 자세 측정. 100%에서 가로형 8점 추가 측정. 사진 정합 미검증 |

Named Pipe 단독 시험은 실제 C++ 전송 코드와 Python 클라이언트를 사용한 echo 프로세스 시험이다.
IronCAD API 성공을 의미하지 않는다. GUI 좌표 시험도 CAD 전역 좌표 규약을 증명하지 않는다.

이전 버전에서 PID 21092의 InitSelf, MFC 창 표시, Capture state 및 JSON 저장을 확인했다.
기존 JSON의 SDK 버전은 29.0.2.20605이며 errors=[]였다.

새 GUI의 모델 점 선택은 상태 보관을 먼저 수행한다. 실제 LEGO 샘플에서 이 단계의
`element->GetChildren(&children)`가 COM E_FAIL(0x80004005)을 반환했다.
SDK의 어셈블리 순회 예제를 따라 어셈블리만 GetChildren을 호출하도록 수정했다.
부품의 전역 변환·바운딩 박스 검사는 유지한다. 실패한 HRESULT에는 API 호출명도 기록한다.
수정 후 `evidence/point-selection-fixed.json`과 실제 GUI에서 P1 표시를 확인했다.
이는 해당 샘플의 좌표 취득 성공이며 전역 좌표 규약 확정은 아니다.

GUI 창 제목을 IronCAD PhotoMatch로 변경했다. 사용자별 파일 잠금으로 반복 실행 시
기존 GUI에 활성화 요청을 전달한다. 호스트 재시작 시 사진 유지/오래된 대응점 초기화,
동일 호스트의 대응점 유지, 복원 실패 시 연결 유지, 별도 프로세스의 중복 실행을 자동 시험한다.
실제 GUI PID 41256 / 창 ID 1644236에서 새 제목, 반복 실행 시 같은 창 유지, 최소화 복구를 확인했다.
IronCAD PID 15612 → 44652 재시작 후 같은 GUI에서 사진을 유지하고 새 호스트에 연결했다.
`evidence/gui-host-reconnected.json`은 GUI의 결과 저장으로 생성했으며 새 호스트 PID,
750×750 이미지 SHA-256과 errors=[]를 확인했다. 화면은 `evidence/gui-host-reconnected.jpg`다.

기본 자동 로드(autoload=true)와 3D 장면의 PhotoMatch 메뉴/리본 버튼 등록을 추가했다.
초기화 시 GUI 자동 실행을 제거하고 메뉴 클릭 시 기존 창 재사용 경로를 호출한다.
설치 설정과 등록 검사는 자동 로드 값도 확인한다. 실제 첫 검사에서 메뉴가 비활성화되어
명령 OnUpdate에서 Enabled를 갱신하도록 수정했다. 배포·재시작 후 메뉴 클릭과
수동 Add-in 체크 없는 기본 로드를 확인했다(`evidence/photomatch-menu-enabled.jpg`).
별도 설정 파일 복사본에서 autoload=true, 단일 항목 유지, 반복 설치의 동일성,
다른 Add-in 항목 보존 및 PhotoMatch 항목 제거를 확인했다. 수정 빌드는 경고 0·오류 0이다.

150% 배율의 첫 투영 검사는 최대 3.0541 물리 픽셀 오차로 **미통과**다.
가로/세로 비교에서 가장 가까운 FOV 후보는 `minimum_radians_full`이었다.
추가로 SDK `XformModelToView3`의 double 좌표를 기록한다. 렌더 행렬을 바꾸거나
이를 전역 좌표로 간주하지 않으며, 기존 `XformWorldToView2` 검증은 그대로 유지한다.

`scripts/analyze-projection.py`로 재현 가능한 화면 끝점 규약 가설을 별도 분석했다.
GetViewExtents=(rw,rh)에 대해 종횡비 (rw-1)/(rh-1), 픽셀 범위 (rw-2,rh-2),
정수 변환 시 0 방향 버림을 가정하면 32개의 정수 투영이 모두 일치했다.
같은 좌표를 double 진단과 비교한 최대 차이는 0.000755 물리 픽셀이었다.
945×669 / 575×669, FOV 0.3711721031 / 0.7 / 1.1, 기존 자세와 축 정렬 자세를 사용했다.
증거: `evidence/camera-double-matrix.json`, `evidence/projection-raster-hypothesis.json`.
이는 **150%의 수치 가설**이며 실제 사진 정합이나 100% 배율 검증을 대신하지 않는다.
이번 소스에는 렌더 픽셀 끝점과 정수 버림 이후 물리 픽셀로 변환하는 규약을 적용했다.
독립 회귀 시험과 v143 빌드가 통과했으나 실행 중 호스트의 DLL은 아직 이전 버전이다.

복원 반복 중 SDK가 Up 벡터를 약 2.2e-16 정규화해 JSON 문자열의 완전 일치 검사가
실패하는 것을 확인했다(`evidence/camera-restore-final.json`). 카메라 값 비교를
`abs(a-b) <= 16 * DBL_EPSILON * max(1,abs(a),abs(b))`로 변경했다.
관측된 오차 허용, 실제 위치·화각 변경 및 NaN 거부 회귀 시험을 통과했다.
JSON에 비교 규약을 함께 저장한다. 모델 변환·바운딩 박스 비교는 기존대로 엄격히 유지한다.
배포 후 PID 41228에서 서로 다른 자세·화각으로 적용→복원을 3회 반복했다.
모두 restored=true, model_transforms_bounds_unchanged=true, errors=[]였다.
실행 증거는 `evidence/camera-restore-verified.json`이다.

다음 검증은 새 DLL을 배포하여 화면 끝점 규약을 실제 사진 모서리·내부 기준점과 대조하는 것이다.
알려진 크기 및 중첩 어셈블리의 전역 좌표 검증도 남아 있다. GUI 상태 조회와 외부 진단
클라이언트 동시 연결 실패는 8개 클라이언트/32개 요청 시험으로 재현했다.
WaitNamedPipe 성공 이후 다른 클라이언트가 먼저 연결하면 CreateFile이 ERROR_PIPE_BUSY를
반환했다. 요청 기한 내에서 연결 단계만 다시 시도하도록 수정했고 재현 시험이 통과했다.
명령을 전송한 이후의 자동 재시도는 추가하지 않았다.

## Windows 표시 및 언어 검증 (2026-09-14 야간)

- 실제 Windows 설정에서 다크/라이트 전환과 GUI 제목 표시줄·화면 색 변경을 확인했다.
  `evidence/ui-dark-english.jpg`, `evidence/ui-light-english.jpg`.
- 언어 선택을 English로 변경해 실제 창의 버튼·설명·입력 레이블 변경을 확인했다.
  시스템 기본값은 Windows ko-KR를 한국어로 해석하며, 수동 설정 저장/재읽기는 자동 시험했다.
- 실제 Windows 배율을 150%에서 100%로 변경했다. 기존 IronCAD는 DPI 144를 유지하면서
  SDK 렌더 크기 945×669를 물리 크기 630×446으로 표시했다. 이 상태의 8개 정수 투영도
  끝점 규약과 일치했다(`evidence/dpi100-landscape.json`). 새 DLL의 호스트 통과 결과는 아니다.
- 실행 중 PhotoMatch 창의 Per Monitor V2 인식과 DPI 96을 Win32 조회로 확인했다.
  같은 시점 IronCAD 창은 Per Monitor V2가 아니며 DPI 144였다(`evidence/native-window-dpi.json`).
- Edge 모의 시험: 5개 DPR × 4개 창 크기 × 2개 테마의 40개 조합, 실행 중 DPR 변경,
  언어 전환, 원본 사진 좌표·사용자 이름 보존, 주요 버튼 경계와 캔버스 픽셀 크기가 통과했다.
  창 크기는 1320×860, 960×640, 600×480, 600×320 CSS px다.
  `evidence/browser-ui-*.png`는 이 모의 GUI의 합성 시험 사진이며 실제 IronCAD 화면이 아니다.

화면 검증 도중 Computer Use가 `IGraphicsCaptureItemInterop.CreateForMonitor ... 0x80070057`로
실패했고 재시도도 실패했다. 접근성 정보도 반환되지 않아 화면 입력을 중단했다.
그 전에 시험 카메라 복원(restored=true, errors=[])과 시험 문서 저장을 완료했다.
IronCAD PID 41228은 계속 응답하며 사용자 원본 모델은 수정하지 않았다.

원래 Windows 다크 테마와 PhotoMatch 시스템 언어 기본값을 저장 설정으로 복구했다.
시험 시 변경된 단일 모니터의 영구 배율 설정도 원래 150%로 복구했다.
**현재 실행 화면은 마지막 Win32 조회에서 GUI DPI 96이었다. 배율의 실시간 150% 복원은 미확인이다.**
정리 기록은 `evidence/display-settings-cleanup.json`이다. 기존 GUI에는 테스트 중 선택한
영어가 남아 있을 수 있고 다음 GUI 실행부터 시스템 언어 기본값을 읽는다.

남은 실행 순서: 화면 접근 복구 → 실제 배율 150% 확인 → 시험 문서 저장/호스트 정상 종료 →
`scripts/configure-host.ps1`로 DLL 교체 → GUI 재시작 → IronCAD 재실행 후 100%/150%의
가로/세로 투영 및 오버레이/배경 복원 시험. 현재 설치 DLL SHA-256은
`99862EF116E9149F22FCE96162A2948D37D91AFE23B87FB30A84A664F6FBBF22`, 새 빌드는
`38E0C378331176ECE19163F538F7C61299A7B522624E885C0C6BF333C2AA4E1A`다.

사용자가 제공한 testsample에는 LEGO Brick v1.ics, LEGO Brick v1.step, 750×750 AVIF가 있다.
AVIF는 여러 블록을 배열한 이미지이므로 이후 대응점 시험은 하나의 블록을 기준으로 해야 한다.
사진 속 형상과 CAD 모델이 같은지는 아직 검증하지 않았다. 단일 카메라 기준의 수치 검증에는
기존 tests/fixture의 알려진 배치와 기준점을 별도로 사용한다. 샘플 원본은 수정하지 않았다.

화면·JSON·설치 백업과 사용자 샘플은 Git에 올리지 않는다. `evidence/`는 로컬 증거 폴더다.
프로젝트는 IronCAD 설치 폴더의 ICAPI/PhotoMatchProto 배치를 전제로 한다.


## Photo point editing and corrected sample follow-up (2026-09-14)

- Photo markers remain editable after camera restoration. Drag a marker directly, or select its row and click a replacement photo location. Original image pixels are stored independently of display scaling.
- Open results reloads the original photo and saved correspondences. It currently requires the same live host session, capture ID and exact model point records; it does not rebind model vertices after an IronCAD restart. Changed image contents and stale model records are rejected.
- The language selector now shows Korean and English only. Windows language remains the default until an explicit language is chosen.
- Live GUI reopening loaded all six pale upper-right blue-block points. The user edited the points successfully, and the GUI saved them as `evidence/sky-blue-user-edited.json`.
- Repeating the exploratory fixed-focal camera fit on those edits gives a maximum residual of 4.546958 original image pixels. This is a calculation result, not an IronCAD screen measurement or precision pass. Focal length remains assumed at 1000 pixels; no lens calibration is claimed.
- Camera Apply now prepares to resume a restored capture without clearing correspondence IDs, after checking document identity, viewport identity and model fingerprint. It saves the current view as the new restoration baseline. This native change builds successfully but is NOT installed or runtime-verified yet; the running IronCAD still has the preceding DLL loaded.
- Validation: 9 Python API tests, 6 browser/coordinate/localization tests, x64 v143 build and native projection tests passed. Browser display checks cover scale factors 100%, 125%, 150%, 175%, and 200%. Native resume and post-restart point rebinding remain separate outstanding work.

## Saved-result recovery and camera fitting: verified update (2026-09-14 morning)

This section supersedes the outstanding recovery/resume statements above.

- Installed DLL SHA-256: `3B081C883BFFAE62FF8B8BF272F960F9FF288C76F25F2D05F72C4C3A2C0056E3`. SDK reported `29.0.2.20605`; Windows remained at 150% (host DPI 144).
- Open results reconnected all six saved vertices after a normal IronCAD restart. Native validation compares the document path, unique object ID, vertex ID, local/global coordinates and full global transform before capturing anything. Wrong document, changed coordinate, changed transform and nonexistent object were rejected without modifying the existing capture (`reconnect-rejection-tests.json`).
- Different documents open in photo review mode; camera actions remain disabled. Photo edits are retained independently of live CAD references. The running host state is never replaced by saved state.
- The GUI now calculates perspective pose and focal length from six or more distinct noncoplanar correspondences. It assumes centered principal point, square pixels and no lens distortion. It shows per-point residuals and predicted positions. Editing invalidates the fit and requires recalculation before preview.
- Actual GUI sequence passed: Open results → Calculate camera → Preview in IronCAD → Restore view → Pick model points / Finish picking → Preview again → Restore → Save results. Existing matches survived re-entering picking after restoration. Normal shutdown and existing-window reuse also passed.
- `sky-blue-product-verified.json` was saved by the actual GUI. Its six photo coordinates and complete model point records are exactly equal to `sky-blue-user-edited.json`. Camera restoration and model transform/bounds preservation both report true, with no host errors.
- User-photo fit: estimated focal 1747.470361 original pixels, maximum residual 3.364254 original pixels, RMS 1.858377. The maximized IronCAD comparison measures 8.880403 physical pixels maximum. Both precision flags remain false. This is a usable preview, not a precision-alignment pass.
- Portrait and maximized overlay views are recorded in `product-preview-portrait.json/.jpg` and `product-preview-landscape.json/.jpg`. The SDK projection candidates identify `minimum_radians_full` with zero physical-pixel discrepancy across those aspects. The image rectangle and camera field update together on resize. `product-gui-fit.jpg` records the GUI residual display.
- The single-aspect preview path accepts the previously verified minimum-axis full-radian convention only for SDK `29.0.2.20605`, and only after a fresh current-capture projection measurement agrees within one physical pixel. Unknown SDK versions still require the portrait/landscape convention check. An initial version-string typo was caught during real preview testing, corrected, rebuilt and reinstalled before the passing run.
- Validation: 31 Python tests, seven browser/coordinate/localization tests (including calculate → preview → edit invalidation), x64 v143 build, native projection tests. The synthetic solver tests recover independently generated camera poses/focal lengths at different scene scales and screen aspects; noisy data does not get a precision pass.

Proto 0 as a whole is still incomplete: the complete runtime matrix for nested rotated assemblies and the latest DLL at 100% DPI is not established. The supplied LEGO photo is a composed image with manually selected points; neither lens calibration nor exact geometric equivalence has been established. Earlier evidence remains historical, not a claim that the newest build passed every matrix item. Local screenshots, saved user results and DLL backups remain in ignored `evidence/`; the user-provided `testsample/` files are tracked at the user's request.

## Known geometry and complete camera restoration (2026-09-14)

This section updates the matrix status above; it does not declare full Proto 0 completion.

- Installed DLL SHA-256: `8C354B732FDD765C1CC14D680F87D1D7764AA9C87958C60F7CD23D4EBB22AD43`. SDK remains `29.0.2.20605`.
- The internal `test_coordinate_fixture` command creates a new empty scene with two 40 x 20 x 30 mm boxes. One has an identity transform; the other is inside two assemblies with explicit rotations and translations. No existing scene geometry is changed. `CreateBlockPart` returned E_NOTIMPL at runtime; the fixture uses the SDK sample's profile/extrusion route instead.
- All 16 vertices matched independently calculated corner coordinates. The nested mapping is `(0.092-x, 0.225+z, 0.333+y)` in model units. Maximum global error was `6.36e-17`, below the `1e-9` tolerance. Evidence: `nested-coordinate-fixture-centerlast.json`.
- Actual mouse selection also passed for both cases, independently of the diagnostic's direct call into the selection handler. The nested selected corner returned local `(0.02, 0.01, 0.015)` and global `(0.072, 0.24, 0.343)`. Evidence: `fixture-ui-vertex-event.json`, `fixture-ui-nested-vertex-event.json/.jpg` and `fixture-ui-events-restored.json`.
- The new fixture reproduced an orbit-center restoration failure even though camera position, direction, field, clipping and scale matched. Reactivation alone did not fix it. Writing CenterOfInterest last, after the other camera setters, restores the complete original state. Two different camera poses/fields and real selection followed by restore passed; model transforms and bounds stayed unchanged. The numeric restore tolerance was not relaxed.
- `scripts/verify-coordinate-fixture.py` provides a repeatable live fixture, projection and complete camera-round-trip test. Run it with the project venv while one IronCAD host is open, or supply `--host`. It creates a separate test scene, restores the camera and saves JSON; it does not assert photo-overlay acceptance.
- `coordinate-roundtrip-150-portrait.json` passed with host DPI 144. `coordinate-roundtrip-100-portrait.json` passed after a fresh IronCAD startup at Windows 100%, host DPI 96. Both cover two poses/fields. The earlier production DLL also passed live 150-to-100% transition, portrait/landscape SDK projection and overlay restoration (`product-dpi100-portrait.json`, `product-dpi100-landscape-final.json`, `product-dpi100-restore.json`). Do not confuse that earlier live transition with the fresh-start test.
- The independent analytical image fitted to `1.61e-13` original pixels. Its expected SDK-quantized coordinates matched all 16 actual SDK projections exactly in the recorded landscape view. However, comparison of integer SDK coordinates directly to continuous photo coordinates gave 1.2911 physical pixels in landscape and 1.3495 in portrait. These raw continuous-comparison results remain failed against the one-pixel threshold. The source photo's 3.3643 original-pixel fit error is a separate issue and is unchanged.
- Latest automated checks: 31 Python tests, seven browser/coordinate/localization tests, x64 v143 build and native projection tests passed. The live fixture runner additionally passed in the two startup-DPI configurations above.
- Fresh 100% screen capture failed twice with `encode latest capture frame failed: window crop is outside captured monitor`. Accessibility remained readable, but its maximize action failed with `coordinate input geometry is unavailable`. No guessed coordinate input or forced process termination was used. Windows scale was restored to 150% and visually confirmed; Settings was closed. The generated test scene remains open with its camera restored.

Remaining acceptance work: complete the latest-build landscape/photo-overlay visual matrix after capture recovery, resolve the continuous-versus-raster photo-alignment measurement without relaxing the threshold, and verify the final installed GUI workflow again. User-edited LEGO points and original sample files remain preserved. General-user installer packaging and lens calibration are not supplied by this checkpoint.
