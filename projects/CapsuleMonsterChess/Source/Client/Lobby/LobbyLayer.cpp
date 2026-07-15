#include "Client/Lobby/LobbyLayer.h"

#include "axmol/2d/DrawNode.h"
#include "axmol/platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace
{
using namespace ax;

constexpr std::string_view FONT_PATH              = "fonts/Marker Felt.ttf";
constexpr std::string_view MONSTER_TABLE_PATH     = "Data/Tables/monster_unit_table.json";
constexpr std::string_view PROGRESSION_TABLE_PATH = "Data/Tables/lobby_progression_table.json";
constexpr std::string_view STAGE_TABLE_PATH       = "Data/Tables/story_stage_table.json";
constexpr std::string_view LOBBY_QUEST_ICON_PATH  = "UI/LobbyFeatures/Lobby/lobby_sub_icon_mission.png";
constexpr std::string_view LOBBY_PASS_ICON_PATH   = "UI/LobbyFeatures/Lobby/lobby_pass_icon.png";
constexpr std::string_view LOBBY_MENU_ICON_PATH   = "UI/LobbyFeatures/_Icons/Pictoicons/256/btn_icon_menu_0.png";
constexpr float BOTTOM_NAVIGATION_HEIGHT          = 94.0F;
constexpr std::size_t STAGES_PER_PAGE             = 20;

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

    auto* lowerEdge = makePanel({size.width, 8.0F}, Color32{87, 50, 24, 255});
    lowerEdge->setPosition({0.0F, 0.0F});
    button->addChild(lowerEdge);

    auto* label = makeText(text, 29.0F, Color32{255, 249, 222, 255});
    label->setPosition({size.width * 0.5F, size.height * 0.5F + 4.0F});
    button->addChild(label);
    return button;
}

void addResourcePill(Node& root,
                     const Vec2& position,
                     const Size& size,
                     std::string_view icon,
                     std::string_view value,
                     const Color32& accent)
{
    auto* pill = makePanel(size, Color32{18, 27, 48, 255}, 244);
    pill->setPosition(position);
    root.addChild(pill);

    auto* iconPanel = makePanel({size.height - 10.0F, size.height - 10.0F}, accent);
    iconPanel->setPosition({5.0F, 5.0F});
    pill->addChild(iconPanel);

    auto* iconText = makeText(icon, 18.0F, Color32{255, 255, 255, 255});
    iconText->setPosition(iconPanel->getContentSize() * 0.5F);
    iconPanel->addChild(iconText);

    auto* valueText = makeText(value, 21.0F, Color32{255, 249, 230, 255});
    valueText->setAnchorPoint({1.0F, 0.5F});
    valueText->setPosition({size.width - 10.0F, size.height * 0.5F});
    pill->addChild(valueText);
}

DrawNode* makeBackdrop(const Size& size, bool opaque)
{
    auto* backdrop    = DrawNode::create();
    const float alpha = opaque ? 1.0F : 0.72F;
    backdrop->drawSolidRect({0.0F, 0.0F}, {size.width, size.height}, Color{0.035F, 0.12F, 0.20F, alpha});

    constexpr float cellSize = 92.0F;
    const Color lineColor{0.20F, 0.50F, 0.62F, opaque ? 0.18F : 0.12F};
    for (float x = -size.height; x < size.width + size.height; x += cellSize)
    {
        backdrop->drawLine({x, 0.0F}, {x + size.height, size.height}, lineColor, 2.0F);
        backdrop->drawLine({x, 0.0F}, {x - size.height, size.height}, lineColor, 2.0F);
    }
    return backdrop;
}

Color32 elementColor(std::string_view element)
{
    if (element == "Fire")
        return {239, 92, 72, 255};
    if (element == "Water")
        return {65, 151, 235, 255};
    if (element == "Grass")
        return {74, 186, 116, 255};
    if (element == "Light")
        return {245, 194, 73, 255};
    return {142, 92, 206, 255};
}

Color32 cardColor(const Color32& accent)
{
    return {static_cast<uint8_t>(34 + accent.r / 7), static_cast<uint8_t>(31 + accent.g / 8),
            static_cast<uint8_t>(52 + accent.b / 7), 255};
}

std::string makeMonogram(std::string_view nameKey)
{
    std::string result;
    for (const unsigned char character : nameKey)
    {
        if (!std::isalpha(character))
            continue;
        result.push_back(static_cast<char>(std::toupper(character)));
        if (result.size() == 2)
            break;
    }
    return result.empty() ? "?" : result;
}

std::string formatCount(std::int64_t value)
{
    std::string text = std::to_string(value);
    for (std::ptrdiff_t position = static_cast<std::ptrdiff_t>(text.size()) - 3; position > 0; position -= 3)
        text.insert(static_cast<std::size_t>(position), ",");
    return text;
}

ui::Layout* makeUnitArtwork(const cmc::client::MonsterCatalogEntry& entry, const Size& size, bool locked)
{
    auto* artwork         = makePanel(size, cardColor(elementColor(entry.element)));
    const auto* fileUtils = FileUtils::getInstance();
    if (!entry.icon.empty() && fileUtils->isFileExist(entry.icon))
    {
        auto* icon = ui::ImageView::create(entry.icon);
        icon->setAutoSize(false);
        icon->setContentSize(size);
        icon->setPosition(size * 0.5F);
        artwork->addChild(icon);
    }
    else
    {
        auto* monogram = makeText(makeMonogram(entry.nameKey), std::min(size.width, size.height) * 0.32F,
                                  Color32{236, 230, 250, 220});
        monogram->setPosition(size * 0.5F);
        artwork->addChild(monogram);
    }

    if (locked)
    {
        auto* shade = makePanel(size, Color32{8, 12, 22, 255}, 188);
        artwork->addChild(shade, 10);
        auto* lock = makeText("LOCKED", 15.0F, Color32{170, 177, 190, 255});
        lock->setPosition(size * 0.5F);
        shade->addChild(lock);
    }
    return artwork;
}

DrawNode* makeCapsuleEgg(float size)
{
    auto* egg = DrawNode::create();
    egg->setContentSize({size, size});

    const Vec2 center{size * 0.5F, size * 0.50F};
    egg->drawSolidCircle(center, size * 0.42F, Color{1.0F, 0.78F, 0.17F, 0.12F});
    egg->drawSolidCircle(center, size * 0.34F, 0.0F, 64, 0.82F, 1.18F, Color{0.99F, 0.78F, 0.22F, 1.0F}, 5.0F,
                         Color{1.0F, 0.94F, 0.62F, 1.0F});
    egg->drawSolidCircle({size * 0.42F, size * 0.63F}, size * 0.07F, Color{1.0F, 0.95F, 0.68F, 0.90F});
    egg->drawSolidCircle({size * 0.61F, size * 0.45F}, size * 0.055F, Color{0.74F, 0.35F, 0.84F, 0.92F});
    egg->drawSolidCircle({size * 0.40F, size * 0.34F}, size * 0.045F, Color{0.32F, 0.73F, 0.95F, 0.90F});
    return egg;
}

bool validateUserProfileCatalog(const cmc::client::UserProfile& profile,
                                const cmc::client::MonsterCatalog& catalog,
                                std::string& error)
{
    for (const auto& unit : profile.ownedUnits)
    {
        if (!catalog.findById(unit.unitId))
        {
            error = "User profile references unknown unitId " + std::to_string(unit.unitId);
            return false;
        }
    }
    return true;
}

bool reconcileLocalProfileProgression(cmc::client::UserProfile& profile, const cmc::client::ProgressionCatalog& catalog)
{
    bool changed                 = false;
    bool rebuildQuestProgress    = profile.questProgress.size() != catalog.quests().size();
    const auto& questDefinitions = catalog.quests();
    const auto& battlePass       = catalog.battlePass();

    if (!rebuildQuestProgress)
    {
        rebuildQuestProgress =
            std::any_of(questDefinitions.begin(), questDefinitions.end(),
                        [&profile](const auto& quest) { return profile.findQuestProgress(quest.questId) == nullptr; });
    }

    if (rebuildQuestProgress)
    {
        std::vector<cmc::client::QuestProgressState> reconciled;
        reconciled.reserve(questDefinitions.size());
        for (const auto& quest : questDefinitions)
        {
            if (const auto* existing = profile.findQuestProgress(quest.questId))
                reconciled.emplace_back(*existing);
            else
                reconciled.push_back({quest.questId, 0, false});
        }
        profile.questProgress = std::move(reconciled);
        changed               = true;
    }

    if (profile.battlePass.seasonId != battlePass.seasonId)
    {
        profile.battlePass = {battlePass.seasonId, 0, false, {}, {}};
        return true;
    }

    const auto tierExists = [&battlePass](int tierId) {
        return std::any_of(battlePass.tiers.begin(), battlePass.tiers.end(),
                           [tierId](const auto& tier) { return tier.tier == tierId; });
    };
    const auto removeUnknownTiers = [&changed, &tierExists](std::vector<int>& tierIds) {
        const auto newEnd =
            std::remove_if(tierIds.begin(), tierIds.end(), [&tierExists](int tierId) { return !tierExists(tierId); });
        if (newEnd != tierIds.end())
        {
            tierIds.erase(newEnd, tierIds.end());
            changed = true;
        }
    };
    removeUnknownTiers(profile.battlePass.claimedFreeTierIds);
    removeUnknownTiers(profile.battlePass.claimedPremiumTierIds);
    return changed;
}

