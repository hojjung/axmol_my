#include "Client/Battle/BattleScene.h"

#include "Client/Rendering/DragonFixtureRenderer.h"
#include "MainScene.h"

#include "axmol/2d/DrawNode.h"
#include "axmol/2d/Light.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/platform/FileUtils.h"
#include "axmol/scene/CameraBackgroundBrush.h"
#include "axmol/ui/CocosGUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace ax;

constexpr std::string_view FONT_PATH            = "fonts/Marker Felt.ttf";
constexpr std::string_view MONSTER_TABLE_PATH   = "Data/Tables/monster_unit_table.json";
constexpr std::string_view HP_FRAME_PATH        = "UI/LobbyFeatures/Play/play_character_hp_bar_front.png";
constexpr std::string_view HP_GREEN_PATH        = "UI/LobbyFeatures/Play/play_character_hp_bar_green.png";
constexpr std::string_view HP_RED_PATH          = "UI/LobbyFeatures/Play/play_character_hp_bar_red.png";
constexpr std::string_view DECK_BACKGROUND_PATH = "UI/LobbyFeatures/Play/play_bottom_bg.png";
constexpr std::string_view AUTO_ICON_PATH       = "UI/LobbyFeatures/Play/play_btn_icon_auto.png";
constexpr std::string_view RESULT_STAR_PATH     = "UI/LobbyFeatures/Play_Continue_Pause_Result/result_star_large.png";
constexpr std::string_view REWARD_GOLD_PATH     = "UI/LobbyFeatures/Common/reward_icon_gold.png";
constexpr unsigned short WORLD_CAMERA_MASK      = static_cast<unsigned short>(CameraFlag::USER1);
constexpr int BOARD_COLUMNS                     = 5;
constexpr int CORE_BOARD_WIDTH                  = 10;
constexpr int CORE_BOARD_HEIGHT                 = 5;
constexpr int DRAGON_CATALOG_UNIT_ID            = 49;
constexpr float HEX_SPACING_RADIUS              = 0.60F;
constexpr float HEX_DRAW_RADIUS                 = 0.54F;
constexpr float SQRT_THREE                      = 1.7320508075688772F;
constexpr float REPLAY_SECONDS_PER_TICK         = 0.030F;
constexpr std::string_view REPLAY_EVENT_NAME    = "cmc.battle.replay";

struct ReplayDispatchPayload final
{
    const cmc::game_core::BattleLogEvent* event = nullptr;
    bool instant                                = false;
};

ui::Layout* makePanel(const Size& size, const Color32& color, uint8_t opacity = 255)
{
    auto* panel = ui::Layout::create();
    panel->setContentSize(size);
    panel->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(color);
    panel->setBackGroundColorOpacity(opacity);
    return panel;
}

ui::Text* makeText(std::string_view text, float size, const Color32& color)
{
    auto* label = ui::Text::create(text, FONT_PATH, size);
    label->setTextColor(color);
    return label;
}

ui::Layout* makeButton(const Size& size, std::string_view text, const Color32& color)
{
    auto* button = makePanel(size, color);
    button->setTouchEnabled(true);
    auto* edge = makePanel({size.width, 7.0F}, Color32{27, 49, 74, 255});
    button->addChild(edge);
    auto* label = makeText(text, 22.0F, Color32{255, 250, 229, 255});
    label->setPosition({size.width * 0.5F, size.height * 0.5F + 3.0F});
    button->addChild(label);
    return button;
}

void addScale9Asset(Node& parent, std::string_view path, const Size& size, int zOrder = 0)
{
    if (!FileUtils::getInstance()->isFileExist(path))
        return;
    auto* image = ui::ImageView::create(path);
    if (!image)
        return;
    image->setScale9Enabled(true);
    const Size sourceSize = image->getContentSize();
    image->setCapInsets(
        {sourceSize.width * 0.30F, sourceSize.height * 0.30F, sourceSize.width * 0.40F, sourceSize.height * 0.40F});
    image->setContentSize(size);
    image->setPosition(size * 0.5F);
    parent.addChild(image, zOrder);
}

ui::ImageView* addFittedAsset(Node& parent,
                              std::string_view path,
                              const Vec2& position,
                              const Size& bounds,
                              int zOrder = 0)
{
    if (!FileUtils::getInstance()->isFileExist(path))
        return nullptr;
    auto* image = ui::ImageView::create(path);
    if (!image)
        return nullptr;
    const Size sourceSize = image->getContentSize();
    if (sourceSize.width > 0.0F && sourceSize.height > 0.0F)
        image->setScale(std::min(bounds.width / sourceSize.width, bounds.height / sourceSize.height));
    image->setPosition(position);
    parent.addChild(image, zOrder);
    return image;
}

Vec3 hexPosition(int column, int row, float y = 0.0F)
{
    const float shiftedColumn = static_cast<float>(column) + ((row & 1) ? 0.5F : 0.0F) - 2.25F;
    return {SQRT_THREE * HEX_SPACING_RADIUS * shiftedColumn, y,
            (4.5F - static_cast<float>(row)) * 1.5F * HEX_SPACING_RADIUS};
}

