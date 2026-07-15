# Capsule Monster Chess Unity UI → Axmol 구현 가이드

이 문서는 Unity 프로젝트의 실제 UI 프리팹, 씬, C# UI 프레임워크를 기준으로 같은 기능을
Axmol/C++17에서 수동 구현하기 위한 비교식 인수인계다. 일반적인 엔진 소개가 아니라,
`CapsuleMonsterChess`에 지금 존재하는 구조와 현재 Axmol 클라이언트의 구조를 직접 대조한다.

## 1. 조사 기준과 결론

- 조사일: 2026-07-15
- Unity 원본: `/Users/ethanjung/Desktop/Dev/Unity/CapsuleMonsterChess/CMS_Unity`
- Unity 버전: `2021.3.45f2`
- Axmol 대상: `projects/CapsuleMonsterChess`
- Axmol UI 기준 해상도: 720×1280
- Axmol 해상도 정책: `ResolutionPolicy::SHOW_ALL`
- 구현 언어: C++17

핵심 결론은 다음과 같다.

1. Unity의 `RectTransform.anchorMin/anchorMax`와 Axmol의 `Node::setAnchorPoint()`는 같은 기능이
   아니다. Axmol의 anchor point는 Unity의 **Pivot**에 대응한다.
2. Unity Anchor/Stretch는 Axmol `ui::LayoutComponent` 또는 이 문서의 RectTransform 계산식으로
   구현한다.
3. Unity `HorizontalLayoutGroup`은 Axmol `ui::HBox`/`LayoutGroup::Type::HORIZONTAL`로 옮길 수 있지만,
   `GridLayoutGroup`의 완전한 대응 클래스는 없다. 프로젝트용 `UiGridLayout`이 필요하다.
4. Unity `UI_Manager + UI_Base + UI_Popup`은 Axmol에서 `UiManager + UiScreen + UiPopup + UiWidget`
   수명주기로 재구성한다. Unity의 enum 이름 기반 reflection 바인딩은 C++에서 명시적 멤버
   바인딩으로 바꾼다.
5. Unity `Button.onClick`/`UnityEvent`와 Axmol Widget callback은 수명 규칙이 다르다. Widget callback은
   노드에 저장되지만 전역 `EventDispatcher` listener는 별도 해제가 필요하다.
6. Unity의 DOTween은 Axmol `Sequence`, `Spawn`, `Ease*`, `CallFunc`로 대부분 옮길 수 있다.
   Animator Controller와 동일한 편집기 상태 머신은 없으므로 이름 기반 `UiAnimator`를 프로젝트
   계층에 둔다.
7. 현재 Unity 프로젝트에는 Vectrosity 5.6.1과 I2 Localization이 설치되어 있지만, 게임 UI에서
   실제로 연결된 사용처는 확인되지 않았다. `WidgetLib.AddLine()`은 주석 처리되어 `null`을 반환하고,
   `I2Languages.asset`은 언어/용어 목록이 비어 있다.
8. 현재 Axmol 가챠는 이미 연출과 보상 반영을 분리한다. 이 트랜잭션 경계를 유지한 채 파티클,
   광선, additive glow를 추가해야 한다.

## 2. 실제 Unity UI 인벤토리

### 2.1 Resources UI 프리팹

실제 경로는 사용자가 말한 `Prefab/UI`가 아니라 복수형 `Prefabs/UI`다.

```text
Assets/Resources/Prefabs/UI/
├─ EventSystem.prefab
├─ Scene/
│  └─ WidgetMain_Stage.prefab
└─ Subitem/
   ├─ WidgetChestFarm.prefab
   ├─ WidgetCurrencyBar.prefab
   └─ WidgetGauge.prefab
```

`Popup.meta`, `DDO.meta`, `WorldSpave.meta`는 남아 있지만 현재 조사 시점에는 해당 폴더 아래
런타임 프리팹이 없다. 특히 `UI_Manager.ShowPopupUI<T>()`는
`Prefabs/UI/Popup/{ClassName}`을 로드하도록 작성되어 있으나 실제 Popup 프리팹은 없다.

주의할 점이 하나 더 있다.

- 실제 폴더: `Subitem`
- `UI_Manager.MakeSubItem<T>()`의 문자열: `UI/SubItem/{name}`

macOS의 기본 대소문자 비구분 파일 시스템에서는 우연히 동작할 수 있지만 Linux, Android 패키징,
대소문자 구분 CI에서는 실패할 수 있다. Axmol 포팅 시에는 리소스 키를 전부 소문자
`ui/subitem/...` 규칙으로 정규화한다.

### 2.2 씬

```text
Assets/Scenes/
├─ BattleScene.unity
├─ CamScene.unity
├─ MainScene.unity
└─ TitleScene.unity
```

`MainScene.unity`에는 프리팹으로 빠지지 않은 prototype UI가 많이 직접 들어 있다.
Button, Toggle, Horizontal/Vertical Layout Group, Content Size Fitter가 섞여 있으므로
`Resources/Prefabs/UI`만 보면 전체 화면 구조를 놓친다. 포팅 기준은 다음 두 축이다.

- 재사용 단위: `WidgetMain_Stage`, `WidgetChestFarm`, `WidgetCurrencyBar`, `WidgetGauge`
- 화면 조합/프로토타입: `MainScene.unity`의 직접 배치 UI

대표 프리팹의 root 계약:

| Prefab | Root RectTransform | 핵심 child/기능 |
|---|---|---|
| `WidgetMain_Stage` | Canvas, 720×1280 Expand | stage buttons, chest/play horizontal rows |
| `WidgetChestFarm` | 150×200 | 125×125 icon, full-stretch timer, top-stretch bar |
| `WidgetCurrencyBar` | 200×45, top-left pivot | value text, currency icon, add button |
| `WidgetGauge` | horizontal stretch, height 21 | full-stretch `m_ImgGaugeBar`, sliced fill |

`WidgetMain_Stage` prefab 자체 Canvas sorting order는 2다. `MainScene`에는 sorting order 0, 15, 100인
Canvas가 함께 존재한다. Axmol에서는 prefab별 임의 숫자를 그대로 복사하지 않고 Screen/Popup/Toast/
Transition layer 상수와 view 내부 local z로 분리한다.

### 2.3 설치 패키지와 실제 사용 상태

| 기능 | Unity 프로젝트 상태 | Axmol 포팅 판단 |
|---|---|---|
| uGUI | 씬/프리팹에서 실제 사용 | `ax::ui`로 포팅 |
| TextMeshPro 3.0.6 | 텍스트와 outline 사용 | `ui::Text`/`Label` + TTF/SDF |
| DOTween | fade/loading/notification에 실제 사용 | Axmol Action + 프로젝트 `UiAnimator` |
| Addressables 1.19.19 | `ResourceManager`에 비동기 경로 존재 | 현재 CMC의 catalog/파일 로더 경계에 통합 |
| I2 Localization | 패키지와 언어 선택 코드 존재, 데이터는 비어 있음 | `LocalizationService`를 새 canonical 경계로 사용 |
| Unity UI Extensions | `UILineRenderer` 코드 흔적만 존재 | 단순 선은 `DrawNode`, 고급 선은 `UiPolylineNode` |
| Vectrosity 5.6.1 | 플러그인 설치, 게임 코드 사용처 없음 | 기능 요구가 생길 때 동적 triangle mesh로 구현 |
| Timeline | 패키지 설치 | UI 팝업 연출에는 Axmol Action 우선 |

“설치되어 있음”과 “게임에서 사용 중”을 동일하게 취급하면 포팅 범위가 불필요하게 커진다.
기능은 없애지 않되, 실제 연결 여부를 구분해서 같은 기능을 구현한다.

## 3. 현재 Unity C# UI 프레임워크

### 3.1 생성과 소유 흐름

```text
Manager
└─ UI_Manager
   ├─ @UI_Root
   ├─ ShowSceneUI<T>()
   │  └─ Manager.Resource → Resources.Load("Prefabs/UI/Scene/{name}")
   ├─ ShowPopupUI<T>()
   │  └─ Manager.Resource → Resources.Load("Prefabs/UI/Popup/{name}")
   └─ MakeSubItem<T>()
      └─ Manager.Resource → Resources.Load("Prefabs/UI/SubItem/{name}")

UI_Base
├─ UI_Scene
├─ UI_Popup
│  └─ UI_MovingPopup
└─ 각 Widget controller
```

관련 소스:

- `Assets/Scripts/Managers/UI_Manager.cs`
- `Assets/Scripts/UIModule/UI_Base.cs`
- `Assets/Scripts/UIModule/UI_Scene.cs`
- `Assets/Scripts/UIModule/UI_Popup.cs`
- `Assets/Scripts/UIModule/UI_MovingPopup.cs`
- `Assets/Scripts/UIModule/UI_EventHandler.cs`
- `Assets/Scripts/Managers/ResourceManager.cs`

`UI_Manager`는 다음 책임을 한 클래스에 가진다.

