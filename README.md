# IronCAD PhotoMatch Proto 0

현재 상태: **구현·x64 빌드·독립 수식 테스트·IronCAD 내부 초기화·검증 창·카메라 상태 읽기 확인, 수치·화면 정합 검증 미완료**. 최신 확인 범위는 `docs/PROTO0_STATUS.md`에 기록한다.
SDK 원본 샘플은 수정하지 않는다. Python 사진 편집 UI, PnP, 렌즈 보정은 포함하지 않는다.

## 빌드와 등록

PowerShell에서 이 폴더를 기준으로 실행한다.

```powershell
& .\scripts\build.ps1
& .\scripts\register.ps1
```

결과 DLL: `build/v143/PhotoMatchProto.dll`.
현재 도구: `D:\BuildTools\VisualStudio2022`, MSVC 14.44.35207 / v143, x64 Unicode, 동적 MFC, 정적 ATL, Windows SDK 10.0.19041.0.
SDK 샘플의 v140 구성을 출발점으로 사용했다. D 드라이브 설치로 변경하면서 현재 기본 도구 집합은 v143이다. 이것은 IronCAD의 공식 필수 버전이라는 뜻이 아니며 실제 호환성 판정에는 호스트 실행 검증이 필요하다.
v140이 별도로 준비된 경우 `build.ps1 -Toolset v140`을 사용할 수 있다. 현재 환경에는 v140을 다시 설치하지 않았다.
새 도구 설치 스크립트는 `scripts/setup-buildtools.ps1`이며 D 드라이브 경로와 다운로드 임시 경로를 사용한다. 기존 Windows SDK와 공용 Windows/VC 구성요소를 임의로 삭제하지 않는다.

등록 스크립트는 관리자 권한의 64비트 PowerShell에서 배포 DLL을 System32/regsvr32.exe로 등록하고 HKLM의 InprocServer32 경로와 Apartment 설정을 확인한다. 일반 PowerShell에서 실행하면 관리자 권한으로 다시 실행한다. 이전 HKCU 수동 등록은 도구 검사에서는 통과했지만 실제 IronCAD는 CLSID를 찾지 못했다. 시스템 등록 후 같은 IronCAD 프로세스에서 재시작 없이 검증 창이 열렸다. **Add-Ins > Add-in Applications**에서 **PhotoMatchProto**를 체크하고 OK로 활성화한다. 최초 목록 설정 변경에는 재시작이 필요할 수 있다.
등록은 호스트 로드 성공을 의미하지 않는다. DLL의 `LoadLibrary` 성공도 `InitSelf` 실행을 증명하지 않는다.

해제하려면 작업을 저장하고 IronCAD를 종료한 뒤 실행한다.

```powershell
& .\scripts\register.ps1 -Unregister
```

DLL을 교체하거나 재빌드하기 전에는 호스트에서 Add-in을 내리거나 IronCAD를 종료한다. 제공 스크립트가 64비트 regsvr32 등록·해제를 함께 처리한다. 해제는 IronCAD 종료가 필요하며, 현재 실행 중인 호스트에서 해제 시험은 수행하지 않았다.

## 빈 장면에서 첫 연결 확인

1. 별도 빈 3D 장면에서 Add-in을 활성화한다. `IronCAD PhotoMatch - Proto 0` 창이 열려야 한다.
2. **Capture state**를 누른다. 현재 카메라 값과 화면 핸들이 캡처되어야 한다.
3. 초기 카메라 값은 유지하고 **Apply test camera**를 누른다. 이후 자동 화면 맞춤은 사용하지 않는다.
4. **Measure projection**을 누른다. 가로형과 세로형 그래픽 창에서 반복한다. SDK FOV는 단위가 미확정이므로 임의로 60을 넣지 말고 먼저 캡처된 유효 값을 사용한다.
5. 서로 다른 자세와 기존 값의 약 0.8배 화각으로 다시 적용·측정한다. 후보가 유일하게 식별되고 최대 오차가 1 물리 픽셀 이하여야 한다.
6. **Restore**, 이어서 **Save JSON**으로 복원과 측정 자료를 저장한다.

빈 장면의 투영 결과만으로 꼭짓점·중첩 어셈블리·사진 정합을 통과로 판정하지 않는다.

## 모델과 사진 시험

세부 절차는 `tests/ACCEPTANCE.md`, 분석용 기준 데이터는 `tests/fixture/expected.json`, 기준 이미지는 `tests/fixture/reference.png`에 있다. 다음 명령은 기준 파일만 재생성하며 IronCAD 모델을 조작하지 않는다.

```powershell
python .\scripts\make-fixture.py
```

**Pick vertices**는 클릭 위치를 무시하고 꼭짓점 ID로 실제 좌표를 조회한다. API 반환 좌표와 전역 변환 적용 후보를 함께 표시한다. 알려진 모델로 좌표계가 확인되기 전에는 두 값 중 어느 쪽도 확정 전역 좌표라고 주장하지 않는다. 바디가 여러 개여서 엔티티 ID가 모호한 부품은 거부한다.

**Probe background**를 먼저 시험한다. 독립 복원 스냅샷을 얻지 못하면 배경을 변경하지 않는다. 스냅샷이 가능하면 텍스처 적용을 시험하되 정확한 배치 여부는 별도 확인한다. 해당 경로가 부적합하면 **Show photo overlay**를 사용한다. 오버레이는 종횡비를 유지하는 클릭 통과 창이며, 기본 투명도는 150/255다. 이미지 중심이 주점이고 가로·세로 초점거리가 같다는 가정이다.