bool validateUserProfileProgression(const cmc::client::UserProfile& profile,
                                    const cmc::client::ProgressionCatalog& catalog,
                                    std::string& error)
{
    if (profile.questProgress.size() != catalog.quests().size())
    {
        error = "User profile quest progress does not match the progression table";
        return false;
    }

    for (const auto& progress : profile.questProgress)
    {
        const auto* quest = catalog.findQuest(progress.questId);
        if (!quest)
        {
            error = "User profile references unknown questId " + std::to_string(progress.questId);
            return false;
        }
        if (progress.rewardClaimed && progress.progress < quest->target)
        {
            error = "User profile claims an incomplete questId " + std::to_string(progress.questId);
            return false;
        }
    }

    for (const auto& quest : catalog.quests())
    {
        if (!profile.findQuestProgress(quest.questId))
        {
            error = "User profile is missing questId " + std::to_string(quest.questId);
            return false;
        }
    }

    const cmc::client::BattlePassDefinition& battlePass = catalog.battlePass();
    if (profile.battlePass.seasonId != battlePass.seasonId)
    {
        error = "User profile battle pass season does not match the progression table";
        return false;
    }

    const auto tierExists = [&battlePass](int claimedTier) {
        return std::any_of(battlePass.tiers.begin(), battlePass.tiers.end(),
                           [claimedTier](const auto& tier) { return tier.tier == claimedTier; });
    };
    const std::int64_t availableTier =
        std::min<std::int64_t>(battlePass.tiers.back().tier,
                               static_cast<std::int64_t>(profile.battlePass.points) / battlePass.pointsPerTier + 1);
    for (const int claimedTier : profile.battlePass.claimedFreeTierIds)
    {
        if (!tierExists(claimedTier) || claimedTier > availableTier)
        {
            error = "User profile references unavailable free battle pass tier " + std::to_string(claimedTier);
            return false;
        }
    }
    for (const int claimedTier : profile.battlePass.claimedPremiumTierIds)
    {
        if (!tierExists(claimedTier) || claimedTier > availableTier)
        {
            error = "User profile references unavailable premium battle pass tier " + std::to_string(claimedTier);
            return false;
        }
    }
    if (!profile.battlePass.premiumUnlocked && !profile.battlePass.claimedPremiumTierIds.empty())
    {
        error = "User profile contains premium rewards without an unlocked battle pass";
        return false;
    }
    return true;
}
}  // namespace

namespace cmc::client
{
bool LobbyLayer::init()
{
    if (!Layer::init())
        return false;

    _safeArea = Director::getInstance()->getSafeAreaRect();

    if (!_monsterCatalog.load(MONSTER_TABLE_PATH, _monsterLoadError))
        AXLOGE("Monster catalog load failed: {}", _monsterLoadError);
    else
        AXLOGI("Monster catalog ready: units={}, icons={}", _monsterCatalog.entries().size(),
               _monsterCatalog.iconCount());

    if (!_progressionCatalog.load(PROGRESSION_TABLE_PATH, _progressionLoadError))
        AXLOGE("Progression catalog load failed: {}", _progressionLoadError);
    else
        AXLOGI("Progression catalog ready: quests={}, season={}, tiers={}", _progressionCatalog.quests().size(),
               _progressionCatalog.battlePass().seasonId, _progressionCatalog.battlePass().tiers.size());

    if (!_stageCatalog.load(STAGE_TABLE_PATH, _stageLoadError))
        AXLOGE("Stage catalog load failed: {}", _stageLoadError);
    else
    {
        AXLOGI("Stage catalog ready: chapter={}, stages={}", _stageCatalog.chapterName(),
               _stageCatalog.entries().size());
        const std::size_t selectedIndex = std::min(_clearedStageCount, _stageCatalog.entries().size() - 1);
        _selectedStageId                = _stageCatalog.entries()[selectedIndex].stageId;
        _selectedStageNumber            = _stageCatalog.entries()[selectedIndex].stageNumber;
    }

    _userDataSource = std::make_unique<LocalJsonUserDataSource>();
    UserProfile candidate;
    if (!_userDataSource->load(candidate, _userDataError))
        AXLOGE("User profile load failed: {}", _userDataError);
    else
    {
        const bool progressionReconciled = _progressionLoadError.empty() && _userDataSource->isWritable() &&
                                           reconcileLocalProfileProgression(candidate, _progressionCatalog);
        const bool monsterProfileValid =
            !_monsterLoadError.empty() || validateUserProfileCatalog(candidate, _monsterCatalog, _userDataError);
        const bool progressionProfileValid =
            !_progressionLoadError.empty() ||
            validateUserProfileProgression(candidate, _progressionCatalog, _userDataError);
        if (!monsterProfileValid || !progressionProfileValid)
            AXLOGE("User profile validation failed: {}", _userDataError);
        else
        {
            _userProfile = std::move(candidate);
            if (progressionReconciled && !_userDataSource->save(_userProfile, _userDataError))
                AXLOGE("Reconciled user profile save failed: {}", _userDataError);
            else
                _userDataError.clear();

            AXLOGI("User profile ready: owned={}, preset={}, quests={}", _userProfile.ownedUnits.size(),
                   _userProfile.selectedPresetIndex + 1, _userProfile.questProgress.size());
        }
    }

    showMainLobby();
    return true;
}

bool LobbyLayer::setUserDataSource(std::unique_ptr<IUserDataSource> source, std::string& error)
{
    error.clear();
    if (!source)
    {
        error = "User data source cannot be null";
        return false;
    }

    UserProfile profile;
    if (!source->load(profile, error))
        return false;
    if (!_monsterLoadError.empty())
    {
        error = "Monster catalog must be loaded before user data";
        return false;
    }
    if (!validateUserProfileCatalog(profile, _monsterCatalog, error))
        return false;
    if (!_progressionLoadError.empty())
    {
        error = "Progression catalog must be loaded before user data";
        return false;
    }
    const bool progressionReconciled =
        source->isWritable() && reconcileLocalProfileProgression(profile, _progressionCatalog);
    if (!validateUserProfileProgression(profile, _progressionCatalog, error))
        return false;
    if (progressionReconciled && !source->save(profile, error))
        return false;

    _userDataSource = std::move(source);
    _userProfile    = std::move(profile);
    _userDataError.clear();
    showMainLobby();
    return true;
}

bool LobbyLayer::saveUserProfile()
{
    if (!_userDataSource || !_userDataSource->isWritable())
    {
        AXLOGW("User profile source is read-only; server command required");
        return false;
    }

    if (!_userDataSource->save(_userProfile, _userDataError))
    {
        AXLOGE("User profile save failed: {}", _userDataError);
        return false;
    }
    return true;
}

void LobbyLayer::setWorldVisibilityCallback(std::function<void(bool)> callback)
{
    _worldVisibilityCallback = std::move(callback);
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(true);
}

void LobbyLayer::setBattleLaunchCallback(std::function<void(const BattlePresentationRequest&)> callback)
{
    _battleLaunchCallback = std::move(callback);
}

Node& LobbyLayer::replaceScreen()
{
    closeOverlay();
    unschedule("cmc_gacha_reveal");

    if (_screenRoot)
    {
        _screenRoot->removeFromParent();
        _screenRoot = nullptr;
    }

    _stageSelectionText = nullptr;
    _stageStartText     = nullptr;
    _selectedStageCard  = nullptr;

    _screenRoot = Node::create();
    addChild(_screenRoot);
    return *_screenRoot;
}

void LobbyLayer::showMainLobby()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(true);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float top    = bottom + _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, false);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* resourceBar = makePanel({width, 78.0F}, Color32{11, 26, 45, 255}, 236);
    resourceBar->setPosition({left, top - 78.0F});
    root.addChild(resourceBar, 10);

    auto* levelBadge = makePanel({54.0F, 54.0F}, Color32{57, 174, 205, 255});
    levelBadge->setPosition({12.0F, 12.0F});
    resourceBar->addChild(levelBadge);
    auto* level = makeText("1", 27.0F, Color32{255, 255, 255, 255});
    level->setPosition(levelBadge->getContentSize() * 0.5F);
    levelBadge->addChild(level);

    auto* xpTrack = makePanel({126.0F, 24.0F}, Color32{12, 19, 34, 255});
    xpTrack->setPosition({77.0F, 27.0F});
    resourceBar->addChild(xpTrack);
    auto* xpFill = makePanel({26.0F, 24.0F}, Color32{70, 191, 214, 255});
    xpTrack->addChild(xpFill);
    auto* xpText = makeText("0 / 20", 15.0F, Color32{255, 255, 255, 255});
    xpText->setPosition(xpTrack->getContentSize() * 0.5F);
    xpTrack->addChild(xpText);

    addResourcePill(*resourceBar, {width - 344.0F, 12.0F}, {154.0F, 54.0F}, "G",
                    formatCount(_userProfile.currencies.gold), Color32{236, 174, 54, 255});
    addResourcePill(*resourceBar, {width - 180.0F, 12.0F}, {168.0F, 54.0F}, "C",
                    formatCount(_userProfile.currencies.gems), Color32{71, 199, 117, 255});

    auto* profile = makePanel({width - 24.0F, 72.0F}, Color32{20, 54, 82, 255}, 240);
    profile->setPosition({left + 12.0F, top - 160.0F});
    root.addChild(profile, 10);

    auto* portrait = makePanel({58.0F, 58.0F}, Color32{56, 116, 148, 255});
    portrait->setPosition({7.0F, 7.0F});
    profile->addChild(portrait);
    auto* portraitText = makeText("YOU", 15.0F, Color32{255, 255, 255, 255});
    portraitText->setPosition(portrait->getContentSize() * 0.5F);
    portrait->addChild(portraitText);

    auto* playerName = makeText("YOU", 24.0F, Color32{255, 248, 224, 255});
    playerName->setAnchorPoint({0.0F, 0.5F});
    playerName->setPosition({78.0F, 46.0F});
    profile->addChild(playerName);
    auto* clan = makeText("NO CLAN", 14.0F, Color32{164, 195, 211, 255});
    clan->setAnchorPoint({0.0F, 0.5F});
    clan->setPosition({78.0F, 21.0F});
    profile->addChild(clan);

    const auto addShortcutButton = [profile](float x, std::string_view iconPath, std::string_view glyph,
                                             std::string_view label, const Color32& color) {
        constexpr Size buttonSize{58.0F, 58.0F};
        auto* button = makePanel(buttonSize, color);
        button->setPosition({x, 7.0F});
        button->setTouchEnabled(true);
        profile->addChild(button);

        auto* lowerEdge = makePanel({buttonSize.width, 5.0F}, Color32{40, 45, 68, 255});
        button->addChild(lowerEdge);

        ui::ImageView* image = nullptr;
        if (!iconPath.empty() && FileUtils::getInstance()->isFileExist(iconPath))
        {
            image = ui::ImageView::create(iconPath);
            if (image)
            {
                const Size originalSize   = image->getContentSize();
                constexpr float maxWidth  = 34.0F;
                constexpr float maxHeight = 32.0F;
                if (originalSize.width > 0.0F && originalSize.height > 0.0F)
                    image->setScale(std::min(maxWidth / originalSize.width, maxHeight / originalSize.height));
                image->setPosition({buttonSize.width * 0.5F, 36.0F});
                button->addChild(image);
            }
        }
        if (!image)
        {
            auto* icon = makeText(glyph, 20.0F, Color32{255, 248, 225, 255});
            icon->setPosition({buttonSize.width * 0.5F, 36.0F});
            button->addChild(icon);
        }

        auto* caption = makeText(label, 9.0F, Color32{255, 248, 225, 255});
        caption->setPosition({buttonSize.width * 0.5F, 13.0F});
        button->addChild(caption);
        return button;
    };

    const float profileWidth = profile->getContentSize().width;
    auto* questButton =
        addShortcutButton(profileWidth - 202.0F, LOBBY_QUEST_ICON_PATH, "Q", "QUEST", Color32{169, 68, 124, 255});
    questButton->addClickEventListener([this](Object*) { showQuestOverlay(_questTab); });

    auto* passButton =
        addShortcutButton(profileWidth - 138.0F, LOBBY_PASS_ICON_PATH, "P", "PASS", Color32{210, 145, 36, 255});
    passButton->addClickEventListener([this](Object*) { showBattlePass(); });

    auto* menuButton =
        addShortcutButton(profileWidth - 74.0F, LOBBY_MENU_ICON_PATH, "III", "MENU", Color32{54, 119, 159, 255});
    menuButton->addClickEventListener([this](Object*) { showSubMenuOverlay(); });

    auto* trainingTitle = makeText("TRAINING PROGRESS", 26.0F, Color32{91, 217, 205, 255});
    trainingTitle->setPosition({left + width * 0.5F, top - 200.0F});
    root.addChild(trainingTitle, 12);

    auto* progressTrack = makePanel({width - 220.0F, 34.0F}, Color32{30, 29, 34, 255}, 245);
    progressTrack->setPosition({left + 110.0F, top - 248.0F});
    root.addChild(progressTrack, 12);
    auto* progressFill = makePanel({(width - 220.0F) * 0.34F, 34.0F}, Color32{58, 185, 176, 255});
    progressTrack->addChild(progressFill);
    auto* progressText = makeText("2 / 5   DEFENSE", 17.0F, Color32{255, 255, 255, 255});
    progressText->setPosition(progressTrack->getContentSize() * 0.5F);
    progressTrack->addChild(progressText);

    auto* heroCaption = makePanel({width - 72.0F, 62.0F}, Color32{16, 30, 55, 255}, 145);
    heroCaption->setPosition({left + 36.0F, top - 330.0F});
    root.addChild(heroCaption, 8);
    auto* deckName = makeText("DRAGON TRAINING DECK", 23.0F, Color32{250, 242, 220, 255});
    deckName->setPosition(heroCaption->getContentSize() * 0.5F);
    heroCaption->addChild(deckName);

    auto* battleButton = makeButton({260.0F, 96.0F}, "BATTLE", Color32{232, 155, 28, 255});
    battleButton->setPosition({left + (width - 260.0F) * 0.5F, bottom + 298.0F});
    battleButton->addClickEventListener([this](Object*) { showStageSelect(); });
    root.addChild(battleButton, 20);

    auto* battleHint = makeText("STORY STAGES", 16.0F, Color32{202, 219, 227, 255});
    battleHint->setPosition({left + width * 0.5F, bottom + 282.0F});
    root.addChild(battleHint, 20);

    constexpr float chestGap = 10.0F;
    const float chestWidth   = (width - 30.0F - chestGap * 3.0F) / 4.0F;
    for (int index = 0; index < 4; ++index)
    {
        const bool ready = index == 0;
        auto* chest =
            makePanel({chestWidth, 132.0F}, ready ? Color32{34, 112, 159, 255} : Color32{27, 66, 94, 255}, 238);
        chest->setPosition({left + 15.0F + static_cast<float>(index) * (chestWidth + chestGap),
                            bottom + BOTTOM_NAVIGATION_HEIGHT + 10.0F});
        root.addChild(chest, 12);

        auto* chestTitle = makeText(ready ? "READY" : "CHEST SLOT", 16.0F,
                                    ready ? Color32{255, 239, 151, 255} : Color32{134, 167, 186, 255});
        chestTitle->setPosition({chestWidth * 0.5F, 91.0F});
        chest->addChild(chestTitle);
        auto* chestDetail = makeText(ready ? "5 SEC" : "EMPTY", 18.0F, Color32{235, 241, 244, 255});
        chestDetail->setPosition({chestWidth * 0.5F, 55.0F});
        chest->addChild(chestDetail);
        auto* chestGlyph = makeText(ready ? "BOX" : "+", ready ? 18.0F : 32.0F, Color32{113, 211, 224, 255});
        chestGlyph->setPosition({chestWidth * 0.5F, 22.0F});
        chest->addChild(chestGlyph);
    }

    buildBottomNavigation(root, ScreenId::MainLobby);
}