- `@UI_Root` 생성/검색
- Scene UI/Popup/Subitem 프리팹 생성
- Canvas sorting order 할당
- 열린 popup 추적
- popup close와 Resource destroy

실제 수명 계약과 포팅 시 보완점:

| Unity 현재 동작 | 확인된 계약 | Axmol 목표 |
|---|---|---|
| overlay/world order가 각각 10에서 시작 | `SetCanvas()` 때 증가 | layer base z + popup stack index |
| 열린 popup을 `HashSet<UI_Popup>`으로 보관 | 열림 순서는 보장하지 않음 | ordered stack으로 top popup 명시 |
| `ShowSceneUI/ShowPopupUI` | 생성 후 `Init()`을 직접 호출하지 않음 | factory → `open()` 한 경로로 강제 |
| `UI_MovingPopup.Awake()` | 여기서는 `Init()` 호출 | base/derived 호출 차이를 제거 |
| `ClosePopupUI()` | hook → order 감소 → Resource destroy | `onClosing` → unbind/cancel → detach |
| `ResourceManager.Destroy()` | Addressables instance release 시도 후 `Destroy` | handle 종류와 Node 소유를 분리 |

Unity 코드를 수정하라는 의미가 아니다. Axmol 포팅에서 암묵적 `Awake()` 호출 여부에 의존하지 않고
모든 view가 동일한 명시적 수명 경로를 통과하도록 만드는 것이다.

Axmol에서는 이 책임을 그대로 거대한 singleton에 복사하지 않는다. 생성/스택은 `UiManager`,
각 화면 수명은 `UiScreen/UiPopup`, 자산 생성은 `WidgetFactory`로 분리한다.

### 3.2 UI_Base 바인딩

Unity `UI_Base.Bind<T>(enum)`은 enum 항목 이름과 자식 GameObject 이름을 맞춰 재귀 검색한다.

```csharp
enum Buttons
{
    m_BtnSettings,
    m_BtnBattle
}

Bind<Button>(typeof(Buttons));
GetButton((int)Buttons.m_BtnSettings);
```

장점은 Inspector reference를 덜 연결해도 된다는 점이다. 단점은 rename이 컴파일 오류가 아니라
런타임 `null`로 나타난다는 점이다.

Axmol 수동 C++에서는 명시적 멤버 바인딩을 기본으로 한다.

```cpp
class StageScreen final : public UiScreen
{
public:
    static StageScreen* create();

private:
    bool init() override;
    void bindEvents() override;
    void unbindEvents() override;

    ax::ui::Button* settingsButton_ = nullptr; // 부모가 소유하는 non-owning pointer
    ax::ui::Button* battleButton_   = nullptr;
};
```

문자열 이름 검색은 개발 검증용으로만 사용한다.

```cpp
auto* button = dynamic_cast<ax::ui::Button*>(root->getChildByName("m_BtnSettings"));
AXASSERT(button != nullptr, "m_BtnSettings binding failed");
```

릴리스 핵심 경로는 화면 생성 함수에서 반환 포인터를 바로 멤버에 저장한다. 이 방식은 rename과 타입
오류가 작성 지점 가까이에서 드러난다.

### 3.3 이벤트 중계

`UI_EventHandler`는 Unity EventSystem 인터페이스를 한 component에 모은다.

- click
- pointer down/enter/exit
- begin drag/drag/end drag
- drop

Axmol에서는 `ui::Widget::addClickEventListener`, `addTouchEventListener`,
`addPointerEventListener`, `addHoverEventListener`를 사용한다. Drag/drop 도메인 의미는 touch
상태를 해석하는 프로젝트 controller에 둔다.

## 4. 목표 Axmol UI 구조

현재 `LobbyLayer`는 화면 생성 helper와 화면별 UI, popup, 가챠까지 한 계층에 모여 있다. 동작하는
코드를 폐기하지 않고 다음 경계로 점진 분리한다.

```text
MainScene
└─ UiRoot
   ├─ ScreenLayer          z = 0
   │  └─ StageScreen
   ├─ PopupLayer           z = 1000
   │  └─ SettingsPopup
   ├─ ToastLayer           z = 2000
   └─ TransitionLayer      z = 3000

UiManager
├─ openScreen<T>()
├─ pushPopup<T>()
├─ closeTopPopup()
└─ closeAll()

WidgetFactory
├─ createCurrencyBar()
├─ createChestFarm()
├─ createGauge()
└─ createUnitCard()
```

Unity/Unreal 대응은 다음과 같다.

| 프로젝트 계층 | Unity | Unreal UMG | Axmol 구현 |
|---|---|---|---|
| `UiRoot` | `@UI_Root` + root Canvas | Viewport root | Safe area 기준 `Node` |
| `UiScreen` | `UI_Scene` | full-screen `UUserWidget` | 화면 수명 `Node` |
| `UiPopup` | `UI_Popup` | modal `UUserWidget` | shield + content root |
| `UiWidget` | `UI_Base` subitem | reusable UserWidget | 재사용 `Node`/`LayoutGroup` |
| `WidgetFactory` | Prefab instantiate | Widget Blueprint create | C++ 생성 함수 |
| `UiManager` | `UI_Manager` | HUD/UI subsystem | layer와 popup stack 관리자 |

권장 수명주기:

```cpp
class UiView : public ax::Node
{
public:
    void open()
    {
        if (opened_)
            return;
        opened_ = true;
        bindEvents();
        onOpened();
    }

    void close()
    {
        if (!opened_)
            return;
        onClosing();
        unbindEvents();
        stopAllActions();
        unscheduleAllCallbacks();
        opened_ = false;
        removeFromParent();
    }

protected:
    virtual void bindEvents() {}
    virtual void unbindEvents() {}
    virtual void onOpened() {}
    virtual void onClosing() {}

private:
    bool opened_ = false;
};
```

`removeFromParent()`만 호출해도 Node 관련 Action/Scheduler는 cleanup되지만, 전역 dispatcher,
도메인 event bus, 네트워크 callback은 Node 트리 밖의 소유이므로 `unbindEvents()`에서 해제한다.

## 5. 한눈에 보는 compare-by-compare

| Unity | Axmol | Unreal | 실무 규칙 |
|---|---|---|---|
| Canvas | `Scene/Layer/UiRoot` | Viewport/UserWidget root | UI root별 z 계층을 고정 |
| CanvasScaler Scale With Screen Size | design resolution | DPI Scaling | 720×1280을 canonical 좌표로 사용 |
| ScreenMatchMode Expand | 현재 `SHOW_ALL`이 가장 가까움 | Scale to Fit + anchors | 정확한 의미 차이는 6장 참고 |
| RectTransform Pivot | `Node::setAnchorPoint` | Slot Alignment | Axmol anchor point를 parent anchor로 오해 금지 |
| RectTransform Anchor Min/Max | `LayoutComponent`/계산식 | Canvas Slot Anchors | stretch 시 네 변 offset 계산 |
| anchoredPosition | `Node::setPosition` | Canvas Slot Position | anchor 기준 좌표로 변환 후 입력 |
| HorizontalLayoutGroup | `HBox`/HORIZONTAL LayoutGroup | HorizontalBox | margin/spacing과 강제 확장을 따로 계산 |
| VerticalLayoutGroup | `VBox`/VERTICAL LayoutGroup | VerticalBox | top-down 방향 확인 |
| GridLayoutGroup | 프로젝트 `UiGridLayout` | UniformGridPanel/GridPanel | Axmol 기본 UI에는 동등 Grid가 없음 |
| ContentSizeFitter | content size 재계산 | SizeBox/desired size | 아이템 변경 시 한 번만 relayout |
| Mask/RectMask2D | Layout clipping/ClippingNode | Retainer/Clipping | 사각형은 scissor 우선 |
| Image | `ui::ImageView`/`Sprite` | Image | 9-slice는 Scale9 |
| TMP Text | `ui::Text`/`Label` | TextBlock | outline/glow는 engine API 사용 |
| Button.onClick | `addClickEventListener` | OnClicked delegate | 동일 callback 재등록 방지 |
| UnityEvent | EventDispatcher/typed bus | multicast delegate | 도메인 이벤트는 enum 기반 wrapper |
| DOTween Sequence | `Sequence` | UMG Animation/Timeline | 병렬은 `Spawn` |
| Animator Controller | 프로젝트 `UiAnimator` | Widget Animation state | named state를 C++로 명시 |
| Destroy(gameObject) | `removeFromParent()` | RemoveFromParent/GC | Axmol create 객체를 직접 delete 금지 |
| Transform.SetParent | retain → detach(false) → attach → release | reparent slot | action 유지 여부를 명시 |
| SetSiblingIndex | local z + arrival order | ZOrder | 중요한 순서는 z 상수로 표현 |
| UI Outline | Text outline 또는 shader | UI material | Sprite outline은 alpha-neighbor shader |
| ParticleSystem | `ParticleSystemQuad` | Niagara | additive particle atlas 사용 |
| LineRenderer | `DrawNode`/custom mesh | Spline/ProceduralMesh | 동적 굵은 선은 단일 vertex buffer |
| Vectrosity | 프로젝트 `UiPolylineNode` | custom Slate/mesh | join/cap/width/color를 mesh로 구현 |
| I2 Localize | `LocalizationService` | String Table | key 기반 UTF-8 table |
| Resources.Load | FileUtils/catalog/factory | asset load | 화면에서 raw path 조립 금지 |
| Addressables | asset catalog + async loader | Asset Manager | handle 소유와 취소를 명시 |