오버레이는 FOV 규약이 판별되고 테스트 카메라가 적용된 경우에만 켤 수 있다. 창 크기 변화에 맞춰 사진 영역과 카메라 화각을 조정한다. 문서 변경·닫기·뷰 불일치 시 작업을 중단한다. 원래 카메라와 변경한 배경은 복원을 시도하며, 실패는 오류로 남긴다. 모델 변경 검사는 변환 및 부품 경계상자 비교이므로 전체 토폴로지 동일성을 대신하지 않는다.

## 결과와 현재 차단 요인

`evidence/status.json`: 실제 확인한 빌드, DLL 로드, 등록, 미검증 항목.
`evidence/runtime-smoke.json`: 실행 중인 호스트에 읽기 전용으로 접속한 결과. 이 외부 COM 경로는 Add-in 내부 호출과 별개다.

현재 외부 COM 연결은 `0x80029C4A TYPE_E_CANTLOADLIBRARY`로 실패한다. IIronCADApp 등록이 존재하지 않는 `C:\Program Files\IronCAD\2025\bin\IRONCAD.tlb`를 참조한다. 이 프로젝트는 기존 IronCAD COM 등록을 덮어쓰지 않았다.
화면 제어 도구는 권한 변경 후 연결되었고, Add-in 목록에 PhotoMatchProto가 나타나는 것을 실제 화면으로 확인했다. 검증 창 표시와 Capture state 성공 화면을 evidence/addin-window-loaded.png 및 evidence/addin-state-captured.png에 저장했다. 사진 정합은 아직 미검증이다. 합성 기준 이미지는 화면 증거가 아니다.

Add-in JSON의 `overall_status`는 시험 행렬을 외부에서 확인하기 전까지 `runtime_matrix_not_completed`로 유지한다. FOV 수식 테스트나 DLL 로드 성공만으로 Proto 0 완료로 바꾸지 않는다.

초기화 진단: 호스트가 InitSelf를 호출하면 DLL 옆 `PhotoMatchProto-load.log`에 단계와 오류 HRESULT가 기록된다. 파일이 없는 경우 파일 쓰기 권한 또는 호스트 미호출을 구분해야 한다.

등록 검증: `tests/RegistrationProbe.ps1`은 시스템 COM 등록, COM 분류 열거, 호스트 설정 및 실제 IronCAD의 DLL 모듈 경로를 검사한다. COM 분류 열거만 통과한 이전 결과는 목록 표시의 충분한 증거가 아니었다.

## IronCAD 2027 목록 표시 설정

실제 2027 화면의 목록은 설치 폴더 `Config/Ironcad.Addin.config`의 AddIns 항목을 따른다. COM 등록과 구형 SDK의 사용자 Applications 레지스트리만으로는 이번 환경의 목록에 나타나지 않았다. `register.ps1`은 이제 `configure-host.ps1`을 호출하여 현재 DLL 경로와 CLSID를 이 파일에도 추가한다. 원본은 `evidence/Ironcad.Addin.config.before-photomatch`에 보관한다. 해제할 때에는 PhotoMatch CLSID에 해당하는 항목만 제거하며, 다른 Add-in 설정은 유지한다. 설정 파일 변경 후 호스트 재시작이 필요하다. 현재 사용자가 재시작을 담당한다.

배포 DLL은 현재 `D:\Program Files\IronCAD\2027\bin\PhotoMatchProto.dll`이다. 빌드 산출물은 별도 프로젝트의 `build/v143`에 유지하고, `configure-host.ps1`이 이 DLL 하나만 bin에 복사한다. 호스트 설정은 기존 내장 Add-in과 동일한 파일명 형식(`PhotoMatchProto.dll`), 빈 identify, system=true를 사용한다. 이 필드는 기존 설정과 맞춘 로딩 모드이며 공식 의미를 확정한 것은 아니다. 다른 DLL이나 모델 파일은 복사·수정하지 않는다. 재빌드 후에는 `register.ps1`을 다시 실행해 배포본을 갱신한다. 언로드/해제 시 설정만 제거하며 배포 DLL은 남는다.

로드 로그는 DLL 옆 `PhotoMatchProto-load.log`에 생성된다. 각 줄의 프로세스 ID와 시간을 실제 IronCAD 프로세스와 대조한다. 별도 PowerShell의 COM 생성 테스트 로그를 호스트 로드 성공으로 판단하지 않는다.

## 추가 확인: Side-by-side COM manifest

진단 과정에서 IronCAD.exe.manifest → IronCAD.External.manifest → IronCAD.AddIn.manifest 연결에 PhotoMatch COM 클래스를 추가했다. 이 변경만으로는 호스트가 로드되지 않았고, 실제 해결은 시스템 COM 등록 후 확인했다. manifest 항목이 필수인지는 분리 시험하지 않았다. `configure-host.ps1`은 이제 `configure-manifest.ps1`을 통해 `bin/IronCAD.AddIn.manifest`에 PhotoMatch DLL의 COM 클래스 항목도 추가한다. 원본 백업은 `evidence/IronCAD.AddIn.manifest.before-photomatch`이다. 해제 시 이 클래스에 해당하는 file 항목만 제거한다. 실행 권한·보안 설정이나 기존 클래스는 바꾸지 않는다.

`tests/ActivationContextProbe.ps1`은 실제 IronCad.exe.manifest로 Windows 활성화 컨텍스트를 생성하여 COM 클래스 조회와 IZAddinServer 생성을 검사한다. 추가 전에는 오류 14007, 추가 후에는 조회 성공 및 생성 HRESULT 0x00000000을 확인했다(`evidence/activation-before.json`, `evidence/activation-after.json`). 이것은 별도 진단 프로세스의 결과이며 호스트 InitSelf 성공을 대신하지 않는다. manifest 변경 뒤 IronCAD 재시작이 필요하다.
