#include "Client/Lobby/LobbyLayer.h"

#include "axmol/2d/DrawNode.h"
#include "axmol/platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace ax;

constexpr std::string_view FONT_PATH = "fonts/Marker Felt.ttf";

constexpr std::string_view CLOSE_ICON_PATH    = "UI/LobbyFeatures/_Icons/Pictoicons/256/btn_icon_close.png";
constexpr std::string_view BACK_ICON_PATH     = "UI/LobbyFeatures/_Icons/Pictoicons/256/btn_icon_arrow_backward.png";
constexpr std::string_view LOCK_ICON_PATH     = "UI/LobbyFeatures/_Icons/Pictoicons/256/btn_icon_lock.png";
constexpr std::string_view SETTINGS_ICON_PATH = "UI/LobbyFeatures/_Icons/Pictoicons/256/btn_icon_setting.png";

constexpr std::string_view FRIENDS_ICON_PATH = "UI/LobbyFeatures/Lobby/lobby_sub_icon_friends.png";
constexpr std::string_view INBOX_ICON_PATH   = "UI/LobbyFeatures/Lobby/lobby_sub_icon_inbox.png";
constexpr std::string_view MISSION_ICON_PATH = "UI/LobbyFeatures/Lobby/lobby_sub_icon_mission.png";
constexpr std::string_view NEWS_ICON_PATH    = "UI/LobbyFeatures/Lobby/lobby_sub_icon_news-.png";
constexpr std::string_view REWARD_ICON_PATH  = "UI/LobbyFeatures/Lobby/lobby_sub_icon_reward.png";
constexpr std::string_view TROPHY_ICON_PATH  = "UI/LobbyFeatures/Lobby/lobby_sub_icon_trophy.png";

constexpr std::string_view QUEST_BOOK_ICON_PATH           = "UI/LobbyFeatures/Mission/mission_icon_book.png";
constexpr std::string_view QUEST_CROWN_ICON_PATH          = "UI/LobbyFeatures/Mission/mission_icon_crown.png";
constexpr std::string_view QUEST_TROPHY_ICON_PATH         = "UI/LobbyFeatures/Mission/mission_icon_trophy.png";
constexpr std::string_view QUEST_PROGRESS_BACKGROUND_PATH = "UI/LobbyFeatures/Mission/mission_prg_bg.png";
constexpr std::string_view QUEST_PROGRESS_FILL_PATH       = "UI/LobbyFeatures/Mission/mission_prg_bar.png";
constexpr std::string_view QUEST_REWARD_ENERGY_PATH       = "UI/LobbyFeatures/Mission/mission_reward_icon_energy.png";
constexpr std::string_view QUEST_REWARD_GEM_PATH          = "UI/LobbyFeatures/Mission/mission_reward_icon_gem.png";
constexpr std::string_view QUEST_REWARD_GOLD_PATH         = "UI/LobbyFeatures/Mission/mission_reward_icon_gold.png";

constexpr std::string_view GOLDEN_PASS_ICON_PATH         = "UI/LobbyFeatures/Pass/icon_golden_pass.png";
constexpr std::string_view NORMAL_PASS_ICON_PATH         = "UI/LobbyFeatures/Pass/icon_normal_pass.png";
constexpr std::string_view PASS_LOCK_ICON_PATH           = "UI/LobbyFeatures/Pass/pass_icon_lock.png";
constexpr std::string_view PASS_FRAME_BLUE_PATH          = "UI/LobbyFeatures/Pass/pass_reward_frame_blue.png";
constexpr std::string_view PASS_FRAME_PURPLE_PATH        = "UI/LobbyFeatures/Pass/pass_reward_frame_purple.png";
constexpr std::string_view PASS_REWARD_CHEST_NORMAL_PATH = "UI/LobbyFeatures/Pass/pass_reward_icon_chest_0.png";
constexpr std::string_view PASS_REWARD_CHEST_GOLD_PATH   = "UI/LobbyFeatures/Pass/pass_reward_icon_chest_1.png";
constexpr std::string_view PASS_REWARD_GEM_PATH          = "UI/LobbyFeatures/Pass/pass_reward_icon_gem.png";
constexpr std::string_view PASS_REWARD_GOLD_PATH         = "UI/LobbyFeatures/Pass/pass_reward_icon_gold.png";

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

    auto* lowerEdge = makePanel({size.width, 7.0F}, Color32{38, 48, 78, 255});
    button->addChild(lowerEdge);

    auto* label = makeText(text, 20.0F, Color32{255, 249, 225, 255});
    label->setPosition({size.width * 0.5F, size.height * 0.5F + 3.0F});
    button->addChild(label);
    return button;
}