## 6. 화면 스케일: Unity Expand와 Axmol SHOW_ALL

### 6.1 Unity 원본 설정

`WidgetMain_Stage.prefab` root Canvas:

```text
Reference Resolution = 720 x 1280
UI Scale Mode        = Scale With Screen Size
Screen Match Mode    = Expand
```

Unity 공식 정의에서 Expand는 캔버스가 reference resolution보다 작아지지 않도록 가로 또는 세로
논리 영역을 확장한다. 비율이 다른 화면에서는 한 축의 논리 좌표가 720 또는 1280보다 커진다.

### 6.2 현재 Axmol 설정

`projects/CapsuleMonsterChess/Source/AppDelegate.cpp`:

```cpp
renderView->setDesignResolutionSize(
    720.0F,
    1280.0F,
    ax::ResolutionPolicy::SHOW_ALL);
```

`SHOW_ALL`은 design resolution 전체를 유지하고 aspect ratio가 다르면 여백이 생길 수 있다.
즉 결과 의도는 비슷하지만 수학적으로 동일하지 않다.

| 상황 | Unity Expand | Axmol SHOW_ALL |
|---|---|---|
| 9:16 | 720×1280 기준 그대로 | 720×1280 그대로 |
| 더 넓은 화면 | 논리 가로 영역 확장 | 720×1280 전체를 보여주고 viewport 여백 가능 |
| 더 긴 화면 | 논리 세로 영역 확장 | 720×1280 전체를 보여주고 viewport 여백 가능 |
| UI crop | 없음 | 없음 |

현재 게임은 portrait 9:16을 기준으로 하고 데스크톱 창도 405×720이므로 `SHOW_ALL`을 유지한다.
다양한 모바일 aspect에서 Unity Expand와 완전히 같은 “추가 논리 영역”이 필요할 때는
무조건 policy를 바꾸지 말고 다음 순서로 처리한다.

1. `Director::getVisibleSize()`와 `getVisibleOrigin()`을 화면 canonical rect로 사용한다.
2. `Director::getSafeAreaRect()`를 조작 가능한 UI rect로 사용한다.
3. background는 visible rect 전체, 핵심 버튼은 safe rect에 배치한다.
4. 실제 기기 비교에서 좌우/상하 추가 영역이 요구될 때만 `FIXED_HEIGHT` 또는 `FIXED_WIDTH`를
   AppDelegate의 aspect별 정책으로 선택한다.

```cpp
const auto* director = ax::Director::getInstance();
const ax::Size visibleSize = director->getVisibleSize();
const ax::Vec2 visibleOrigin = director->getVisibleOrigin();
const ax::Rect safeArea = director->getSafeAreaRect();
```

Unity의 `SafeAreaPanel.cs`는 이름과 달리 `Screen.safeArea` 기반 notch 처리기가 아니다.
고정 9:16 letterbox를 만들기 위해 full-stretch anchor를 보정한다. Axmol에서는 이 코드를 복제하지
않고 엔진 safe area와 design resolution을 기준으로 한다.

## 7. Anchor, Pivot, Stretch 정확한 변환

### 7.1 가장 중요한 대응

```text
Unity RectTransform.pivot        = Axmol Node.anchorPoint
Unity anchorMin/anchorMax        = Axmol LayoutComponent 또는 수동 layout constraint
Unity anchoredPosition          = 변환 후 Axmol Node.position
Unity sizeDelta                 = non-stretch일 때 Axmol contentSize
```

Unity와 Axmol UI 좌표는 기본적으로 local bottom-left 원점으로 생각할 수 있다.

### 7.2 non-stretch 공식

부모 크기를 `P`, Unity anchor를 `a`, anchored position을 `p`, pivot을 `v`, 크기를 `s`라 하면:

```text
axmolAnchorPoint = v
axmolContentSize = s
axmolPosition.x  = P.width  * a.x + p.x
axmolPosition.y  = P.height * a.y + p.y
```

재사용 helper:

```cpp
struct UiRect final
{
    ax::Vec2 anchor;
    ax::Vec2 pivot;
    ax::Vec2 offset;
    ax::Size size;
};

inline void applyFixedRect(ax::Node& node, const ax::Size& parentSize, const UiRect& rect)
{
    node.setAnchorPoint(rect.pivot);
    node.setContentSize(rect.size);
    node.setPosition(
        {parentSize.width * rect.anchor.x + rect.offset.x,
         parentSize.height * rect.anchor.y + rect.offset.y});
}
```

실제 `m_BtnSettings`:

```text
anchorMin = anchorMax = (1, 1)
anchoredPosition      = (-16, -100)
sizeDelta             = (70, 76)
pivot                 = (1, 1)
```

Axmol:

```cpp
applyFixedRect(
    *settingsButton,
    root.getContentSize(),
    {{1.0F, 1.0F}, {1.0F, 1.0F}, {-16.0F, -100.0F}, {70.0F, 76.0F}});
```

결과는 “부모 오른쪽 위에서 왼쪽 16, 아래 100”이다.

### 7.3 stretch 공식

Unity stretch RectTransform은 `sizeDelta`만 복사하면 안 된다. 부모 크기 `P`에서:

```text
left   = anchorMin.x * P.width  + offsetMin.x
right  = anchorMax.x * P.width  + offsetMax.x
bottom = anchorMin.y * P.height + offsetMin.y
top    = anchorMax.y * P.height + offsetMax.y

width  = right - left
height = top - bottom

position.x = left   + pivot.x * width
position.y = bottom + pivot.y * height
```

Unity Inspector에서 right/top offset은 직관과 반대로 음수가 저장되는 경우가 많다.
YAML의 `m_SizeDelta`만 보고 여백을 추측하지 말고 `offsetMin/offsetMax` 의미로 환산한다.

```cpp
struct UiStretchRect final
{
    ax::Vec2 anchorMin;
    ax::Vec2 anchorMax;
    ax::Vec2 offsetMin;
    ax::Vec2 offsetMax;
    ax::Vec2 pivot;
};

inline void applyStretchRect(
    ax::Node& node,
    const ax::Size& parent,
    const UiStretchRect& rect)
{
    const float left = rect.anchorMin.x * parent.width + rect.offsetMin.x;
    const float right = rect.anchorMax.x * parent.width + rect.offsetMax.x;
    const float bottom = rect.anchorMin.y * parent.height + rect.offsetMin.y;
    const float top = rect.anchorMax.y * parent.height + rect.offsetMax.y;
    const ax::Size size{right - left, top - bottom};

    AXASSERT(size.width >= 0.0F && size.height >= 0.0F, "invalid stretch rect");
    node.setAnchorPoint(rect.pivot);
    node.setContentSize(size);
    node.setPosition(
        {left + rect.pivot.x * size.width,
         bottom + rect.pivot.y * size.height});
}
```

### 7.4 LayoutComponent로 stretch

고정된 좌우/상하 margin이면 수식보다 `LayoutComponent`가 읽기 쉽다.

```cpp
auto* panel = ax::ui::Layout::create();
parent.addChild(panel);

auto* layout = ax::ui::LayoutComponent::bindLayoutComponent(panel);
layout->setStretchWidthEnabled(true);
layout->setHorizontalEdge(ax::ui::LayoutComponent::HorizontalEdge::Center);
layout->setLeftMargin(25.0F);
layout->setRightMargin(25.0F);
layout->setSizeHeight(125.0F);
layout->setVerticalEdge(ax::ui::LayoutComponent::VerticalEdge::Bottom);
layout->setBottomMargin(152.0F);
layout->refreshLayout();
```

`LayoutComponent`는 부모 content size가 유효한 뒤 refresh해야 한다. 화면 resize가 가능한 플랫폼은
root size 변경 시 한 번만 전체 layout을 갱신한다. 매 frame refresh하지 않는다.

## 8. 부모·자식, 삭제, Z order

### 8.1 부모와 자식

| Unity | Axmol |
|---|---|
| `transform.SetParent(parent, false)` | `parent->addChild(child)` |
| `transform.parent` | `node->getParent()` |
| `transform.GetChild(i)` | `node->getChildren().at(i)` |
| `GetComponentInChildren` | 명시적 포인터 또는 `getChildByName` |
| `gameObject.SetActive(false)` | `node->setVisible(false)` + 필요 시 input disable |

Axmol child는 동시에 두 부모를 가질 수 없다. 기존 child의 action/state를 보존해 옮길 때:

```cpp
void reparentWithoutCleanup(ax::Node& child, ax::Node& newParent, int zOrder)
{
    child.retain();
    child.removeFromParentAndCleanup(false);
    newParent.addChild(&child, zOrder);
    child.release();
}
```

