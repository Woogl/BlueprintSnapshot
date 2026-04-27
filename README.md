# DwBlueprintSnapshot

블루프린트 에셋을 저장할 때마다 그 안의 내용(기본값, 컴포넌트, 변수, 함수, 그래프 로직)을 JSON 파일로 자동 기록합니다.

## 이 플러그인으로 할 수 있는 것

### 1. 블루프린트 변경점을 diff로 읽기
`.uasset`은 바이너리라 source control에서 `Binary files differ`만 뜹니다.  
이 플러그인을 사용하면 JSON 파일을 리포지토리에 커밋할 수 있으므로, 에디터를 열지 않아도 "누가 언제 BP의 컴포넌트나 변수 값을 수정했는지"를 추적할 수 있습니다.

### 2. AI에게 블루프린트 내용 전달
생성된 JSON은 BP의 **구조·설정·대략적 로직 흐름**을 텍스트로 담기 때문에 AI 코드 어시스턴트에 붙여넣어 질문·리뷰 보조로 쓸 수 있습니다.  
스크린샷보다 파싱 정확도가 높고, 토큰을 절약할 수 있습니다.

### 3. 블루프린트 일괄 검수
JSON이라 **전체 프로젝트 BP를 스캔**할 수 있습니다.  
예: "Tick이 켜진 Actor BP 찾기", "특정 인터페이스를 구현한 BP 전체 목록".

### 4. UMG MVVM 플러그인 지원
UMG MVVM의 뷰모델과 뷰바인딩 상태도 JSON으로 추출합니다.  
WBP의 뷰바인딩 현황 파악, WBP 에셋 히스토리 관리, UI 코드 리뷰에 도움이 됩니다.

### 5. BP 삭제 시 스냅샷 자동 정리
콘텐츠 브라우저에서 BP 에셋을 삭제하면 대응되는 JSON 파일도 자동으로 삭제됩니다.  

---

## Quick Start

1. 이 플러그인 전체를 프로젝트의 Plugins 폴더에 추가.
2. `Project Settings > Dw > DwBlueprintSnapshot`에서 BP에서 추출할 데이터 선택.
3. 블루프린트를 저장 (Ctrl+S).  
4. `Plugins/DwBlueprintSnapshot/Snapshots/<BP 패키지 경로>.json` 파일이 생성/업데이트됨.

---

## 어떤 블루프린트가 대상인가?

| 블루프린트 종류 | 지원 | 비고 |
|---|---|---|
| UObject Blueprint | ✅ | Character, Actor, GameplayAbility 등 UObject를 상속한 모든 BP |
| Widget Blueprint (UMG) | ✅ | 위젯 트리 + MVVM 바인딩까지 추출 |
| Animation Blueprint | ⚠️ | AnimGraph 스테이트머신은 지원 안됨 |
| Data Asset | ❌ | 블루프린트가 아닌 UObject 에셋 |
| Data Table / Curve Table | ❌ | CSV/JSON Export 사용 권장 |
| Blueprint Interface | ❌ | 함수 시그니처만 있는 BP. 스킵 |
| Blueprint Macro Library | ❌ | 스킵 |
| Blueprint Function Library | ❌ | 스킵 |
| Editor Utility Blueprint | ❌ | 스킵 |
| Control Rig / Niagara / Metasound | ❌ | 전용 그래프 포맷, 미지원 |

---

## 블루프린트의 어떤 데이터가 추출되는가?

### Details 패널의 데이터

`bSkipUnchangedDefaults=true` 시 부모 클래스 CDO 대비 변경점만 기록(기본 세팅).  
`bSkipUnchangedDefaults=false` 시 전체 기록.

| 항목 | 지원 |
|---|---|
| 멤버 변수 | ✅ |
| 에셋 참조 (StaticMesh, Material 등) | ✅ 경로 문자열 |
| TArray, TMap, TSet 원소 | ✅ |
| Struct 멤버 | ✅ 재귀 기록 |
| Instanced Subobject | ✅ 재귀 기록 |
| `EditAnywhere`/`VisibleAnywhere`/`BlueprintReadWrite`/`BlueprintReadOnly`/`BlueprintAssignable` 속성 | ✅ |
| `Transient` / `DuplicateTransient` / `NonPIEDuplicateTransient` / `Deprecated` / `EditorOnly` 속성 | ❌ 의도적으로 제외 |