Vec3 coreCellPosition(int cellIndex, float y = 0.03F)
{
    if (cellIndex < 0)
        return {};
    const int coreX = cellIndex % CORE_BOARD_WIDTH;
    const int coreZ = cellIndex / CORE_BOARD_WIDTH;
    return hexPosition(coreZ, coreX, y);
}

cmc::game_core::BattleSimulateRequest makeSimulationRequest(const cmc::client::BattlePresentationRequest& request,
                                                            const cmc::client::MonsterCatalogEntry* dragon)
{
    using namespace cmc::game_core;

    BattleSimulateRequest simulation;
    simulation.battleId         = "stage-" + std::to_string(request.stageId);
    simulation.seed             = 0x434D43 + request.stageId * 31 + request.stageNumber;
    simulation.tableVersion     = "bootstrap-1";
    simulation.board.width      = CORE_BOARD_WIDTH;
    simulation.board.height     = CORE_BOARD_HEIGHT;
    simulation.options.maxTicks = DEFAULT_MAX_TICKS;

    for (int teamIndex = 0; teamIndex < 2; ++teamIndex)
    {
        BattleTeamSetup team;
        team.teamId = teamIndex == 0 ? TeamId::Player : TeamId::Enemy;
        team.units.reserve(10);

        for (int unitIndex = 0; unitIndex < 10; ++unitIndex)
        {
            BattleUnitSetup unit;
            unit.unitId            = (teamIndex == 0 ? 101 : 201) + unitIndex;
            unit.monsterId         = dragon ? dragon->nameKey : "Drago1";
            unit.displayName       = teamIndex == 0 ? unit.monsterId : "Dummy " + unit.monsterId;
            unit.teamId            = team.teamId;
            unit.element           = ElementType::Fire;
            unit.role              = RoleType::Queen;
            unit.evolutionStage    = 1;
            unit.position.x        = teamIndex == 0 ? unitIndex / CORE_BOARD_HEIGHT : 8 + unitIndex / CORE_BOARD_HEIGHT;
            unit.position.z        = unitIndex % CORE_BOARD_HEIGHT;
            unit.stats.maxHp       = dragon ? dragon->hp : 660;
            unit.stats.attack      = dragon ? dragon->ad : 64;
            unit.stats.defense     = dragon ? dragon->adDefense : 29;
            unit.stats.attackRange = dragon ? dragon->range : 1;
            unit.stats.attackIntervalTicks = 16;
            unit.stats.moveIntervalTicks   = 5;
            unit.stats.energyOnAttack      = 12;
            unit.stats.energyOnHit         = 4;
            unit.stats.energyOnKill        = 25;
            team.units.push_back(std::move(unit));
        }
        simulation.teams.push_back(std::move(team));
    }
    return simulation;
}

MeshRenderer* createHexRegion(int firstRow, int lastRow, const Color& color)
{
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texCoords;
    IndexArray indices(rhi::IndexFormat::U_SHORT);
    const int cellCount = std::max(0, lastRow - firstRow) * BOARD_COLUMNS;
    positions.reserve(static_cast<size_t>(cellCount) * 7U * 3U);
    normals.reserve(static_cast<size_t>(cellCount) * 7U * 3U);
    texCoords.reserve(static_cast<size_t>(cellCount) * 7U * 2U);
    indices.reserve(static_cast<size_t>(cellCount) * 18U);

    for (int row = firstRow; row < lastRow; ++row)
    {
        for (int column = 0; column < BOARD_COLUMNS; ++column)
        {
            const Vec3 center = hexPosition(column, row, 0.0F);
            const auto base   = static_cast<uint16_t>(positions.size() / 3U);
            positions.insert(positions.end(), {center.x, center.y, center.z});
            normals.insert(normals.end(), {0.0F, 1.0F, 0.0F});
            texCoords.insert(texCoords.end(), {0.5F, 0.5F});

            for (int corner = 0; corner < 6; ++corner)
            {
                constexpr float PI = 3.14159265358979323846F;
                const float angle  = (30.0F + static_cast<float>(corner) * 60.0F) * PI / 180.0F;
                positions.insert(positions.end(), {center.x + std::cos(angle) * HEX_DRAW_RADIUS, center.y,
                                                   center.z + std::sin(angle) * HEX_DRAW_RADIUS});
                normals.insert(normals.end(), {0.0F, 1.0F, 0.0F});
                texCoords.insert(texCoords.end(), {0.5F + std::cos(angle) * 0.5F, 0.5F + std::sin(angle) * 0.5F});
            }

            for (uint16_t corner = 0; corner < 6; ++corner)
            {
                indices.push_back<uint16_t>(base);
                indices.push_back<uint16_t>(static_cast<uint16_t>(base + 1U + ((corner + 1U) % 6U)));
                indices.push_back<uint16_t>(static_cast<uint16_t>(base + 1U + corner));
            }
        }
    }

    StylizedMaterialDesc description;
    description.baseTexture       = Director::getInstance()->getTextureCache()->getWhiteTexture();
    description.baseColor         = color;
    description.diffuseTint       = Color::white;
    description.highlightColor    = Color{1.0F, 0.96F, 0.80F, 1.0F};
    description.shadowColor       = Color{0.12F, 0.18F, 0.22F, 1.0F};
    description.bandThreshold     = 0.62F;
    description.bandSoftness      = 0.18F;
    description.minimumBrightness = 0.18F;
    description.unlitStrength     = 0.30F;
    description.rimIntensity      = 0.0F;

    auto* renderer = MeshRenderer::create();
    renderer->addMesh(Mesh::create(positions, normals, texCoords, indices));
    renderer->setMaterial(StylizedMaterial::create(description));
    renderer->setCameraMask(WORLD_CAMERA_MASK);
    renderer->setCastShadow(false);
    renderer->setReceiveShadow(true);
    return renderer;
}