`retain/release` 사이를 RAII helper로 감싸도 된다. 단순 UI 재구성이라면 상태를 들고 재parent하기보다
기존 화면을 닫고 factory로 다시 만드는 편이 수명 추적이 쉽다.

### 8.2 삭제

Unity:

```csharp
Destroy(gameObject);
```

Axmol:

```cpp
popup->removeFromParent(); // cleanup=true
popup = nullptr;           // non-owning 멤버도 즉시 정리
```

`Node::create()` 계열은 autorelease + 부모 retain으로 관리된다. 직접 `delete`하지 않는다.
부모에서 제거한 뒤 raw pointer를 계속 보관하면 dangling pointer가 될 수 있다.

화면을 닫을 때:

1. 전역/도메인 listener 해제
2. 비동기 callback 취소 또는 generation 검증
3. action/schedule 정지
4. `removeFromParent()`
5. 보관하던 non-owning pointer를 `nullptr`

### 8.3 Z order와 sibling order

Unity Canvas sorting order와 sibling index를 모두 Axmol local z 하나로 뭉개지 않는다.

```cpp
namespace UiZ
{
constexpr int kScreen = 0;
constexpr int kPopup = 1000;
constexpr int kToast = 2000;
constexpr int kTransition = 3000;
}
```

```cpp
uiRoot.addChild(screenLayer, UiZ::kScreen);
uiRoot.addChild(popupLayer, UiZ::kPopup);
popupLayer->reorderChild(popup, 10);
popup->setLocalZOrder(10);
```

Axmol은 local z가 작은 child를 먼저 그린다. 같은 local z이면 도착 순서(order of arrival)가
정렬에 관여한다. 중요한 순서를 “나중에 add했으니 위”에 의존하지 말고 z 상수로 표현한다.
negative z child는 부모 자체 draw보다 먼저, 0 이상 child는 부모 draw 뒤에 방문된다.

## 9. Horizontal Layout, Vertical Layout, Grid

### 9.1 실제 `m_HoriChests`

`WidgetMain_Stage.prefab`의 chest row:

```text
anchorMin/Max         = (0, 0) / (1, 0)
anchoredPosition     = (0, 152)
sizeDelta            = (0, 125)
Horizontal spacing  = 25
padding left/right   = 25 / 25
child force expand   = width true, height true
children             = WidgetChestFarm x 4
```

단순한 Axmol HBox:

```cpp
auto* row = ax::ui::HBox::create({720.0F, 125.0F});
row->setAnchorPoint({0.5F, 0.0F});
row->setPosition({360.0F, 152.0F});
root.addChild(row);

for (const auto& chest : chestModels)
{
    auto* item = WidgetFactory::createChestFarm(chest);
    auto* parameter = ax::ui::LinearLayoutParameter::create();
    parameter->setMargin({12.5F, 0.0F, 12.5F, 0.0F});
    item->setLayoutParameter(parameter);
    row->addChild(item);
}
row->requestDoLayout();
```

다만 Unity의 `Child Force Expand Width=true`는 남은 폭을 flexible width 비율로 분배한다.
Axmol HBox에 같은 계약을 기대하지 말고 cell width를 명시적으로 계산한다.

```text
availableWidth = rowWidth - paddingLeft - paddingRight
                 - spacing * (count - 1)
cellWidth      = availableWidth / count
```

```cpp
float expandedCellWidth(
    float rowWidth,
    float paddingLeft,
    float paddingRight,
    float spacing,
    std::size_t count)
{
    if (count == 0U)
        return 0.0F;
    const float gaps = spacing * static_cast<float>(count - 1U);
    return std::max(0.0F, (rowWidth - paddingLeft - paddingRight - gaps) /
                              static_cast<float>(count));
}
```

현재 Unity `WidgetChestFarm` root는 150×200인데 chest row 높이는 125다. 따라서 원본에서도 child
preferred size, layout control, clipping 여부에 따라 튀어나올 수 있다. 숫자를 기계 복사하지 말고
실제 Game View를 기준으로 다음 중 하나를 확정한다.

- row가 chest의 일부만 보여주는 디자인이면 clipping을 켠다.
- 전체 chest가 보여야 하면 row height를 200 이상으로 맞춘다.
- 시각 크기를 125에 맞추려면 item scale 대신 factory의 canonical size를 조정한다.

### 9.2 실제 `m_HoriPlay`

```text
horizontal stretch
bottom offset = 389
height        = 150
spacing       = 21.47
force expand  = false
```

이 row는 각 child 고유 크기를 유지해야 한다. HBox + 각 child content size + margin으로 충분하다.
강제 확장 계산을 공유하지 않는다.

### 9.3 Vertical layout

Unity `VerticalLayoutGroup`의 top-to-bottom 목록은 Axmol `VBox`를 사용하되, 좌표 방향을 확인한다.
필요하면 child 추가 순서를 뒤집지 말고 gravity/alignment와 container anchor를 고정한다.

긴 목록은 `VBox`에 모든 child를 생성하지 않는다.

| 항목 수 | 구현 |
|---:|---|
| 1~30 | `VBox`/수동 layout |
| 30~200 | `ui::ListView` 또는 cell 재사용 |
| 수백 이상/동적 | viewport 기반 recycling list/grid |

### 9.4 GridLayoutGroup 대응

Axmol 기본 `ax::ui`에는 Unity `GridLayoutGroup`과 동등한 fixed row/column grid가 없다.
`TableView`는 1차원 재사용 리스트이지 2차원 GridLayoutGroup이 아니다.

작은 고정 grid는 다음 C++17 helper로 배치한다.

```cpp
#include "axmol.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace cmc::client::ui
{
struct GridSpec final
{
    ax::Size cellSize;
    ax::Vec2 spacing;
    ax::Vec2 topLeft;
    std::size_t columns = 1U;
};

inline float layoutFixedGrid(
    const GridSpec& spec,
    const std::vector<ax::Node*>& children)
{
    const std::size_t columns = std::max<std::size_t>(1U, spec.columns);

    for (std::size_t i = 0; i < children.size(); ++i)
    {
        ax::Node* child = children[i];
        AXASSERT(child != nullptr, "grid child must not be null");

        const std::size_t column = i % columns;
        const std::size_t row = i / columns;
        child->setAnchorPoint({0.0F, 1.0F});
        child->setContentSize(spec.cellSize);
        child->setPosition(
            {spec.topLeft.x +
                 static_cast<float>(column) * (spec.cellSize.width + spec.spacing.x),
             spec.topLeft.y -
                 static_cast<float>(row) * (spec.cellSize.height + spec.spacing.y)});
    }

    const std::size_t rowCount =
        (children.size() + columns - 1U) / columns;
    if (rowCount == 0U)
        return 0.0F;

    return static_cast<float>(rowCount) * spec.cellSize.height +
           static_cast<float>(rowCount - 1U) * spec.spacing.y;
}
} // namespace cmc::client::ui
```

`ScrollView` 안에서는 반환한 content height를 inner container에 반영한다. child 전체를 매 frame
재배치하지 말고 모델 수, column 수, viewport width가 바뀔 때만 dirty layout을 실행한다.

대형 unit/monster grid의 `UiGridLayout` 계약:

```cpp
class UiGridDataSource
{
public:
    virtual ~UiGridDataSource() = default;
    virtual std::size_t itemCount() const = 0;
    virtual ax::Node* acquireCell() = 0;
    virtual void bindCell(ax::Node& cell, std::size_t index) = 0;
    virtual void recycleCell(ax::Node& cell) = 0;
};
```

viewport에 보이는 row ± 1만 생성하고 나머지는 pool로 반환한다. 이 구조가 Vectrosity/particle과
같은 고비용 child가 섞인 목록에서도 프레임 시간을 안정시킨다.

## 10. 이벤트, 버튼 구독, 해제

### 10.1 Widget callback

Unity:

```csharp
button.onClick.AddListener(OnClickBattle);
button.onClick.RemoveListener(OnClickBattle);
```

Axmol:

```cpp
battleButton_->addClickEventListener(
    [this](ax::Object*) { onClickBattle(); });
```

현재 Axmol `Widget::addClickEventListener()`는 callback 목록에 append하지 않고 내부 handler 하나를
대입한다. 다시 호출하면 이전 callback을 교체한다.

명시적 해제:

```cpp
battleButton_->addClickEventListener({});
battleButton_->setTouchEnabled(false);
```

노드가 제거되면 그 Widget callback도 함께 사라진다. 하지만 callback이 `[this]`를 capture하고
버튼이 화면 controller보다 오래 살면 위험하다. 화면이 버튼의 부모/동일 수명이어야 한다.

### 10.2 중복 구독 방지

Unity `UI_Base.BindUIEvent()`는 같은 delegate를 `-=`한 뒤 `+=`한다. Axmol Widget callback은 교체
형식이므로 동일 버튼에 여러 listener가 누적되지는 않지만, `bindEvents()`를 여러 번 호출하면 어떤
handler가 최종인지 불명확해진다.