### 컴포넌트 (Components 탭에 추가한 것)
| 항목 | 지원 |
|---|---|
| BP의 Components 탭에서 직접 추가한 컴포넌트 | ✅ `components` 필드 (class/attachParent/attachSocket + CDO delta) |
| 컴포넌트 간 어태치 (부모 + 소켓) | ✅ |
| 부모 BP/C++에서 상속된 컴포넌트의 Override 값 | ✅ `classDefaults` 델타에 instanced subobject로 기록 |

### 위젯 블루프린트 (WBP)

MVVM 추출은 프로젝트 세팅에서 비활성화 가능.

| 항목 | 지원 |
|---|---|
| Widget Tree | ✅ 각 위젯에 class/parent/slot 기록 |
| 각 위젯의 Details 값 | ✅ |
| 각 위젯의 PanelSlot 값 | ✅ |
| MVVM ViewModel 컨텍스트 | ✅ class, creationType, optional, setter/getter 등 |
| MVVM 바인딩 | ✅ source/destination/bindingType + conversion 함수 |
| MVVM Conversion Function 인자 핀 | ✅ property path / literal value / orphaned 상태까지 |
| 비활성/비컴파일 바인딩 | ❌ 스킵 |
| Widget Animation | ❌ |
| UMG 구식 Property Binding | ❌ MVVM만 지원 |
| Named Slot의 내용물 | ❌ 런타임 주입이라 BP엔 없음 |

### 이벤트/함수 그래프 (의사코드)

이벤트 그래프는 `eventGraph` 필드에 하나의 문자열 배열로 평탄화된다.  
함수 그래프는 `newFunctions` 필드에 함수 이름을 키로 한 object(각 함수는 라인 배열)로 기록된다.  
Construction Script도 함수 그래프의 하나로 취급되어 `newFunctions.UserConstructionScript`에 동일한 의사 코드 포맷으로 기록된다.

| 항목 | 지원 |
|---|---|
| Event, Custom Event | ✅ |
| Branch (if/else) | ✅ |
| Sequence | ✅ `# sequence[N]` 코멘트와 함께 |
| 변수 Set | ✅ |
| 함수 호출 (CallFunction) | ✅ 인자는 non-default/연결된 핀만 |
| 자동 형 변환 (`Conv_*`) | ✅ `Cast<Type>(expr)` 형태로 렌더링 |
| Cast | ✅ `if (Cast<Type>(expr)):` / `else:` 분기 렌더링 |
| 데이터 핀 역추적 (Variable Get, Self, Literal) | ✅ 최대 32 depth 재귀 |
| Exec cycle(goto) | ✅ `goto NodeName`으로 표시 |
| Return | ✅ |
| ForEach / ForLoop / While (Macro) | ✅ LoopBody/Completed 분기 렌더링 |
| Delay, PrintString 등 Latent/유틸 함수 호출 | ✅ 일반 함수 호출로 렌더 (`KismetSystemLibrary::Delay(Duration=...)`) |
| Timeline, Gate, Async Action, 기타 전용 K2Node | ⚠️ 노드 타이틀 한 줄 fallback |
| Event Dispatcher (Call / Bind / Assign) | ⚠️ 노드 타이틀 한 줄 fallback |
| 매크로 내부 본문 | ❌ 호출 라인만 |
| Knot(리라우팅 노드) | ❌ 와이어 경로만 따라가고 노드 자체는 출력에 표시 안 함 |
| 로컬 변수 | ❌ |
| 코멘트 박스 | ❌ |
| 노드 위치/색상 | ❌ |

### 그 외
| 항목 | 지원 |
|---|---|
| 부모 클래스 | ✅ |
| 변수 목록 | ✅ `newVariables` 필드에 {type, value} 기록
| 구현 인터페이스 목록 | ✅ `interfaces` 필드에 기록 |

---

## 무엇을 위한 도구로 쓰면 안되는가?

- **블루프린트 백업/복원 도구가 아닙니다.**
  - JSON에서 `.uasset`을 재생성하지 않습니다.
  - 백업은 Source Control로 하세요.
- **시각적 그래프 뷰어가 아닙니다.**
  - 노드 위치·색상·코멘트를 기록하지 않습니다.
  - BP를 눈으로 보고 싶으면 에디터를 여세요.
- **런타임 도구가 아닙니다.**
  - 에디터 전용입니다.
  - 패키징된 게임에 포함되지 않습니다.
- **실시간 분석기가 아닙니다.**
  - BP를 저장할 때만 기록합니다.

---

## 트러블슈팅

