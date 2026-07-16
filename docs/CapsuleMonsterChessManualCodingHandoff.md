# Capsule Monster Chess 수동 코딩 인수인계

이 문서는 Axmol로 UI와 게임 로직을 직접 작성할 때 필요한 API, 현재 Capsule Monster Chess 구조,
Unity·Unreal Engine과의 개념 대응, 안전한 확장 위치를 한곳에 정리한 실무용 인수인계다.

Unity 원본의 `Resources/Prefabs/UI`, 씬, `UI_Manager`/`UI_Base` C# 프레임워크를 실제로 대조한
Anchor·Stretch·Layout·이벤트·애니메이션·효과·현지화 상세 구현은
[Unity UI → Axmol 구현 가이드](CapsuleMonsterChessUnityUiPortingGuide.md)를 따른다.

## 1. 기준과 핵심 결론

- 조사 기준일: 2026-07-15
- 저장소: `axmol_my`
- 브랜치: `feature/cartoon3d-v3`
- 코드 기준 커밋: `b4ad80e47` (`Add Capsule Monster Chess battle runtime`)
- 엔진 기준: 로컬 `v3.0.0-alpha22` 계열
- 게임 클라이언트 기준 해상도: 720×1280, `ResolutionPolicy::SHOW_ALL`
- 클라이언트 프레임 설정: 30 FPS
- 전투 판정: 순수 C++17 `GameCore`, 50 ms fixed tick

가장 중요한 차이는 다음 한 문장으로 정리된다.

> Axmol에는 Unity Editor의 Scene/Prefab/Inspector나 Unreal Editor의 Level/Blueprint/UMG에 해당하는
> 통합 제작기가 없다. 화면 트리, 입력 연결, 스타일, 씬 전환을 C++로 직접 만들되, 전투 판정은
> `ax::Node`에서 분리한 순수 C++ `GameCore`에 둔다.

현재 프로젝트는 이 원칙에 따라 다음 두 세계를 분리한다.

```text
Axmol 클라이언트 세계                    순수 게임 규칙 세계

AppDelegate                              BattleTypes
  └─ Director                            HexBoard
      └─ Scene                           DeterministicRandom
          ├─ LobbyLayer                  BattleSimulator
          └─ BattleScene                 BattleSimulateResult
               └─ 리플레이 표현  <────── BattleLogEvent
```

`BattleScene`은 승패를 새로 판단하지 않는다. `BattleSimulator`가 만든 이벤트를 카메라, 모델, UI,
Action으로 보여주는 프레젠테이션 계층이다.

## 2. Unity·Unreal·Axmol 대응표

아래 대응은 학습을 위한 개념 비교다. 클래스 기능이 완전히 같다는 뜻은 아니다.

| 목적 | Axmol | Unity | Unreal Engine | 이 프로젝트 |
|---|---|---|---|---|
| 프로세스 시작·플랫폼 생명주기 | `ax::Application`, `AppDelegate` | Player bootstrap, `Application` 이벤트 | Engine/GameInstance 초기화, 플랫폼 앱 | `Source/AppDelegate.*` |
| 전역 프레임·씬 관리자 | `ax::Director` | Player loop + `SceneManager` 일부 | `UGameEngine` + viewport + world travel 일부 | 첫 씬 실행, 로비↔전투 교체 |
| 화면/월드 루트 | `ax::Scene` | Unity Scene의 실행 계층 | `UWorld`/Level | `MainScene`, `BattleScene` |
| 계층 노드·Transform | `ax::Node` | `GameObject` + `Transform`에 가까움 | `AActor` 또는 `USceneComponent`에 가까움 | 화면 root, 유닛 root |
| 화면 단위 컨테이너 | `ax::Layer` | Canvas 아래 화면 controller | `UUserWidget` root에 가까움 | `LobbyLayer` |
| UI 기본형 | `ax::ui::Widget` | uGUI `Selectable`/`Graphic` 계층 | UMG `UWidget` | 터치 가능한 패널·텍스트·이미지 |
| UI 화면 묶음 | `ax::ui::Layout` | `RectTransform` container, Layout Group | Panel Widget | 카드, top bar, overlay |
| 텍스트 | `ax::ui::Text`, `ax::Label` | `Text`/TextMeshPro | `UTextBlock` | 조작 UI는 주로 `ui::Text` |
| 버튼 | `ax::ui::Button` 또는 touch-enabled `Layout` | `Button.onClick` | `UButton::OnClicked` | 현재는 `Layout` 기반 커스텀 버튼 |
| 이미지 | `ax::ui::ImageView`, `ax::Sprite` | `Image`, `SpriteRenderer` | `UImage`, Sprite/mesh component | 카드 아이콘, 9-slice 프레임 |
| 스크롤 | `ax::ui::ScrollView`, `ListView` | `ScrollRect` | `UScrollBox`, `UListView` | 유닛·랭킹·퀘스트 목록 |
| 매 프레임 실행 | `scheduleUpdate()` + `update(dt)` | `MonoBehaviour.Update()` | `AActor::Tick()`/component tick | 전투 리플레이 시간 진행 |
| 지연·주기 실행 | `schedule`, `scheduleOnce` | Coroutine, `Invoke`, timer | `FTimerManager`, latent action | 가챠·결과 지연 |
| 짧은 연출 | `runAction()` + `Sequence` | Animator/Coroutine/tween | Timeline/latent sequence | 이동·점프·공격·사망 |
| 이벤트 버스 | `EventDispatcher`, `CustomEvent` | C# event/`UnityEvent` 일부 | delegate/event dispatcher 일부 | 전투 로그→프레젠테이션 전달 |
| 리소스·저장 경로 | `FileUtils` | `Resources`, `persistentDataPath` 일부 | `FPaths`, `IPlatformFile` 일부 | JSON 테이블·로컬 프로필 |
| 데이터 에셋 역할 | 런타임 JSON + catalog class | `ScriptableObject` | `UDataAsset`, Data Table | `MonsterCatalog`, `StageCatalog` |
| 순수 규칙 모듈 | 일반 C++ 정적 라이브러리 | 순수 C# assembly/asmdef | UObject 비의존 Runtime module | `GameCore` |
| 영속 데이터 교체점 | C++ interface | repository/service interface | subsystem/service interface | `IUserDataSource` |

### Director를 `GameInstance`라고 부르면 안 되는 이유