```cpp
void StageScreen::bindEvents()
{
    AXASSERT(!eventsBound_, "StageScreen events already bound");
    eventsBound_ = true;

    settingsButton_->addClickEventListener(
        [this](ax::Object*) { openSettings(); });
    battleButton_->addClickEventListener(
        [this](ax::Object*) { requestBattle(); });
}

void StageScreen::unbindEvents()
{
    if (!eventsBound_)
        return;
    eventsBound_ = false;

    settingsButton_->addClickEventListener({});
    battleButton_->addClickEventListener({});
}
```

### 10.3 Touch/drag/drop

```cpp
dragHandle_->addTouchEventListener(
    [this](ax::Object*, ax::ui::Widget::TouchEventType type)
    {
        switch (type)
        {
        case ax::ui::Widget::TouchEventType::BEGAN:
            beginDrag();
            break;
        case ax::ui::Widget::TouchEventType::MOVED:
            updateDrag();
            break;
        case ax::ui::Widget::TouchEventType::ENDED:
            endDrag(false);
            break;
        case ax::ui::Widget::TouchEventType::CANCELED:
            endDrag(true);
            break;
        }
    });
```

screen/world/local 좌표 변환은 해당 root를 기준으로 한다.

```cpp
const ax::Vec2 local = popupRoot->convertToNodeSpace(worldPosition);
const ax::Vec2 world = popupRoot->convertToWorldSpace(localPosition);
```

Unity `UI_MovingPopup`의 double click close는 click 간격을 Scheduler 시간으로 재현하되,
drag threshold를 넘긴 gesture를 click으로 처리하지 않는다.

### 10.4 전역 EventDispatcher

scene graph priority listener는 Node와 연결되지만 custom fixed-priority listener는 반드시 해제한다.

```cpp
class ScopedEventListener final
{
public:
    ScopedEventListener() = default;
    ScopedEventListener(const ScopedEventListener&) = delete;
    ScopedEventListener& operator=(const ScopedEventListener&) = delete;

    ~ScopedEventListener() { reset(); }

    void assign(ax::EventListener* listener)
    {
        reset();
        listener_ = listener;
    }

    void reset()
    {
        if (!listener_)
            return;
        ax::Director::getInstance()
            ->getEventDispatcher()
            ->removeEventListener(listener_);
        listener_ = nullptr;
    }

private:
    ax::EventListener* listener_ = nullptr;
};
```

프로젝트 도메인 이벤트를 문자열 custom event로 직접 흩뿌리지 않는다.

```cpp
enum class UiEvent : std::uint8_t
{
    CurrencyChanged,
    LanguageChanged,
    InventoryChanged,
    ConnectionStateChanged
};
```

`UiEventBus::subscribe()`가 token을 반환하고 token destructor가 unsubscribe하는 구조로 만든다.
Unity 프로젝트의 `EventDispatcher`도 enum/int routing을 의도하므로 이 경계를 유지한다.

### 10.5 비동기 callback

팝업을 닫은 뒤 Addressables/HTTP callback이 도착하는 문제는 Node listener 해제로 해결되지 않는다.

```cpp
const std::uint64_t requestGeneration = ++requestGeneration_;
service.load(
    [this, requestGeneration](Result result)
    {
        if (!isRunning() || requestGeneration != requestGeneration_)
            return;
        apply(std::move(result));
    });
```

close 시 generation을 증가시키고 가능한 backend request는 취소한다. raw `this` capture를
작업 큐에 무기한 남기지 않는다.

## 11. 애니메이션: DOTween과 Animator를 어떻게 옮기는가

### 11.1 기본 대응

| Unity/DOTween | Axmol |
|---|---|
| `DOMove` | `MoveTo`/`MoveBy` |
| `DOScale` | `ScaleTo`/`ScaleBy` |
| `DORotate` | `RotateTo`/`RotateBy` |
| `DOFade` | `FadeTo`/`FadeIn`/`FadeOut` |
| `Sequence.Append` | `Sequence::create` |
| `Sequence.Join` | `Spawn::create` |
| `AppendInterval` | `DelayTime` |
| `OnComplete` | `CallFunc` |
| `SetLoops` | `Repeat`/`RepeatForever` |
| ease | `EaseBackOut`, `EaseSineInOut` 등 |
| `DOKill(target)` | action tag 정지 또는 `stopAllActions()` |

Popup open:

```cpp
constexpr int kPopupOpenActionTag = 0x504F504E;

void animatePopupOpen(ax::Node& content)
{
    content.stopActionByTag(kPopupOpenActionTag);
    content.setScale(0.86F);
    content.setOpacity(0);

    auto* action = ax::Spawn::create(
        ax::EaseBackOut::create(ax::ScaleTo::create(0.22F, 1.0F)),
        ax::FadeIn::create(0.16F),
        nullptr);
    action->setTag(kPopupOpenActionTag);
    content.runAction(action);
}
```

Popup close:

```cpp
void animatePopupClose(ax::Node& popup)
{
    popup.stopAllActions();
    popup.runAction(
        ax::Sequence::create(
            ax::Spawn::create(
                ax::ScaleTo::create(0.14F, 0.92F),
                ax::FadeOut::create(0.14F),
                nullptr),
            ax::RemoveSelf::create(true),
            nullptr));
}
```

중요한 business callback은 연출 완료에만 의존하지 않는다. 예를 들어 가챠 보상 반영은 idempotent
상태를 먼저 검사하고, 애니메이션 skip/중복 완료에도 한 번만 실행돼야 한다.

### 11.2 Animator Controller

Axmol에는 UI Animator Controller editor가 없다. 복잡한 popup은 named clip/state를 얇게 만든다.

```cpp
enum class UiAnimState : std::uint8_t
{
    Hidden,
    Opening,
    Idle,
    Closing
};

class UiAnimator final
{
public:
    explicit UiAnimator(ax::Node& target) : target_(target) {}

    void play(UiAnimState state);
    void stop();
    UiAnimState state() const noexcept { return state_; }

private:
    ax::Node& target_;
    UiAnimState state_ = UiAnimState::Hidden;
};
```

Sprite sheet animation은 `Animation` + `Animate`, 숫자/색/진행률 보간은 `ActionFloat` 또는
`schedule`을 사용한다. 화면 전체 상태 전환은 UI Action과 게임 상태를 섞지 말고 controller가
명시적으로 호출한다.

### 11.3 pause와 time scale

Unity DOTween의 `SetUpdate(true)`처럼 게임 time scale과 무관한 UI가 필요하면 어떤 Scheduler를
사용하는지 명시해야 한다. CMC의 전투 simulation fixed tick과 UI presentation time은 분리한다.
보상 트랜잭션, 네트워크 timeout을 UI Action 시간으로 측정하지 않는다.

## 12. Scene, Popup, Subitem을 실제로 옮기는 방법

### 12.1 Scene UI

Unity:

```csharp
Manager.UI.ShowSceneUI<WidgetMain_Stage>();
```

Axmol:

```cpp
auto* stage = StageScreen::create();
uiManager.openScreen(*stage);
```

`StageScreen::init()`에서 다음을 조합한다.

1. background/visible rect
2. safe-area top currency bar
3. stage selector
4. `m_HoriChests`
5. `m_HoriPlay`
6. bottom navigation

현재 `LobbyLayer::replaceScreen()`이 이미 screen root 교체 책임을 가진다. 첫 단계는 동작을 바꾸지
않고 이 함수가 `UiManager::openScreen()`을 호출하도록 경계를 추출하는 것이다.

### 12.2 Popup

Unity popup path는 존재하지만 실제 프리팹은 없다. Axmol popup contract를 먼저 고정한다.

```text
PopupLayer
├─ InputShield       z=-10, full safe/visible rect
└─ PopupContent      z=0
```

규칙:

- modal popup마다 input shield를 둔다.
- 바깥 클릭 close 여부는 popup 옵션이다.
- Escape/Back은 top popup 하나만 닫는다.
- popup stack order는 monotonically increasing local z로 관리한다.
- close callback은 한 번만 호출한다.

현재 `LobbyLayer::createOverlay()`는 `_overlayRoot`를 z=1000에 두고 input shield를 -10에 둔다.
이 코드를 `UiPopup`의 초기 구현으로 이동하면 된다.

### 12.3 Subitem

Unity `WidgetChestFarm`, `WidgetCurrencyBar`, `WidgetGauge`는 Axmol factory 함수로 옮긴다.

```cpp
class WidgetFactory final
{
public:
    static ax::Node* createChestFarm(const ChestFarmModel& model);
    static ax::Node* createCurrencyBar(
        const CurrencyBarModel& model,
        std::function<void()> onAdd);
    static ax::Node* createGauge(const GaugeModel& model);
};
```

factory가 business service를 직접 호출하지 않는다. model과 command callback만 받아 UI를 만든다.
Unity prefab의 serialized hierarchy는 factory 내부 hierarchy가 된다.

`WidgetGauge`의 `Image.fillAmount` 대응:

- bar/linear: `ui::LoadingBar`
- radial: `ProgressTimer`
- 9-sliced fill을 유지해야 함: clipping width를 조절하는 Scale9 child 또는 전용 shader