ui::Layout* addAssetOrFallback(Node& parent,
                               std::string_view path,
                               const Vec2& position,
                               const Size& size,
                               std::string_view fallback,
                               const Color32& fallbackColor,
                               int zOrder = 0)
{
    auto* container = makePanel(size, fallbackColor, fallbackColor.a);
    container->setPosition(position);
    parent.addChild(container, zOrder);

    ui::ImageView* image = nullptr;
    if (!path.empty() && FileUtils::getInstance()->isFileExist(path))
    {
        image = ui::ImageView::create(path);
        if (image)
        {
            const Size sourceSize = image->getContentSize();
            if (sourceSize.width > 0.0F && sourceSize.height > 0.0F)
                image->setScale(std::min(size.width / sourceSize.width, size.height / sourceSize.height));
            image->setPosition(size * 0.5F);
            container->addChild(image, 2);
        }
    }

    if (!image)
    {
        auto* label = makeText(fallback, std::min(size.width, size.height) * 0.28F, Color32{255, 247, 220, 255});
        label->setPosition(size * 0.5F);
        container->addChild(label, 2);
    }
    return container;
}

void addScale9Asset(Node& parent, std::string_view path, const Size& size, int zOrder)
{
    if (!FileUtils::getInstance()->isFileExist(path))
        return;

    auto* image = ui::ImageView::create(path);
    if (!image)
        return;

    image->setScale9Enabled(true);
    image->setCapInsets({10.0F, 10.0F, 22.0F, 22.0F});
    image->setContentSize(size);
    image->setPosition(size * 0.5F);
    parent.addChild(image, zOrder);
}

ui::Layout* makeIconButton(const Size& size, std::string_view iconPath, std::string_view fallback, const Color32& color)
{
    auto* button = makePanel(size, color);
    button->setTouchEnabled(true);
    addAssetOrFallback(*button, iconPath, {7.0F, 7.0F}, {size.width - 14.0F, size.height - 14.0F}, fallback,
                       Color32{0, 0, 0, 0}, 2);
    return button;
}

ui::Layout* makeProgressBar(const Size& size, float fraction, const Color32& fillColor)
{
    fraction    = std::clamp(fraction, 0.0F, 1.0F);
    auto* track = makePanel(size, Color32{32, 43, 68, 255});
    addScale9Asset(*track, QUEST_PROGRESS_BACKGROUND_PATH, size, 1);

    const float fillWidth = size.width * fraction;
    if (fillWidth > 0.0F)
    {
        auto* fill = makePanel({fillWidth, size.height}, fillColor);
        track->addChild(fill, 2);
        if (FileUtils::getInstance()->isFileExist(QUEST_PROGRESS_FILL_PATH))
        {
            auto* fillTexture = ui::ImageView::create(QUEST_PROGRESS_FILL_PATH);
            if (fillTexture)
            {
                fillTexture->setScale9Enabled(true);
                fillTexture->setCapInsets({8.0F, 8.0F, 11.0F, 16.0F});
                fillTexture->setContentSize({fillWidth, size.height});
                fillTexture->setPosition({fillWidth * 0.5F, size.height * 0.5F});
                fill->addChild(fillTexture, 1);
            }
        }
    }
    return track;
}

std::string_view questRewardIcon(std::string_view rewardType)
{
    if (rewardType == "GEMS")
        return QUEST_REWARD_GEM_PATH;
    if (rewardType == "ENERGY")
        return QUEST_REWARD_ENERGY_PATH;
    return QUEST_REWARD_GOLD_PATH;
}

std::string_view passRewardIcon(std::string_view reward, bool premium)
{
    if (reward.find("GEM") != std::string_view::npos)
        return PASS_REWARD_GEM_PATH;
    if (reward.find("GOLD") != std::string_view::npos && reward.find("CHEST") == std::string_view::npos)
        return PASS_REWARD_GOLD_PATH;
    if (reward.find("ENERGY") != std::string_view::npos)
        return QUEST_REWARD_ENERGY_PATH;
    if (reward.find("CHEST") != std::string_view::npos)
        return premium ? PASS_REWARD_CHEST_GOLD_PATH : PASS_REWARD_CHEST_NORMAL_PATH;
    return {};
}

bool containsTier(const std::vector<int>& claimedTierIds, int tier)
{
    return std::find(claimedTierIds.begin(), claimedTierIds.end(), tier) != claimedTierIds.end();
}