`Director`는 씬 실행, 프레임, Scheduler, ActionManager, EventDispatcher, Renderer 접근을 가진 엔진
런타임 관리자다. Unreal의 `UGameInstance`처럼 “레벨 교체 후에도 보존되는 게임 데이터 보관소”로
사용하는 클래스가 아니다. 영속 사용자 상태는 현재 `UserProfile`과 `IUserDataSource`가 담당한다.

### Node를 Actor라고만 부르면 안 되는 이유

`Node`는 장면 트리, transform, 자식, Action, Scheduler, 이벤트 연결을 함께 가진 경량 객체다.
Unity의 `GameObject + Transform`이나 Unreal의 `Actor/SceneComponent`와 비슷하지만, Axmol에는
동일한 reflection 기반 component authoring이나 replication이 자동 제공되지 않는다.

## 3. 실제 부팅 흐름과 Director

### 현재 부팅 순서

```text
플랫폼별 main
  → AppDelegate::applicationWillLaunch()
  → AppDelegate::applicationDidFinishLaunching()
      → Director::getInstance()
      → RenderView 생성
      → 720×1280 SHOW_ALL
      → 30 FPS
      → MainScene 생성
      → Director::runWithScene()
```

구현 위치:

- `projects/CapsuleMonsterChess/Source/AppDelegate.h`
- `projects/CapsuleMonsterChess/Source/AppDelegate.cpp`
- `projects/CapsuleMonsterChess/Source/MainScene.h`
- `projects/CapsuleMonsterChess/Source/MainScene.cpp`

### 자주 쓰는 Director API

| API | 용도 | Unity 비유 | Unreal 비유 |
|---|---|---|---|
| `Director::getInstance()` | 전역 Director 접근 | `SceneManager`/player loop 접근 | engine/world manager 접근 |
| `runWithScene(scene)` | 앱의 첫 Scene 실행 전용 | 최초 Scene 로드 | 최초 map/world 시작 |
| `replaceScene(scene)` | 현재 Scene 폐기 후 교체 | `LoadSceneMode.Single` | `OpenLevel`에 가까움 |
| `pushScene(scene)` | 현재 Scene을 스택에 보존하고 새 Scene 실행 | additive가 아닌 별도 scene stack | 일시적인 world stack에 가까우나 동일 개념 없음 |
| `popScene()` | 이전 Scene 복귀 | 수동 scene stack 복귀 | 직접 대응 없음 |
| `getRunningScene()` | 현재 Scene | active Scene | current `UWorld`/Level 일부 |
| `getCanvasSize()` | 설계 좌표계 전체 크기 | Canvas reference resolution | viewport logical size |
| `getVisibleSize()` | 정책 적용 후 보이는 크기 | safe display 영역 계산 전 canvas | viewport rect 일부 |
| `getVisibleOrigin()` | visible 영역 원점 | letterbox 보정값 | viewport offset |
| `getSafeAreaRect()` | notch 포함 안전 영역 | `Screen.safeArea` | platform safe zone |
| `getScheduler()` | 전역 callback scheduler | player loop/timer | world timer/tick manager 일부 |
| `getEventDispatcher()` | 입력·custom event | EventSystem/C# event 일부 | input/delegate 일부 |
| `getActionManager()` | Node Action 실행 | tween/Animator runtime 일부 | latent/timeline 일부 |
| `getTextureCache()` | 텍스처 캐시 | loaded texture cache | streaming/resource cache 일부 |
| `postTask(fn, timing)` | 다른 스레드 또는 현재 프레임 밖에서 main thread 작업 예약 | main-thread dispatcher | game-thread task |

### 이 프로젝트의 씬 전환 규칙

`MainScene`에서 `BattleScene`으로, 또는 반대로 교체할 때 즉시 `replaceScene()`을 호출하지 않는다.
구조 변경을 안전한 프레임 경계로 미룬다.

```cpp
#include "axmol/axmol.h"

void replaceAtFrameBoundary(ax::Scene* nextScene)
{
    if (!nextScene)
        return;

    nextScene->retain();
    ax::Director::getInstance()->postTask(
        [nextScene] {
            ax::Director::getInstance()->replaceScene(nextScene);
            nextScene->release();
        },
        ax::Director::TaskTiming::FrameBoundary);
}
```

현재 게임 코드처럼 request를 값으로 캡처한 뒤 task 안에서 Scene을 생성하는 방식이 더 단순하다.

```cpp
#include "Client/Battle/BattleScene.h"

void openBattleScene()
{
    cmc::client::BattlePresentationRequest request;
    request.stageId     = 1001;
    request.stageNumber = 1;
    request.enemyPower  = 120;
    request.rewardGold  = 100;

    ax::Director::getInstance()->postTask(
        [request] {
            auto* scene = cmc::client::BattleScene::create(request);
            if (scene)
                ax::Director::getInstance()->replaceScene(scene);
        },
        ax::Director::TaskTiming::FrameBoundary);
}
```

`runWithScene()`은 첫 Scene에만 사용한다. 로비와 전투 사이에는 `replaceScene()`을 사용한다.

## 4. Scene, Node, Layer 생명주기

### 기본 생성 패턴

Axmol Node 계열은 C++ 생성자에서 초기화하지 않고 `create()` → `init()`의 2단계 생성 패턴을 쓴다.

```cpp
#pragma once

#include "axmol/axmol.h"

class InventoryLayer final : public ax::Layer
{
public:
    CREATE_FUNC(InventoryLayer);

    bool init() override
    {
        if (!Layer::init())
            return false;

        return true;
    }
};
```

`CREATE_FUNC`는 다음을 수행한다.

1. `new InventoryLayer`
2. `init()`
3. 성공 시 `autorelease()`
4. 실패 시 `delete`

### 실행 생명주기

| Axmol | Unity | Unreal | 사용 기준 |
|---|---|---|---|
| `init()` | `Awake()`/초기 setup 일부 | constructor 이후 초기화 일부 | 노드 트리와 불변 연결 생성 |
| `onEnter()` | `OnEnable()`/`Start()` 일부 | `BeginPlay()`에 가까움 | Scene graph에 진입한 뒤 시작할 작업 |
| `scheduleUpdate()` | Update 활성 | tick 활성 | 실제 프레임 갱신이 필요할 때만 |
| `update(float dt)` | `Update()` | `Tick(float)` | 프레젠테이션 시간 진행 |
| `onExit()` | `OnDisable()` 일부 | `EndPlay()` 일부 | 외부 등록·비소유 자원 정리 |
| `removeFromParent()` | `Destroy(gameObject)`와 다름 | `DestroyActor()`와 다름 | 부모에서 제거하고 참조 카운트 감소 |

