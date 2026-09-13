# IronCAD PhotoMatch

Python + Tailwind 기반 독립 GUI와 IronCAD 2027용 C++ 연결 Add-in.
기본 화면은 사진 → 대응점 → 정합 흐름이며, 카메라 입력과 진단은 접힌 영역에 둔다.
깊이가 다른 대응점 6개 이상으로 원근 카메라 자세와 초점거리를 계산한다.
렌즈 보정은 포함하지 않으며, Proto 0의 전체 수치·화면 시험은 아직 미완료다.

## 설치와 실행

프로젝트를 IronCAD 설치 폴더의 `ICAPI/PhotoMatchProto`에 둔다. 현재 작업 경로는
`D:\Program Files\IronCAD\2027\ICAPI\PhotoMatchProto`다.

```powershell
& .\scripts\setup-gui.ps1
& .\scripts\build.ps1
& .\scripts\register.ps1
& .\scripts\start-gui.ps1
```

Python 3.13, Node.js, MSVC v143 x64 MFC/ATL, Windows SDK 10.0.19041.0을 사용한다.
GUI는 pywebview/Edge WebView2로 실행한다. 현재 PC에서는 기존 WebView2로 화면 표시를 확인했다.
Python 가상 환경, 패키지, GUI 캐시와 CSS 빌드 산출물은 프로젝트가 있는 D 드라이브에 둔다.
Python/Node 실행 파일은 기존 PC 설치를 사용한다. SDK·MSVC·Python·WebView2는 저장소에 포함하지 않는다.

`register.ps1`은 관리자 권한으로 64비트 regsvr32를 실행하고 시스템 COM 경로를 검증한다.
배포 DLL은 `../../bin/PhotoMatchProto.dll`이며, 호스트 목록 설정과 Add-in manifest는 원본을
`evidence/`에 백업하고 PhotoMatch 항목만 추가한다. manifest의 필수 여부는 분리 시험하지 않았다.
설치 스크립트는 PhotoMatch의 자동 로드를 기본값으로 등록한다. Add-in Applications에서 직접 체크할 필요가 없다.
IronCAD 시작 시 연결 모듈과 메뉴를 등록하며 GUI는 자동으로 띄우지 않는다.
3D 장면의 Add-Ins → IronCAD PhotoMatch → PhotoMatch 열기로 독립 GUI를 열거나 기존 창을 다시 표시한다.
클래식 메뉴 환경에도 IronCAD PhotoMatch 메뉴를 등록한다. 실제 메뉴 표시·자동 로드는 배포 후 화면 검증 대상이다.

현재는 개발용 설치이며, 일반 PC에서는 최초 설치 준비가 필요하다.
배포용 설치 프로그램은 아직 없다. 일반 배포 시 Python 런타임·패키지·GUI 자산과 C++ Add-in을
함께 설치하고 WebView2/VC++ 런타임 및 COM 등록을 처리해야 한다. Node/MSVC는 사용자 실행 환경에 필요하지 않다.

같은 Windows 사용자와 설치 경로에서는 GUI 창 하나를 재사용한다. 추가 실행은 기존 창을 표시하고
최소화를 해제한다. IronCAD 재시작으로 호스트가 바뀌면 사진은 유지하고 이전 문서의 대응점 연결은 초기화한다.
살아 있는 다른 호스트에서 전환할 때는 기존 캡처 복원 성공 후 전환한다.
이 기능 도입 전부터 열린 GUI는 새 코드를 읽지 않으므로 최초 업데이트 때 한 번 종료해야 한다.

GUI는 Windows 앱 색 모드에 따라 밝은/어두운 테마와 제목 표시줄을 갱신한다.
상단 언어 선택은 한국어 / English를 제공하며 처음 실행할 때 Windows 언어를 따른다.
Windows 표시 언어가 한국어이면 한국어, 그 외 언어이면 현재 지원하는 영어를 사용한다.
수동 선택은 사용자별로 보관한다. 사진 이름, 모델 이름과 대응점 값은 번역하지 않는다.

GUI는 창 생성 전에 Per Monitor V2 DPI 인식을 설정하고, 캔버스의 실제 픽셀 크기를
현재 배율에 맞춘다. 작은 작업 영역에서는 창 크기를 줄이고 패널을 스크롤할 수 있게 한다.
사진 대응점은 DPI와 무관한 원본 픽셀로 유지한다. IronCAD의 가상화된 렌더 크기는
별도로 취급한다. 다중 모니터 간 이동과 새 DLL의 전체 실화면 시험은 아직 미검증이다.

등록·목록 표시·독립 클래스 생성은 실제 호스트 API 성공과 구분한다.
호스트가 DLL을 메모리에 유지하면 체크 해제만으로 교체할 수 없으므로 작업 저장 후 IronCAD를 종료해야 한다.
재빌드 후 IronCAD를 종료하고 등록 스크립트로 배포본을 갱신한다.

해제는 IronCAD를 종료한 뒤 수행한다. 다른 Add-in 설정과 배포 DLL 파일은 보존한다.

```powershell
& .\scripts\register.ps1 -Unregister
```

## 작업 흐름