## 13. Outline, glow, 빛나는 효과

### 13.1 텍스트

Unity TMP:

```csharp
tmp.outlineWidth = 0.15f;
tmp.outlineColor = color;
```

Axmol:

```cpp
auto* title = ax::Label::createWithTTF("NEW UNIT!", fontPath, 38.0F);
title->enableOutline(ax::Color32{72, 32, 0, 255}, 3.0F);
title->enableGlow(ax::Color32{255, 205, 64, 180}, 6.0F);
```

또는:

```cpp
uiText->enableOutline(ax::Color32{72, 32, 0, 255}, 3);
uiText->enableGlow(ax::Color32{255, 205, 64, 180});
```

font atlas/SDF 설정에 따라 outline 품질과 비용이 다르므로 동일 화면에서 같은 font/outline preset을
재사용한다.

### 13.2 Sprite outline

선택지가 세 가지다.

| 방식 | 용도 | 비용/제약 |
|---|---|---|
| 뒤에 tint sprite 복제 | 작은 수의 카드, 빠른 구현 | 둥근 outline 정확도 낮음 |
| alpha-neighbor fragment shader | production 선택/강조 | texture texel size uniform 필요 |
| RenderTexture 후처리 | 큰 복합 hierarchy | render pass/메모리 비용 큼 |

production sprite outline shader는 현재 UV 주변 alpha를 8방향 또는 원형 kernel로 sample한다.

```text
outlineAlpha = max(neighborAlpha) - centerAlpha
result = baseColor + outlineColor * outlineAlpha
```

uniform:

- `u_texelSize`
- `u_outlineWidth`
- `u_outlineColor`
- `u_glowIntensity`

Axmol `Sprite::setProgramState()`로 shader state를 연결한다. 다수의 grid cell에서 sprite를 8개씩
복제하지 않는다.

### 13.3 Glow

정적 glow:

- soft radial sprite
- additive blend
- 뒤쪽 z
- opacity pulse

```cpp
auto* glow = ax::Sprite::create("ui/effects/soft_glow.ktx2");
glow->setBlendFunc(ax::BlendFunc::ADDITIVE);
glow->setColor(ax::Color32{255, 204, 70, 255});
glow->setOpacity(90);
glow->runAction(
    ax::RepeatForever::create(
        ax::Sequence::create(
            ax::FadeTo::create(0.7F, 160),
            ax::FadeTo::create(0.7F, 75),
            nullptr)));
```

여러 효과가 같은 glow texture와 blend state를 공유하도록 atlas/batching을 유지한다.

## 14. 가챠 연출

### 14.1 현재 Axmol 구현

`LobbyLayer::showGachaReveal()`:

- full backdrop
- `DrawNode` 원형 glow 2개
- capsule egg
- scale/rotate `Sequence`
- 1.05초 뒤 `finishGachaReveal()`

`finishGachaReveal()`:

- pending reward 확인
- `_pendingRewardApplied`로 중복 반영 방지
- writable data source 확인
- 신규 unit 또는 duplicate shard 반영
- 저장 성공 후 result 화면

`showGachaResult()`:

- 원형 burst
- rarity text
- unit artwork card
- collection 결과

이 구조에서 가장 중요한 것은 **연출과 보상 트랜잭션이 분리되어 있다는 점**이다. 파티클을
추가하면서 보상 반영을 particle callback 내부로 옮기지 않는다.

### 14.2 production 연출 stack

```text
z=-20  dark vignette/background
z=-10  large radial glow
z=0    back rays / rotating gradient
z=5    capsule
z=8    crack/spark particles
z=10   flash
z=12   reward card
z=15   foreground particles
z=20   title / skip / collect input
```

Axmol 구현:

- spark/confetti: `ParticleSystemQuad::create(plist)`
- additive flash: fullscreen white `LayerColor` + FadeOut
- rotating rays: ray sprite 또는 triangle fan Node 회전
- capsule shake: tagged Sequence
- burst ring: `Sprite` scale/fade 또는 `DrawNode`
- trail: `MotionStreak`
- high-tier reveal: shader dissolve/edge glow

```cpp
auto* sparks = ax::ParticleSystemQuad::create("ui/effects/gacha_sparks.plist");
sparks->setPosition(capsulePosition);
sparks->setAutoRemoveOnFinish(true);
effectRoot->addChild(sparks, 8);
```

고급 연출도 다음 상태를 반드시 지킨다.

```text
Idle
→ RequestingServerResult
→ ResultReserved
→ Revealing
→ ApplyingRewardOnce
→ ShowingResult
→ Collected
```

서버 authoritative 결과가 도입되면 클라이언트가 random reward를 결정하지 않는다.
`ResultReserved`에 서버 receipt/idempotency key를 보관하고 `ApplyingRewardOnce`는 같은 key를
중복 처리하지 않는다.

skip 버튼은 연출을 끝내되 상태를 생략하지 않는다.

```cpp
void GachaPresenter::skip()
{
    effectRoot_->stopAllActions();
    effectRoot_->removeAllChildren();
    finishRevealOnce();
}
```

## 15. Gradient 효과

### 15.1 Unity 원본

`Assets/Scripts/Widget/UIGradient.cs`는 `BaseMeshEffect`에서 UI vertex color를 두 색 사이로
보간한다. angle과 aspect compensation을 지원한다.

`WidgetGradientBG.cs`는 `LateUpdate()`에서 angle을 회전시키고 `ForceUpdateMesh()`를 호출한다.
현재 runtime scene/prefab serialized reference는 확인되지 않았다. 즉 구현 코드는 있지만 현재 화면에
연결된 사용처는 없다.

### 15.2 단순 두 색 gradient

Axmol 내장 선택:

- `LayerGradient`: 방향성 2색 gradient
- `LayerRadialGradient`: radial gradient
- `ui::LayoutGroup` background gradient

```cpp
auto* gradient = ax::LayerGradient::create(
    ax::Color32{24, 55, 96, 255},
    ax::Color32{5, 12, 31, 255},
    ax::Vec2{0.0F, -1.0F});
gradient->setContentSize(size);
root.addChild(gradient, -10);
```

정적인 2색 background는 custom shader를 만들지 않는다.

### 15.3 네 꼭짓점/multi-stop gradient

`DrawNode::drawColoredTriangle()` 두 개로 quad를 그릴 수 있다.

```cpp
void drawGradientQuad(
    ax::DrawNode& node,
    const ax::Size& size,
    const ax::Color& bottomLeft,
    const ax::Color& bottomRight,
    const ax::Color& topRight,
    const ax::Color& topLeft)
{
    const ax::Vec2 triangle0[3] = {
        {0.0F, 0.0F},
        {size.width, 0.0F},
        {size.width, size.height}};
    const ax::Color color0[3] = {bottomLeft, bottomRight, topRight};
    node.drawColoredTriangle(triangle0, color0);

    const ax::Vec2 triangle1[3] = {
        {0.0F, 0.0F},
        {size.width, size.height},
        {0.0F, size.height}};
    const ax::Color color1[3] = {bottomLeft, topRight, topLeft};
    node.drawColoredTriangle(triangle1, color1);
}
```

multi-stop은 stop 경계마다 quad strip을 나눈다. 색/크기/angle이 바뀔 때만 geometry를 rebuild한다.

### 15.4 회전 gradient

Unity처럼 매 frame CPU vertex를 다시 만드는 방식은 큰 배경에서 불필요한 비용이 생긴다.
회전 angle이 계속 변하면 fragment shader를 사용한다.

```text
t = dot(localUv - 0.5, direction(angle)) + 0.5
color = lerp(colorA, colorB, saturate(t))
```

`rhi::ProgramState`에 `u_direction`, `u_colorA`, `u_colorB`만 갱신한다. geometry는 고정 quad 하나다.
multi-stop은 작은 1D gradient texture를 sample하면 분기와 uniform 수를 줄일 수 있다.

## 16. LineRenderer, UILineRenderer, Vectrosity

### 16.1 Unity 프로젝트의 실제 상태

- Vectrosity 5.6.1 source가 설치되어 있다.
- 게임 자체 C#에서 `VectorLine` 사용은 확인되지 않았다.
- `WidgetLib.AddLine()`은 Unity UI Extensions `UILineRenderer`를 만들려던 코드가 주석 처리되어
  현재 `null`을 반환한다.

따라서 “현재 보이는 선을 1:1 포팅”할 대상은 없고, 향후 필요한 line API를 Axmol에 정의해야 한다.

### 16.2 요구별 선택

| 요구 | Axmol 구현 |
|---|---|
| 고정 1px/단순 선 | `DrawNode::drawLine` |
| 굵은 선/round cap | `DrawNode::drawSegment` |
| 연결 polyline | `DrawNode::drawPoly` |
| Bezier | `drawQuadBezier`/`drawCubicBezier` |
| spline | `drawCardinalSpline`/`drawCatmullRom` |
| 드래그 궤적 | `MotionStreak` |
| 파티클 trail | `ParticleSystemQuad` |
| per-point width/color, join/cap, texture | 프로젝트 `UiPolylineNode` |
| 3D world line + depth | dynamic mesh + camera/depth state |