### 소유권 규칙

Axmol의 `ax::Object`는 `retain()`/`release()` 참조 카운팅과 autorelease pool을 사용한다.

- `create()`로 받은 Node는 autorelease 객체다.
- `parent->addChild(child)`가 child를 보유한다.
- `removeFromParent()` 또는 부모 파괴 시 보유 참조가 해제된다.
- 부모가 보유하는 child를 가리키는 멤버는 raw pointer를 사용할 수 있다.
- child를 부모 수명 밖에서도 보관해야 할 때만 명시적으로 `retain()`하고 짝이 맞는 `release()`를 한다.
- `ax::Node*`를 일반 `std::unique_ptr`로 감싸지 않는다.
- 반대로 게임 도메인 데이터와 service는 `std::unique_ptr`, 값 타입, STL container를 우선한다.

현재 `LobbyLayer`의 `_screenRoot`, `_overlayRoot`, `_stageSelectionText` 같은 포인터는 모두 Scene graph가
소유한다. 화면을 제거한 즉시 관련 포인터를 `nullptr`로 만드는 이유가 이것이다.

```cpp
if (_screenRoot)
{
    _screenRoot->removeFromParent();
    _screenRoot = nullptr;
}
_stageSelectionText = nullptr;
```

### 좌표, anchor, content size

- Axmol 2D 기본 좌표는 왼쪽 아래가 원점이다.
- `setPosition()`은 부모 좌표계에서 anchor 위치를 정한다.
- `setAnchorPoint({0.5F, 0.5F})`가 일반적인 중앙 anchor다.
- `setContentSize()`는 Layout, 터치 판정, clipping, 정렬의 기준이다.
- 세로 UI는 절대 화면 픽셀 대신 `getSafeAreaRect()`에서 `left`, `bottom`, `width`, `top`을 계산한다.
- 3D 월드 노드는 `setPosition3D()`와 별도 camera mask를 사용한다.

## 5. Axmol UI API

UI를 사용할 파일은 다음 헤더를 포함한다.

```cpp
#include "axmol/ui/CocosGUI.h"
```

### 핵심 Widget

| 타입 | 핵심 API | 현재 사용 |
|---|---|---|
| `ax::ui::Layout` | `setContentSize`, background color/image, clipping, layout type | 모든 카드·bar·overlay·커스텀 버튼 |
| `ax::ui::Text` | `setString`, font, size, color, 정렬, text area | 화면 텍스트 |
| `ax::ui::Button` | normal/pressed/disabled texture, title, 9-slice | 엔진 제공 표준 버튼이 필요할 때 |
| `ax::ui::ImageView` | texture, auto size, 9-slice, cap inset | 아이콘·프레임·배경 |
| `ax::ui::ScrollView` | 방향, viewport, inner container, bounce, scrollbar, clipping | 유닛·스테이지·랭킹 |
| `ax::ui::ListView` | 항목 기반 목록 | 동일 크기 반복 항목에 사용 가능 |
| `ax::ui::LoadingBar` | percent 기반 bar | 단순 HP/진행도에 사용 가능 |
| `ax::ui::Slider` | 값 선택 | 설정 화면 |
| `ax::ui::CheckBox`, `RadioButton` | 선택 상태 | 옵션·필터 |
| `ax::ui::PageView` | 페이지 스와이프 | 카드/챕터 페이지 |
| `ax::ui::RichText` | mixed text node | 설명·아이템 효과 |
| `ax::ui::EditBox`, `InputField` | 플랫폼 텍스트 입력 | 이름·채팅 |

### 현재 프로젝트식 Panel

다음 함수는 현재 코드의 `makePanel` 패턴과 같은 프로젝트 내 컴파일 가능한 최소 helper다.

```cpp
#include "axmol/ui/CocosGUI.h"

#include <cstdint>

[[nodiscard]] ax::ui::Layout* makeSolidPanel(
    const ax::Size& size,
    const ax::Color32& color,
    std::uint8_t opacity = 255)
{
    auto* panel = ax::ui::Layout::create();
    panel->setContentSize(size);
    panel->setBackGroundColorType(ax::ui::Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(color);
    panel->setBackGroundColorOpacity(opacity);
    return panel;
}
```

### 현재 프로젝트식 Text

```cpp
#include "axmol/ui/CocosGUI.h"

#include <string_view>

[[nodiscard]] ax::ui::Text* makeUiText(
    std::string_view text,
    float fontSize,
    const ax::Color32& color)
{
    constexpr std::string_view FONT_PATH = "fonts/Marker Felt.ttf";
    auto* label = ax::ui::Text::create(text, FONT_PATH, fontSize);
    label->setTextColor(color);
    return label;
}
```

고정 영역에서 줄바꿈·정렬을 제어하려면 auto size를 끈다.

```cpp
auto* label = makeUiText("LONG UNIT NAME", 16.0F, ax::Color32{255, 255, 255, 255});
label->setAutoSize(false);
label->setTextAreaSize({180.0F, 42.0F});
label->setTextHorizontalAlignment(ax::TextHAlignment::CENTER);
label->setTextVerticalAlignment(ax::TextVAlignment::CENTER);
```

### 클릭 입력

현재 프로젝트는 `ui::Button`보다 touch-enabled `ui::Layout`으로 커스텀 버튼을 많이 만든다.

```cpp
auto* button = makeSolidPanel({240.0F, 72.0F}, ax::Color32{61, 171, 79, 255});
button->setTouchEnabled(true);
button->addClickEventListener([this](ax::Object*) { showStageSelect(); });
addChild(button);
```

주의할 점:

- `setTouchEnabled(true)`가 없으면 click callback이 실행되지 않는다.
- 비활성 시 입력만 막으려면 `setEnabled(false)`를 사용한다.
- 비활성 외형도 바꾸려면 `setBright(false)` 또는 직접 색/texture를 바꾼다.
- 전체 화면 overlay의 input shield는 아래 Widget의 입력을 막기 위해 touch를 활성화한다.
- callback에서 `this`를 캡처할 때 listener와 target이 같은 Scene graph 수명을 갖는지 확인한다.

### ScrollView

```cpp
auto* scroll = ax::ui::ScrollView::create();
scroll->setDirection(ax::ui::ScrollView::Direction::VERTICAL);
scroll->setContentSize({640.0F, 520.0F});
scroll->setInnerContainerSize({640.0F, 1200.0F});
scroll->setBounceEnabled(true);
scroll->setScrollBarEnabled(false);
scroll->setClippingType(ax::ui::Layout::ClippingType::SCISSOR);
addChild(scroll);
```