MeshRenderer* createGround()
{
    constexpr std::array<float, 12> POSITION_DATA = {-4.4F, -0.08F, -6.2F, 4.4F,  -0.08F, -6.2F,
                                                     4.4F,  -0.08F, 6.2F,  -4.4F, -0.08F, 6.2F};
    const std::vector<float> positions(POSITION_DATA.begin(), POSITION_DATA.end());
    const std::vector<float> normals   = {0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const std::vector<float> texCoords = {0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F, 1.0F};
    const IndexArray indices{uint16_t{0}, uint16_t{2}, uint16_t{1}, uint16_t{0}, uint16_t{3}, uint16_t{2}};

    StylizedMaterialDesc description;
    description.baseTexture       = Director::getInstance()->getTextureCache()->getWhiteTexture();
    description.baseColor         = Color{0.035F, 0.07F, 0.11F, 1.0F};
    description.minimumBrightness = 0.25F;
    description.unlitStrength     = 0.4F;
    description.rimIntensity      = 0.0F;

    auto* renderer = MeshRenderer::create();
    renderer->addMesh(Mesh::create(positions, normals, texCoords, indices));
    renderer->setMaterial(StylizedMaterial::create(description));
    renderer->setCameraMask(WORLD_CAMERA_MASK);
    renderer->setCastShadow(false);
    renderer->setReceiveShadow(false);
    return renderer;
}

ui::Layout* makeUnitCard(const cmc::client::MonsterCatalogEntry* entry, const Size& size, int slot)
{
    const std::array<Color32, 5> colors = {Color32{72, 77, 151, 255}, Color32{80, 125, 168, 255},
                                           Color32{184, 113, 48, 255}, Color32{76, 149, 102, 255},
                                           Color32{138, 75, 157, 255}};
    auto* card                          = makePanel(size, colors[static_cast<size_t>(slot) % colors.size()]);
    auto* top                           = makePanel({size.width, 7.0F}, Color32{101, 229, 218, 255});
    top->setPosition({0.0F, size.height - 7.0F});
    card->addChild(top);

    if (entry && !entry->icon.empty() && FileUtils::getInstance()->isFileExist(entry->icon))
    {
        auto* icon = ui::ImageView::create(entry->icon);
        icon->setAutoSize(false);
        icon->setContentSize({size.width - 12.0F, size.height - 43.0F});
        icon->setPosition({size.width * 0.5F, 24.0F + (size.height - 43.0F) * 0.5F});
        card->addChild(icon);
    }
    else
    {
        auto* fixture = makeText("DR", 24.0F, Color32{230, 246, 255, 255});
        fixture->setPosition({size.width * 0.5F, size.height * 0.57F});
        card->addChild(fixture);
    }

    auto* name = makeText(entry ? entry->nameKey : "EMPTY", 12.0F, Color32{255, 250, 231, 255});
    name->setPosition({size.width * 0.5F, 15.0F});
    card->addChild(name, 3);
    return card;
}

std::string_view executionModeName(cmc::BattleExecutionMode mode)
{
    return mode == cmc::BattleExecutionMode::Local ? "LOCAL" : "REMOTE";
}

void pauseNodeTree(Node& node)
{
    node.pause();
    for (auto* child : node.getChildren())
        pauseNodeTree(*child);
}
}  // namespace

