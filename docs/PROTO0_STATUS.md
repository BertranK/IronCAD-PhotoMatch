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
| DLL 배포·시스템 COM 등록 | 점 선택 수정본 배포 완료. 이후 창 재사용 호출 수정본은 빌드 완료, 배포 대기 |
| 새 연결 모듈의 IronCAD 내부 초기화 | 통과: 실제 호스트 InitSelf 및 Named Pipe 상태 응답 확인 |
| 실제 LEGO 샘플 상태 보관·점 선택 | 통과: PID 44744, P1 / vertex 1194 좌표 취득, errors=[] |
| 새 GUI에서 적용·복원·저장 | 호스트 실행 검증 대기 |
| 꼭짓점 전역 좌표·FOV 규약·사진 정합 | 전체 기준 미검증 |
| 가로/세로 및 Windows 배율 100%/150% 행렬 | 미검증 |

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
현재 열린 구버전 창은 유지했으며 새 제목·기존 창 활성화의 실제 화면 검증은 재실행 후 필요하다.

사용자가 제공한 testsample에는 LEGO Brick v1.ics, LEGO Brick v1.step, 750×750 AVIF가 있다.
AVIF는 여러 블록을 배열한 이미지이므로 이후 대응점 시험은 하나의 블록을 기준으로 해야 한다.
사진 속 형상과 CAD 모델이 같은지는 아직 검증하지 않았다. 단일 카메라 기준의 수치 검증에는
기존 tests/fixture의 알려진 배치와 기준점을 별도로 사용한다. 샘플 원본은 수정하지 않았다.

화면·JSON·설치 백업과 사용자 샘플은 Git에 올리지 않는다. `evidence/`는 로컬 증거 폴더다.
프로젝트는 IronCAD 설치 폴더의 ICAPI/PhotoMatchProto 배치를 전제로 한다.