반복 항목은 inner container의 위쪽부터 아래로 배치한다. 현재 유닛 grid, 랭킹, 퀘스트 화면이 이
방식을 사용한다.

### 9-slice

Unity의 `Image.Type = Sliced`, Unreal의 UMG Brush margin에 대응하는 기능이다.

```cpp
auto* frame = ax::ui::ImageView::create("UI/Common/panel_frame.png");
frame->setScale9Enabled(true);
frame->setCapInsets({12.0F, 12.0F, 24.0F, 24.0F});
frame->setContentSize({420.0F, 180.0F});
frame->setPosition({210.0F, 90.0F});
```

cap inset은 원본 이미지에서 늘어나지 않을 모서리·테두리를 제외한 중앙 stretch 영역이다.

### `ax::Label`과 `ax::ui::Text` 선택

- UI 조작, text area, Widget layout, focus가 필요하면 `ax::ui::Text`.
- 단순 world/screen label이고 Widget 기능이 필요 없으면 `ax::Label`.
- 현재 `MainScene::showStatus()`는 `Label`, 로비와 전투 HUD는 `ui::Text`를 사용한다.

## 6. 현재 UI 스타일 API의 실제 상태

### 현재는 공통 공개 Theme API가 없다

Unity의 USS/UI Toolkit stylesheet, Unreal의 Slate Style Set 또는 UMG style asset과 같은 전역
스타일 시스템은 현재 Capsule Monster Chess에 없다. 각 `.cpp`의 anonymous namespace helper가
스타일 역할을 한다.

| 파일 | 파일 내부 helper |
|---|---|
| `LobbyLayer.cpp` | `makePanel`, `makeText`, `makeButton`, `addResourcePill`, `makeBackdrop`, `elementColor`, `cardColor`, `makeUnitArtwork` |
| `LobbyProgression.cpp` | `makePanel`, `makeText`, `makeButton`, `makeIconButton`, `makeProgressBar`, `addScale9Asset`, `makePassRewardCard` |
| `LobbyRanking.cpp` | `makePanel`, `makeText`, `addImageFit`, `rankAccent`, `rankCardColor` |
| `BattleScene.cpp` | `makePanel`, `makeText`, `makeButton`, `addScale9Asset`, `addFittedAsset`, `makeUnitCard` |

이 함수들은 해당 `.cpp` 밖에서 호출할 수 없다. 즉 이름은 같아도 프로젝트 공용 API가 아니며 일부
색상과 button edge 높이도 파일마다 다르다.

수동 코딩 시 규칙:

1. 기존 화면을 수정할 때는 그 파일의 helper와 palette를 그대로 사용한다.
2. 새 화면 하나만 추가할 때는 가장 가까운 화면 파일의 helper 패턴을 복사한다.
3. 여러 화면이 같은 component를 사용하기 시작하면 그때 `Source/Client/UI` 아래 공용 factory/theme
   경계를 만든다.
4. 색상, spacing, font path를 게임 규칙이나 catalog에 넣지 않는다.
5. UI helper에서 사용자 재화·전투 판정을 변경하지 않는다.

### 현재 palette와 배치 관례

- font: `fonts/Marker Felt.ttf`
- 기본 배경: 짙은 navy/teal
- 강조: gold, cyan, green
- 위험/적: red
- 설계 해상도: 720×1280
- 하단 navigation 높이: 94
- 화면 외곽은 `Director::getSafeAreaRect()` 기준
- 화면 root: 기본 z-order
- top bar/navigation: 10~40
- overlay: 1000
- 3D world: `CameraFlag::USER1`
- native UI: default camera

### UI asset fallback

라이선스 이미지가 로컬에 없을 수 있으므로 현재 코드는 다음 순서를 사용한다.

1. `FileUtils::getInstance()->isFileExist(path)`
2. 있으면 `ui::ImageView`
3. 없으면 색상 panel + glyph/monogram

이 fallback을 제거하면 공개 checkout에서 UI가 비게 된다.

### 3D 스타일 API

Dragon fixture의 게임 쪽 공개 style은 다음 구조체다.

```cpp
struct DragonFixtureStyle final
{
    ax::Color diffuseTint;
    ax::Color rimColor;
    float targetExtent;
    float yawDegrees;
};
```

사용 위치:

- `Source/Client/Rendering/DragonFixtureRenderer.h`
- `Source/Client/Rendering/DragonFixtureRenderer.cpp`

```cpp
cmc::client::DragonFixtureStyle style;
style.diffuseTint = ax::Color{0.62F, 0.82F, 1.0F, 1.0F};
style.rimColor    = ax::Color{0.45F, 0.88F, 1.0F, 1.0F};
style.targetExtent = 0.96F;
style.yawDegrees   = 0.0F;

std::string error;
ax::MeshRenderer* dragon = cmc::client::createDragonFixture(style, error);
```

Unity의 Material property override 또는 Unreal의 Dynamic Material Instance parameter에 가까운 역할을
한다. 엔진 전체 stylized material은 `ax::StylizedMaterialDesc`, 씬 품질 설정은
`ax::StylizedRendererConfig`가 담당한다. 상세 내용은 `docs/StylizedRenderer.md`를 따른다.

## 7. Capsule Monster Chess 디렉터리 구조

```text
projects/CapsuleMonsterChess/
├─ Source/
│  ├─ AppDelegate.*                 플랫폼 앱/Director/RenderView 시작
│  ├─ MainScene.*                   로비 3D 배경과 LobbyLayer 조립
│  └─ Client/
│     ├─ Battle/                    전투 Scene, 요청 DTO, 실행 모드
│     ├─ Content/                   JSON catalog loader
│     ├─ Lobby/                     로비 UI, progression, ranking
│     ├─ Rendering/                 Dragon fixture와 stylized material 연결
│     └─ UserData/                  UserProfile, codec, data source
├─ GameCore/
│  ├─ include/cmc/game_core/        순수 C++17 공개 규칙 API
│  └─ src/                          simulator와 hex board 구현
├─ Server/
│  ├─ src/                          HTTP, catalog, battle API, Axmol runtime adapter
│  ├─ AxmolHeadless/                headless EventDispatcher
│  ├─ Tests/                        서버 계약 테스트
│  └─ examples/                     실행 요청 fixture
├─ Backend/                         Firebase 경계 문서/향후 adapter
├─ Content/
│  ├─ Data/Local/                   최초 로컬 사용자 프로필
│  ├─ Data/Tables/                  canonical/runtime JSON
│  ├─ fonts/                        패키징 글꼴
│  └─ Local/                        추적하지 않는 변환 GLB
├─ Tests/GameCore/                  결정성·hex·validation 테스트
├─ Tools/                           Unity content importer
├─ cmake/modules/                   앱 feature와 source/build 설정
└─ docs/                            게임 설계·아키텍처·포팅 기록
```