namespace cmc::client
{
BattleScene* BattleScene::create(const BattlePresentationRequest& request)
{
    auto* scene = new BattleScene();
    if (scene->initWithRequest(request))
    {
        scene->autorelease();
        return scene;
    }
    delete scene;
    return nullptr;
}

void BattleScene::onEnter()
{
    Scene::onEnter();
    beginReplay();
}

bool BattleScene::initWithRequest(const BattlePresentationRequest& request)
{
    if (!Scene::init() || request.stageId <= 0 || request.stageNumber <= 0)
        return false;

    _request = request;
    std::string catalogError;
    if (!_monsterCatalog.load(MONSTER_TABLE_PATH, catalogError))
        AXLOGE("Battle monster catalog load failed: {}", catalogError);

    _simulationRequest = makeSimulationRequest(request, _monsterCatalog.findById(DRAGON_CATALOG_UNIT_ID));
    const game_core::BattleSimulator simulator;
    _simulationResult = simulator.simulate(_simulationRequest);

    buildWorld();
    buildInterface();

    auto* listener = CustomEventListener::create(REPLAY_EVENT_NAME, [this](CustomEvent* customEvent) {
        const auto* payload = static_cast<const ReplayDispatchPayload*>(customEvent->getUserData());
        if (payload && payload->event)
            replayEvent(*payload->event, payload->instant);
    });
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
    return true;
}

void BattleScene::buildWorld()
{
    auto* director = Director::getInstance();
    auto* uiCamera = getDefaultCamera();
    uiCamera->setBackgroundBrush(CameraBackgroundBrush::createNoneBrush());

    const Size canvas = director->getCanvasSize();
    auto* worldCamera = Camera::createPerspective(40.0F, canvas.width / canvas.height, 0.1F, 60.0F);
    worldCamera->setCameraFlag(CameraFlag::USER1);
    worldCamera->setDepth(-1);
    worldCamera->setPosition3D({0.0F, 12.7F, 10.8F});
    worldCamera->lookAt({0.0F, 0.0F, -0.15F});
    worldCamera->setBackgroundBrush(CameraBackgroundBrush::createColorBrush(Color{0.025F, 0.075F, 0.12F, 1.0F}, 1.0F));
    addChild(worldCamera);

    auto* mainLight = DirectionLight::create(Vec3{0.42F, -1.0F, -0.62F}, Color32{255, 226, 196, 255});
    mainLight->setCameraMask(WORLD_CAMERA_MASK);
    mainLight->setIntensity(1.1F);
    addChild(mainLight);

    auto* ambient = AmbientLight::create(Color32{132, 157, 184, 255});
    ambient->setCameraMask(WORLD_CAMERA_MASK);
    ambient->setIntensity(0.72F);
    addChild(ambient);

    StylizedRendererConfig rendererConfig;
    rendererConfig.qualityPreset                   = StylizedQualityPreset::Balanced;
    rendererConfig.reserveDefaultCameraForNativeUi = true;
    auto* stylized                                 = StylizedRenderer::attach(*this, rendererConfig);
    stylized->setMainLight(mainLight);

    _battleWorldRoot = Node::create();
    _battleWorldRoot->setCameraMask(WORLD_CAMERA_MASK);
    addChild(_battleWorldRoot);
    _battleWorldRoot->addChild(createGround());
    _battleWorldRoot->addChild(createHexRegion(0, 2, Color{0.08F, 0.23F, 0.43F, 1.0F}));
    _battleWorldRoot->addChild(createHexRegion(2, 8, Color{0.14F, 0.27F, 0.28F, 1.0F}));
    _battleWorldRoot->addChild(createHexRegion(8, 10, Color{0.43F, 0.12F, 0.16F, 1.0F}));

    int createdUnits = 0;
    for (const game_core::BattleTeamSetup& team : _simulationRequest.teams)
    {
        const bool enemy = team.teamId == game_core::TeamId::Enemy;
        for (const game_core::BattleUnitSetup& setup : team.units)
        {
            DragonFixtureStyle style;
            style.targetExtent = 0.96F;
            style.yawDegrees   = enemy ? 180.0F : 0.0F;
            style.diffuseTint  = enemy ? Color{1.0F, 0.58F, 0.54F, 1.0F} : Color{0.62F, 0.82F, 1.0F, 1.0F};
            style.rimColor     = enemy ? Color{1.0F, 0.35F, 0.28F, 1.0F} : Color{0.45F, 0.88F, 1.0F, 1.0F};

            std::string error;
            auto* dragon = createDragonFixture(style, error);
            if (!dragon)
            {
                AXLOGE("Battle dragon fixture {} failed: {}", setup.unitId, error);
                continue;
            }

            const int cellIndex = setup.position.x + setup.position.z * _simulationRequest.board.width;
            auto* unitRoot      = Node::create();
            unitRoot->setCameraMask(WORLD_CAMERA_MASK);
            unitRoot->setPosition3D(coreCellPosition(cellIndex));
            unitRoot->addChild(dragon);
            _battleWorldRoot->addChild(unitRoot);

            UnitPresentation presentation;
            presentation.root      = unitRoot;
            presentation.cellIndex = cellIndex;
            presentation.hp        = setup.stats.maxHp;
            presentation.maxHp     = std::max(1, setup.stats.maxHp);
            presentation.teamId    = team.teamId;
            presentation.alive     = true;
            _units.emplace(setup.unitId, presentation);
            ++createdUnits;
        }
    }
    AXLOGI("Battle simulation ready: stage={}, units={}/20, events={}, ticks={}, winner={}, mode={}", _request.stageId,
           createdUnits, _simulationResult.events.size(), _simulationResult.durationTicks,
           _simulationResult.hasWinner ? static_cast<int>(_simulationResult.winnerTeam) : -1,
           executionModeName(_request.executionMode));
}

void BattleScene::buildInterface()
{
    const Rect safeArea             = Director::getInstance()->getSafeAreaRect();
    const float left                = safeArea.origin.x;
    const float bottom              = safeArea.origin.y;
    const float width               = safeArea.size.width;
    const float top                 = bottom + safeArea.size.height;
    constexpr float TOP_BAR_HEIGHT  = 116.0F;
    constexpr float DECK_BAR_HEIGHT = 204.0F;

    auto* topBar = makePanel({width, TOP_BAR_HEIGHT}, Color32{9, 22, 39, 255}, 244);
    topBar->setPosition({left, top - TOP_BAR_HEIGHT});
    addChild(topBar, 100);

    auto* player = makeText("YOU  10", 23.0F, Color32{112, 222, 255, 255});
    player->setAnchorPoint({0.0F, 0.5F});
    player->setPosition({20.0F, 82.0F});
    topBar->addChild(player);
    _playerCountLabel = player;
    auto* playerTrack = makePanel({190.0F, 16.0F}, Color32{23, 32, 45, 255});
    playerTrack->setPosition({20.0F, 52.0F});
    topBar->addChild(playerTrack);
    auto* playerHealth = makePanel({176.0F, 12.0F}, Color32{67, 213, 120, 255});
    playerHealth->setAnchorPoint({0.0F, 0.0F});
    playerHealth->setPosition({2.0F, 2.0F});
    playerTrack->addChild(playerHealth);
    _playerHealthFill = playerHealth;
    addScale9Asset(*playerHealth, HP_GREEN_PATH, playerHealth->getContentSize(), 1);
    addScale9Asset(*playerTrack, HP_FRAME_PATH, playerTrack->getContentSize(), 3);

    auto* enemy = makeText("DUMMY  10", 23.0F, Color32{255, 132, 122, 255});
    enemy->setAnchorPoint({1.0F, 0.5F});
    enemy->setPosition({width - 20.0F, 82.0F});
    topBar->addChild(enemy);
    _enemyCountLabel = enemy;
    auto* enemyTrack = makePanel({190.0F, 16.0F}, Color32{23, 32, 45, 255});
    enemyTrack->setPosition({width - 210.0F, 52.0F});
    topBar->addChild(enemyTrack);
    auto* enemyHealth = makePanel({176.0F, 12.0F}, Color32{239, 83, 73, 255});
    enemyHealth->setAnchorPoint({1.0F, 0.0F});
    enemyHealth->setPosition({188.0F, 2.0F});
    enemyTrack->addChild(enemyHealth);
    _enemyHealthFill = enemyHealth;
    addScale9Asset(*enemyHealth, HP_RED_PATH, enemyHealth->getContentSize(), 1);
    addScale9Asset(*enemyTrack, HP_FRAME_PATH, enemyTrack->getContentSize(), 3);

    auto* stage = makeText("STAGE " + std::to_string(_request.stageNumber), 24.0F, Color32{255, 228, 132, 255});
    stage->setPosition({width * 0.5F, 83.0F});
    topBar->addChild(stage);
    auto* mode = makeText(std::string(executionModeName(_request.executionMode)) + "  -  AUTO", 14.0F,
                          Color32{166, 202, 218, 255});
    mode->setPosition({width * 0.5F, 51.0F});
    topBar->addChild(mode);
    auto* power = makeText("ENEMY POWER  " + std::to_string(_request.enemyPower), 13.0F, Color32{156, 178, 191, 255});
    power->setPosition({width * 0.5F, 25.0F});
    topBar->addChild(power);

    auto* skip = makeButton({88.0F, 42.0F}, "SKIP", Color32{50, 83, 112, 255});
    skip->setPosition({width - 100.0F, -50.0F});
    skip->addClickEventListener([this](Object*) { fastForwardReplay(); });
    topBar->addChild(skip);

    auto* fixtureBadge = makePanel({244.0F, 34.0F}, Color32{13, 31, 52, 255}, 224);
    fixtureBadge->setPosition({left + (width - 244.0F) * 0.5F, bottom + DECK_BAR_HEIGHT + 12.0F});
    addChild(fixtureBadge, 90);
    auto* fixtureText = makeText("DETERMINISTIC CORE  -  10 vs 10", 14.0F, Color32{199, 226, 236, 255});
    fixtureText->setPosition(fixtureBadge->getContentSize() * 0.5F);
    fixtureBadge->addChild(fixtureText);

    auto* deckBar = makePanel({width, DECK_BAR_HEIGHT}, Color32{10, 27, 48, 255}, 252);
    deckBar->setPosition({left, bottom});
    addChild(deckBar, 100);
    addScale9Asset(*deckBar, DECK_BACKGROUND_PATH, deckBar->getContentSize());

    auto* title = makeText("ACTIVE DECK", 18.0F, Color32{255, 231, 151, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({16.0F, DECK_BAR_HEIGHT - 20.0F});
    deckBar->addChild(title);
    auto* autoState = makeText("AUTO BATTLE  ON", 15.0F, Color32{114, 235, 150, 255});
    autoState->setAnchorPoint({1.0F, 0.5F});
    autoState->setPosition({width - 16.0F, DECK_BAR_HEIGHT - 20.0F});
    deckBar->addChild(autoState);
    addFittedAsset(*deckBar, AUTO_ICON_PATH, {width - 190.0F, DECK_BAR_HEIGHT - 20.0F}, {28.0F, 28.0F});

    constexpr float GAP   = 8.0F;
    const float cardWidth = (width - 24.0F - GAP * 4.0F) / 5.0F;
    const Size cardSize{cardWidth, 148.0F};
    for (int slot = 0; slot < 5; ++slot)
    {
        const MonsterCatalogEntry* entry = _monsterCatalog.findById(_request.playerUnitIds[static_cast<size_t>(slot)]);
        auto* card                       = makeUnitCard(entry, cardSize, slot);
        card->setPosition({12.0F + static_cast<float>(slot) * (cardWidth + GAP), 8.0F});
        deckBar->addChild(card);
    }

    updateTeamHud();
}

void BattleScene::beginReplay()
{
    if (_replayStarted)
        return;

    _replayStarted     = true;
    _nextReplayEvent   = 0;
    _replayTick        = 0;
    _replayAccumulator = 0.0F;
    scheduleUpdate();
    advanceReplayTick();
}

void BattleScene::update(float deltaSeconds)
{
    Scene::update(deltaSeconds);
    if (!_replayStarted || _resultRoot || _battleEndQueued)
        return;

    _replayAccumulator += std::clamp(deltaSeconds, 0.0F, 0.25F);
    while (_replayAccumulator >= REPLAY_SECONDS_PER_TICK && !_resultRoot && !_battleEndQueued)
    {
        _replayAccumulator -= REPLAY_SECONDS_PER_TICK;
        ++_replayTick;
        advanceReplayTick();
    }
}

void BattleScene::advanceReplayTick()
{
    if (_resultRoot || _battleEndQueued)
        return;

    while (_nextReplayEvent < _simulationResult.events.size())
    {
        const game_core::BattleLogEvent& event = _simulationResult.events[_nextReplayEvent];
        if (event.tick > _replayTick)
            break;
        ++_nextReplayEvent;
        dispatchReplayEvent(event, false);
        if (_battleEndQueued || _resultRoot)
            return;
    }

    if (_nextReplayEvent == _simulationResult.events.size())
    {
        _battleEndQueued = true;
        unscheduleUpdate();
        const bool victory = _simulationResult.hasWinner && _simulationResult.winnerTeam == game_core::TeamId::Player;
        scheduleOnce([this, victory](float) { showResult(victory); }, 0.35F, "cmc_battle_core_result");
    }
}

void BattleScene::dispatchReplayEvent(const game_core::BattleLogEvent& event, bool instant)
{
    ReplayDispatchPayload payload{&event, instant};
    _eventDispatcher->dispatchCustomEvent(REPLAY_EVENT_NAME, &payload);
}

void BattleScene::replayEvent(const game_core::BattleLogEvent& event, bool instant)
{
    auto findUnit = [this](int unitId) -> UnitPresentation* {
        const auto found = _units.find(unitId);
        return found == _units.end() ? nullptr : &found->second;
    };

    switch (event.type)
    {
    case game_core::BattleEventType::UnitSpawn:
        if (UnitPresentation* unit = findUnit(event.actorId))
        {
            unit->root->stopAllActions();
            unit->cellIndex = event.toCell;
            unit->hp        = event.amount > 0 ? event.amount : unit->maxHp;
            unit->alive     = true;
            unit->root->setScale(1.0F);
            unit->root->setVisible(true);
            unit->root->setPosition3D(coreCellPosition(event.toCell));
            updateTeamHud();
        }
        break;

    case game_core::BattleEventType::UnitMove:
        if (UnitPresentation* unit = findUnit(event.actorId))
        {
            unit->root->stopAllActions();
            unit->cellIndex        = event.toCell;
            const Vec3 destination = coreCellPosition(event.toCell);
            if (instant)
                unit->root->setPosition3D(destination);
            else
                unit->root->runAction(EaseSineInOut::create(MoveTo::create(0.13F, destination)));
        }
        break;

    case game_core::BattleEventType::UnitJump:
        if (UnitPresentation* unit = findUnit(event.actorId))
        {
            unit->root->stopAllActions();
            unit->cellIndex        = event.toCell;
            const Vec3 destination = coreCellPosition(event.toCell);
            if (instant)
            {
                unit->root->setPosition3D(destination);
            }
            else
            {
                Vec3 midpoint = (unit->root->getPosition3D() + destination) * 0.5F;
                midpoint.y += 0.62F;
                unit->root->runAction(Sequence::create(EaseSineOut::create(MoveTo::create(0.09F, midpoint)),
                                                       EaseSineIn::create(MoveTo::create(0.11F, destination)),
                                                       nullptr));
            }
        }
        break;

    case game_core::BattleEventType::BasicAttack:
    case game_core::BattleEventType::SkillCast:
        if (!instant)
        {
            UnitPresentation* actor  = findUnit(event.actorId);
            UnitPresentation* target = findUnit(event.targetId);
            if (actor && target && actor->alive)
            {
                actor->root->stopAllActions();
                const Vec3 home      = coreCellPosition(actor->cellIndex);
                const Vec3 delta     = target->root->getPosition3D() - home;
                const float distance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
                Vec3 lunge           = home;
                if (distance > 0.001F)
                {
                    lunge.x += delta.x * (0.22F / distance);
                    lunge.z += delta.z * (0.22F / distance);
                }
                actor->root->runAction(Sequence::create(EaseSineOut::create(MoveTo::create(0.055F, lunge)),
                                                        EaseSineIn::create(MoveTo::create(0.085F, home)), nullptr));
            }
        }
        break;

    case game_core::BattleEventType::Damage:
    case game_core::BattleEventType::Heal:
        if (UnitPresentation* unit = findUnit(event.targetId))
        {
            unit->hp = std::clamp(event.value, 0, unit->maxHp);
            updateTeamHud();
        }
        break;

    case game_core::BattleEventType::UnitDeath:
        if (UnitPresentation* unit = findUnit(event.targetId))
        {
            unit->hp    = 0;
            unit->alive = false;
            unit->root->stopAllActions();
            if (instant)
            {
                unit->root->setVisible(false);
            }
            else
            {
                Node* root = unit->root;
                root->runAction(Sequence::create(EaseBackIn::create(ScaleTo::create(0.18F, 0.08F)),
                                                 CallFunc::create([root] { root->setVisible(false); }), nullptr));
            }
            updateTeamHud();
        }
        break;

    case game_core::BattleEventType::BattleEnd:
    {
        _battleEndQueued = true;
        unscheduleUpdate();
        const bool victory = event.teamId == game_core::TeamId::Player;
        if (instant)
            showResult(victory);
        else
            scheduleOnce([this, victory](float) { showResult(victory); }, 0.35F, "cmc_battle_core_result");
        break;
    }

    default:
        break;
    }
}

void BattleScene::fastForwardReplay()
{
    if (_resultRoot)
        return;

    unscheduleUpdate();
    while (_nextReplayEvent < _simulationResult.events.size() && !_resultRoot)
    {
        dispatchReplayEvent(_simulationResult.events[_nextReplayEvent], true);
        ++_nextReplayEvent;
    }

    if (!_resultRoot)
    {
        _battleEndQueued   = true;
        const bool victory = _simulationResult.hasWinner && _simulationResult.winnerTeam == game_core::TeamId::Player;
        showResult(victory);
    }
}

void BattleScene::updateTeamHud()
{
    int playerAlive = 0;
    int enemyAlive  = 0;
    int playerHp    = 0;
    int enemyHp     = 0;
    int playerMaxHp = 0;
    int enemyMaxHp  = 0;

    for (const auto& entry : _units)
    {
        const UnitPresentation& unit = entry.second;
        const int hp                 = std::clamp(unit.hp, 0, unit.maxHp);
        if (unit.teamId == game_core::TeamId::Player)
        {
            playerAlive += unit.alive ? 1 : 0;
            playerHp += hp;
            playerMaxHp += unit.maxHp;
        }
        else
        {
            enemyAlive += unit.alive ? 1 : 0;
            enemyHp += hp;
            enemyMaxHp += unit.maxHp;
        }
    }

    if (_playerCountLabel)
        static_cast<ui::Text*>(_playerCountLabel)->setString("YOU  " + std::to_string(playerAlive));
    if (_enemyCountLabel)
        static_cast<ui::Text*>(_enemyCountLabel)->setString("DUMMY  " + std::to_string(enemyAlive));
    if (_playerHealthFill)
        _playerHealthFill->setScaleX(playerMaxHp > 0 ? static_cast<float>(playerHp) / static_cast<float>(playerMaxHp)
                                                     : 0.0F);
    if (_enemyHealthFill)
        _enemyHealthFill->setScaleX(enemyMaxHp > 0 ? static_cast<float>(enemyHp) / static_cast<float>(enemyMaxHp)
                                                   : 0.0F);
}

void BattleScene::showResult(bool victory)
{
    if (_resultRoot)
        return;

    unscheduleUpdate();
    unschedule("cmc_battle_core_result");
    if (_battleWorldRoot)
        pauseNodeTree(*_battleWorldRoot);

    const Rect safeArea = Director::getInstance()->getSafeAreaRect();
    const float left    = safeArea.origin.x;
    const float bottom  = safeArea.origin.y;
    const float width   = safeArea.size.width;
    const float height  = safeArea.size.height;

    _resultRoot = Node::create();
    addChild(_resultRoot, 1000);
    auto* shield = makePanel(safeArea.size, Color32{2, 8, 18, 255}, 210);
    shield->setPosition(safeArea.origin);
    shield->setTouchEnabled(true);
    _resultRoot->addChild(shield, -10);

    const float panelWidth = width - 48.0F;
    auto* panel            = makePanel({panelWidth, 690.0F}, Color32{18, 37, 62, 255}, 250);
    panel->setPosition({left + 24.0F, bottom + (height - 690.0F) * 0.5F});
    _resultRoot->addChild(panel);

    auto* enemyPanel = makePanel({panelWidth - 32.0F, 112.0F}, Color32{128, 37, 65, 255});
    enemyPanel->setPosition({16.0F, 546.0F});
    panel->addChild(enemyPanel);
    auto* enemyStatus = makeText(victory ? "TRAINING DUMMY  -  DEFEATED" : "TRAINING DUMMY  -  WINNER", 22.0F,
                                 Color32{255, 222, 225, 255});
    enemyStatus->setPosition(enemyPanel->getContentSize() * 0.5F);
    enemyPanel->addChild(enemyStatus);

    auto* versus = makeText("VS", 20.0F, Color32{196, 215, 225, 255});
    versus->setPosition({panelWidth * 0.5F, 520.0F});
    panel->addChild(versus);
    auto* outcome = makeText(victory ? "VICTORY!" : "DEFEAT", 48.0F,
                             victory ? Color32{88, 228, 255, 255} : Color32{255, 114, 107, 255});
    outcome->setPosition({panelWidth * 0.5F, 472.0F});
    panel->addChild(outcome);
    addFittedAsset(*panel, RESULT_STAR_PATH, {panelWidth * 0.5F - 150.0F, 472.0F}, {70.0F, 70.0F});

    auto* playerPanel = makePanel({panelWidth - 32.0F, 112.0F}, Color32{31, 95, 156, 255});
    playerPanel->setPosition({16.0F, 332.0F});
    panel->addChild(playerPanel);
    auto* playerStatus =
        makeText(victory ? "YOU  -  STAGE CLEAR" : "YOU  -  TRY AGAIN", 25.0F, Color32{234, 250, 255, 255});
    playerStatus->setPosition(playerPanel->getContentSize() * 0.5F);
    playerPanel->addChild(playerStatus);

    auto* rewardPanel = makePanel({panelWidth - 88.0F, 126.0F}, Color32{76, 55, 36, 255});
    rewardPanel->setPosition({44.0F, 180.0F});
    panel->addChild(rewardPanel);
    auto* rewardTitle = makeText(victory ? "REWARD PREVIEW" : "NO REWARD", 19.0F, Color32{255, 222, 123, 255});
    rewardTitle->setPosition({rewardPanel->getContentSize().width * 0.5F, 90.0F});
    rewardPanel->addChild(rewardTitle);
    auto* reward = makeText(victory ? "GOLD  +" + std::to_string(_request.rewardGold) : "RETURN AND GROW STRONGER",
                            28.0F, Color32{255, 241, 190, 255});
    reward->setPosition({rewardPanel->getContentSize().width * 0.5F + (victory ? 24.0F : 0.0F), 54.0F});
    rewardPanel->addChild(reward);
    if (victory)
        addFittedAsset(*rewardPanel, REWARD_GOLD_PATH, {52.0F, 54.0F}, {58.0F, 58.0F});
    const std::string checksum =
        _simulationResult.checksum.substr(0, std::min<std::size_t>(8U, _simulationResult.checksum.size()));
    auto* authority =
        makeText(std::string(executionModeName(_request.executionMode)) + " CORE  -  CHECKSUM " + checksum, 12.0F,
                 Color32{174, 178, 181, 255});
    authority->setPosition({rewardPanel->getContentSize().width * 0.5F, 20.0F});
    rewardPanel->addChild(authority);

    auto* ok = makeButton({230.0F, 76.0F}, "OK", Color32{54, 157, 220, 255});
    ok->setPosition({(panelWidth - 230.0F) * 0.5F, 74.0F});
    ok->addClickEventListener([this](Object*) { returnToLobby(); });
    panel->addChild(ok);
}

void BattleScene::returnToLobby()
{
    if (_transitionQueued)
        return;
    _transitionQueued = true;

    Director::getInstance()->postTask([] {
        auto* scene = utils::createInstance<MainScene>();
        Director::getInstance()->replaceScene(scene);
    }, Director::TaskTiming::FrameBoundary);
}
}  // namespace cmc::client