단순 연결선:

```cpp
void redrawConnections(
    ax::DrawNode& drawNode,
    const std::vector<ax::Vec2>& points,
    float width,
    const ax::Color& color)
{
    drawNode.clear();
    if (points.size() < 2U)
        return;

    const float radius = std::max(0.5F, width * 0.5F);
    for (std::size_t i = 1; i < points.size(); ++i)
        drawNode.drawSegment(points[i - 1U], points[i], radius, color);
}
```

### 16.3 Vectrosity 대응 `UiPolylineNode`

Vectrosity급 기능은 segment마다 Node를 만들지 않고 하나의 dynamic triangle buffer로 만든다.

필수 데이터:

```cpp
struct UiPolylinePoint final
{
    ax::Vec2 position;
    float width = 1.0F;
    ax::Color32 color = ax::Color32::white;
    float u = 0.0F;
};

enum class LineJoin : std::uint8_t
{
    Miter,
    Bevel,
    Round
};

enum class LineCap : std::uint8_t
{
    Butt,
    Square,
    Round
};
```

생성 규칙:

1. 각 point에서 이전/다음 segment normal 계산
2. miter vector와 길이 계산
3. miter limit 초과 시 bevel로 fallback
4. round join/cap은 허용 오차 기준 segment 수로 tessellate
5. 누적 길이로 UV `u` 생성
6. points/width/color가 dirty일 때만 vertex/index buffer 갱신
7. draw마다 새 `std::vector` allocation 금지

API:

```cpp
class UiPolylineNode final : public ax::Node
{
public:
    static UiPolylineNode* create();

    void setPoints(std::vector<UiPolylinePoint> points);
    void setJoin(LineJoin join);
    void setCap(LineCap cap);
    void setClosed(bool closed);
    void setTexture(std::string_view assetKey);
    void setMiterLimit(float limit);

private:
    void rebuildGeometry();
};
```

world-space line은 UI polyline을 억지로 재사용하지 않는다. 3D point를 camera로 매 frame
screen-space projection해야 하는 선택선은 UI root로 변환하거나, 실제 3D dynamic mesh와 depth
test를 사용한다.

## 17. 번역/현지화

### 17.1 Unity 원본 상태

`OptionManager.ELang`:

```text
Korean           ko
English          en
Japanese         ja
ChineseSimple    zh-CN
ChineseTaiwan    zh-TW
Portuguese       pt
Spanish          es
```

`OptionManager.SetLanguage()`는 I2 `LocalizationManager.CurrentLanguageCode`를 변경한다.
그러나 `Assets/Resources/I2Languages.asset`의 `mTerms`와 `mLanguages`는 모두 비어 있고,
현재 runtime scene/prefab에서 I2 Localize component 연결도 확인되지 않았다.

즉 언어 선택 framework는 있지만 번역 content는 아직 구축되지 않은 상태다.

### 17.2 Axmol canonical 구조

Axmol optional `sceneio` extension에도 JSON localization manager가 있지만, 현재 CMC 모듈 profile은
SceneIO를 빌드에 넣지 않는다. 또한 I2의 language change, formatting, fallback, live binding 계약을
그대로 제공하지 않는다. 게임 전용 `LocalizationService`를 둔다.

```text
Content/Data/Localization/
├─ ko.json
├─ en.json
├─ ja.json
├─ zh-CN.json
├─ zh-TW.json
├─ pt.json
└─ es.json
```

```json
{
  "ui.stage.battle": "전투",
  "ui.settings.title": "설정",
  "ui.gacha.new_unit": "새 유닛!",
  "monster.slime.name": "슬라임"
}
```

API:

```cpp
enum class Language : std::uint8_t
{
    Korean,
    English,
    Japanese,
    ChineseSimplified,
    ChineseTraditional,
    Portuguese,
    Spanish
};

class LocalizationService final
{
public:
    bool load(Language language, std::string& error);
    void setFallbackLanguage(Language language) noexcept;
    std::string_view text(std::string_view key) const noexcept;
    Language language() const noexcept;
};
```

`text()` 반환 view는 다음 language load 전까지만 유효하다는 계약을 명시하거나, 안전하게
`std::string`을 반환한다. UI는 raw 영문을 저장하지 않고 key를 저장한다.

현재 Axmol 가챠 결과에서 `reward->nameKey`를 그대로 표시하는 부분은 다음처럼 바뀐다.

```cpp
name->setString(localization.text(reward->nameKey));
```

### 17.3 language change

언어 변경 시:

1. canonical JSON table 교체
2. `UiEvent::LanguageChanged` publish
3. 보이는 `LocalizedText`만 새 문자열 적용
4. 문자열 길이 변화가 있는 container만 dirty layout
5. font preset 변경이 필요하면 atlas를 갱신

```cpp
class LocalizedText final
{
public:
    LocalizedText(
        ax::ui::Text& target,
        LocalizationService& localization,
        std::string key)
        : target_(target),
          localization_(localization),
          key_(std::move(key))
    {
        refresh();
    }

    void refresh()
    {
        target_.setString(localization_.text(key_));
    }

private:
    ax::ui::Text& target_;
    LocalizationService& localization_;
    std::string key_;
};
```

### 17.4 font와 레이아웃

- UTF-8을 source/data canonical encoding으로 사용한다.
- 한글/일본어/중국어 glyph coverage가 있는 font family를 language preset으로 지정한다.
- glyph fallback은 반드시 실제 기기에서 검증한다.
- Portuguese/Spanish accent glyph를 atlas에 포함한다.
- CJK line break와 긴 독일어/포르투갈어 수준의 길이 증가를 고려해 fixed width text에 wrapping을
  설정한다.
- 숫자/화폐/날짜는 문자열 연결 대신 locale-aware formatter 경계에서 만든다.
- 향후 Arabic/Hebrew 추가 시 bidi와 RTL mirror를 별도 기능으로 구현한다. 단순 문자열 reverse는
  금지한다.

버튼 폭을 한국어 한 단어에 맞춰 고정하지 않는다. 최소 폭 + horizontal padding + text measure로
계산하거나 충분한 디자인 폭을 둔다.

## 18. 리소스와 Prefab 대체

Unity의 prefab을 Axmol에서 거대한 C++ 함수 하나로 복사하면 유지보수가 다시 나빠진다.

```text
Unity Prefab                    Axmol

serialized hierarchy           WidgetFactory 생성 함수
serialized component values    UiStyle/UiMetrics 상수
Sprite reference               asset catalog key
MonoBehaviour controller       UiScreen/UiWidget C++ class
Prefab variant                 factory argument/style variant
Addressables handle            async asset handle/request token
```

권장 경계:

```cpp
struct UiMetrics final
{
    static constexpr ax::Size kDesignSize{720.0F, 1280.0F};
    static constexpr float kPopupCorner = 24.0F;
    static constexpr float kHorizontalPadding = 25.0F;
    static constexpr float kTouchMin = 44.0F;
};

struct UiPalette final
{
    static constexpr ax::Color32 kPanel{38, 69, 108, 255};
    static constexpr ax::Color32 kTextPrimary{255, 248, 222, 255};
    static constexpr ax::Color32 kTextSecondary{176, 205, 220, 255};
    static constexpr ax::Color32 kAccent{255, 216, 71, 255};
};
```

실제 프로젝트의 style 값은 기존 `UiStyle`에 합치고 중복 상수를 만들지 않는다.
asset path 문자열은 화면 class에서 직접 조립하지 않고 catalog/factory가 관리한다.

## 19. 현재 프로젝트에 적용하는 순서

기능을 포기하거나 삭제하는 순서가 아니라, 현재 동작을 보존하면서 경계를 세우는 순서다.

### 단계 A: layout primitive

1. `UiRect`, `UiStretchRect`
2. `applyFixedRect`, `applyStretchRect`
3. `UiGridLayout`
4. safe/visible rect test fixture

검증 aspect:

```text
720×1280  9:16 canonical
1080×1920 9:16 high resolution
1080×2340 tall phone
1440×2560 wide/tall variant
tablet portrait
notch safe area
```

### 단계 B: WidgetFactory

Unity 순서 그대로:

1. `WidgetGauge`
2. `WidgetCurrencyBar`
3. `WidgetChestFarm`
4. stage row

작은 widget부터 visual parity screenshot을 만든 뒤 `WidgetMain_Stage`를 조립한다.

### 단계 C: UI 수명주기

1. 현재 `_screenRoot`를 `ScreenLayer`로 감싼다.
2. 현재 `_overlayRoot`를 `PopupLayer`로 감싼다.
3. `replaceScreen()` 동작을 `UiManager::openScreen()`으로 이동한다.
4. `createOverlay()/closeOverlay()`를 `UiPopup`으로 이동한다.
5. listener token과 async generation을 추가한다.

### 단계 D: effect

1. text outline/glow preset
2. static radial glow
3. gacha particle/burst
4. rotating gradient shader
5. 필요한 화면이 확정되면 `UiPolylineNode`