ui::Layout* makePassRewardCard(const Size& size, std::string_view reward, bool premium, bool locked, bool claimed)
{
    const Color32 baseColor = premium ? Color32{142, 52, 175, 255} : Color32{48, 124, 178, 255};
    auto* card              = makePanel(size, baseColor, 255);

    const std::string_view framePath = premium ? PASS_FRAME_PURPLE_PATH : PASS_FRAME_BLUE_PATH;
    addAssetOrFallback(*card, framePath, {12.0F, 9.0F}, {size.width - 24.0F, size.height - 18.0F}, "REWARD",
                       Color32{83, 91, 125, 255}, 1);

    addAssetOrFallback(*card, passRewardIcon(reward, premium), {size.width * 0.5F - 41.0F, 50.0F}, {82.0F, 68.0F}, "?",
                       Color32{45, 58, 86, 255}, 3);

    auto* rewardText = makeText(reward, 13.0F, Color32{255, 248, 226, 255});
    rewardText->setAutoSize(false);
    rewardText->setTextAreaSize({size.width - 18.0F, 30.0F});
    rewardText->setTextHorizontalAlignment(TextHAlignment::CENTER);
    rewardText->setTextVerticalAlignment(TextVAlignment::CENTER);
    rewardText->setPosition({size.width * 0.5F, 28.0F});
    card->addChild(rewardText, 5);

    if (locked)
    {
        auto* shade = makePanel(size, Color32{12, 18, 35, 255}, 150);
        card->addChild(shade, 8);
        addAssetOrFallback(*shade, PASS_LOCK_ICON_PATH, {size.width - 45.0F, size.height - 50.0F}, {36.0F, 42.0F}, "L",
                           Color32{57, 66, 86, 255}, 2);
    }
    else if (claimed)
    {
        auto* claimedText = makeText("CLAIMED", 14.0F, Color32{122, 255, 183, 255});
        claimedText->setPosition({size.width * 0.5F, size.height - 17.0F});
        card->addChild(claimedText, 9);
    }
    else
    {
        auto* readyText = makeText("SERVER", 13.0F, Color32{200, 225, 238, 255});
        readyText->setPosition({size.width * 0.5F, size.height - 17.0F});
        card->addChild(readyText, 9);
    }
    return card;
}

bool addQuestCurrency(cmc::client::UserCurrencies& currencies, std::string_view rewardType, int rewardAmount)
{
    std::int64_t* currency = nullptr;
    if (rewardType == "GOLD")
        currency = &currencies.gold;
    else if (rewardType == "GEMS")
        currency = &currencies.gems;
    else if (rewardType == "ENERGY")
        currency = &currencies.energy;

    if (!currency || rewardAmount <= 0 || *currency > std::numeric_limits<std::int64_t>::max() - rewardAmount)
        return false;

    *currency += rewardAmount;
    return true;
}
}  // namespace