### 파일을 어디에 추가할지

| 추가 기능 | 위치 |
|---|---|
| 새 로비 화면 | `Source/Client/Lobby` |
| 새 UI 공용 component/theme | `Source/Client/UI` 신규 경계 |
| 전투 HUD/모델 연출 | `Source/Client/Battle` |
| 렌더 material/fixture 조립 | `Source/Client/Rendering` |
| 새 JSON master data loader | `Source/Client/Content` |
| 사용자 저장 schema | `Source/Client/UserData` |
| 승패·피해·스킬·이동 판정 | `GameCore` |
| HTTP schema·server validation | `Server` |
| 플랫폼별 Firebase/network bridge | `Source/Platform` 또는 명시적 client service 경계 |
| 빌드 feature on/off | `cmake/modules/AXGameEngineOptions.cmake` |

`CMakeLists.txt`가 `Source/*.h`와 `Source/*.cpp`를 `GLOB_RECURSE CONFIGURE_DEPENDS`로 수집하므로
`Source` 아래 새 파일은 configure 시 자동으로 target에 포함된다. `GameCore`와 `Server`는 각
`CMakeLists.txt`의 명시적 source 목록을 확인한다.

## 8. 현재 런타임 구조

### 로비에서 전투까지

```text
AppDelegate
  → MainScene::init()
      → 3D camera/light/StylizedRenderer/Dragon
      → LobbyLayer::create()
          → catalog 3종 load
          → UserProfile load/validate
          → showMainLobby()
              → showStageSelect()
                  → launchSelectedStage(Local)
                      → BattlePresentationRequest
                          → frame-boundary postTask
                              → BattleScene::create(request)
                                  → BattleSimulator::simulate()
                                  → BattleLogEvent vector
                                  → update()로 replay tick 진행
                                  → CustomEvent dispatch
                                  → Node Action/HUD 갱신
                                  → 결과 UI
                                  → MainScene으로 replace
```

### LobbyLayer

`LobbyLayer` 하나가 여러 화면을 교체한다. Unity에서 Canvas 아래 panel을 켜고 끄는 controller,
Unreal에서 하나의 root `UUserWidget`이 여러 panel state를 관리하는 구조에 가깝다.

현재 `ScreenId`:

- `MainLobby`
- `Monsters`
- `Stages`
- `Shop`
- `Placeholder`
- `Ranking`

현재 overlay:

- sub menu
- quest

별도 전체 화면 흐름:

- battle pass
- gacha reveal/result
- unit detail

화면 전환은 `_screenRoot`를 제거하고 새 `Node::create()` root를 붙이는 `replaceScreen()`이 담당한다.
가챠 timer도 이 지점에서 취소한다.

### Catalog

| Catalog | 파일 | 현재 데이터 |
|---|---|---|
| `MonsterCatalog` | `monster_unit_table.json` | 168 유닛, 숫자 `unitId` 안정 키 |
| `StageCatalog` | `story_stage_table.json` | 24 개발용 stage, `authoritative: false` |
| `ProgressionCatalog` | `lobby_progression_table.json` | 9 quest, 8 battle pass tier 개발 seed |

로더의 공통 계약:

```cpp
bool load(std::string_view resourcePath, std::string& error);
```

실패를 예외로 던지지 않고 `false + error`로 반환한다. UI는 error를 표시하고 앱 전체를 종료하지 않는다.

### UserData

`UserProfile`은 다음 데이터를 가진다.

- gold, gems, energy
- 보유 유닛 level/shards/equipment
- 5칸 덱 preset 최대 5개
- 선택된 preset
- quest progress
- battle pass progress

`IUserDataSource`:

```cpp
class IUserDataSource
{
public:
    virtual ~IUserDataSource() = default;
    virtual bool load(UserProfile& profile, std::string& error) const = 0;
    virtual bool save(const UserProfile& profile, std::string& error) = 0;
    [[nodiscard]] virtual bool isWritable() const noexcept = 0;
};
```

구현:

- `LocalJsonUserDataSource`: writable path에 저장, WASM은 IDBFS flush
- `WebPayloadUserDataSource`: 서버 payload read-only

Unity의 save repository/service, Unreal의 GameInstance subsystem/save service에 해당하는 경계다.

### GameCore

핵심 공개 API:

```cpp
cmc::game_core::BattleSimulateResult
cmc::game_core::BattleSimulator::simulate(
    const cmc::game_core::BattleSimulateRequest& request) const;
```

보조 API:

- `validateBattleRequest(request)`
- `computeBattleChecksum(result)`
- `createSmokeBattleRequest()`
- `HexBoard`
- `DeterministicRandom`

판정 규칙:

- 10×5 기본 hex board
- 50 ms fixed tick
- 정수 전투 수치
- 명시적 32-bit seed
- stable iteration/order
- 최대 64 unit
- 최대 90초/1800 tick
- 입력 validation 후 simulate

GameCore에서 금지:

- `axmol/` include
- UI/Renderer/FileUtils
- wall clock
- frame `deltaSeconds`
- 전역 가변 상태
- 순회 순서가 불안정한 container에 의존한 판정

### BattleScene

`BattleScene`은 `BattleLogEvent`를 `cmc.battle.replay` custom event로 보내고 같은 Scene에 등록한
scene-graph priority listener가 표현을 수행한다.

| 이벤트 | 표현 |
|---|---|
| `UnitSpawn` | 위치·HP·visible 초기화 |
| `UnitMove` | `MoveTo` |
| `UnitJump` | 2단 `MoveTo` |
| `BasicAttack`, `SkillCast` | 짧은 lunge |
| `Damage`, `Heal` | HUD HP 갱신 |
| `UnitDeath` | 축소 후 hide |
| `BattleEnd` | 결과 panel |