1. 사진 열기: PNG/JPG/BMP/AVIF 지원. AVIF는 원본 크기의 PNG를 로컬 캐시에 생성해 IronCAD에 전달한다.
2. 모델 점 선택: 현재 상태를 보관하고 IronCAD의 정확한 꼭짓점을 선택한다.
3. 대응점 행 선택 → 사진 클릭. 이미 배치한 점은 직접 드래그해 수정한다. 원본 사진 픽셀로 저장된다.
4. 카메라 계산: 원근 자세와 초점거리를 추정하고 점별 오차 및 예상 위치를 사진에 표시한다.
5. IronCAD에서 확인: 계산한 카메라와 사진을 미리 본다. 보기 복원은 기존 카메라로 돌아간다.
6. 결과 저장으로 사진 정보·대응점·계산·호스트 검증 결과를 JSON에 저장한다. 결과 열기로 사진과 점을 다시 연다.

IronCAD 재시작 후 같은 문서에서 결과를 열면 저장된 객체/꼭짓점 ID, 좌표와 전역 변환을
다시 조회해 검증한 뒤 연결한다. 다른 문서는 사진 검토·편집만 허용하며 카메라 적용을 막는다.
점 수정 후에는 다시 계산해야 한다. 사진 중심을 주점으로, 정사각 픽셀과 왜곡 없는 렌즈를 가정한다.
계산 오차는 원본 사진 픽셀, IronCAD 투영 시험은 물리 화면 픽셀로 구분한다.
작은 잔차만으로 실제 렌즈 보정이나 모델/사진 형상의 일치를 보장하지 않는다.

이미지는 EXIF 회전을 자동 적용하지 않은 원본 픽셀 기준이다. 양쪽 표시를 같은 방향으로 유지한다.
API 반환 좌표와 전역 변환 후보를 함께 보존하며, 알려진 시험 모델로 확인하기 전에는 좌표 규약을 확정하지 않는다.
결과 JSON은 항상 `runtime_matrix_not_completed`를 유지한다. 전체 행렬 통과 여부는 별도로 검토한다.

## 연결 구조

`gui/web`(HTML/JavaScript/Tailwind) ↔ `gui/app.py` ↔ Named Pipe ↔ `src/HostSession.cpp` ↔ IronCAD API.
HTTP로 CAD 제어를 노출하지 않는다. pywebview는 정적 화면을 로컬에서 제공한다.
Named Pipe는 현재 사용자만 접근하며 원격 클라이언트를 받지 않는다.

C++의 보이지 않는 메시지 창이 호스트 스레드에서 비동기 I/O를 확인하고 API를 호출한다.
문서 세션과 요청 기한을 검증해 오래된 명령을 거부한다. 문서 전환/종료 시 기존 작업을 중단한다.
클라이언트 타임아웃은 실행 실패를 보장하지 않으므로 쓰기 명령을 자동 재시도하지 않는다.
GUI 종료 시 활성 캡처가 있으면 복원을 요청한다. 호스트가 살아 있는데 복원에 실패하면 GUI를 유지한다.

배경 경로는 독립 복원 스냅샷을 먼저 확인하고 불가능하면 변경하지 않는다.
사진 오버레이만 IronCAD 뷰에 붙는 클릭 통과 창으로 남긴다.
모델 보존 검사는 변환·부품 경계상자 비교이며 전체 토폴로지 동일성 증명은 아니다.

## 검증

```powershell
& .\tests\RegistrationProbe.ps1
& .\scripts\test-gui.ps1
```

독립 투영 수식 테스트는 build.ps1에 포함한다.
GUI 좌표/잘못된 이미지/문서 전환 테스트와 실제 Win32 Named Pipe 전송 테스트는 test-gui.ps1로 실행한다.
전송 테스트의 C++ echo 프로세스는 IronCAD 런타임 검증을 대신하지 않는다.
브라우저 시험은 설치된 Microsoft Edge의 headless 모드를 사용한다(개발용 Playwright).
100/125/150/175/200% 배율, 실행 중 DPI 변경, 네 가지 창 크기, 테마·언어 전환과
대응점 보존을 검사한다. 이 모의 시험은 실제 Windows 배율 시험과 구분한다.

정밀 투영 진단을 포함한 호스트 JSON은 다음 명령으로 화면 끝점 규약 가설과 비교할 수 있다.
이 분석은 호스트의 통과 판정을 바꾸지 않는다.

```powershell
& .\.venv\Scripts\python.exe .\scripts\analyze-projection.py .\evidence\camera-double-matrix.json --output .\evidence\projection-raster-hypothesis.json
```

실제 호스트 검증 범위는 `docs/PROTO0_STATUS.md`, 전체 기준은 `tests/ACCEPTANCE.md`를 따른다.
`tests/fixture`의 합성 기준 자료는 호스트 화면 증거와 구분한다.
사용자가 제공한 샘플은 별도 testsample 경로에서 읽으며 원본을 수정하거나 Git에 추가하지 않는다.
`evidence/`의 화면·JSON·설치 백업과 `gui/.runtime/`의 캐시는 로컬에만 보존한다.

## 의존성

- pywebview 6.2.1 / Pillow 12.3.0: `gui/requirements.txt`
- Tailwind CSS 4.3.0: `gui/web/package-lock.json`; 컴파일된 CSS를 로컬에 포함
- nlohmann/json 3.12.0: `third_party/nlohmann/json.hpp`, MIT 라이선스 동봉
  - 원본: https://github.com/nlohmann/json/releases/tag/v3.12.0
  - SHA-256: aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
