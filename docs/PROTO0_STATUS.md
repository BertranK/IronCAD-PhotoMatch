# Proto 0 검증 현황

2026-09-14 기준. **전체 Proto 0는 미완료**이며, 실제 호스트 초기화와 수치·화면 시험을 이어가야 한다.

| 검사 | 확인 결과 |
|---|---|
| MSVC v143 / x64 Unicode / MFC·ATL 빌드 | 통과, 경고 0·오류 0 |
| 독립 투영 수식 테스트 | 통과 |
| DLL LoadLibrary 및 독립 IZAddinServer 생성 | 통과 |
| IronCAD Add-in Applications 목록 표시 | Computer Use 화면 및 사용자 확인으로 통과 |
| 실제 IronCAD manifest의 COM 클래스 조회 | 등록 전 오류 14007, 등록 후 성공 |
| manifest 활성화 컨텍스트에서 IZAddinServer 생성 | HRESULT 0x00000000 |
| IronCAD 프로세스의 InitSelf·검증 창 표시 | 마지막 manifest 변경 후 재검증 대기 |
| 꼭짓점 전역 좌표·카메라 규약·사진 정합·복원 | 미검증 |
| 가로/세로 및 Windows 배율 100%/150% 시험 | 미검증 |

실제 2027 환경의 배포에는 `Config/Ironcad.Addin.config`의 목록 항목과 `bin/IronCAD.AddIn.manifest`의 COM 클래스 연결이 필요했다. 현재 DLL 배포 위치는 `bin/PhotoMatchProto.dll`이다. 제공 스크립트는 원본 설정을 백업하고 PhotoMatch 항목만 추가·제거한다.

단독 프로세스에서의 클래스 생성 성공은 IronCAD 내부 초기화 성공을 의미하지 않는다. 화면 캡처·원본 설정 백업·진단 JSON은 로컬 `evidence/`에 보존하며 Git에는 포함하지 않는다. `tests/fixture/`는 수학적으로 생성한 시험 자료이며 호스트 화면 증거가 아니다.

현재 프로젝트는 IronCAD SDK의 `ICAPI/PhotoMatchProto` 아래에 두는 배치를 전제로 한다. SDK 원본, 형식 라이브러리, MFC/ATL 및 Windows SDK는 저장소에 포함하지 않는다. 빌드·시험 지침은 README.md와 tests/ACCEPTANCE.md를 따른다.