`scheduleUpdate()`는 리플레이 시계에만 사용한다. 판정 tick을 `update(dt)`에서 계산하지 않는다.
현재 `REPLAY_SECONDS_PER_TICK`은 0.030초라서 50 ms 판정 tick을 화면에서는 30 ms 간격으로 재생한다.
이 값은 재생 속도일 뿐 승패, checksum, 최종 상태를 바꾸지 않는다.

## 9. 현재 구현 상태: 완료와 fixture 경계

이 표는 수동 코딩 시 잘못된 가정을 막기 위한 현재 코드 기준이다.

| 영역 | 현재 상태 |
|---|---|
| 앱 부팅, 세로 safe area | 구현됨 |
| main lobby, deck/unit, stage, shop, ranking UI | 구현됨 |
| quest/pass UI | 개발 seed 기반 구현 |
| 로컬 JSON profile | 구현됨 |
| Web profile payload | read-only decoder 구현, 실제 Firebase bridge 미연결 |
| Monster catalog | 168 유닛 JSON loader 구현 |
| Stage catalog | 24개 비권위 bootstrap |
| deck 편성 | 로컬 profile에 5칸 preset 저장 |
| GameCore 기본 전투 | hex 이동·기본 공격·피해·사망·승패·checksum 구현 |
| 전투 화면 | Dragon 10대10 local fixture 재생 구현 |
| 선택 덱의 실제 전투 반영 | 미구현. 현재 `playerUnitIds`는 하단 카드 표시에만 사용 |
| 실제 유닛별 3D 모델 | 미구현. 현재 양 팀 모두 Dragon fixture |
| 원격 PvP client adapter | 미구현 |
| `AuthoritativeRemote` | enum/request 필드는 있으나 현재 client network 실행 경로 없음 |
| headless battle server | HTTP simulate/health와 validation 구현 |
| Firebase Auth/Function/Firestore 연결 | 미구현 |
| 승리 골드 반영 | 결과 preview만 표시, profile에는 미반영 |
| stage energy 차감/클리어 저장 | 미구현. 현재 cleared count는 UI 개발 값 |
| ranking | 정적 preview, live MMR 미연결 |
| gacha | 로컬 테스트 profile 변경 경로 |
| quest reward | 로컬 writable source용 개발 경로, 상용 서버 명령 미연결 |
| 스킬·장비·진화 전체 판정 | type과 일부 실행 경로는 존재하나 상용 규칙은 미완성 |

특히 `BattleExecutionMode::AuthoritativeRemote` 값을 화면에 넘기는 것만으로 서버 전투가 되지 않는다.
현재 `BattleScene::initWithRequest()`는 mode와 무관하게 로컬 `BattleSimulator`를 즉시 실행한다.

## 10. 게임 로직을 수동으로 추가하는 규칙

### 판정과 표현 분리

다음 질문으로 위치를 결정한다.

> 서버와 클라이언트가 같은 입력에서 같은 결과를 내야 하는가?

- 예: `GameCore`
- 아니고 화면에서만 필요한가: `Source/Client`

예:

| 기능 | 올바른 위치 |
|---|---|
| 공격 대상 선택 | `GameCore` |
| 피해량·방어력 | `GameCore` |
| stun 지속 tick | `GameCore` |
| 유닛이 점프해 보이는 곡선 | `BattleScene` |
| 피격 flash | `BattleScene` |
| HP bar tween | `BattleScene` |
| 서버 request JSON 검증 | `Server` |

### 새 전투 규칙 추가 순서

1. `BattleTypes.h`에 직렬화 가능한 입력/출력/enum을 추가한다.
2. `validateBattleRequest()`에 경계값 검증을 추가한다.
3. `BattleSimulator.cpp`에 정수/fixed tick 규칙을 구현한다.
4. `BattleLogEvent`에 표현에 필요한 결과만 기록한다.
5. `Tests/GameCore/GameCoreTests.cpp`에 결정성·경계·golden test를 추가한다.
6. `BattleScene::replayEvent()`에 표현을 추가한다.
7. server request/response schema가 바뀌면 `BattleApi`와 server test를 함께 갱신한다.

게임 규칙을 `BattleScene::update()` 또는 Action callback 안에 구현하면 안 된다. 렌더 FPS와 skip 여부에
따라 승패가 달라진다.

### 새 리플레이 이벤트 추가

```text
BattleEventType
  → BattleSimulator가 event 기록
  → checksum 직렬화에 포함
  → server response serialization
  → BattleScene::replayEvent switch
  → fast-forward에서도 같은 최종 상태인지 test
```

`instant == true`는 skip용이다. 새 이벤트는 일반 재생과 instant 재생 모두 같은 최종 presentation
상태에 도달해야 한다.

### 비동기 작업과 main thread

파일/network/worker thread에서 Node와 Widget을 직접 수정하지 않는다.

```cpp
ax::Director::getInstance()->postTask(
    [weakStateOrValueCopy] {
        // Axmol main thread에서 Scene/Widget 갱신
    },
    ax::Director::TaskTiming::NextUpdate);
```

Scene 교체나 scene graph 구조 변경은 `FrameBoundary`를 사용한다. callback이 실행될 때 target Scene이
이미 사라질 수 있으면 raw `this`를 캡처하지 말고 값 DTO 또는 명시적 수명 토큰을 사용한다.

## 11. 새 로비 화면을 추가하는 체크리스트

예를 들어 Inventory 화면을 추가한다.

1. `LobbyLayer.h`의 `ScreenId`에 `Inventory`를 추가한다.
2. `void showInventory();`를 private method로 선언한다.
3. `LobbyLayer.cpp` 또는 별도 `LobbyInventory.cpp`에 구현한다.
4. 시작할 때 world visibility를 결정한다.
5. `Node& root = replaceScreen();`으로 이전 화면과 timer를 정리한다.
6. `_safeArea`에서 `left`, `bottom`, `width`, `top`을 계산한다.
7. background → top bar → content → navigation 순서로 child를 추가한다.
8. 버튼은 `setTouchEnabled(true)` 후 callback을 연결한다.
9. 임시 child raw pointer를 멤버에 저장했다면 `replaceScreen()`에서 `nullptr`로 초기화한다.
10. `buildBottomNavigation()`에 선택 상태와 route를 연결한다.

`LobbyLayer.cpp` 안에 추가하는 구조 예:

