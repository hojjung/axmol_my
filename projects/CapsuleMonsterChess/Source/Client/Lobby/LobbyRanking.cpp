#include "Client/Lobby/LobbyLayer.h"

#include "axmol/2d/DrawNode.h"
#include "axmol/platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
using namespace ax;

constexpr std::string_view FONT_PATH        = "fonts/Marker Felt.ttf";
constexpr float BOTTOM_NAVIGATION_HEIGHT    = 94.0F;
constexpr std::string_view TROPHY_ICON_PATH = "UI/LobbyFeatures/Ranking/ranking_icon_trophy.png";
constexpr std::array<std::string_view, 3> MEDAL_ICON_PATHS{{
    "UI/LobbyFeatures/Ranking/ranking_medal_gold.png",
    "UI/LobbyFeatures/Ranking/ranking_medal_silver.png",
    "UI/LobbyFeatures/Ranking/ranking_medal_bronze.png",
}};

struct RankingPreviewEntry final
{
    int rank;
    std::string_view playerName;
    std::string_view clanName;
    int trophies;
};

constexpr std::array<RankingPreviewEntry, 6> RANKING_PREVIEW{{
    {1, "Astra", "CAPSULE CROWN", 24745},
    {2, "CalcCliff", "HEX GUARD", 22118},
    {3, "Shoemach", "DRAGON TRAIL", 22094},
    {4, "Luci", "SKY LEGION", 20410},
    {5, "Tooooth", "CAPSULE CROWN", 13843},
    {6, "KoreaKing", "SEOUL KNIGHTS", 13601},
}};

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

ui::ImageView* addImageFit(Node& root, std::string_view path, const Vec2& position, const Size& bounds)
{
    if (!FileUtils::getInstance()->isFileExist(path))
        return nullptr;

    auto* image = ui::ImageView::create(path);
    if (!image)
        return nullptr;

    const Size sourceSize = image->getContentSize();
    if (sourceSize.width <= 0.0F || sourceSize.height <= 0.0F)
        return nullptr;

    image->setScale(std::min(bounds.width / sourceSize.width, bounds.height / sourceSize.height));
    image->setPosition(position);
    root.addChild(image);
    return image;
}

DrawNode* makeBackdrop(const Size& size)
{
    auto* backdrop = DrawNode::create();
    backdrop->drawSolidRect({0.0F, 0.0F}, {size.width, size.height}, Color{0.035F, 0.12F, 0.20F, 1.0F});

    constexpr float cellSize = 92.0F;
    const Color lineColor{0.20F, 0.50F, 0.62F, 0.18F};
    for (float x = -size.height; x < size.width + size.height; x += cellSize)
    {
        backdrop->drawLine({x, 0.0F}, {x + size.height, size.height}, lineColor, 2.0F);
        backdrop->drawLine({x, 0.0F}, {x - size.height, size.height}, lineColor, 2.0F);
    }
    return backdrop;
}

Color32 rankAccent(int rank)
{
    if (rank == 1)
        return {255, 196, 52, 255};
    if (rank == 2)
        return {184, 207, 222, 255};
    if (rank == 3)
        return {220, 130, 65, 255};
    return {65, 155, 214, 255};
}

Color32 rankCardColor(int rank)
{
    if (rank == 1)
        return {34, 135, 149, 255};
    if (rank == 2)
        return {177, 111, 35, 255};
    if (rank == 3)
        return {27, 92, 177, 255};
    return {28, 70, 119, 255};
}

std::string formatCount(int value)
{
    std::string text = std::to_string(value);
    for (std::ptrdiff_t position = static_cast<std::ptrdiff_t>(text.size()) - 3; position > 0; position -= 3)
        text.insert(static_cast<std::size_t>(position), ",");
    return text;
}
}  // namespace

namespace cmc::client
{
void LobbyLayer::showRanking()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float top    = bottom + _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* topBar = makePanel({width, 82.0F}, Color32{11, 26, 45, 255}, 250);
    topBar->setPosition({left, top - 82.0F});
    root.addChild(topBar, 20);