namespace cmc::client
{
Node& LobbyLayer::createOverlay()
{
    closeOverlay();

    _overlayRoot = Node::create();
    addChild(_overlayRoot, 1000);

    auto* inputShield = makePanel(_safeArea.size, Color32{3, 10, 24, 255}, 204);
    inputShield->setPosition(_safeArea.origin);
    inputShield->setTouchEnabled(true);
    inputShield->addClickEventListener([this](Object*) { closeOverlay(); });
    _overlayRoot->addChild(inputShield, -10);
    return *_overlayRoot;
}

void LobbyLayer::closeOverlay()
{
    _chatMessageList = nullptr;
    _chatInput       = nullptr;
    _directTarget    = nullptr;
    _chatStatusText  = nullptr;
    if (!_overlayRoot)
        return;

    _overlayRoot->removeFromParent();
    _overlayRoot = nullptr;
}

void LobbyLayer::showSubMenuOverlay()
{
    Node& overlay      = createOverlay();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float height = _safeArea.size.height;

    const float panelWidth  = std::min(388.0F, width - 48.0F);
    const float panelHeight = std::min(730.0F, height - 250.0F);
    auto* panel             = makePanel({panelWidth, panelHeight}, Color32{214, 232, 241, 255}, 255);
    panel->setPosition({left + width - panelWidth - 38.0F, bottom + height - panelHeight - 190.0F});
    panel->setTouchEnabled(true);
    overlay.addChild(panel, 10);

    struct MenuItem final
    {
        std::string_view label;
        std::string_view iconPath;
        std::string_view fallback;
        std::string_view badge;
        bool locked;
    };

    constexpr std::array<MenuItem, 9> items{{
        {"FRIENDS", FRIENDS_ICON_PATH, "FR", {}, true},
        {"RUSH TV", {}, "TV", {}, true},
        {"RANKING", TROPHY_ICON_PATH, "#1", {}, true},
        {"DAILY GIFT", REWARD_ICON_PATH, "GIFT", {}, true},
        {"MAIL", INBOX_ICON_PATH, "MAIL", {}, true},
        {"NEWS", NEWS_ICON_PATH, "NEWS", {}, true},
        {"ENCYCLOPEDIA", MISSION_ICON_PATH, "BOOK", {}, true},
        {"SETTINGS", SETTINGS_ICON_PATH, "SET", {}, true},
        {"COMMUNITY", {}, "CLAN", {}, true},
    }};

    constexpr float rowHeight = 66.0F;
    constexpr float rowGap    = 8.0F;
    const Size viewportSize{panelWidth - 32.0F, panelHeight - 32.0F};
    auto* scrollView = ui::ScrollView::create();
    scrollView->setDirection(ui::ScrollView::Direction::VERTICAL);
    scrollView->setContentSize(viewportSize);
    scrollView->setPosition({16.0F, 16.0F});
    scrollView->setBounceEnabled(true);
    scrollView->setScrollBarEnabled(false);
    panel->addChild(scrollView, 4);

    const float contentHeight =
        static_cast<float>(items.size()) * rowHeight + static_cast<float>(items.size() - 1) * rowGap;
    const float innerHeight = std::max(viewportSize.height, contentHeight);
    scrollView->setInnerContainerSize({viewportSize.width, innerHeight});

    for (std::size_t index = 0; index < items.size(); ++index)
    {
        const auto& item = items[index];
        auto* row        = makePanel({viewportSize.width, rowHeight},
                              item.locked ? Color32{73, 103, 146, 255} : Color32{47, 126, 224, 255});
        const float rowY = innerHeight - rowHeight - static_cast<float>(index) * (rowHeight + rowGap);
        row->setPosition({0.0F, rowY});
        scrollView->addChild(row);

        auto* lowerEdge = makePanel({viewportSize.width, 6.0F}, Color32{19, 70, 139, 255});
        row->addChild(lowerEdge);
        addAssetOrFallback(*row, item.iconPath, {10.0F, 9.0F}, {52.0F, 48.0F}, item.fallback, Color32{45, 89, 145, 255},
                           2);

        auto* label =
            makeText(item.label, 22.0F, item.locked ? Color32{195, 208, 221, 255} : Color32{255, 249, 230, 255});
        label->setPosition({viewportSize.width * 0.56F, rowHeight * 0.55F});
        row->addChild(label, 3);

        if (item.locked)
        {
            addAssetOrFallback(*row, LOCK_ICON_PATH, {viewportSize.width - 50.0F, 14.0F}, {38.0F, 42.0F}, "L",
                               Color32{60, 74, 102, 255}, 4);
        }
        else if (!item.badge.empty())
        {
            auto* badge = makePanel({34.0F, 34.0F}, Color32{241, 166, 38, 255});
            badge->setPosition({viewportSize.width - 42.0F, rowHeight - 27.0F});
            row->addChild(badge, 5);
            auto* badgeText = makeText(item.badge, 16.0F, Color32{255, 255, 255, 255});
            badgeText->setPosition(badge->getContentSize() * 0.5F);
            badge->addChild(badgeText);
        }
    }
}

void LobbyLayer::showQuestOverlay(QuestTab tab)
{
    _questTab     = tab;
    Node& overlay = createOverlay();

    const float left        = _safeArea.origin.x;
    const float bottom      = _safeArea.origin.y;
    const float width       = _safeArea.size.width;
    const float height      = _safeArea.size.height;
    const float modalWidth  = std::min(644.0F, width - 32.0F);
    const float modalHeight = std::min(1010.0F, height - 72.0F);

    auto* modal = makePanel({modalWidth, modalHeight}, Color32{213, 231, 242, 255}, 255);
    modal->setPosition({left + (width - modalWidth) * 0.5F, bottom + (height - modalHeight) * 0.5F});
    modal->setTouchEnabled(true);
    overlay.addChild(modal, 10);

    auto* header = makePanel({modalWidth, 74.0F}, Color32{64, 112, 144, 255});
    header->setPosition({0.0F, modalHeight - 74.0F});
    modal->addChild(header, 5);

    std::string_view category = "CAREER";
    std::string_view title    = "CAREER QUESTS";
    if (tab == QuestTab::Daily)
    {
        category = "DAILY";
        title    = "DAILY QUESTS";
    }
    else if (tab == QuestTab::Season)
    {
        category = "SEASON";
        title    = "SEASON QUESTS";
    }

    auto* titleLabel = makeText(title, 30.0F, Color32{255, 250, 234, 255});
    titleLabel->setPosition({modalWidth * 0.5F, 38.0F});
    header->addChild(titleLabel);

    auto* close = makeIconButton({50.0F, 50.0F}, CLOSE_ICON_PATH, "X", Color32{208, 62, 49, 255});
    close->setPosition({modalWidth - 60.0F, 12.0F});
    close->addClickEventListener([this](Object*) { closeOverlay(); });
    header->addChild(close, 5);

    struct TabItem final
    {
        QuestTab tab;
        std::string_view label;
        std::string_view icon;
    };
    const std::array<TabItem, 3> tabs{{
        {QuestTab::Daily, "DAILY", QUEST_BOOK_ICON_PATH},
        {QuestTab::Career, "CAREER", QUEST_TROPHY_ICON_PATH},
        {QuestTab::Season, "SEASON", QUEST_CROWN_ICON_PATH},
    }};

    const float tabWidth = (modalWidth - 32.0F) / 3.0F;
    for (std::size_t index = 0; index < tabs.size(); ++index)
    {
        const auto& item    = tabs[index];
        const bool selected = item.tab == tab;
        auto* tabButton =
            makePanel({tabWidth, 72.0F}, selected ? Color32{109, 87, 186, 255} : Color32{150, 177, 200, 255});
        tabButton->setPosition({16.0F + static_cast<float>(index) * tabWidth, modalHeight - 146.0F});
        tabButton->setTouchEnabled(true);
        modal->addChild(tabButton, 4);
        addAssetOrFallback(*tabButton, item.icon, {14.0F, 11.0F}, {50.0F, 50.0F}, "Q",
                           selected ? Color32{84, 68, 154, 255} : Color32{105, 139, 166, 255}, 2);
        auto* tabLabel = makeText(item.label, 16.0F, selected ? Color32{255, 246, 224, 255} : Color32{44, 69, 91, 255});
        tabLabel->setPosition({tabWidth * 0.65F, 36.0F});
        tabButton->addChild(tabLabel, 3);
        tabButton->addClickEventListener([this, selectedTab = item.tab](Object*) { showQuestOverlay(selectedTab); });
    }

    std::vector<const QuestDefinition*> quests;
    for (const auto& quest : _progressionCatalog.quests())
    {
        if (quest.category == category)
            quests.push_back(&quest);
    }

    std::size_t completedCount = 0;
    for (const auto* quest : quests)
    {
        const auto* progress = _userProfile.findQuestProgress(quest->questId);
        if (progress && progress->progress >= quest->target)
            ++completedCount;
    }

    auto* summary = makePanel({modalWidth - 40.0F, 132.0F}, Color32{157, 132, 220, 255});
    summary->setPosition({20.0F, modalHeight - 292.0F});
    modal->addChild(summary, 4);
    addAssetOrFallback(*summary, tab == QuestTab::Season ? QUEST_CROWN_ICON_PATH : QUEST_TROPHY_ICON_PATH,
                       {summary->getContentSize().width - 98.0F, 26.0F}, {76.0F, 76.0F}, "BOX",
                       Color32{117, 93, 177, 255}, 2);

    auto* chapter = makeText(std::string(category) + " CHAPTER 1", 22.0F, Color32{39, 49, 78, 255});
    chapter->setAnchorPoint({0.0F, 0.5F});
    chapter->setPosition({18.0F, 100.0F});
    summary->addChild(chapter);

    auto* chapterDetail =
        makeText("Complete the objectives to collect every reward.", 14.0F, Color32{61, 70, 104, 255});
    chapterDetail->setAnchorPoint({0.0F, 0.5F});
    chapterDetail->setPosition({18.0F, 72.0F});
    summary->addChild(chapterDetail);

    const float summaryFraction =
        quests.empty() ? 0.0F : static_cast<float>(completedCount) / static_cast<float>(quests.size());
    auto* summaryProgress =
        makeProgressBar({summary->getContentSize().width - 134.0F, 28.0F}, summaryFraction, Color32{72, 220, 76, 255});
    summaryProgress->setPosition({18.0F, 19.0F});
    summary->addChild(summaryProgress, 3);
    auto* summaryText = makeText(std::to_string(completedCount) + " / " + std::to_string(quests.size()), 16.0F,
                                 Color32{255, 255, 255, 255});
    summaryText->setPosition(summaryProgress->getContentSize() * 0.5F);
    summaryProgress->addChild(summaryText, 4);

    const Size viewportSize{modalWidth - 40.0F, modalHeight - 326.0F};
    auto* scrollView = ui::ScrollView::create();
    scrollView->setDirection(ui::ScrollView::Direction::VERTICAL);
    scrollView->setContentSize(viewportSize);
    scrollView->setPosition({20.0F, 18.0F});
    scrollView->setBounceEnabled(true);
    scrollView->setScrollBarEnabled(false);
    modal->addChild(scrollView, 4);

    constexpr float cardHeight = 150.0F;
    constexpr float cardGap    = 12.0F;
    const float contentHeight  = quests.empty() ? viewportSize.height
                                                : static_cast<float>(quests.size()) * cardHeight +
                                                     static_cast<float>(quests.size() - 1) * cardGap;
    const float innerHeight    = std::max(viewportSize.height, contentHeight);
    scrollView->setInnerContainerSize({viewportSize.width, innerHeight});

    for (std::size_t index = 0; index < quests.size(); ++index)
    {
        const QuestDefinition& quest       = *quests[index];
        const QuestProgressState* progress = _userProfile.findQuestProgress(quest.questId);
        const int currentProgress          = progress ? std::clamp(progress->progress, 0, quest.target) : 0;
        const bool complete                = currentProgress >= quest.target;
        const bool claimed                 = progress && progress->rewardClaimed;
        bool locked                        = false;
        if (quest.prerequisiteQuestId > 0)
        {
            const QuestDefinition* prerequisite            = _progressionCatalog.findQuest(quest.prerequisiteQuestId);
            const QuestProgressState* prerequisiteProgress = _userProfile.findQuestProgress(quest.prerequisiteQuestId);
            locked = !prerequisite || !prerequisiteProgress || prerequisiteProgress->progress < prerequisite->target;
        }

        auto* card = makePanel({viewportSize.width, cardHeight},
                               locked ? Color32{137, 151, 161, 255} : Color32{184, 207, 222, 255});
        card->setPosition({0.0F, innerHeight - cardHeight - static_cast<float>(index) * (cardHeight + cardGap)});
        scrollView->addChild(card);

        addAssetOrFallback(*card, index % 2 == 0 ? QUEST_TROPHY_ICON_PATH : QUEST_BOOK_ICON_PATH, {14.0F, 60.0F},
                           {64.0F, 64.0F}, "Q", Color32{99, 130, 157, 255}, 2);

        auto* questTitle = makeText(quest.title, 19.0F, locked ? Color32{75, 84, 94, 255} : Color32{37, 54, 72, 255});
        questTitle->setAnchorPoint({0.0F, 0.5F});
        questTitle->setPosition({90.0F, 118.0F});
        card->addChild(questTitle, 3);

        auto* description = makeText(locked ? "Complete the previous objective first." : quest.description, 13.0F,
                                     Color32{74, 91, 107, 255});
        description->setAnchorPoint({0.0F, 0.5F});
        description->setPosition({90.0F, 90.0F});
        card->addChild(description, 3);

        addAssetOrFallback(*card, questRewardIcon(quest.rewardType), {viewportSize.width - 82.0F, 70.0F},
                           {60.0F, 60.0F}, quest.rewardType, Color32{93, 111, 139, 255}, 3);
        auto* rewardAmount = makeText("x" + std::to_string(quest.rewardAmount), 16.0F, Color32{255, 217, 68, 255});
        rewardAmount->setPosition({viewportSize.width - 52.0F, 63.0F});
        card->addChild(rewardAmount, 5);

        auto* progressBar = makeProgressBar({viewportSize.width - 150.0F, 28.0F},
                                            static_cast<float>(currentProgress) / static_cast<float>(quest.target),
                                            complete ? Color32{80, 219, 85, 255} : Color32{58, 157, 225, 255});
        progressBar->setPosition({18.0F, 18.0F});
        card->addChild(progressBar, 3);
        auto* progressText = makeText(std::to_string(currentProgress) + " / " + std::to_string(quest.target), 14.0F,
                                      Color32{255, 255, 255, 255});
        progressText->setPosition(progressBar->getContentSize() * 0.5F);
        progressBar->addChild(progressText, 4);

        const bool writable = _userDataError.empty() && _userDataSource && _userDataSource->isWritable();
        if (complete && !claimed && !locked && writable)
        {
            auto* claim = makeButton({112.0F, 38.0F}, "CLAIM", Color32{67, 173, 74, 255});
            claim->setPosition({viewportSize.width - 126.0F, 14.0F});
            claim->addClickEventListener([this, questId = quest.questId](Object*) { claimQuestReward(questId); });
            card->addChild(claim, 6);
        }
        else
        {
            std::string_view state = locked ? "LOCKED" : claimed ? "CLAIMED" : complete ? "SERVER" : "IN PROGRESS";
            auto* stateText = makeText(state, 12.0F, claimed ? Color32{45, 132, 85, 255} : Color32{70, 83, 101, 255});
            stateText->setPosition({viewportSize.width - 70.0F, 32.0F});
            card->addChild(stateText, 6);
        }

        if (locked)
        {
            addAssetOrFallback(*card, LOCK_ICON_PATH, {viewportSize.width - 47.0F, cardHeight - 44.0F}, {34.0F, 36.0F},
                               "L", Color32{93, 104, 118, 255}, 8);
        }
    }

    if (quests.empty())
    {
        auto* empty = makeText(_progressionLoadError.empty() ? "NO QUESTS IN THIS CATEGORY" : _progressionLoadError,
                               18.0F, Color32{65, 78, 97, 255});
        empty->setPosition(viewportSize * 0.5F);
        scrollView->addChild(empty, 5);
    }
}

void LobbyLayer::showBattlePass()
{
    closeOverlay();
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float height = _safeArea.size.height;
    const float top    = bottom + height;

    auto* backdrop = makePanel(_safeArea.size, Color32{22, 83, 136, 255});
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -20);