```cpp
void cmc::client::LobbyLayer::showInventory()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    ax::Node& root = replaceScreen();
    const float left = _safeArea.origin.x;
    const float top  = _safeArea.origin.y + _safeArea.size.height;

    auto* panel = makePanel(
        {_safeArea.size.width - 28.0F, 720.0F},
        ax::Color32{22, 31, 58, 255},
        248);
    panel->setPosition({left + 14.0F, top - 820.0F});
    root.addChild(panel, 10);
}
```

별도 `.cpp`로 나누면 그 파일에서는 다른 `.cpp`의 anonymous helper를 호출할 수 없다. 공용 helper가
필요해지는 시점에는 `Source/Client/UI`에 header/source로 승격한다.

## 12. 데이터 schema를 수정하는 체크리스트

### UserProfile 필드 추가

1. `UserProfile.h` DTO에 필드를 추가한다.
2. `USER_PROFILE_SCHEMA_VERSION`을 올린다.
3. `UserProfileJsonCodec::decode()`에 이전 schema migration을 추가한다.
4. `encode()`에 새 필드를 기록한다.
5. `validate()`에 범위·중복·상호 조건을 추가한다.
6. `default_user_profile.json`을 갱신한다.
7. `LocalJsonUserDataSource`와 `WebPayloadUserDataSource` 양쪽을 테스트한다.
8. server authoritative 필드라면 client local save에서 변경하지 못하게 한다.

### Catalog 필드 추가

1. JSON schema/version과 canonical hash 규칙을 먼저 확정한다.
2. catalog entry DTO를 갱신한다.
3. loader가 타입, 범위, 필수값, 중복 ID를 거부하게 한다.
4. importer와 sample JSON을 함께 갱신한다.
5. client와 server가 동일 `contentHash`를 쓰는지 검증한다.

`unitId`, `stageId`, `questId`는 화면 표시 문자열이 아니라 영속 안정 키다. 이름이나 vector index를
저장 키로 바꾸지 않는다.

## 13. Action, Scheduler, EventDispatcher 사용 기준

### Action

짧은 시각 연출에만 사용한다.

```cpp
node->runAction(ax::Sequence::create(
    ax::EaseSineOut::create(ax::ScaleTo::create(0.10F, 1.08F)),
    ax::EaseSineIn::create(ax::ScaleTo::create(0.12F, 1.00F)),
    nullptr));
```

판정, 재화 변경, save transaction을 Action 완료 callback에 의존시키지 않는다.

### Scheduler

- 매 프레임: `scheduleUpdate()` / `unscheduleUpdate()`
- 1회 지연: `scheduleOnce(callback, delay, key)`
- 취소: `unschedule(key)`
- 화면 교체 시 살아남으면 안 되는 timer는 고유 key로 등록한다.

현재 예:

- `cmc_gacha_reveal`
- `cmc_battle_core_result`

### EventDispatcher

Scene 생명주기와 함께 사라질 listener는 scene-graph priority를 사용한다.

```cpp
auto* listener = ax::CustomEventListener::create(
    "cmc.inventory.changed",
    [this](ax::CustomEvent* event) {
        const auto* payload = static_cast<const InventoryChanged*>(event->getUserData());
        if (payload)
            refreshInventory(*payload);
    });

_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
```

`dispatchCustomEvent()`의 user data는 `void*`이며 dispatcher가 소유하지 않는다. 현재 전투처럼 동기
dispatch 중에만 유효한 stack payload를 넘길 수 있지만, listener가 포인터를 저장하면 즉시 dangling
pointer가 된다. 비동기로 보관할 데이터는 값 복사 또는 명시적 소유 DTO를 사용한다.

## 14. 자산과 FileUtils

게임 코드에서 파일은 저장소 절대 경로가 아니라 패키징 resource 경로로 연다.

```cpp
constexpr std::string_view TABLE_PATH = "Data/Tables/monster_unit_table.json";
const std::string json =
    ax::FileUtils::getInstance()->getStringFromFile(TABLE_PATH);
```

경로 기준:

| 데이터 | 저장 위치 | Git |
|---|---|---|
| 공개 JSON table | `Content/Data/Tables` | 추적 |
| default profile | `Content/Data/Local` | 추적 |
| font | `Content/fonts` | 추적 |
| Unity 파생 icon | `Content/UI/MonsterIcons` | 로컬, ignore |
| 변환 GLB | `Content/Local` | 로컬, ignore |
| 사용자 save | `FileUtils::getWritablePath()` | 런타임 |

로컬 GLB나 icon이 없을 때 fallback UI가 동작하는지 항상 확인한다.

## 15. 빌드와 테스트

### macOS 클라이언트

프로젝트 루트에서:

```sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
AX_ROOT=/Users/ethanjung/Desktop/Dev/Cpp/axmol_my \
  ../../tools/cmdline/axmol build -p osx -a arm64 -f \
  -xc '-DAX_RENDER_API=gl'
```

위 명령의 현재 작업 디렉터리는 `projects/CapsuleMonsterChess`다.

### 기존 native build에서 GameCore test

저장소 루트에서:

```sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
  cmake --build projects/CapsuleMonsterChess/build \
  --config RelWithDebInfo \
  --target cmc_game_core_tests

DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
  ctest --test-dir projects/CapsuleMonsterChess/build \
  -C RelWithDebInfo \
  --output-on-failure
```

### Headless server

```sh
cmake -S projects/CapsuleMonsterChess/Server \
  -B projects/CapsuleMonsterChess/build_server \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build projects/CapsuleMonsterChess/build_server -j 8
ctest --test-dir projects/CapsuleMonsterChess/build_server --output-on-failure
```

공유 GameCore는 C++17이다. 현재 headless server adapter는 로컬 Axmol header 요구 때문에 C++23으로
컴파일한다.

### WebAssembly

`projects/CapsuleMonsterChess`에서:

```sh
source ../../tools/external/emsdk/emsdk_env.sh

AX_ROOT=/Users/ethanjung/Desktop/Dev/Cpp/axmol_my \
  ../../tools/cmdline/axmol build -p wasm -f \
  -xb '--config','Release'
```

pthread build이므로 COOP/COEP header를 제공하는 서버로 실행한다.

```sh
cd build_wasm/bin/CapsuleMonsterChess
emrun --no_browser --port 8765 CapsuleMonsterChess.html
```

### 최소 제출 전 검사

```sh
git diff --check
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
  ctest --test-dir projects/CapsuleMonsterChess/build -C RelWithDebInfo --output-on-failure
ctest --test-dir projects/CapsuleMonsterChess/build_server --output-on-failure
```

