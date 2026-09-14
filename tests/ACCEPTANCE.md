# Proto 0 실행 검증표

새 GUI/연결 모듈의 호스트 시험은 재실행 후 검증한다. 최신 결과는 docs/PROTO0_STATUS.md에 기록한다. 결과 JSON과 실제 화면 캡처를 함께 보관한다.

## 기준 모델

사용자 작업 문서와 분리된 시험 장면을 만든다. 원점 기준 상자 A의 API 단위 크기는 (100,80,60), 로컬 범위는 x=[0,100], y=[0,80], z=[0,60]이다. UI 치수 단위와 API 단위가 같다고 가정하지 않는다. 알려진 치수의 작은 상자에서 꼭짓점 간 차이를 먼저 측정하여 환산을 기록한다.

중첩 어셈블리의 상자 B는 로컬 크기 (40,30,20)이다. 오른손 좌표계의 열벡터 기준으로 안쪽 어셈블리에 Ry(+90도), 이동 (50,20,10)을 적용하고, 바깥 어셈블리에 Rz(+90도), 이동 (180,60,0)을 적용한다. 각 단계는 회전 후 이동이다. 예상 전역 좌표는 (160-y,110+z,10-x)이다. 실제 IronCAD의 회전 UI 규약이 다르면 동일한 최종 배치가 되도록 확인한다.

상자 A와 B의 8개 꼭짓점을 각각 선택한다. A만으로는 로컬·전역 좌표계를 구분할 수 없다. B에서 API 반환 좌표와 변환 후보를 `fixture/expected.json`의 로컬·전역 값과 비교한다. 테스트 입력의 ID는 런타임 선택 ID와 다르므로 좌표와 부품 이름을 기준으로 대응을 기록한다. API 단위, 적용된 변환 순서, 좌표계 판정과 최대 좌표 오차를 시험 기록에 적는다.

## 시험 행렬

아래의 각 조합을 별도 JSON과 화면 증거로 남긴다. 복원 전후 화면은 같은 창 크기로 비교한다.

| 항목 | 시험값 | 통과 기준 |
|---|---|---|
| 모델 | 단일 A / 중첩 B | 정확한 꼭짓점 API 값과 알려진 전역 좌표 일치 |
| 카메라 | 다른 자세 2개 이상 | 입력·읽기값 기록, 축 방향 일치 |
| 화각 | 유효한 값 2개 이상 | 단위·기준축·전체/반각 규약이 유일하게 판별됨 |
| 그래픽 창 | 가로형 / 세로형 | 예상과 SDK 투영의 최대 거리 <= 1 물리 px |
| Windows 배율 | 100% / 150% | 실제 설정·렌더 크기·물리 크기 기록, 같은 1px 기준 |
| 사진 | 모서리 / 내부 꼭짓점 | 크기·이동·배율 변경 후 정합 유지 |
| 복원 | 카메라 / 변경한 배경 | 값·화면 비교 통과, 모델 형상·변환 유지 |
| 수명 | 문서 전환 / 문서 닫기 | 다른 문서에 적용 안 됨, 호스트 계속 응답 |
| 오류 입력 | 비이미지 / 손상 파일 | 오류 표시, 호스트 계속 응답, 복원 가능 |

화면 DPI API가 96을 반환해도 Windows 설정이 100%라고 단정하지 않는다. DPI 가상화가 있을 수 있으므로 실제 Windows 배율과 렌더/물리 크기 비율을 함께 기록한다. 화면 배율을 바꾼 뒤 새 측정 파일로 시험한다.

## 기준 이미지 정합