    auto* hero = makePanel({width, 430.0F}, Color32{73, 186, 219, 255});
    hero->setPosition({left, top - 430.0F});
    root.addChild(hero, -5);

    auto* sky = DrawNode::create();
    sky->drawSolidCircle({width * 0.17F, 336.0F}, 82.0F, Color{0.78F, 0.94F, 1.0F, 0.55F});
    sky->drawSolidCircle({width * 0.78F, 326.0F}, 110.0F, Color{0.72F, 0.91F, 1.0F, 0.48F});
    sky->drawSolidRect({width * 0.35F, 120.0F}, {width * 0.52F, 310.0F}, Color{0.48F, 0.34F, 0.72F, 0.48F});
    hero->addChild(sky, 0);

    const BattlePassDefinition& pass = _progressionCatalog.battlePass();
    const std::string passTitle      = pass.subtitle.empty() ? "ADVENTURE AWAITS!" : pass.subtitle;
    auto* subtitle                   = makeText(passTitle, 34.0F, Color32{255, 252, 240, 255});
    subtitle->setPosition({width * 0.5F, 352.0F});
    hero->addChild(subtitle, 5);

    addAssetOrFallback(*hero, GOLDEN_PASS_ICON_PATH, {70.0F, 202.0F}, {142.0F, 92.0F}, "GOLD PASS",
                       Color32{212, 163, 47, 255}, 3);
    addAssetOrFallback(*hero, NORMAL_PASS_ICON_PATH, {width - 212.0F, 202.0F}, {142.0F, 92.0F}, "FREE PASS",
                       Color32{86, 139, 178, 255}, 3);