UI, asset, shader, Scene 변경은 단위 테스트만으로 끝내지 않고 macOS 실행 또는 WebGL smoke까지 확인한다.

## 16. 문제 발생 시 확인 순서

### UI가 보이지 않는다

1. child가 실제 parent에 `addChild()`됐는지 확인한다.
2. `contentSize`, `anchorPoint`, `position`을 확인한다.
3. safe area 좌표를 두 번 더하거나 빼지 않았는지 확인한다.
4. z-order와 overlay input shield를 확인한다.
5. `FileUtils::isFileExist()`와 fallback을 확인한다.
6. world node라면 camera mask, UI라면 default camera를 확인한다.
7. ScrollView라면 viewport, inner size, clipping을 확인한다.

### 클릭이 안 된다

1. `setTouchEnabled(true)`
2. `setEnabled(true)`
3. 상위 overlay/shield가 가로채는지 확인
4. Widget content size가 0인지 확인
5. ScrollView clipping 밖인지 확인
6. callback target이 이미 화면 교체로 제거됐는지 확인

### 화면 교체 뒤 crash

1. 제거된 child raw pointer를 `nullptr`로 만들었는지 확인한다.
2. `scheduleOnce` key를 취소했는지 확인한다.
3. background task가 raw `this`를 캡처했는지 확인한다.
4. custom event payload pointer를 listener가 저장했는지 확인한다.
5. Action callback이 제거된 Node를 참조하는지 확인한다.

### 전투 checksum이 달라진다

1. `unordered_map` 순회가 판정 순서를 결정하는지 확인한다.
2. float, wall clock, frame `dt`를 GameCore에 넣었는지 확인한다.
3. RNG 호출 횟수와 순서가 바뀌었는지 확인한다.
4. 동일 tick command의 tie-break가 안정적인지 확인한다.
5. checksum serialization field 순서와 endian을 확인한다.
6. client/server table version과 content hash를 확인한다.

### JSON load가 실패한다

1. resource 상대 경로를 확인한다.
2. packaged `Content`에 파일이 포함됐는지 확인한다.
3. schema version을 확인한다.
4. ID 중복, 음수, 범위 초과를 확인한다.
5. error 문자열을 화면과 log 양쪽에서 확인한다.

## 17. 수정 금지 경계와 유지보수 원칙

- UI 편의를 위해 GameCore에 Axmol 타입을 넣지 않는다.
- frame rate에 의존하는 전투 판정을 만들지 않는다.
- 원격 PvP 결과를 client local save로 확정하지 않는다.
- `AuthoritativeRemote` 이름만 연결하고 실제 local simulate를 원격 결과처럼 취급하지 않는다.
- stage bootstrap을 상용 authoritative data로 오인하지 않는다.
- 라이선스 Unity icon/GLB를 공개 Git에 추가하지 않는다.
- 새 기능 때문에 전체 Axmol optional extension을 한꺼번에 켜지 않는다.
- 사용 중인 기능을 제거하거나 우회해 테스트를 통과시키지 않는다.
- 문제가 반복되면 마지막 정상 시점으로 돌아가 입력→규칙→이벤트→표현 순서로 한 단계씩 확인한다.

## 18. 빠른 파일 색인

| 알고 싶은 것 | 먼저 볼 파일 |
|---|---|
| 앱이 어디서 시작하는가 | `Source/AppDelegate.cpp` |
| 첫 화면 조립 | `Source/MainScene.cpp` |
| 로비 화면/스타일 | `Source/Client/Lobby/LobbyLayer.cpp` |
| quest/pass/overlay | `Source/Client/Lobby/LobbyProgression.cpp` |
| ranking | `Source/Client/Lobby/LobbyRanking.cpp` |
| 전투 요청과 scene 전환 | `LobbyLayer::launchSelectedStage`, `MainScene.cpp` |
| 전투 화면/리플레이 | `Source/Client/Battle/BattleScene.cpp` |
| 전투 DTO와 enum | `GameCore/include/cmc/game_core/BattleTypes.h` |
| 육각 보드 | `GameCore/include/cmc/game_core/HexBoard.h` |
| 전투 판정 | `GameCore/src/BattleSimulator.cpp` |
| 결정적 RNG | `GameCore/include/cmc/game_core/DeterministicRandom.h` |
| 사용자 저장 | `Source/Client/UserData` |
| master data loader | `Source/Client/Content` |
| Dragon style | `Source/Client/Rendering/DragonFixtureRenderer.*` |
| stylized renderer 전체 | `docs/StylizedRenderer.md` |
| 서버 HTTP 계약 | `Server/README.md`, `Server/src/BattleApi.*` |
| 게임 목표 | `projects/CapsuleMonsterChess/docs/GameDesign.md` |
| 전체 시스템 경계 | `projects/CapsuleMonsterChess/docs/Architecture.md` |
| Unity 원본 포팅 상태 | `projects/CapsuleMonsterChess/docs/UnityPortMap.md` |
| Axmol module on/off | `projects/CapsuleMonsterChess/docs/AxmolModuleProfile.md` |

## 19. 공식 레퍼런스

Axmol:

- [Axmol Director](https://axmol.dev/manual/latest/d4/d72/classax_1_1_director.html)
- [Axmol Node](https://axmol.dev/manual/latest/df/da2/classax_1_1_node.html)
- [Axmol Widget](https://axmol.dev/manual/latest/dc/db1/classax_1_1ui_1_1_widget.html)
- [Axmol LayoutGroup (`Layout` alias)](https://axmol.dev/manual/latest/de/dff/classax_1_1ui_1_1_layout_group)
- [Axmol Button](https://axmol.dev/manual/latest/d8/d5c/classax_1_1ui_1_1_button.html)
- [Axmol Scheduler](https://axmol.dev/manual/latest/db/dfa/classax_1_1_scheduler.html)
- [Axmol EventDispatcher](https://axmol.dev/manual/latest/de/d23/classax_1_1_event_dispatcher.html)

Unity:

- [Unity 6 GameObject](https://docs.unity3d.com/6000.0/Documentation/Manual/class-GameObject.html)
- [Unity 6 MonoBehaviour](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MonoBehaviour.html)
- [Unity 6 SceneManager.LoadScene](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.LoadScene.html)

Unreal Engine:

- [Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine)
- [Game Objects: Unity와 Unreal 비교](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-objects-in-unreal-engine)
- [UWorld](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld)
- [UGameInstance](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameInstance)
- [UUserWidget](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UUserWidget)
- [Actor Ticking](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine)