**스냅샷이 안 생겨요**
- BP를 `Ctrl+S`로 실제 저장했는지 확인 (Autosave는 무시됨).
- `Project Settings > Dw > DwBlueprintSnapshot`에서 `bEnabled`가 켜져있는지.
- `IncludeDirectories`에 값이 있다면 내 BP 경로가 포함되는지.
- BP가 **Dirty / Error 상태**면 스킵됨. 컴파일 성공 후 저장 필요.
- 대상이 Blueprint Interface / Macro Library / Function Library면 의도적으로 스킵됨.

**PIE / 쿠킹 중엔 추출이 안되나요**
- 네. PIE, Cook, Autosave, Commandlet 실행 중엔 동작하지 않도록 의도했습니다.

**특정 BP만 스냅샷이 안 생겨요**
- Output Log에서 `LogDwBPSnapshot: Error` 로그를 확인하세요.
- 예상 경로가 240자를 넘으면(`Plugins/DwBlueprintSnapshot/Snapshots/...` 기준) Windows MAX_PATH(260) 제한으로 저장이 실패하므로 해당 BP는 스킵됩니다.
- 해결: 프로젝트를 더 짧은 드라이브 경로에 두거나, `Project Settings > Dw > DwBlueprintSnapshot`에서 `OutputDirectory`의 경로를 변경하세요. 

---

## 설정

`Project Settings > Dw > DwBlueprintSnapshot`

| 항목 | 기본값 | 설명 |
|---|---|---|
| `bEnabled` | true | 전체 기능 on/off |
| `FileExtension` | `.json` | 스냅샷 파일 확장자. 점을 포함해 입력 (예: `.json`, `.bpj`) |
| `OutputDirectory` | `Plugins/DwBlueprintSnapshot/Snapshots` | 스냅샷 저장 루트 폴더. |
| `IncludeDirectories` | [] | 대상 BP 폴더 (비어있으면 전체) |
| `ExcludeDirectories` | [] | 제외 BP 폴더 |
| `bSkipUnchangedDefaults` | true | 기본값과 동일한 프로퍼티를 classDefaults/컴포넌트/위젯 delta에서 제외 (false로 두면 전체 덤프) |
| `bIncludeComponents` | true | Components 탭 추출 |
| `bIncludeVariables` | true | 변수 목록 추출 |
| `bIncludeInterfaces` | true | 구현 인터페이스 추출 |
| `bIncludeWidgetTree` | true | WBP 위젯 트리 추출 |
| `bIncludeMVVM` | true | WBP MVVM 바인딩 추출 |
| `bIncludeGraphs` | true | 이벤트/함수 그래프 의사코드 추출 |

---

## 출력

- **경로**: `<OutputDirectory>/<BP 패키지 경로><FileExtension>` (기본값: `Plugins/DwBlueprintSnapshot/Snapshots/...`)
- **포맷**: UTF-8 (no BOM), 키 알파벳 정렬, 들여쓰기 2-space pretty print
- **ReadOnly 플래그**는 자동 해제 후 덮어쓰기 (Perforce 등에서 편의)
- **삭제 동기화**: BP 에셋을 삭제하면 대응되는 JSON 파일도 함께 제거됨
- **예시 파일**: 플러그인 Content 폴더의 `BP_SampleCharacter.uasset`에 대응되는 `Snapshots/DwBlueprintSnapshot/BP_SampleCharacter.json` 참조

### 최상위 JSON 필드

| 필드 | 타입 | 조건 |
|---|---|---|
| `blueprintPath` | string | 항상 |
| `parentClass` | string | 항상 |
| `classDefaults` | object | CDO 델타가 비어있지 않을 때 (NewVariables는 중복 제외) |
| `components` | object | `bIncludeComponents` + Components 탭에 컴포넌트 1개 이상 |
| `newVariables` | object | `bIncludeVariables` + 변수 1개 이상 (`{type, value}` 맵) |
| `interfaces` | object | `bIncludeInterfaces` + 구현 1개 이상 (`{implemented: [...]}`) |
| `eventGraph` | array&lt;string&gt; | `bIncludeGraphs` + Ubergraph에 entry 1개 이상 |
| `newFunctions` | object | `bIncludeGraphs` + 함수 그래프 1개 이상 |
| `widgetTree` | object | WBP + `bIncludeWidgetTree` |
| `mvvm` | object | WBP + `bIncludeMVVM` + MVVM 확장 존재 |

---

## 요구사항

- Unreal Engine 5.7
- 에디터 빌드 (`*Editor.Target.cs`)
- 의존 모듈: `ModelViewViewModel` (MVVM 추출용, 5.7 기본 포함)