    const bool premiumUnlocked = _userProfile.battlePass.premiumUnlocked;
    auto* purchase = makeButton({330.0F, 74.0F}, premiumUnlocked ? "PREMIUM PASS ACTIVE" : "SERVER PURCHASE",
                                premiumUnlocked ? Color32{65, 163, 92, 255} : Color32{126, 112, 84, 255});
    purchase->setPosition({(width - 330.0F) * 0.5F, 210.0F});
    purchase->setTouchEnabled(false);
    hero->addChild(purchase, 7);

    const int pointsPerTier = std::max(pass.pointsPerTier, 1);
    const int totalPoints   = std::max(_userProfile.battlePass.points, 0);
    const int maxTier       = pass.tiers.empty() ? 1 : pass.tiers.back().tier;
    const std::int64_t calculatedTier =
        static_cast<std::int64_t>(totalPoints) / static_cast<std::int64_t>(pointsPerTier) + 1;
    const int currentTier = static_cast<int>(std::min<std::int64_t>(calculatedTier, maxTier));
    int tierPoints        = totalPoints % pointsPerTier;
    if (calculatedTier > maxTier)
        tierPoints = pointsPerTier;

    auto* passName = makeText(pass.name.empty() ? "BATTLE PASS" : pass.name, 20.0F, Color32{255, 244, 216, 255});
    passName->setPosition({width * 0.5F, 185.0F});
    hero->addChild(passName, 5);