void LobbyLayer::showMonsterCatalog()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root        = replaceScreen();
    const float left  = _safeArea.origin.x;
    const float top   = _safeArea.origin.y + _safeArea.size.height;
    const float width = _safeArea.size.width;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* topBar = makePanel({width, 78.0F}, Color32{18, 29, 57, 255}, 252);
    topBar->setPosition({left, top - 78.0F});
    root.addChild(topBar, 20);

    auto* title = makeText("DECK & UNITS", 28.0F, Color32{255, 246, 221, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({20.0F, 39.0F});
    topBar->addChild(title);

    auto* currency = makeText(formatCount(_userProfile.currencies.gold) + "  GOLD", 19.0F, Color32{255, 213, 92, 255});
    currency->setAnchorPoint({1.0F, 0.5F});
    currency->setPosition({width - 20.0F, 39.0F});
    topBar->addChild(currency);

    if (!_userDataError.empty())
        showLoadError(root, _userDataError);
    else if (_monsterLoadError.empty())
    {
        buildDeckPanel(root);
        buildMonsterGrid(root);
    }
    else
        showLoadError(root, _monsterLoadError);

    buildBottomNavigation(root, ScreenId::Monsters);
}

void LobbyLayer::showStageSelect()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float top    = bottom + _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* topBar = makePanel({width, 88.0F}, Color32{14, 27, 48, 255}, 252);
    topBar->setPosition({left, top - 88.0F});
    root.addChild(topBar, 20);

    auto* backButton = makeButton({58.0F, 58.0F}, "<", Color32{70, 82, 110, 255});
    backButton->setPosition({12.0F, 15.0F});
    backButton->addClickEventListener([this](Object*) { showMainLobby(); });
    topBar->addChild(backButton);

    auto* title = makeText("STORY STAGES", 28.0F, Color32{255, 247, 226, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({84.0F, 46.0F});
    topBar->addChild(title);

    addResourcePill(*topBar, {width - 258.0F, 16.0F}, {118.0F, 54.0F}, "E", formatCount(_userProfile.currencies.energy),
                    Color32{46, 165, 229, 255});
    addResourcePill(*topBar, {width - 132.0F, 16.0F}, {120.0F, 54.0F}, "G", formatCount(_userProfile.currencies.gold),
                    Color32{236, 174, 54, 255});

    auto* chapter = makePanel({width - 32.0F, 132.0F}, Color32{27, 62, 91, 255}, 246);
    chapter->setPosition({left + 16.0F, top - 234.0F});
    root.addChild(chapter, 10);

    auto* chapterBadge = makePanel({94.0F, 94.0F}, Color32{55, 135, 174, 255});
    chapterBadge->setPosition({18.0F, 19.0F});
    chapter->addChild(chapterBadge);
    auto* badgeText = makeText("DR", 30.0F, Color32{255, 239, 192, 255});
    badgeText->setPosition(chapterBadge->getContentSize() * 0.5F);
    chapterBadge->addChild(badgeText);

    auto* chapterName = makeText(_stageCatalog.chapterName().empty() ? "STAGE DATA" : _stageCatalog.chapterName(),
                                 29.0F, Color32{255, 205, 71, 255});
    chapterName->setAnchorPoint({0.0F, 0.5F});
    chapterName->setPosition({132.0F, 85.0F});
    chapter->addChild(chapterName);
    auto* chapterInfo = makeText("TRAINING CHAPTER 01", 16.0F, Color32{161, 204, 221, 255});
    chapterInfo->setAnchorPoint({0.0F, 0.5F});
    chapterInfo->setPosition({132.0F, 51.0F});
    chapter->addChild(chapterInfo);
    const std::size_t clearedCount = std::min(_clearedStageCount, _stageCatalog.entries().size());
    auto* chapterProgress = makeText(std::to_string(clearedCount) + " CLEARED", 16.0F, Color32{106, 224, 168, 255});
    chapterProgress->setAnchorPoint({0.0F, 0.5F});
    chapterProgress->setPosition({132.0F, 25.0F});
    chapter->addChild(chapterProgress);

    if (!_stageLoadError.empty())
    {
        showLoadError(root, _stageLoadError);
        buildBottomNavigation(root, ScreenId::Stages);
        return;
    }

    auto* gridTitle = makeText("SELECT A STAGE", 23.0F, Color32{238, 245, 247, 255});
    gridTitle->setAnchorPoint({0.0F, 0.5F});
    gridTitle->setPosition({left + 22.0F, top - 268.0F});
    root.addChild(gridTitle, 12);

    _stageSelectionText =
        makeText("STAGE " + std::to_string(_selectedStageNumber) + " SELECTED", 16.0F, Color32{255, 217, 100, 255});
    _stageSelectionText->setAnchorPoint({1.0F, 0.5F});
    _stageSelectionText->setPosition({left + width - 22.0F, top - 268.0F});
    root.addChild(_stageSelectionText, 12);

    const std::size_t pageCount      = (_stageCatalog.entries().size() + STAGES_PER_PAGE - 1) / STAGES_PER_PAGE;
    _stagePage                       = std::min(_stagePage, pageCount - 1);
    const std::size_t pageBegin      = _stagePage * STAGES_PER_PAGE;
    const std::size_t pageEnd        = std::min(pageBegin + STAGES_PER_PAGE, _stageCatalog.entries().size());
    const std::size_t pageEntryCount = pageEnd - pageBegin;

    constexpr float pageButtonWidth  = 92.0F;
    constexpr float pageButtonHeight = 38.0F;
    constexpr float pageButtonGap    = 18.0F;
    const float pageControlsY        = top - 324.0F;
    const float pageControlsWidth    = pageButtonWidth * 2.0F + pageButtonGap * 2.0F + 98.0F;
    const float pageControlsX        = left + (width - pageControlsWidth) * 0.5F;

    const auto addPageButton = [this, &root](std::string_view label, const Vec2& position, bool enabled,
                                             int pageDelta) {
        auto* button = makePanel({pageButtonWidth, pageButtonHeight},
                                 enabled ? Color32{49, 91, 119, 255} : Color32{36, 48, 61, 255});
        button->setPosition(position);
        root.addChild(button, 12);

        auto* text = makeText(label, 15.0F, enabled ? Color32{232, 242, 246, 255} : Color32{99, 112, 123, 255});
        text->setPosition(button->getContentSize() * 0.5F);
        button->addChild(text);

        if (enabled)
        {
            button->setTouchEnabled(true);
            button->addClickEventListener([this, pageDelta](Object*) {
                _stagePage = static_cast<std::size_t>(static_cast<int>(_stagePage) + pageDelta);
                showStageSelect();
            });
        }
    };

    addPageButton("PREV", {pageControlsX, pageControlsY}, _stagePage > 0, -1);
    auto* pageText = makeText("PAGE " + std::to_string(_stagePage + 1) + " / " + std::to_string(pageCount), 15.0F,
                              Color32{173, 205, 218, 255});
    pageText->setPosition({left + width * 0.5F, pageControlsY + pageButtonHeight * 0.5F});
    root.addChild(pageText, 12);
    addPageButton("NEXT", {pageControlsX + pageButtonWidth + pageButtonGap * 2.0F + 98.0F, pageControlsY},
                  _stagePage + 1 < pageCount, 1);

    constexpr float outerPadding      = 14.0F;
    constexpr float gridGap           = 10.0F;
    constexpr float cardHeight        = 134.0F;
    constexpr std::size_t columnCount = 4;
    const float viewportBottom        = bottom + BOTTOM_NAVIGATION_HEIGHT + 96.0F;
    const float viewportTop           = top - 340.0F;
    const Size viewportSize{width - outerPadding * 2.0F, viewportTop - viewportBottom};

    auto* scrollView = ui::ScrollView::create();
    scrollView->setDirection(ui::ScrollView::Direction::VERTICAL);
    scrollView->setContentSize(viewportSize);
    scrollView->setPosition({left + outerPadding, viewportBottom});
    scrollView->setBounceEnabled(true);
    scrollView->setScrollBarEnabled(false);
    scrollView->setClippingType(ui::Layout::ClippingType::SCISSOR);
    root.addChild(scrollView, 10);

    const float cardWidth = (viewportSize.width - outerPadding * 2.0F - gridGap * static_cast<float>(columnCount - 1)) /
                            static_cast<float>(columnCount);
    const std::size_t rowCount = (pageEntryCount + columnCount - 1) / columnCount;
    const float innerHeight =
        std::max(viewportSize.height, outerPadding * 2.0F + static_cast<float>(rowCount) * cardHeight +
                                          static_cast<float>(rowCount > 0 ? rowCount - 1 : 0) * gridGap);
    scrollView->setInnerContainerSize({viewportSize.width, innerHeight});

    for (std::size_t localIndex = 0; localIndex < pageEntryCount; ++localIndex)
    {
        const StageCatalogEntry& entry = _stageCatalog.entries()[pageBegin + localIndex];
        const std::size_t row          = localIndex / columnCount;
        const std::size_t column       = localIndex % columnCount;
        const std::size_t stageIndex   = pageBegin + localIndex;
        const bool cleared             = stageIndex < _clearedStageCount;
        const bool unlocked            = stageIndex <= _clearedStageCount;
        const bool selected            = entry.stageId == _selectedStageId;
        const Color32 baseColor        = !unlocked ? Color32{37, 43, 55, 255}
                                         : cleared ? Color32{31, 101, 125, 255}
                                                   : Color32{102, 66, 43, 255};

        auto* card = makePanel({cardWidth, cardHeight}, selected ? Color32{181, 116, 37, 255} : baseColor, 255);
        card->setPosition({outerPadding + static_cast<float>(column) * (cardWidth + gridGap),
                           innerHeight - outerPadding - cardHeight - static_cast<float>(row) * (cardHeight + gridGap)});
        scrollView->addChild(card);

        auto* strip = makePanel({cardWidth, 7.0F}, !unlocked ? Color32{91, 96, 111, 255}
                                                   : cleared ? Color32{70, 211, 178, 255}
                                                             : Color32{255, 188, 64, 255});
        card->addChild(strip);

        auto* stageNumber = makeText(std::to_string(entry.stageNumber), 34.0F,
                                     unlocked ? Color32{255, 248, 226, 255} : Color32{126, 131, 143, 255});
        stageNumber->setPosition({cardWidth * 0.5F, 100.0F});
        card->addChild(stageNumber);

        auto* stageName = makeText(unlocked ? entry.name : "LOCKED", 13.0F,
                                   unlocked ? Color32{232, 239, 241, 255} : Color32{119, 124, 136, 255});
        stageName->setAutoSize(false);
        stageName->setTextAreaSize({cardWidth - 10.0F, 24.0F});
        stageName->setTextHorizontalAlignment(TextHAlignment::CENTER);
        stageName->setTextVerticalAlignment(TextVAlignment::CENTER);
        stageName->setPosition({cardWidth * 0.5F, 69.0F});
        card->addChild(stageName);

        const std::string powerText = unlocked ? "PWR " + std::to_string(entry.enemyPower) : "---";
        auto* power                 = makeText(powerText, 11.0F, Color32{181, 205, 214, 255});
        power->setPosition({cardWidth * 0.5F, 43.0F});
        card->addChild(power);

        const std::string rewardText =
            unlocked ? "E " + std::to_string(entry.energyCost) + "   G " + std::to_string(entry.rewardGold)
                     : "CLEAR PREVIOUS";
        auto* reward =
            makeText(rewardText, 10.0F, unlocked ? Color32{255, 211, 101, 255} : Color32{104, 110, 122, 255});
        reward->setPosition({cardWidth * 0.5F, 20.0F});
        card->addChild(reward);

        if (entry.stageNumber % 6 == 0)
        {
            auto* boss = makeText("BOSS", 10.0F, Color32{255, 130, 111, 255});
            boss->setAnchorPoint({1.0F, 0.5F});
            boss->setPosition({cardWidth - 7.0F, cardHeight - 14.0F});
            card->addChild(boss);
        }

        if (selected)
        {
            _selectedStageCard      = card;
            _selectedStageBaseColor = baseColor;
        }

        if (unlocked)
        {
            card->setTouchEnabled(true);
            card->addClickEventListener(
                [this, card, baseColor, stageId = entry.stageId, stageNumber = entry.stageNumber](Object*) {
                if (_selectedStageCard)
                    _selectedStageCard->setBackGroundColor(_selectedStageBaseColor);

                _selectedStageId        = stageId;
                _selectedStageNumber    = stageNumber;
                _selectedStageCard      = card;
                _selectedStageBaseColor = baseColor;
                card->setBackGroundColor(Color32{181, 116, 37, 255});
                _stageSelectionText->setString("STAGE " + std::to_string(stageNumber) + " SELECTED");
                _stageStartText->setString("START STAGE " + std::to_string(stageNumber));
                AXLOGI("Story stage selected: id={}", stageId);
            });
        }
    }
    scrollView->jumpToTop();

    auto* startButton = makeButton({250.0F, 72.0F}, "", Color32{61, 171, 79, 255});
    startButton->setPosition({left + (width - 250.0F) * 0.5F, bottom + BOTTOM_NAVIGATION_HEIGHT + 12.0F});
    startButton->addClickEventListener([this](Object*) { launchSelectedStage(cmc::BattleExecutionMode::Local); });
    root.addChild(startButton, 20);
    _stageStartText =
        makeText("START STAGE " + std::to_string(_selectedStageNumber), 22.0F, Color32{255, 255, 255, 255});
    _stageStartText->setPosition({125.0F, 40.0F});
    startButton->addChild(_stageStartText);

    buildBottomNavigation(root, ScreenId::Stages);
}

void LobbyLayer::launchSelectedStage(cmc::BattleExecutionMode mode)
{
    const StageCatalogEntry* stage = _stageCatalog.findById(_selectedStageId);
    const DeckPreset* deck         = _userProfile.selectedDeck();
    if (!stage || !deck)
    {
        AXLOGE("Story battle launch rejected: stage={}, deck={}", stage != nullptr, deck != nullptr);
        return;
    }
    if (!_battleLaunchCallback)
    {
        AXLOGE("Story battle launch rejected: no battle launch callback");
        return;
    }

    BattlePresentationRequest request;
    request.stageId       = stage->stageId;
    request.stageNumber   = stage->stageNumber;
    request.enemyPower    = stage->enemyPower;
    request.rewardGold    = stage->rewardGold;
    request.playerUnitIds = deck->unitIds;
    request.executionMode = mode;
    AXLOGI("Story battle launch: id={}, mode={}", request.stageId,
           mode == cmc::BattleExecutionMode::Local ? "local" : "remote");
    _battleLaunchCallback(request);
}

void LobbyLayer::buildBottomNavigation(Node& root, ScreenId selectedScreen)
{
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    auto* navigation   = makePanel({width, BOTTOM_NAVIGATION_HEIGHT}, Color32{14, 25, 44, 255}, 252);
    navigation->setPosition({left, bottom});
    root.addChild(navigation, 40);

    constexpr std::array<std::string_view, 5> labels = {"SHOP", "DECK", "LOBBY", "NEXT", "RANK"};
    constexpr std::array<std::string_view, 5> glyphs = {"$", "M", "X", "...", "#"};
    constexpr std::array<std::string_view, 5> icons  = {
        "UI/LobbyFeatures/Lobby/lobby_menu_shop.png", "UI/LobbyFeatures/Lobby/lobby_menu_cards.png",
        "UI/LobbyFeatures/Lobby/lobby_play_icon_stage.png", "UI/LobbyFeatures/Lobby/lobby_menu_clan_dim.png",
        "UI/LobbyFeatures/_Icons/ItemIcons/256/common_icon_ranking.png"};
    const float itemWidth = width / static_cast<float>(labels.size());
    for (size_t index = 0; index < labels.size(); ++index)
    {
        const bool selected =
            (index == 0 && selectedScreen == ScreenId::Shop) || (index == 1 && selectedScreen == ScreenId::Monsters) ||
            (index == 2 && (selectedScreen == ScreenId::MainLobby || selectedScreen == ScreenId::Stages)) ||
            (index == 3 && selectedScreen == ScreenId::Placeholder) ||
            (index == 4 && selectedScreen == ScreenId::Ranking);
        auto* item = makePanel({itemWidth, BOTTOM_NAVIGATION_HEIGHT},
                               selected ? Color32{31, 66, 92, 255} : Color32{14, 25, 44, 255});
        item->setPosition({itemWidth * static_cast<float>(index), 0.0F});
        navigation->addChild(item);

        ui::ImageView* icon = nullptr;
        if (FileUtils::getInstance()->isFileExist(icons[index]))
        {
            icon = ui::ImageView::create(icons[index]);
            if (icon)
            {
                const Size sourceSize = icon->getContentSize();
                if (sourceSize.width > 0.0F && sourceSize.height > 0.0F)
                    icon->setScale(std::min(46.0F / sourceSize.width, 42.0F / sourceSize.height));
                icon->setPosition({itemWidth * 0.5F, 59.0F});
                icon->setColor(selected ? Color32{255, 255, 255, 255} : Color32{155, 174, 191, 255});
                item->addChild(icon);
            }
        }
        if (!icon)
        {
            auto* glyph =
                makeText(glyphs[index], 24.0F, selected ? Color32{255, 211, 80, 255} : Color32{139, 158, 176, 255});
            glyph->setPosition({itemWidth * 0.5F, 59.0F});
            item->addChild(glyph);
        }
        auto* label =
            makeText(labels[index], 14.0F, selected ? Color32{255, 239, 192, 255} : Color32{139, 158, 176, 255});
        label->setPosition({itemWidth * 0.5F, 25.0F});
        item->addChild(label);

        if (selected)
        {
            auto* underline = makePanel({itemWidth * 0.56F, 5.0F}, Color32{255, 192, 58, 255});
            underline->setPosition({itemWidth * 0.22F, 4.0F});
            item->addChild(underline);
        }

        if (index < labels.size())
        {
            item->setTouchEnabled(true);
            item->addClickEventListener([this, index](Object*) {
                if (index == 0)
                    showShop();
                else if (index == 1)
                    showMonsterCatalog();
                else if (index == 2)
                    showMainLobby();
                else if (index == 3)
                    showPlaceholder();
                else if (index == 4)
                    showRanking();
            });
        }
    }
}

void LobbyLayer::buildDeckPanel(Node& root)
{
    constexpr float panelHeight = 258.0F;
    constexpr float gap         = 8.0F;
    const float left            = _safeArea.origin.x;
    const float top             = _safeArea.origin.y + _safeArea.size.height;
    const float panelWidth      = _safeArea.size.width - 28.0F;
    auto* panel                 = makePanel({panelWidth, panelHeight}, Color32{47, 40, 67, 255}, 248);
    panel->setPosition({left + 14.0F, top - 348.0F});
    root.addChild(panel, 10);

    const DeckPreset* deck    = _userProfile.selectedDeck();
    std::size_t equippedCount = 0;
    if (deck)
    {
        equippedCount = static_cast<std::size_t>(
            std::count_if(deck->unitIds.begin(), deck->unitIds.end(), [](int unitId) { return unitId != 0; }));
    }

    auto* heading = makeText("ACTIVE DECK", 24.0F, Color32{255, 236, 181, 255});
    heading->setAnchorPoint({0.0F, 0.5F});
    heading->setPosition({12.0F, 231.0F});
    panel->addChild(heading);

    auto* count = makeText(std::to_string(equippedCount) + " / 5", 18.0F, Color32{110, 224, 179, 255});
    count->setAnchorPoint({1.0F, 0.5F});
    count->setPosition({panelWidth - 12.0F, 231.0F});
    panel->addChild(count);

    const float slotWidth = (panelWidth - 20.0F - gap * 4.0F) / 5.0F;
    for (std::size_t index = 0; index < USER_DECK_SLOT_COUNT; ++index)
    {
        auto* slot = makePanel({slotWidth, 132.0F}, Color32{27, 32, 51, 255});
        slot->setPosition({10.0F + static_cast<float>(index) * (slotWidth + gap), 72.0F});
        panel->addChild(slot);

        const int unitId                 = deck ? deck->unitIds[index] : 0;
        const MonsterCatalogEntry* entry = _monsterCatalog.findById(unitId);
        if (entry)
        {
            auto* artwork = makeUnitArtwork(*entry, {slotWidth - 10.0F, 88.0F}, false);
            artwork->setPosition({5.0F, 38.0F});
            slot->addChild(artwork);

            auto* name = makeText(entry->nameKey, 14.0F, Color32{249, 245, 255, 255});
            name->setPosition({slotWidth * 0.5F, 20.0F});
            slot->addChild(name);
            slot->setTouchEnabled(true);
            slot->addClickEventListener([this, unitId](Object*) { showUnitDetail(unitId, UnitDetailTab::Stats); });
        }
        else
        {
            auto* empty = makeText("+", 34.0F, Color32{105, 134, 154, 255});
            empty->setPosition({slotWidth * 0.5F, 76.0F});
            slot->addChild(empty);
            auto* label = makeText("EMPTY", 13.0F, Color32{126, 145, 160, 255});
            label->setPosition({slotWidth * 0.5F, 28.0F});
            slot->addChild(label);
        }
    }

    const float presetWidth = slotWidth;
    for (std::size_t index = 0; index < USER_DECK_PRESET_LIMIT; ++index)
    {
        const bool available = index < _userProfile.deckPresets.size();
        const bool selected  = available && index == _userProfile.selectedPresetIndex;
        auto* preset =
            makePanel({presetWidth, 46.0F}, selected ? Color32{226, 152, 35, 255} : Color32{66, 58, 82, 255});
        preset->setPosition({10.0F + static_cast<float>(index) * (presetWidth + gap), 12.0F});
        panel->addChild(preset);

        auto* label = makeText("DECK " + std::to_string(index + 1), 14.0F,
                               selected ? Color32{255, 250, 224, 255} : Color32{196, 190, 211, 255});
        label->setPosition(preset->getContentSize() * 0.5F);
        preset->addChild(label);
        if (available)
        {
            preset->setTouchEnabled(true);
            preset->addClickEventListener([this, index](Object*) { selectDeckPreset(index); });
        }
    }
}

void LobbyLayer::buildMonsterGrid(Node& root)
{
    constexpr float headerHeight = 60.0F;
    constexpr float outerPadding = 8.0F;
    constexpr float gridGap      = 10.0F;
    constexpr float cardHeight   = 184.0F;
    constexpr int columnCount    = 4;

    const float left        = _safeArea.origin.x;
    const float bottom      = _safeArea.origin.y;
    const float top         = bottom + _safeArea.size.height;
    const float panelBottom = bottom + BOTTOM_NAVIGATION_HEIGHT;
    const float panelTop    = top - 358.0F;
    const float panelHeight = std::max(220.0F, panelTop - panelBottom);
    auto* panel             = makePanel({_safeArea.size.width, panelHeight}, Color32{22, 31, 58, 255}, 248);
    panel->setPosition({left, panelBottom});
    root.addChild(panel, 10);

    auto* heading = makeText("UNIT COLLECTION", 25.0F, Color32{255, 247, 224, 255});
    heading->setAnchorPoint({0.0F, 0.5F});
    heading->setPosition({16.0F, panelHeight - headerHeight * 0.5F});
    panel->addChild(heading);

    const std::string countText = "OWNED " + std::to_string(_userProfile.ownedUnits.size()) + " / " +
                                  std::to_string(_monsterCatalog.entries().size()) + "  |  " +
                                  std::to_string(_monsterCatalog.iconCount()) + " PHOTOS";
    auto* count = makeText(countText, 14.0F, Color32{171, 194, 213, 255});
    count->setAnchorPoint({1.0F, 0.5F});
    count->setPosition({_safeArea.size.width - 16.0F, panelHeight - headerHeight * 0.5F});
    panel->addChild(count);

    const Size viewportSize{_safeArea.size.width - 24.0F, panelHeight - headerHeight - 8.0F};
    auto* scrollView = ui::ScrollView::create();
    scrollView->setDirection(ui::ScrollView::Direction::VERTICAL);
    scrollView->setContentSize(viewportSize);
    scrollView->setPosition({12.0F, 8.0F});
    scrollView->setBounceEnabled(true);
    scrollView->setScrollBarEnabled(false);
    scrollView->setClippingType(ui::Layout::ClippingType::SCISSOR);
    panel->addChild(scrollView);

    const float cardWidth = (viewportSize.width - outerPadding * 2.0F - gridGap * static_cast<float>(columnCount - 1)) /
                            static_cast<float>(columnCount);
    const size_t rowCount = (_monsterCatalog.entries().size() + columnCount - 1) / columnCount;
    const float innerHeight =
        std::max(viewportSize.height, outerPadding * 2.0F + static_cast<float>(rowCount) * cardHeight +
                                          static_cast<float>(rowCount > 0 ? rowCount - 1 : 0) * gridGap);
    scrollView->setInnerContainerSize({viewportSize.width, innerHeight});

    for (size_t index = 0; index < _monsterCatalog.entries().size(); ++index)
    {
        const MonsterCatalogEntry& entry = _monsterCatalog.entries()[index];
        const size_t row                 = index / columnCount;
        const size_t column              = index % columnCount;
        const Color32 accent             = elementColor(entry.element);
        const OwnedUnitState* owned      = _userProfile.findOwnedUnit(entry.unitId);

        auto* card = makePanel({cardWidth, cardHeight}, owned ? cardColor(accent) : Color32{35, 40, 54, 255});
        card->setTouchEnabled(true);
        card->setPosition({outerPadding + static_cast<float>(column) * (cardWidth + gridGap),
                           innerHeight - outerPadding - cardHeight - static_cast<float>(row) * (cardHeight + gridGap)});
        card->addClickEventListener(
            [this, unitId = entry.unitId](Object*) { showUnitDetail(unitId, UnitDetailTab::Stats); });
        scrollView->addChild(card);

        auto* accentStrip = makePanel({cardWidth, 7.0F}, owned ? accent : Color32{83, 88, 103, 255});
        card->addChild(accentStrip);

        auto* artwork = makeUnitArtwork(entry, {cardWidth - 12.0F, 116.0F}, owned == nullptr);
        artwork->setPosition({6.0F, 49.0F});
        card->addChild(artwork);

        auto* name = makeText(entry.nameKey, 16.0F, owned ? Color32{250, 247, 255, 255} : Color32{153, 158, 170, 255});
        name->setAutoSize(false);
        name->setTextAreaSize({cardWidth - 10.0F, 27.0F});
        name->setTextHorizontalAlignment(TextHAlignment::CENTER);
        name->setTextVerticalAlignment(TextVAlignment::CENTER);
        name->setPosition({cardWidth * 0.5F, 30.0F});
        card->addChild(name);

        const std::string status = owned ? "LV " + std::to_string(owned->level) : "NOT OWNED";
        auto* statusText = makeText(status, 12.0F, owned ? Color32{111, 224, 177, 255} : Color32{126, 131, 143, 255});
        statusText->setPosition({cardWidth * 0.5F, 12.0F});
        card->addChild(statusText);
    }

    scrollView->jumpToTop();
}

void LobbyLayer::selectDeckPreset(std::size_t presetIndex)
{
    if (presetIndex >= _userProfile.deckPresets.size() || presetIndex == _userProfile.selectedPresetIndex)
        return;

    _userProfile.selectedPresetIndex = presetIndex;
    saveUserProfile();
    showMonsterCatalog();
}

void LobbyLayer::toggleUnitInSelectedDeck(int unitId)
{
    if (!_userDataSource || !_userDataSource->isWritable() || !_userProfile.findOwnedUnit(unitId))
        return;

    DeckPreset* deck = _userProfile.selectedDeck();
    if (!deck)
        return;

    const auto existing = std::find(deck->unitIds.begin(), deck->unitIds.end(), unitId);
    if (existing != deck->unitIds.end())
    {
        *existing = 0;
        std::stable_partition(deck->unitIds.begin(), deck->unitIds.end(), [](int value) { return value != 0; });
    }
    else
    {
        const auto empty = std::find(deck->unitIds.begin(), deck->unitIds.end(), 0);
        if (empty == deck->unitIds.end())
        {
            AXLOGW("Active deck is full; unit {} was not equipped", unitId);
            return;
        }
        *empty = unitId;
    }
    saveUserProfile();
}

void LobbyLayer::showShop()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float top    = bottom + _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* topBar = makePanel({width, 78.0F}, Color32{18, 29, 57, 255}, 252);
    topBar->setPosition({left, top - 78.0F});
    root.addChild(topBar, 20);

    auto* title = makeText("SHOP", 29.0F, Color32{255, 246, 221, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({20.0F, 39.0F});
    topBar->addChild(title);

    addResourcePill(*topBar, {width - 330.0F, 12.0F}, {150.0F, 54.0F}, "G", formatCount(_userProfile.currencies.gold),
                    Color32{236, 174, 54, 255});
    addResourcePill(*topBar, {width - 170.0F, 12.0F}, {158.0F, 54.0F}, "C", formatCount(_userProfile.currencies.gems),
                    Color32{158, 86, 221, 255});

    auto* section = makePanel({width - 36.0F, 66.0F}, Color32{170, 45, 142, 255}, 252);
    section->setPosition({left + 18.0F, top - 168.0F});
    root.addChild(section, 10);
    auto* sectionTitle = makeText("FEATURED CAPSULE", 25.0F, Color32{255, 247, 222, 255});
    sectionTitle->setPosition(section->getContentSize() * 0.5F);
    section->addChild(sectionTitle);

    auto* featured = makePanel({width - 36.0F, 566.0F}, Color32{37, 73, 117, 255}, 248);
    featured->setPosition({left + 18.0F, top - 750.0F});
    root.addChild(featured, 10);

    auto* offerTitle = makeText("MONSTER EGG", 34.0F, Color32{255, 219, 83, 255});
    offerTitle->setPosition({featured->getContentSize().width * 0.5F, 516.0F});
    featured->addChild(offerTitle);

    auto* offerDetail = makeText("One capsule. One unit result.", 18.0F, Color32{205, 226, 239, 255});
    offerDetail->setPosition({featured->getContentSize().width * 0.5F, 478.0F});
    featured->addChild(offerDetail);

    auto* egg = makeCapsuleEgg(270.0F);
    egg->setPosition({(featured->getContentSize().width - 270.0F) * 0.5F, 184.0F});
    featured->addChild(egg);

    const bool canPull = _userDataError.empty() && _userDataSource && _userDataSource->isWritable() &&
                         _monsterLoadError.empty() && !_monsterCatalog.entries().empty();
    auto* pullButton = makeButton({332.0F, 82.0F}, canPull ? "OPEN FREE TEST EGG" : "SERVER COMMAND REQUIRED",
                                  canPull ? Color32{230, 155, 32, 255} : Color32{74, 82, 96, 255});
    pullButton->setPosition({(featured->getContentSize().width - 332.0F) * 0.5F, 72.0F});
    featured->addChild(pullButton);

    if (canPull)
    {
        pullButton->addClickEventListener([this](Object*) {
            const MonsterCatalogEntry* reward = nullptr;
            for (const auto& entry : _monsterCatalog.entries())
            {
                if (!entry.icon.empty() && !_userProfile.findOwnedUnit(entry.unitId))
                {
                    reward = &entry;
                    break;
                }
            }
            if (!reward)
            {
                const auto withIcon =
                    std::find_if(_monsterCatalog.entries().begin(), _monsterCatalog.entries().end(),
                                 [](const MonsterCatalogEntry& entry) { return !entry.icon.empty(); });
                reward = withIcon != _monsterCatalog.entries().end() ? &*withIcon : &_monsterCatalog.entries().front();
            }

            _pendingRewardUnitId   = reward->unitId;
            _pendingRewardWasOwned = _userProfile.findOwnedUnit(reward->unitId) != nullptr;
            _pendingRewardApplied  = false;
            showGachaReveal();
        });
    }

    auto* sourceBadge = makePanel({width - 36.0F, 86.0F}, Color32{24, 42, 69, 255}, 248);
    sourceBadge->setPosition({left + 18.0F, bottom + BOTTOM_NAVIGATION_HEIGHT + 22.0F});
    root.addChild(sourceBadge, 10);
    auto* sourceTitle = makeText(canPull ? "LOCAL TEST DATA" : "WEB PROFILE IS READ-ONLY", 18.0F,
                                 canPull ? Color32{101, 226, 178, 255} : Color32{255, 171, 113, 255});
    sourceTitle->setPosition({sourceBadge->getContentSize().width * 0.5F, 57.0F});
    sourceBadge->addChild(sourceTitle);
    auto* sourceDetail = makeText(canPull ? "The same profile schema can be loaded from a web payload."
                                          : "Purchase results must arrive from the authoritative server.",
                                  14.0F, Color32{181, 204, 218, 255});
    sourceDetail->setPosition({sourceBadge->getContentSize().width * 0.5F, 27.0F});
    sourceBadge->addChild(sourceDetail);

    buildBottomNavigation(root, ScreenId::Shop);
}

void LobbyLayer::showGachaReveal()
{
    if (_pendingRewardUnitId <= 0)
    {
        showShop();
        return;
    }

    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float height = _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* title = makeText("CAPSULE OPENING", 31.0F, Color32{255, 234, 150, 255});
    title->setPosition({left + width * 0.5F, bottom + height - 120.0F});
    root.addChild(title, 10);

    auto* hint = makeText("The reward is revealed once.", 17.0F, Color32{176, 205, 220, 255});
    hint->setPosition({left + width * 0.5F, bottom + height - 158.0F});
    root.addChild(hint, 10);

    auto* glow = DrawNode::create();
    glow->drawSolidCircle({0.0F, 0.0F}, 236.0F, Color{1.0F, 0.72F, 0.18F, 0.10F});
    glow->drawSolidCircle({0.0F, 0.0F}, 172.0F, Color{1.0F, 0.88F, 0.46F, 0.13F});
    glow->setPosition({left + width * 0.5F, bottom + height * 0.53F});
    root.addChild(glow, 1);

    auto* egg = makeCapsuleEgg(360.0F);
    egg->setAnchorPoint({0.5F, 0.5F});
    egg->setPosition({left + width * 0.5F, bottom + height * 0.53F});
    egg->setScale(0.82F);
    root.addChild(egg, 5);
    egg->runAction(Sequence::create(ScaleTo::create(0.20F, 1.04F), RotateBy::create(0.13F, -8.0F),
                                    RotateBy::create(0.13F, 16.0F), RotateBy::create(0.13F, -8.0F),
                                    ScaleTo::create(0.22F, 1.18F), nullptr));

    auto* status = makeText("OPENING...", 25.0F, Color32{255, 250, 224, 255});
    status->setPosition({left + width * 0.5F, bottom + 226.0F});
    root.addChild(status, 10);

    scheduleOnce([this](float) { finishGachaReveal(); }, 1.05F, "cmc_gacha_reveal");
}

void LobbyLayer::finishGachaReveal()
{
    if (_pendingRewardUnitId <= 0 || _pendingRewardApplied)
        return;

    if (!_userDataSource || !_userDataSource->isWritable())
    {
        AXLOGW("Gacha reward cannot be applied to a read-only user profile");
        _pendingRewardUnitId = 0;
        showShop();
        return;
    }

    UserProfile updated = _userProfile;
    if (auto* owned = updated.findOwnedUnit(_pendingRewardUnitId))
    {
        _pendingRewardWasOwned = true;
        owned->shards += 10;
    }
    else
    {
        _pendingRewardWasOwned = false;
        OwnedUnitState acquired;
        acquired.unitId = _pendingRewardUnitId;
        updated.ownedUnits.emplace_back(acquired);
    }

    std::string error;
    if (!_userDataSource->save(updated, error))
    {
        _userDataError = std::move(error);
        AXLOGE("Gacha reward save failed: {}", _userDataError);
        _pendingRewardUnitId = 0;
        showShop();
        return;
    }

    _userProfile          = std::move(updated);
    _pendingRewardApplied = true;
    _userDataError.clear();
    showGachaResult();
}

void LobbyLayer::showGachaResult()
{
    const MonsterCatalogEntry* reward = _monsterCatalog.findById(_pendingRewardUnitId);
    if (!reward || !_pendingRewardApplied)
    {
        showShop();
        return;
    }

    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float height = _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* burst = DrawNode::create();
    burst->drawSolidCircle({0.0F, 0.0F}, 310.0F, Color{1.0F, 0.77F, 0.18F, 0.10F});
    burst->drawSolidCircle({0.0F, 0.0F}, 220.0F, Color{1.0F, 0.91F, 0.52F, 0.13F});
    burst->setPosition({left + width * 0.5F, bottom + height * 0.61F});
    root.addChild(burst, 0);

    auto* rarity = makeText(_pendingRewardWasOwned ? "DUPLICATE REWARD" : "NEW UNIT!", 38.0F,
                            _pendingRewardWasOwned ? Color32{183, 154, 255, 255} : Color32{255, 216, 71, 255});
    rarity->setPosition({left + width * 0.5F, bottom + height - 128.0F});
    root.addChild(rarity, 10);

    auto* card = makePanel({width - 104.0F, 670.0F}, Color32{38, 69, 108, 255}, 250);
    card->setPosition({left + 52.0F, bottom + 322.0F});
    root.addChild(card, 5);

    auto* artwork = makeUnitArtwork(*reward, {440.0F, 440.0F}, false);
    artwork->setPosition({(card->getContentSize().width - 440.0F) * 0.5F, 168.0F});
    card->addChild(artwork);

    auto* name = makeText(reward->nameKey, 36.0F, Color32{255, 248, 222, 255});
    name->setPosition({card->getContentSize().width * 0.5F, 118.0F});
    card->addChild(name);

    auto* result = makeText(_pendingRewardWasOwned ? "+10 SHARDS" : "ADDED TO YOUR COLLECTION", 21.0F,
                            Color32{112, 228, 179, 255});
    result->setPosition({card->getContentSize().width * 0.5F, 72.0F});
    card->addChild(result);

    auto* collect = makeButton({330.0F, 86.0F}, "COLLECT", Color32{78, 180, 65, 255});
    collect->setPosition({left + (width - 330.0F) * 0.5F, bottom + 170.0F});
    collect->addClickEventListener([this](Object*) {
        _pendingRewardUnitId   = 0;
        _pendingRewardWasOwned = false;
        _pendingRewardApplied  = false;
        showMonsterCatalog();
    });
    root.addChild(collect, 10);
}

void LobbyLayer::showUnitDetail(int unitId, UnitDetailTab tab)
{
    const MonsterCatalogEntry* entry = _monsterCatalog.findById(unitId);
    if (!entry)
    {
        showMonsterCatalog();
        return;
    }

    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    const OwnedUnitState* owned = _userProfile.findOwnedUnit(unitId);
    const bool writable         = _userDataSource && _userDataSource->isWritable();
    const DeckPreset* deck      = _userProfile.selectedDeck();
    const bool equipped = deck && std::find(deck->unitIds.begin(), deck->unitIds.end(), unitId) != deck->unitIds.end();
    const bool hasEmptySlot = deck && std::find(deck->unitIds.begin(), deck->unitIds.end(), 0) != deck->unitIds.end();

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float height = _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size, true);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    const Size modalSize{width - 36.0F, height - 36.0F};
    auto* modal = makePanel(modalSize, Color32{31, 48, 77, 255}, 252);
    modal->setPosition({left + 18.0F, bottom + 18.0F});
    root.addChild(modal, 10);

    auto* name = makeText(entry->nameKey, 34.0F, Color32{255, 246, 220, 255});
    name->setAnchorPoint({0.0F, 0.5F});
    name->setPosition({24.0F, modalSize.height - 46.0F});
    modal->addChild(name);

    const std::string statusText =
        owned ? "OWNED  |  LEVEL " + std::to_string(owned->level) : "NOT OWNED  |  COLLECTION PREVIEW";
    auto* status = makeText(statusText, 16.0F, owned ? Color32{103, 225, 174, 255} : Color32{180, 185, 198, 255});
    status->setAnchorPoint({0.0F, 0.5F});
    status->setPosition({26.0F, modalSize.height - 78.0F});
    modal->addChild(status);

    auto* close = makeButton({58.0F, 58.0F}, "X", Color32{177, 59, 55, 255});
    close->setPosition({modalSize.width - 74.0F, modalSize.height - 75.0F});
    close->addClickEventListener([this](Object*) { showMonsterCatalog(); });
    modal->addChild(close, 20);

    auto* preview = makePanel({modalSize.width - 48.0F, 442.0F}, cardColor(elementColor(entry->element)), 255);
    preview->setPosition({24.0F, 630.0F});
    modal->addChild(preview);

    auto* artwork = makeUnitArtwork(*entry, {410.0F, 410.0F}, false);
    artwork->setPosition({(preview->getContentSize().width - 410.0F) * 0.5F, 16.0F});
    preview->addChild(artwork);

    auto* type = makePanel({190.0F, 38.0F}, elementColor(entry->element), 244);
    type->setPosition({12.0F, preview->getContentSize().height - 50.0F});
    preview->addChild(type, 10);
    auto* typeText = makeText(entry->element + " / " + entry->role, 15.0F, Color32{255, 255, 255, 255});
    typeText->setPosition(type->getContentSize() * 0.5F);
    type->addChild(typeText);

    if (entry->modelId.empty())
    {
        auto* fallback = makeText("PHOTO PREVIEW  |  3D MODEL NOT MAPPED", 13.0F, Color32{225, 232, 239, 255});
        fallback->setPosition({preview->getContentSize().width * 0.5F, 20.0F});
        preview->addChild(fallback, 10);
    }

    constexpr std::array<std::string_view, 3> tabLabels = {"STATS", "EQUIPMENT", "SKILLS"};
    const float tabWidth = (modalSize.width - 48.0F) / static_cast<float>(tabLabels.size());
    for (std::size_t index = 0; index < tabLabels.size(); ++index)
    {
        const auto currentTab = static_cast<UnitDetailTab>(index);
        const bool selected   = currentTab == tab;
        auto* tabButton =
            makePanel({tabWidth, 58.0F}, selected ? Color32{45, 139, 193, 255} : Color32{49, 61, 86, 255});
        tabButton->setPosition({24.0F + tabWidth * static_cast<float>(index), 558.0F});
        tabButton->setTouchEnabled(true);
        tabButton->addClickEventListener([this, unitId, currentTab](Object*) { showUnitDetail(unitId, currentTab); });
        modal->addChild(tabButton);

        auto* label =
            makeText(tabLabels[index], 17.0F, selected ? Color32{255, 247, 221, 255} : Color32{174, 190, 204, 255});
        label->setPosition(tabButton->getContentSize() * 0.5F);
        tabButton->addChild(label);
    }

    auto* content = makePanel({modalSize.width - 48.0F, 344.0F}, Color32{228, 237, 240, 255}, 250);
    content->setPosition({24.0F, 202.0F});
    modal->addChild(content);

    if (tab == UnitDetailTab::Stats)
    {
        const std::array<std::pair<std::string_view, int>, 6> stats = {
            std::pair<std::string_view, int>{"HP", entry->hp},
            {"ATTACK", entry->ad},
            {"ABILITY", entry->ap},
            {"AD DEFENSE", entry->adDefense},
            {"AP DEFENSE", entry->apDefense},
            {"RANGE", entry->range},
        };
        const float statWidth = (content->getContentSize().width - 36.0F) * 0.5F;
        for (std::size_t index = 0; index < stats.size(); ++index)
        {
            const std::size_t row    = index / 2;
            const std::size_t column = index % 2;
            auto* stat               = makePanel({statWidth, 88.0F}, Color32{199, 216, 225, 255}, 255);
            stat->setPosition(
                {12.0F + static_cast<float>(column) * (statWidth + 12.0F), 238.0F - static_cast<float>(row) * 104.0F});
            content->addChild(stat);

            auto* statName = makeText(stats[index].first, 15.0F, Color32{48, 65, 81, 255});
            statName->setAnchorPoint({0.0F, 0.5F});
            statName->setPosition({12.0F, 58.0F});
            stat->addChild(statName);
            auto* statValue = makeText(std::to_string(stats[index].second), 28.0F, Color32{25, 40, 58, 255});
            statValue->setAnchorPoint({0.0F, 0.5F});
            statValue->setPosition({12.0F, 28.0F});
            stat->addChild(statValue);
        }
    }
    else if (tab == UnitDetailTab::Equipment)
    {
        const float slotWidth = (content->getContentSize().width - 48.0F) / 3.0F;
        for (std::size_t index = 0; index < USER_EQUIPMENT_SLOT_COUNT; ++index)
        {
            const std::size_t row    = index / 3;
            const std::size_t column = index % 3;
            const int itemId         = owned ? owned->equipmentItemIds[index] : 0;
            auto* slot =
                makePanel({slotWidth, 112.0F}, itemId > 0 ? Color32{82, 93, 119, 255} : Color32{184, 199, 208, 255});
            slot->setPosition(
                {12.0F + static_cast<float>(column) * (slotWidth + 12.0F), 202.0F - static_cast<float>(row) * 126.0F});
            content->addChild(slot);

            auto* slotTitle = makeText("SLOT " + std::to_string(index + 1), 14.0F,
                                       itemId > 0 ? Color32{218, 228, 237, 255} : Color32{72, 89, 102, 255});
            slotTitle->setPosition({slotWidth * 0.5F, 82.0F});
            slot->addChild(slotTitle);
            auto* item = makeText(itemId > 0 ? "ITEM " + std::to_string(itemId) : "EMPTY", 19.0F,
                                  itemId > 0 ? Color32{255, 219, 104, 255} : Color32{94, 109, 120, 255});
            item->setPosition({slotWidth * 0.5F, 50.0F});
            slot->addChild(item);
            auto* action =
                makeText(itemId > 0 && writable ? "TAP TO UNEQUIP" : "-", 11.0F, Color32{130, 151, 163, 255});
            action->setPosition({slotWidth * 0.5F, 20.0F});
            slot->addChild(action);

            if (itemId > 0 && owned && writable)
            {
                slot->setTouchEnabled(true);
                slot->addClickEventListener([this, unitId, index](Object*) {
                    auto* mutableUnit = _userProfile.findOwnedUnit(unitId);
                    if (!mutableUnit || mutableUnit->equipmentItemIds[index] == 0)
                        return;
                    const int previous                   = mutableUnit->equipmentItemIds[index];
                    mutableUnit->equipmentItemIds[index] = 0;
                    if (!saveUserProfile())
                        mutableUnit->equipmentItemIds[index] = previous;
                    showUnitDetail(unitId, UnitDetailTab::Equipment);
                });
            }
        }

        auto* note = makeText(owned ? "ITEM INVENTORY TABLE NOT IMPORTED - EQUIPPED ITEMS CAN BE REMOVED"
                                    : "ACQUIRE THIS UNIT TO EDIT EQUIPMENT",
                              12.0F, Color32{62, 79, 92, 255});
        note->setPosition({content->getContentSize().width * 0.5F, 15.0F});
        content->addChild(note);
    }
    else
    {
        if (entry->skills.empty())
        {
            auto* emptyTitle = makeText("NO SKILL MAPPING IN MASTER TABLE", 22.0F, Color32{51, 72, 88, 255});
            emptyTitle->setPosition({content->getContentSize().width * 0.5F, 195.0F});
            content->addChild(emptyTitle);
            auto* emptyDetail = makeText("This tab will bind directly to skill IDs when the table is imported.", 15.0F,
                                         Color32{92, 111, 124, 255});
            emptyDetail->setPosition({content->getContentSize().width * 0.5F, 153.0F});
            content->addChild(emptyDetail);
        }
        else
        {
            for (std::size_t index = 0; index < entry->skills.size(); ++index)
            {
                auto* skill = makePanel({content->getContentSize().width - 24.0F, 58.0F}, Color32{194, 211, 222, 255});
                skill->setPosition(
                    {12.0F, content->getContentSize().height - 70.0F - static_cast<float>(index) * 66.0F});
                content->addChild(skill);
                auto* skillText = makeText(entry->skills[index], 17.0F, Color32{38, 55, 70, 255});
                skillText->setAnchorPoint({0.0F, 0.5F});
                skillText->setPosition({16.0F, 29.0F});
                skill->addChild(skillText);
            }
        }
    }

    std::string actionLabel;
    bool actionEnabled = false;
    if (!owned)
        actionLabel = "NOT OWNED";
    else if (!writable)
        actionLabel = "SERVER DATA - READ ONLY";
    else if (equipped)
    {
        actionLabel   = "REMOVE FROM ACTIVE DECK";
        actionEnabled = true;
    }
    else if (hasEmptySlot)
    {
        actionLabel   = "EQUIP TO ACTIVE DECK";
        actionEnabled = true;
    }
    else
        actionLabel = "ACTIVE DECK FULL (5 / 5)";

    auto* deckAction =
        makeButton({420.0F, 82.0F}, actionLabel, actionEnabled ? Color32{45, 156, 99, 255} : Color32{79, 88, 101, 255});
    deckAction->setPosition({(modalSize.width - 420.0F) * 0.5F, 82.0F});
    modal->addChild(deckAction);
    if (actionEnabled)
    {
        deckAction->addClickEventListener([this, unitId, tab](Object*) {
            toggleUnitInSelectedDeck(unitId);
            showUnitDetail(unitId, tab);
        });
    }

    auto* preset = makeText("ACTIVE PRESET  " + std::to_string(_userProfile.selectedPresetIndex + 1), 14.0F,
                            Color32{159, 181, 198, 255});
    preset->setPosition({modalSize.width * 0.5F, 54.0F});
    modal->addChild(preset);
}

void LobbyLayer::showLoadError(Node& root, std::string_view message)
{
    auto* panel = makePanel({_safeArea.size.width - 48.0F, 180.0F}, Color32{72, 27, 49, 255}, 244);
    panel->setPosition({_safeArea.origin.x + 24.0F, _safeArea.origin.y + _safeArea.size.height * 0.35F});
    root.addChild(panel, 30);

    auto* title = makeText("DATA LOAD FAILED", 24.0F, Color32{255, 137, 137, 255});
    title->setPosition({panel->getContentSize().width * 0.5F, 125.0F});
    panel->addChild(title);

    auto* detail = makeText(message, 15.0F, Color32{255, 224, 229, 255});
    detail->setAutoSize(false);
    detail->setTextAreaSize({panel->getContentSize().width - 30.0F, 78.0F});
    detail->setTextHorizontalAlignment(TextHAlignment::CENTER);
    detail->setTextVerticalAlignment(TextVAlignment::CENTER);
    detail->setPosition({panel->getContentSize().width * 0.5F, 55.0F});
    panel->addChild(detail);
}
}  // namespace cmc::client