### 단계 E: localization

1. 7개 language canonical table 생성
2. `LocalizationService`
3. visible text binding
4. language change relayout
5. font/glyph QA

## 20. 포팅 체크리스트

### 화면/배치

- [ ] Axmol anchor point를 Unity anchor로 잘못 복사하지 않았는가
- [ ] pivot이 동일한가
- [ ] stretch는 네 변 offset으로 계산했는가
- [ ] visible rect와 safe rect 역할을 분리했는가
- [ ] background는 추가 화면 영역까지 채우는가
- [ ] touch UI는 notch/home indicator를 피하는가
- [ ] resize 시 layout을 한 번만 갱신하는가

### 레이아웃

- [ ] Horizontal force-expand를 실제 폭 계산으로 재현했는가
- [ ] spacing과 padding을 이중 적용하지 않았는가
- [ ] Grid column 수와 scroll content height가 일치하는가
- [ ] 대형 목록은 cell recycling을 하는가
- [ ] 번역 후 text preferred size가 layout에 반영되는가

### 이벤트/수명

- [ ] button callback이 중복 바인딩되지 않는가
- [ ] close 시 widget callback을 해제하거나 버튼과 동일 수명인가
- [ ] fixed-priority/global listener token을 해제하는가
- [ ] async callback이 닫힌 view를 다시 만지지 않는가
- [ ] remove 후 raw pointer를 null로 만드는가
- [ ] 보상/구매 callback은 idempotent한가

### 렌더링/효과

- [ ] 같은 glow/particle texture가 atlas와 batch를 공유하는가
- [ ] 다수 cell outline을 sprite 복제 8장으로 만들지 않았는가
- [ ] animated gradient가 매 frame CPU mesh를 rebuild하지 않는가
- [ ] polyline이 segment마다 Node를 만들지 않는가
- [ ] particle/action cleanup이 popup close와 함께 되는가
- [ ] WebGL/mobile에서 shader precision과 blend를 확인했는가

### 번역

- [ ] UI 문자열이 key 기반인가
- [ ] 모든 JSON이 UTF-8인가
- [ ] missing key가 log/QA에서 드러나는가
- [ ] 7개 언어 glyph가 실제 font에 있는가
- [ ] 언어 변경 시 보이는 text만 갱신하는가
- [ ] 긴 문자열과 줄바꿈을 테스트했는가

## 21. FAQ

### Q. Axmol `setAnchorPoint({1, 1})`이면 Unity 오른쪽 위 anchor인가?

아니다. Axmol anchor point는 노드 자체의 pivot이다. 부모 오른쪽 위 고정은 pivot 설정과 함께
`parentSize + offset` 위치 계산 또는 `LayoutComponent` right/top dock가 필요하다.

### Q. Unity Expand니까 Axmol도 `NO_BORDER`로 바꾸면 되는가?

아니다. `NO_BORDER`는 viewport를 채우며 일부가 잘릴 수 있어 Expand의 “reference보다 작지 않은
canvas”와 다르다. 현재 `SHOW_ALL + visible/safe rect`를 유지하고 실제 aspect QA 후 정책을 결정한다.

### Q. Grid는 `TableView`로 해결되는가?

작은 grid는 아니다. `TableView`는 1차원 list recycling에 가깝다. 고정 grid helper 또는
2차원 recycling `UiGridLayout`이 필요하다.

### Q. 버튼 listener는 `removeEventListener`로 지우는가?

`ui::Widget::addClickEventListener`로 넣은 callback은 Widget 내부 handler다.
`addClickEventListener({})`로 비운다. `EventDispatcher`로 등록한 listener만 dispatcher에서 제거한다.

### Q. `removeFromParent()` 뒤에 `delete`해야 하는가?

하지 않는다. `create()` 객체는 autorelease/ref-count와 부모 소유를 따른다.

### Q. Popup을 닫으면 모든 callback이 안전한가?

Node Action/Scheduler와 scene-graph listener는 cleanup되지만, 전역 event bus, HTTP, worker task,
repository callback은 별도 취소/token/generation 검증이 필요하다.

### Q. Unity Gradient code를 그대로 CPU vertex 갱신으로 옮겨도 되는가?

정적 또는 가끔 바뀌는 작은 quad는 가능하다. 전체 화면 angle을 매 frame 회전하면 shader uniform
갱신이 더 적합하다.

### Q. Vectrosity를 Axmol에 그대로 가져와야 하는가?

아니다. 현재 게임 사용처가 없고 Unity C# 구현에 종속된다. 요구가 단순하면 DrawNode를 사용하고,
per-point width/color, join/cap, textured line이 실제로 필요할 때 `UiPolylineNode`를 구현한다.
기능을 생략하는 것이 아니라 요구 수준에 맞는 Axmol native 경계를 만드는 것이다.

### Q. 번역은 I2 asset을 변환하면 끝나는가?

현재 I2 data가 비어 있어 변환할 content가 없다. 7개 language JSON, key 규칙, font, live binding,
fallback 정책을 먼저 canonical하게 구축해야 한다.

## 22. 근거 파일

### Unity 프로젝트

- `ProjectSettings/ProjectVersion.txt`
- `Packages/manifest.json`
- `Assets/Resources/Prefabs/UI/Scene/WidgetMain_Stage.prefab`
- `Assets/Resources/Prefabs/UI/Subitem/WidgetChestFarm.prefab`
- `Assets/Resources/Prefabs/UI/Subitem/WidgetCurrencyBar.prefab`
- `Assets/Resources/Prefabs/UI/Subitem/WidgetGauge.prefab`
- `Assets/Scenes/MainScene.unity`
- `Assets/Scripts/Managers/UI_Manager.cs`
- `Assets/Scripts/Managers/ResourceManager.cs`
- `Assets/Scripts/Managers/OptionManager.cs`
- `Assets/Scripts/UIModule/UI_Base.cs`
- `Assets/Scripts/UIModule/UI_EventHandler.cs`
- `Assets/Scripts/UIModule/UI_Scene.cs`
- `Assets/Scripts/UIModule/UI_Popup.cs`
- `Assets/Scripts/UIModule/UI_MovingPopup.cs`
- `Assets/Scripts/Lib/WidgetLib.cs`
- `Assets/Scripts/Widget/UIGradient.cs`
- `Assets/Scripts/Widget/WidgetGradientBG.cs`
- `Assets/Resources/I2Languages.asset`
- `Assets/Plugins/Vectrosity/Vectrosity Documentation/Vectrosity changelog.txt`

### Axmol 프로젝트/엔진

- [`projects/CapsuleMonsterChess/Source/AppDelegate.cpp`](../projects/CapsuleMonsterChess/Source/AppDelegate.cpp)
- [`projects/CapsuleMonsterChess/Source/Client/Lobby/LobbyLayer.cpp`](../projects/CapsuleMonsterChess/Source/Client/Lobby/LobbyLayer.cpp)
- [`projects/CapsuleMonsterChess/Source/Client/Lobby/LobbyProgression.cpp`](../projects/CapsuleMonsterChess/Source/Client/Lobby/LobbyProgression.cpp)
- [`projects/CapsuleMonsterChess/docs/AxmolModuleProfile.md`](../projects/CapsuleMonsterChess/docs/AxmolModuleProfile.md)
- [`axmol/scene/Node.h`](../axmol/scene/Node.h)
- [`axmol/ui/Widget.h`](../axmol/ui/Widget.h)
- [`axmol/ui/LayoutGroup.h`](../axmol/ui/LayoutGroup.h)
- [`axmol/ui/LayoutComponent.h`](../axmol/ui/LayoutComponent.h)
- [`axmol/2d/DrawNode.h`](../axmol/2d/DrawNode.h)
- [`axmol/2d/Label.h`](../axmol/2d/Label.h)
- [`axmol/2d/ParticleSystemQuad.h`](../axmol/2d/ParticleSystemQuad.h)

### 공식 문서

- [Unity 2021.3 Basic UI Layout](https://docs.unity3d.com/2021.3/Documentation/Manual/UIBasicLayout.html)
- [Unity Canvas Scaler](https://docs.unity3d.com/2021.3/Documentation/Manual/script-CanvasScaler.html)
- [Unity 2021.3 Grid Layout Group](https://docs.unity3d.com/2021.3/Documentation/Manual/script-GridLayoutGroup.html)
- [Unity 2021.3 Canvas](https://docs.unity3d.com/2021.3/Documentation/Manual/class-Canvas.html)
- [Unity Line Renderer](https://docs.unity3d.com/2021.3/Documentation/Manual/class-LineRenderer.html)
- [Axmol class index](https://axmol.dev/manual/latest/classes)
- [Axmol LayoutComponent](https://axmol.dev/manual/latest/d9/d2a/classax_1_1ui_1_1_layout_component.html)
- [Axmol Node](https://axmol.dev/manual/latest/df/da2/classax_1_1_node.html)

이 문서의 Unity/Axmol/Unreal 대응은 개념 비교다. 외부 엔진 코드를 복사했다는 의미가 아니다.