    auto* progress =
        makeProgressBar({330.0F, 32.0F}, static_cast<float>(tierPoints) / static_cast<float>(pointsPerTier),
                        Color32{29, 187, 235, 255});
    progress->setPosition({(width - 330.0F) * 0.5F, 124.0F});
    hero->addChild(progress, 5);
    auto* progressText = makeText(std::to_string(tierPoints) + " / " + std::to_string(pointsPerTier), 16.0F,
                                  Color32{255, 255, 255, 255});
    progressText->setPosition(progress->getContentSize() * 0.5F);
    progress->addChild(progressText, 6);

    auto* tierBadge = makePanel({58.0F, 58.0F}, Color32{235, 151, 42, 255});
    tierBadge->setPosition({width * 0.5F + 180.0F, 111.0F});
    hero->addChild(tierBadge, 5);
    auto* tierText = makeText(std::to_string(currentTier), 23.0F, Color32{255, 255, 255, 255});
    tierText->setPosition(tierBadge->getContentSize() * 0.5F);
    tierBadge->addChild(tierText);

    auto* remaining = makeText(pass.seasonEndsLabel.empty() ? "SEASON TIME NOT SET" : pass.seasonEndsLabel, 15.0F,
                               Color32{255, 247, 225, 255});
    remaining->setPosition({width * 0.5F, 76.0F});
    hero->addChild(remaining, 5);

    constexpr float footerHeight = 146.0F;
    const float trackHeight      = std::max(180.0F, height - 430.0F - footerHeight);
    auto* premiumLane            = makePanel({width * 0.5F, trackHeight}, Color32{154, 44, 180, 255}, 245);
    premiumLane->setPosition({left, bottom + footerHeight});
    root.addChild(premiumLane, -4);
    auto* freeLane = makePanel({width * 0.5F, trackHeight}, Color32{35, 126, 180, 255}, 245);
    freeLane->setPosition({left + width * 0.5F, bottom + footerHeight});
    root.addChild(freeLane, -4);

    auto* premiumTitle = makeText("PREMIUM", 17.0F, Color32{255, 225, 120, 255});
    premiumTitle->setPosition({left + width * 0.25F, bottom + footerHeight + trackHeight - 22.0F});
    root.addChild(premiumTitle, 10);
    auto* freeTitle = makeText("FREE", 17.0F, Color32{221, 245, 255, 255});
    freeTitle->setPosition({left + width * 0.75F, bottom + footerHeight + trackHeight - 22.0F});
    root.addChild(freeTitle, 10);

    const Size viewportSize{width, trackHeight - 42.0F};
    auto* scrollView = ui::ScrollView::create();
    scrollView->setDirection(ui::ScrollView::Direction::VERTICAL);
    scrollView->setContentSize(viewportSize);
    scrollView->setPosition({left, bottom + footerHeight});
    scrollView->setBounceEnabled(true);
    scrollView->setScrollBarEnabled(false);
    root.addChild(scrollView, 5);

