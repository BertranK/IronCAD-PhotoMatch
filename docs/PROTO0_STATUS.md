# Proto 0 검증 현황

2026-09-14. **전체 Proto 0는 미완료**다. 기존 MFC 창을 독립 Python GUI로 전환했다.
이전 버전의 호스트 성공과 새 연결 모듈의 시험 결과를 구분한다.

| 검사 | 확인 결과 |
|---|---|
| v143 / x64 / MFC·ATL 빌드 | 새 DLL 통과, 경고 0·오류 0 |
| 독립 투영 수식 테스트 | 통과 |
| Python / Named Pipe 테스트 | 16개 통과: 기존 10개 + 창 재사용·호스트 전환 6개 |
| JavaScript 사진 좌표 테스트 | 2개 통과: 확대·이동·가로/세로 화면 변환과 영역 밖 입력 |
| Python + Tailwind 실제 창 표시 | 통과: evidence/gui-empty.png |
| 사용자 AVIF 샘플 표시·확대 | 통과: 750×750 원본, evidence/gui-sample-avif.png |
| DLL 배포·시스템 COM 등록 | 메뉴 활성화·정밀 투영 진단·복원 비교 수정본 배포 완료 |
| 기본 로드 / 추가 메뉴 | 통과: 수동 체크 없이 재시작, Add-Ins → IronCAD PhotoMatch → PhotoMatch 열기 |
| 기존 GUI 재사용 | 통과: 반복 메뉴 클릭·최소화 복구·호스트 재시작 후 같은 창과 사진 유지 |
| 새 연결 모듈의 IronCAD 내부 초기화 | 통과: 실제 호스트 InitSelf 및 Named Pipe 상태 응답 확인 |
| 실제 LEGO 샘플 상태 보관·점 선택 | 통과: PID 44744, P1 / vertex 1194 좌표 취득, errors=[] |
| 카메라 적용·복원 / GUI 저장 | 호스트 API 적용·복원 수행, GUI 결과 저장 확인. 아래 복원 비교 수정 참고 |
| 꼭짓점 전역 좌표·FOV 규약·사진 정합 | 전체 기준 미검증 |
| 가로/세로 및 Windows 배율 100%/150% 행렬 | 150%에서 가로/세로·3개 화각·2개 자세 측정. 100% 및 사진 정합 미검증 |

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
호스트의 통과 판정과 사진 표시 수식에는 아직 적용하지 않았다.

복원 반복 중 SDK가 Up 벡터를 약 2.2e-16 정규화해 JSON 문자열의 완전 일치 검사가
실패하는 것을 확인했다(`evidence/camera-restore-final.json`). 카메라 값 비교를
`abs(a-b) <= 16 * DBL_EPSILON * max(1,abs(a),abs(b))`로 변경했다.
관측된 오차 허용, 실제 위치·화각 변경 및 NaN 거부 회귀 시험을 통과했다.
JSON에 비교 규약을 함께 저장한다. 모델 변환·바운딩 박스 비교는 기존대로 엄격히 유지한다.
배포 후 PID 41228에서 서로 다른 자세·화각으로 적용→복원을 3회 반복했다.
모두 restored=true, model_transforms_bounds_unchanged=true, errors=[]였다.
실행 증거는 `evidence/camera-restore-verified.json`이다.

다음 검증은 화면 끝점 규약을 실제 사진 모서리·내부 기준점과 대조하고 100%에서 재검사하는 것이다.
알려진 크기 및 중첩 어셈블리의 전역 좌표 검증도 남아 있다. GUI 상태 조회와 외부 진단
클라이언트가 동시에 연결할 때 한 차례 파이프 연결 실패가 관찰되어 동시 연결 재현도 필요하다.

사용자가 제공한 testsample에는 LEGO Brick v1.ics, LEGO Brick v1.step, 750×750 AVIF가 있다.
AVIF는 여러 블록을 배열한 이미지이므로 이후 대응점 시험은 하나의 블록을 기준으로 해야 한다.
사진 속 형상과 CAD 모델이 같은지는 아직 검증하지 않았다. 단일 카메라 기준의 수치 검증에는
기존 tests/fixture의 알려진 배치와 기준점을 별도로 사용한다. 샘플 원본은 수정하지 않았다.

화면·JSON·설치 백업과 사용자 샘플은 Git에 올리지 않는다. `evidence/`는 로컬 증거 폴더다.
프로젝트는 IronCAD 설치 폴더의 ICAPI/PhotoMatchProto 배치를 전제로 한다.