1. `expected.json`에 적힌 모델 배치와 API 단위 환산을 확인한다.
2. FOV 식별용 측정을 가로·세로 창에서 먼저 완료한다.
3. 테스트 카메라 Position=(350,-450,350), Direction=(-270,500,-320), Up=(0,0,1)을 적용한다. 화각은 먼저 확인한 유효 값을 유지한다.
4. 진단의 **배경 경로 시험**를 실행해 복원 가능 여부를 기록한다. 지원되면 배경 적용/복원 및 배치 정밀도를 확인한다. 부적합하면 복원 후 오버레이 경로를 사용한다.
5. 진단의 초점거리=1000, `fixture/reference.png`를 선택해 **IronCAD에서 확인**를 켠다. 이미지 크기는 1600x1000이다.
6. 두 상자의 내부 표식, 네 모서리, 창 중앙을 확인한다. 픽셀 수치의 기준은 `expected.json`이며 PNG 선 두께를 좌표 오차로 사용하지 않는다.
7. 창의 가로·세로 비율과 위치를 바꾸고 동일한 점을 비교한다. 카메라 자동 화면 맞춤은 누르지 않는다.
8. **원래 보기로** 후 **결과 저장**. 원래 카메라 전체 값과 모델 변환/경계상자 결과를 확인하고, 형상 동일성은 호스트에서도 별도로 확인한다.

1 물리 화면 픽셀 기준은 이번 연결 검증 기준이다. 다음 단계 계산부의 0.1 원본 이미지 픽셀 기준과 혼용하지 않는다.

## 증거 파일

권장 이름: `dpi100-landscape-camera1.json`, `dpi150-portrait-camera2.json` 등. 화면 증거에는 그래픽 창 전체, 사진 영역 경계, GUI 진단 결과가 포함되어야 한다. 원본/복원 화면과 실패 사례도 보존한다. 모든 조합이 통과한 뒤에만 전체 완료를 판정한다.

## 독립 GUI 연결 추가 확인

- MFC 검증 대화상자가 나타나지 않고 Python 창에서 현재 문서가 표시된다.
- 모델 꼭짓점 선택 이벤트가 대응점 목록에 반영된다. 사진에서 지정한 위치는 원본 픽셀로 저장된다.
- 같은 문서에서는 검증 가능한 모델 참조와 사진 좌표를 유지한다. 다른 문서의 이전 대응점은 검토 상태로 보존하고, 이전 세션의 적용 요청은 거부한다.
- Python GUI 종료 시 테스트 카메라가 복원된다. GUI가 중단되어도 IronCAD를 종료시키지 않는다.
- 연결 단절 시 CAD 작업 버튼이 비활성화된다. 전송 실패한 쓰기 명령을 자동 재시도하지 않는다.
- README에 기재된 14개 이미지 형식 및 잘못된 이미지 입력을 시험한다. 캐시 변환은 원본 크기/방향을 보존한다.

## Windows 표시와 언어

- Windows 앱 색 모드의 밝게/어둡게 전환이 GUI와 제목 표시줄에 반영된다.
- 언어 선택은 한국어 / English를 제공하고 수동 선택은 다음 실행에 유지된다. 별도의 시스템 선택 항목은 표시하지 않는다.
- 새 설정의 기본 언어는 영어이다. 사용자가 선택한 언어는 유지된다.
- 언어·테마 전환이 사진, 사용자 모델 이름, 선택한 대응점, 원본 이미지 좌표를 변경하지 않는다.
- Windows 100%/150%에서 실제 창과 캔버스 DPI를 확인한다. 가능하면 배율이 다른 모니터로 이동한다.
- 높은 배율의 작은 작업 영역에서 창이 들어가며, 패널 스크롤로 모든 기능에 접근할 수 있다.
- GUI 브라우저 모의 시험의 DPR 결과를 실제 IronCAD 투영 결과로 대신하지 않는다.
- 시험을 마치면 사용자의 원래 Windows 배율·테마와 언어 기본값을 복구하고 확인한다.

## Point workflow and package checks

- Finish model picking: photo selection starts at P1, or the first remaining card.
- Clear: only photo coordinates disappear; model points, IDs, and photo remain.
- Delete one pair: remaining IDs do not shift. Delete all model points: next ID is P1.
- Model markers remain readable at 100% and 150% DPI. New points select their cards.
- Photo opacity: test 0%, 25%, and 100%, including during camera adjustment and after resize.
  Camera state, image placement, and point coordinates must remain unchanged.
- Start without IronCAD: opening the first photo works. Matching requires a host.
  A failed restore must retain the previous photo and points.
- Run the setup EXE on a Windows x64 / IronCAD 2027 machine without Python or Node.
  Verify WebView2, Add-Ins launch, writable cache, save/load, and preserved unrelated add-ins.
- Close the host before updating or removing the package. Never overwrite unsaved work.