    constexpr float tierRowHeight = 190.0F;
    const float contentHeight     = static_cast<float>(pass.tiers.size()) * tierRowHeight;
    const float innerHeight       = std::max(viewportSize.height, contentHeight);
    scrollView->setInnerContainerSize({viewportSize.width, innerHeight});

    for (std::size_t index = 0; index < pass.tiers.size(); ++index)
    {
        const auto& tier = pass.tiers[index];
        const float rowY = innerHeight - static_cast<float>(index + 1) * tierRowHeight;

        auto* separator = makePanel({width, 3.0F}, Color32{211, 232, 245, 255}, 90);
        separator->setPosition({0.0F, rowY});
        scrollView->addChild(separator, 1);

        const bool reached        = tier.tier <= currentTier;
        const bool premiumClaimed = containsTier(_userProfile.battlePass.claimedPremiumTierIds, tier.tier);
        const bool freeClaimed    = containsTier(_userProfile.battlePass.claimedFreeTierIds, tier.tier);

        auto* premiumReward = makePassRewardCard({146.0F, 154.0F}, tier.premiumReward, true,
                                                 !reached || !premiumUnlocked, premiumClaimed);
        premiumReward->setPosition({width * 0.25F - 73.0F, rowY + 18.0F});
        scrollView->addChild(premiumReward, 3);

        auto* freeReward = makePassRewardCard({146.0F, 154.0F}, tier.freeReward, false, !reached, freeClaimed);
        freeReward->setPosition({width * 0.75F - 73.0F, rowY + 18.0F});
        scrollView->addChild(freeReward, 3);

        auto* levelBadge = makePanel({64.0F, 64.0F}, Color32{109, 157, 199, 255});
        levelBadge->setPosition({width * 0.5F - 32.0F, rowY + 63.0F});
        levelBadge->setRotation(45.0F);
        scrollView->addChild(levelBadge, 6);
        auto* level = makeText(std::to_string(tier.tier), 22.0F, Color32{255, 255, 255, 255});
        level->setRotation(-45.0F);
        level->setPosition(levelBadge->getContentSize() * 0.5F);
        levelBadge->addChild(level);
    }

    if (!pass.tiers.empty() && innerHeight > viewportSize.height)
    {
        const auto current =
            std::find_if(pass.tiers.begin(), pass.tiers.end(),
                         [currentTier](const BattlePassTierDefinition& tier) { return tier.tier >= currentTier; });
        const std::size_t currentIndex = current == pass.tiers.end()
                                             ? pass.tiers.size() - 1
                                             : static_cast<std::size_t>(std::distance(pass.tiers.begin(), current));
        const float scrollableRows =
            std::max(1.0F, static_cast<float>(pass.tiers.size()) - viewportSize.height / tierRowHeight);
        const float percent = std::clamp(static_cast<float>(currentIndex) / scrollableRows, 0.0F, 1.0F) * 100.0F;
        scrollView->scrollToPercentVertical(percent, 0.0F, false);
    }

    auto* footer = makePanel({width, footerHeight}, Color32{22, 91, 145, 255});
    footer->setPosition({left, bottom});
    root.addChild(footer, 20);

    auto* back = makeIconButton({82.0F, 82.0F}, BACK_ICON_PATH, "<", Color32{31, 170, 221, 255});
    back->setPosition({28.0F, 30.0F});
    back->addClickEventListener([this](Object*) { showMainLobby(); });
    footer->addChild(back, 4);

    auto* footerNote = makeText("REWARDS ARE SERVER AUTHORITATIVE", 14.0F, Color32{190, 225, 240, 255});
    footerNote->setPosition({width * 0.5F, 65.0F});
    footer->addChild(footerNote);

    if (!_progressionLoadError.empty())
        showLoadError(root, _progressionLoadError);
}

void LobbyLayer::claimQuestReward(int questId)
{
    if (!_userDataError.empty())
    {
        AXLOGW("Quest reward claim blocked by invalid user profile: questId={}", questId);
        return;
    }

    const QuestDefinition* quest = _progressionCatalog.findQuest(questId);
    QuestProgressState* progress = _userProfile.findQuestProgress(questId);
    if (!quest || !progress || progress->rewardClaimed || progress->progress < quest->target)
    {
        AXLOGW("Quest reward claim rejected: questId={}", questId);
        return;
    }
    if (!_userDataSource || !_userDataSource->isWritable())
    {
        AXLOGW("Quest reward claim requires an authoritative server command: questId={}", questId);
        return;
    }

    UserProfile previousProfile = _userProfile;
    progress->rewardClaimed     = true;
    if (!addQuestCurrency(_userProfile.currencies, quest->rewardType, quest->rewardAmount))
    {
        _userProfile = std::move(previousProfile);
        AXLOGE("Quest reward currency overflow or invalid type: questId={}", questId);
        return;
    }

    if (!saveUserProfile())
    {
        _userProfile = std::move(previousProfile);
        return;
    }

    showQuestOverlay(_questTab);
}
}  // namespace cmc::client