    auto* title = makeText("WEEKLY RANKING", 29.0F, Color32{255, 238, 182, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({22.0F, 46.0F});
    topBar->addChild(title);

    auto* authority = makePanel({246.0F, 44.0F}, Color32{38, 110, 144, 255});
    authority->setPosition({width - 260.0F, 19.0F});
    topBar->addChild(authority);
    auto* authorityText = makeText("SERVER AUTHORITATIVE", 14.0F, Color32{225, 249, 255, 255});
    authorityText->setPosition(authority->getContentSize() * 0.5F);
    authority->addChild(authorityText);

    auto* season = makePanel({width - 28.0F, 142.0F}, Color32{63, 45, 113, 255}, 248);
    season->setPosition({left + 14.0F, top - 236.0F});
    root.addChild(season, 12);

    auto* trophy = makePanel({92.0F, 92.0F}, Color32{237, 166, 37, 255});
    trophy->setPosition({18.0F, 25.0F});
    season->addChild(trophy);
    if (!addImageFit(*trophy, TROPHY_ICON_PATH, trophy->getContentSize() * 0.5F, {72.0F, 66.0F}))
    {
        auto* trophyText = makeText("#1", 35.0F, Color32{255, 250, 221, 255});
        trophyText->setPosition(trophy->getContentSize() * 0.5F);
        trophy->addChild(trophyText);
    }

    auto* seasonTitle = makeText("PRE-SEASON", 32.0F, Color32{255, 213, 80, 255});
    seasonTitle->setAnchorPoint({0.0F, 0.5F});
    seasonTitle->setPosition({130.0F, 94.0F});
    season->addChild(seasonTitle);
    auto* seasonDetail = makeText("DEVELOPMENT SNAPSHOT", 17.0F, Color32{205, 206, 239, 255});
    seasonDetail->setAnchorPoint({0.0F, 0.5F});
    seasonDetail->setPosition({132.0F, 57.0F});
    season->addChild(seasonDetail);
    auto* seasonStatus = makeText("LIVE MMR SYNC: NOT CONNECTED", 14.0F, Color32{137, 218, 239, 255});
    seasonStatus->setAnchorPoint({0.0F, 0.5F});
    seasonStatus->setPosition({132.0F, 29.0F});
    season->addChild(seasonStatus);

    constexpr float infoHeight = 112.0F;
    auto* info                 = makePanel({width - 28.0F, infoHeight}, Color32{224, 235, 239, 255}, 248);
    info->setPosition({left + 14.0F, bottom + BOTTOM_NAVIGATION_HEIGHT + 10.0F});
    root.addChild(info, 20);
    auto* infoIcon = makePanel({64.0F, 64.0F}, Color32{244, 167, 32, 255});
    infoIcon->setPosition({17.0F, 24.0F});
    info->addChild(infoIcon);
    auto* infoGlyph = makeText("i", 35.0F, Color32{255, 255, 255, 255});
    infoGlyph->setPosition(infoIcon->getContentSize() * 0.5F);
    infoIcon->addChild(infoGlyph);
    auto* infoText = makeText("Preview only. MMR, rewards and rank changes\nare accepted from the battle server only.",
                              17.0F, Color32{55, 78, 101, 255});
    infoText->setAnchorPoint({0.0F, 0.5F});
    infoText->setPosition({98.0F, infoHeight * 0.5F});
    info->addChild(infoText);

    constexpr float outerPadding = 14.0F;
    constexpr float rowHeight    = 124.0F;
    constexpr float rowGap       = 12.0F;
    const float viewportBottom   = bottom + BOTTOM_NAVIGATION_HEIGHT + infoHeight + 28.0F;
    const float viewportTop      = top - 254.0F;
    const Size viewportSize{width - outerPadding * 2.0F, std::max(240.0F, viewportTop - viewportBottom)};

    auto* ranking = ui::ScrollView::create();
    ranking->setDirection(ui::ScrollView::Direction::VERTICAL);
    ranking->setContentSize(viewportSize);
    ranking->setPosition({left + outerPadding, viewportBottom});
    ranking->setBounceEnabled(true);
    ranking->setScrollBarEnabled(false);
    ranking->setClippingType(ui::Layout::ClippingType::SCISSOR);
    root.addChild(ranking, 14);

    const float contentHeight = outerPadding * 2.0F + static_cast<float>(RANKING_PREVIEW.size()) * rowHeight +
                                static_cast<float>(RANKING_PREVIEW.size() - 1) * rowGap;
    const float innerHeight = std::max(viewportSize.height + 1.0F, contentHeight);
    ranking->setInnerContainerSize({viewportSize.width, innerHeight});

    for (std::size_t index = 0; index < RANKING_PREVIEW.size(); ++index)
    {
        const RankingPreviewEntry& entry = RANKING_PREVIEW[index];
        const Color32 accent             = rankAccent(entry.rank);
        auto* row                        = makePanel({viewportSize.width, rowHeight}, rankCardColor(entry.rank), 250);
        row->setPosition(
            {0.0F, innerHeight - outerPadding - rowHeight - static_cast<float>(index) * (rowHeight + rowGap)});
        ranking->addChild(row);

        auto* accentStrip = makePanel({7.0F, rowHeight}, accent);
        row->addChild(accentStrip);

        auto* rankBadge = makePanel({66.0F, 66.0F}, accent);
        rankBadge->setPosition({18.0F, 29.0F});
        row->addChild(rankBadge);
        const std::string_view medalPath =
            entry.rank <= static_cast<int>(MEDAL_ICON_PATHS.size()) ? MEDAL_ICON_PATHS[entry.rank - 1] : "";
        if (medalPath.empty() ||
            !addImageFit(*rankBadge, medalPath, rankBadge->getContentSize() * 0.5F, {74.0F, 74.0F}))
        {
            auto* rankText = makeText(std::to_string(entry.rank), 29.0F, Color32{255, 255, 255, 255});
            rankText->setPosition(rankBadge->getContentSize() * 0.5F);
            rankBadge->addChild(rankText);
        }
        else
        {
            rankBadge->setBackGroundColorOpacity(0);
        }

        auto* avatar = makePanel({70.0F, 70.0F}, Color32{37, 48, 75, 255});
        avatar->setPosition({98.0F, 27.0F});
        row->addChild(avatar);
        auto* avatarText = makeText(std::string(1, entry.playerName.front()), 30.0F, accent);
        avatarText->setPosition(avatar->getContentSize() * 0.5F);
        avatar->addChild(avatarText);

        auto* player = makeText(entry.playerName, entry.rank <= 3 ? 24.0F : 22.0F, Color32{255, 250, 233, 255});
        player->setAnchorPoint({0.0F, 0.5F});
        player->setPosition({184.0F, 78.0F});
        row->addChild(player);
        auto* clan = makeText(entry.clanName, 14.0F, Color32{180, 211, 229, 255});
        clan->setAnchorPoint({0.0F, 0.5F});
        clan->setPosition({184.0F, 45.0F});
        row->addChild(clan);

        auto* score = makePanel({172.0F, 62.0F}, Color32{17, 38, 67, 255}, 235);
        score->setPosition({viewportSize.width - 188.0F, 31.0F});
        row->addChild(score);
        const bool hasTrophyIcon = addImageFit(*score, TROPHY_ICON_PATH, {29.0F, 31.0F}, {36.0F, 32.0F}) != nullptr;
        auto* scoreLabel         = makeText("TROPHY", 12.0F, Color32{151, 190, 215, 255});
        scoreLabel->setAnchorPoint({0.0F, 0.5F});
        scoreLabel->setPosition({hasTrophyIcon ? 52.0F : 12.0F, 41.0F});
        score->addChild(scoreLabel);
        auto* scoreValue = makeText(formatCount(entry.trophies), 22.0F, Color32{255, 218, 91, 255});
        scoreValue->setAnchorPoint({0.0F, 0.5F});
        scoreValue->setPosition({hasTrophyIcon ? 52.0F : 12.0F, 19.0F});
        score->addChild(scoreValue);

        if (entry.rank <= 3)
        {
            auto* topMark = makeText("TOP " + std::to_string(entry.rank), 12.0F, accent);
            topMark->setAnchorPoint({1.0F, 0.5F});
            topMark->setPosition({viewportSize.width - 10.0F, rowHeight - 12.0F});
            row->addChild(topMark);
        }
    }
    ranking->jumpToTop();

    buildBottomNavigation(root, ScreenId::Ranking);
}

void LobbyLayer::showPlaceholder()
{
    if (_worldVisibilityCallback)
        _worldVisibilityCallback(false);

    Node& root         = replaceScreen();
    const float left   = _safeArea.origin.x;
    const float bottom = _safeArea.origin.y;
    const float width  = _safeArea.size.width;
    const float top    = bottom + _safeArea.size.height;

    auto* backdrop = makeBackdrop(_safeArea.size);
    backdrop->setPosition(_safeArea.origin);
    root.addChild(backdrop, -10);

    auto* topBar = makePanel({width, 82.0F}, Color32{11, 26, 45, 255}, 250);
    topBar->setPosition({left, top - 82.0F});
    root.addChild(topBar, 20);
    auto* title = makeText("NEW MODE", 29.0F, Color32{255, 238, 182, 255});
    title->setPosition({width * 0.5F, 44.0F});
    topBar->addChild(title);

    constexpr Size cardSize{560.0F, 360.0F};
    auto* card = makePanel(cardSize, Color32{25, 66, 101, 255}, 250);
    card->setPosition(
        {left + (width - cardSize.width) * 0.5F, bottom + (_safeArea.size.height - cardSize.height) * 0.5F});
    root.addChild(card, 10);

    auto* lock = makePanel({112.0F, 112.0F}, Color32{54, 112, 151, 255});
    lock->setPosition({(cardSize.width - 112.0F) * 0.5F, 205.0F});
    card->addChild(lock);
    auto* lockText = makeText("...", 38.0F, Color32{183, 222, 238, 255});
    lockText->setPosition(lock->getContentSize() * 0.5F);
    lock->addChild(lockText);

    auto* coming = makeText("COMING NEXT", 42.0F, Color32{255, 211, 73, 255});
    coming->setPosition({cardSize.width * 0.5F, 160.0F});
    card->addChild(coming);
    auto* detail = makeText("THIS TAB IS RESERVED FOR THE NEXT GAME MODE", 17.0F, Color32{174, 207, 224, 255});
    detail->setPosition({cardSize.width * 0.5F, 111.0F});
    card->addChild(detail);
    auto* status = makePanel({300.0F, 46.0F}, Color32{35, 48, 70, 255});
    status->setPosition({(cardSize.width - 300.0F) * 0.5F, 38.0F});
    card->addChild(status);
    auto* statusText = makeText("NO CONTENT DOWNLOADED", 14.0F, Color32{135, 165, 183, 255});
    statusText->setPosition(status->getContentSize() * 0.5F);
    status->addChild(statusText);

    buildBottomNavigation(root, ScreenId::Placeholder);
}
}  // namespace cmc::client
