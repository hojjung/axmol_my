#include "Client/Lobby/LobbyLayer.h"

#include "axmol/base/UserDefault.h"
#include "axmol/platform/Application.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#ifndef CMC_DEFAULT_CHAT_URL
#    define CMC_DEFAULT_CHAT_URL "ws://127.0.0.1:8090/v1/chat"
#endif

namespace
{
using namespace ax;

ui::Layout* makeChatPanel(const Size& size, const Color32& color, std::uint8_t opacity = 255)
{
    auto* panel = ui::Layout::create();
    panel->setContentSize(size);
    panel->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(color);
    panel->setBackGroundColorOpacity(opacity);
    return panel;
}

ui::Text* makeChatText(std::string_view text, float size, const Color32& color, std::string_view language = "en")
{
    auto* label = ui::Text::create(text, cmc::client::chatFontPath(language), size);
    label->setTextColor(color);
    return label;
}

ui::Layout* makeChatButton(const Size& size,
                           std::string_view text,
                           const Color32& color,
                           std::string_view language = "en")
{
    auto* button = makeChatPanel(size, color);
    button->setTouchEnabled(true);
    auto* lowerEdge = makeChatPanel({size.width, 5.0F}, Color32{17, 45, 67, 255});
    button->addChild(lowerEdge);
    auto* label = makeChatText(text, 17.0F, Color32{255, 250, 234, 255}, language);
    label->setPosition(size * 0.5F + Vec2{0.0F, 2.0F});
    button->addChild(label);
    return button;
}

bool sameChannel(const cmc::client::ChatChannel& left, const cmc::client::ChatChannel& right)
{
    return left.kind == right.kind && left.scope == right.scope;
}
}  // namespace

namespace cmc::client
{
void LobbyLayer::initializeChat()
{
    auto* defaults             = ax::UserDefault::getInstance();
    const std::string language = normalizeChatLanguage(ax::Application::getInstance()->getCurrentLanguageCode());
    _chatUrl                   = std::string(defaults->getStringForKey("cmc.chat.url", CMC_DEFAULT_CHAT_URL));

    ChatIdentity identity;
    identity.userId      = std::string(defaults->getStringForKey("cmc.chat.user_id", "local-user"));
    identity.displayName = std::string(defaults->getStringForKey("cmc.chat.display_name", "YOU"));
    identity.language    = language;
    identity.guildId     = std::string(defaults->getStringForKey("cmc.chat.guild_id", "rookies"));
    _chatChannel         = {ChatChannelKind::Language, language};

    _chatClient = std::make_unique<ChatClient>();
    _chatClient->setStateCallback([this](ChatConnectionState state) {
        switch (state)
        {
        case ChatConnectionState::Connected:
            _chatReconnectScheduled = false;
            updateChatStatus("CONNECTED", ax::Color32{98, 225, 159, 255});
            break;
        case ChatConnectionState::Connecting:
            updateChatStatus("CONNECTING...", ax::Color32{255, 215, 112, 255});
            break;
        case ChatConnectionState::Disconnected:
            updateChatStatus("OFFLINE - RETRYING", ax::Color32{255, 136, 119, 255});
            if (!_chatReconnectScheduled)
            {
                _chatReconnectScheduled = true;
                scheduleOnce([this](float) { reconnectChat(); }, 3.0F, "cmc_chat_reconnect");
            }
            break;
        }
    });
    _chatClient->setHistoryCallback([this](const ChatChannel& channel, std::vector<ChatMessage> messages) {
        const std::string key = chatRoomKey(_chatClient->identity().userId, channel);
        _chatHistory.insert_or_assign(key, std::move(messages));
        if (sameChannel(channel, _chatChannel))
            refreshChatMessages();
    });
    _chatClient->setMessageCallback([this](ChatMessage message) {
        ChatChannel room{message.channel, message.scope};
        if (message.channel == ChatChannelKind::Guild)
            room.scope = _chatClient->identity().guildId;
        else if (message.channel == ChatChannelKind::Direct && message.senderId != _chatClient->identity().userId)
            room.scope = message.senderId;

        auto& messages       = _chatHistory[chatRoomKey(_chatClient->identity().userId, room)];
        const bool duplicate = std::any_of(messages.begin(), messages.end(), [&message](const ChatMessage& existing) {
            return existing.messageId == message.messageId;
        });
        if (!duplicate)
        {
            messages.emplace_back(std::move(message));
            if (messages.size() > 50)
                messages.erase(messages.begin(), messages.begin() + static_cast<std::ptrdiff_t>(messages.size() - 50));
        }
        if (sameChannel(room, _chatChannel))
            refreshChatMessages();
    });
    _chatClient->setErrorCallback([this](std::string error) {
        _chatLastError = std::move(error);
        updateChatStatus(_chatLastError, ax::Color32{255, 136, 119, 255});
    });
    _chatClient->connect(_chatUrl, std::move(identity));
}

void LobbyLayer::reconnectChat()
{
    _chatReconnectScheduled = false;
    if (!_chatClient || _chatClient->state() != ChatConnectionState::Disconnected)
        return;
    ChatIdentity identity = _chatClient->identity();
    _chatClient->connect(_chatUrl, std::move(identity));
}

void LobbyLayer::showChatOverlay()
{
    Node& overlay           = createOverlay();
    const float left        = _safeArea.origin.x;
    const float bottom      = _safeArea.origin.y;
    const float width       = _safeArea.size.width;
    const float height      = _safeArea.size.height;
    const float modalWidth  = std::min(680.0F, width - 28.0F);
    const float modalHeight = std::min(1060.0F, height - 54.0F);

    auto* modal = makeChatPanel({modalWidth, modalHeight}, Color32{211, 229, 239, 255});
    modal->setPosition({left + (width - modalWidth) * 0.5F, bottom + (height - modalHeight) * 0.5F});
    modal->setTouchEnabled(true);
    overlay.addChild(modal, 10);

    auto* header = makeChatPanel({modalWidth, 76.0F}, Color32{25, 69, 96, 255});
    header->setPosition({0.0F, modalHeight - 76.0F});
    modal->addChild(header, 5);
    auto* title = makeChatText("LOBBY CHAT", 27.0F, Color32{255, 250, 232, 255});
    title->setAnchorPoint({0.0F, 0.5F});
    title->setPosition({18.0F, 49.0F});
    header->addChild(title);

    _chatStatusText = makeChatText("OFFLINE", 12.0F, Color32{255, 173, 154, 255});
    _chatStatusText->setAnchorPoint({0.0F, 0.5F});
    _chatStatusText->setPosition({20.0F, 19.0F});
    header->addChild(_chatStatusText);

    auto* close = makeChatButton({52.0F, 52.0F}, "X", Color32{196, 62, 55, 255});
    close->setPosition({modalWidth - 64.0F, 12.0F});
    close->addClickEventListener([this](Object*) { closeOverlay(); });
    header->addChild(close, 5);

    struct MainTab final
    {
        ChatChannelKind kind;
        std::string_view label;
    };
    constexpr std::array<MainTab, 3> mainTabs{{
        {ChatChannelKind::Language, "LANGUAGE"},
        {ChatChannelKind::Guild, "GUILD"},
        {ChatChannelKind::Direct, "DIRECT"},
    }};
    const float mainTabWidth = (modalWidth - 24.0F) / 3.0F;
    for (std::size_t index = 0; index < mainTabs.size(); ++index)
    {
        const MainTab& tab  = mainTabs[index];
        const bool selected = _chatChannel.kind == tab.kind;
        auto* button        = makeChatButton({mainTabWidth, 58.0F}, tab.label,
                                      selected ? Color32{47, 151, 143, 255} : Color32{83, 121, 146, 255});
        button->setPosition({12.0F + static_cast<float>(index) * mainTabWidth, modalHeight - 138.0F});
        button->addClickEventListener([this, kind = tab.kind](Object*) {
            ChatChannel channel{kind, {}};
            if (kind == ChatChannelKind::Language)
                channel.scope = _chatClient->identity().language;
            else if (kind == ChatChannelKind::Guild)
                channel.scope = _chatClient->identity().guildId;
            else
                channel.scope = std::string(
                    ax::UserDefault::getInstance()->getStringForKey("cmc.chat.direct_target", "friend-user"));
            selectChatChannel(std::move(channel));
        });
        modal->addChild(button, 4);
    }

    const float contextY = modalHeight - 202.0F;
    if (_chatChannel.kind == ChatChannelKind::Language)
    {
        struct LanguageTab final
        {
            std::string_view code;
            std::string_view label;
        };
        constexpr std::array<LanguageTab, 4> languages{{
            {"en", "English"},
            {"zh", "中文"},
            {"ko", "한국어"},
            {"ja", "日本語"},
        }};
        const float languageWidth = (modalWidth - 24.0F) / 4.0F;
        for (std::size_t index = 0; index < languages.size(); ++index)
        {
            const LanguageTab& language = languages[index];
            const bool selected         = _chatChannel.scope == language.code;
            auto* button =
                makeChatButton({languageWidth, 50.0F}, language.label,
                               selected ? Color32{121, 91, 183, 255} : Color32{134, 158, 176, 255}, language.code);
            button->setPosition({12.0F + static_cast<float>(index) * languageWidth, contextY});
            button->addClickEventListener([this, code = std::string(language.code)](Object*) {
                selectChatChannel({ChatChannelKind::Language, code});
            });
            modal->addChild(button, 4);
        }
    }
    else if (_chatChannel.kind == ChatChannelKind::Guild)
    {
        auto* guild = makeChatPanel({modalWidth - 24.0F, 50.0F}, Color32{121, 91, 183, 255});
        guild->setPosition({12.0F, contextY});
        modal->addChild(guild, 4);
        const std::string guildLabel = _chatClient->identity().guildId.empty()
                                           ? "NO GUILD MEMBERSHIP"
                                           : "GUILD  #" + _chatClient->identity().guildId;
        auto* label = makeChatText(guildLabel, 17.0F, Color32{255, 250, 234, 255}, _chatClient->identity().language);
        label->setPosition(guild->getContentSize() * 0.5F);
        guild->addChild(label);
    }
    else
    {
        auto* targetBackground = makeChatPanel({modalWidth - 142.0F, 50.0F}, Color32{238, 244, 248, 255});
        targetBackground->setPosition({12.0F, contextY});
        modal->addChild(targetBackground, 4);
        _directTarget = ui::EditBox::create({modalWidth - 164.0F, 42.0F}, ui::Scale9Sprite::create());
        _directTarget->setPosition({(modalWidth - 142.0F) * 0.5F, 25.0F});
        _directTarget->setFont(chatFontPath(_chatClient->identity().language), 17);
        _directTarget->setPlaceholderFont(chatFontPath(_chatClient->identity().language), 17);
        _directTarget->setFontColor(Color32{27, 45, 62, 255});
        _directTarget->setPlaceholderFontColor(Color32{120, 137, 149, 255});
        _directTarget->setPlaceHolder("TARGET USER ID");
        _directTarget->setInputMode(ui::EditBox::InputMode::SINGLE_LINE);
        _directTarget->setReturnType(ui::EditBox::KeyboardReturnType::DONE);
        _directTarget->setMaxLength(64);
        _directTarget->setText(_chatChannel.scope);
        targetBackground->addChild(_directTarget);

        auto* open = makeChatButton({112.0F, 50.0F}, "OPEN", Color32{121, 91, 183, 255});
        open->setPosition({modalWidth - 124.0F, contextY});
        open->addClickEventListener([this](Object*) {
            if (!_directTarget)
                return;
            std::string target(_directTarget->getText());
            if (target.empty() || target == _chatClient->identity().userId)
            {
                updateChatStatus("ENTER ANOTHER USER ID", Color32{255, 136, 119, 255});
                return;
            }
            ax::UserDefault::getInstance()->setStringForKey("cmc.chat.direct_target", target);
            selectChatChannel({ChatChannelKind::Direct, std::move(target)});
        });
        modal->addChild(open, 4);
    }

    const float listTop    = contextY - 12.0F;
    const float listBottom = 158.0F;
    _chatMessageList       = ui::ListView::create();
    _chatMessageList->setDirection(ui::ScrollView::Direction::VERTICAL);
    _chatMessageList->setContentSize({modalWidth - 24.0F, listTop - listBottom});
    _chatMessageList->setPosition({12.0F, listBottom});
    _chatMessageList->setItemsMargin(5.0F);
    _chatMessageList->setBounceEnabled(true);
    _chatMessageList->setScrollBarEnabled(false);
    _chatMessageList->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    _chatMessageList->setBackGroundColor(Color32{230, 239, 245, 255});
    _chatMessageList->setBackGroundColorOpacity(255);
    modal->addChild(_chatMessageList, 3);

    auto* inputBackground = makeChatPanel({modalWidth - 126.0F, 58.0F}, Color32{250, 252, 253, 255});
    inputBackground->setPosition({12.0F, 82.0F});
    modal->addChild(inputBackground, 4);
    const std::string_view inputLanguage = _chatChannel.kind == ChatChannelKind::Language
                                               ? std::string_view(_chatChannel.scope)
                                               : std::string_view(_chatClient->identity().language);
    _chatInput = ui::EditBox::create({modalWidth - 150.0F, 50.0F}, ui::Scale9Sprite::create());
    _chatInput->setPosition({(modalWidth - 126.0F) * 0.5F, 29.0F});
    _chatInput->setFont(chatFontPath(inputLanguage), 18);
    _chatInput->setPlaceholderFont(chatFontPath(inputLanguage), 18);
    _chatInput->setFontColor(Color32{25, 43, 59, 255});
    _chatInput->setPlaceholderFontColor(Color32{128, 143, 155, 255});
    _chatInput->setPlaceHolder("TYPE A MESSAGE");
    _chatInput->setInputMode(ui::EditBox::InputMode::SINGLE_LINE);
    _chatInput->setReturnType(ui::EditBox::KeyboardReturnType::SEND);
    _chatInput->setMaxLength(200);
    inputBackground->addChild(_chatInput);

    auto* send = makeChatButton({102.0F, 58.0F}, "SEND", Color32{42, 158, 118, 255});
    send->setPosition({modalWidth - 114.0F, 82.0F});
    send->addClickEventListener([this](Object*) { sendChatMessage(); });
    modal->addChild(send, 4);

    auto* limit = makeChatText("200 CHARACTERS MAX  |  5 MESSAGES / 10 SEC", 11.0F, Color32{79, 105, 122, 255});
    limit->setPosition({modalWidth * 0.5F, 52.0F});
    modal->addChild(limit, 4);

    refreshChatMessages();
    switch (_chatClient->state())
    {
    case ChatConnectionState::Connected:
        updateChatStatus("CONNECTED", Color32{98, 225, 159, 255});
        break;
    case ChatConnectionState::Connecting:
        updateChatStatus("CONNECTING...", Color32{255, 215, 112, 255});
        break;
    case ChatConnectionState::Disconnected:
        updateChatStatus(_chatLastError.empty() ? "OFFLINE - RETRYING" : _chatLastError, Color32{255, 136, 119, 255});
        break;
    }
}

void LobbyLayer::selectChatChannel(ChatChannel channel)
{
    if (!_chatClient)
        return;
    if (channel.kind == ChatChannelKind::Guild && _chatClient->identity().guildId.empty())
    {
        updateChatStatus("JOIN A GUILD TO USE GUILD CHAT", Color32{255, 136, 119, 255});
        return;
    }
    _chatChannel = std::move(channel);
    _chatClient->selectChannel(_chatChannel);
    if (_overlayRoot)
        showChatOverlay();
}

void LobbyLayer::sendChatMessage()
{
    if (!_chatInput || !_chatClient)
        return;
    const std::string message(_chatInput->getText());
    if (message.empty())
        return;
    if (!_chatClient->sendMessage(message))
    {
        updateChatStatus("CHAT IS NOT CONNECTED", Color32{255, 136, 119, 255});
        return;
    }
    _chatInput->setText({});
}

void LobbyLayer::refreshChatMessages()
{
    if (!_chatMessageList || !_chatClient)
        return;
    _chatMessageList->removeAllItems();
    const std::string room = chatRoomKey(_chatClient->identity().userId, _chatChannel);
    const auto history     = _chatHistory.find(room);
    if (history == _chatHistory.end() || history->second.empty())
    {
        auto* empty =
            makeChatPanel({_chatMessageList->getContentSize().width - 12.0F, 64.0F}, Color32{241, 246, 249, 255});
        auto* label =
            makeChatText("NO MESSAGES YET", 16.0F, Color32{110, 133, 148, 255}, _chatClient->identity().language);
        label->setPosition(empty->getContentSize() * 0.5F);
        empty->addChild(label);
        _chatMessageList->pushBackCustomItem(empty);
        return;
    }

    for (const ChatMessage& message : history->second)
    {
        const bool ownMessage           = message.senderId == _chatClient->identity().userId;
        const std::string_view language = message.channel == ChatChannelKind::Language
                                              ? std::string_view(message.scope)
                                              : std::string_view(message.senderLanguage);
        auto* row                       = makeChatPanel({_chatMessageList->getContentSize().width - 12.0F, 74.0F},
                                  ownMessage ? Color32{214, 240, 231, 255} : Color32{247, 249, 251, 255});
        auto* sender                    = makeChatText(ownMessage ? "YOU" : message.senderName, 14.0F,
                                    ownMessage ? Color32{31, 133, 98, 255} : Color32{48, 101, 139, 255}, language);
        sender->setAnchorPoint({0.0F, 0.5F});
        sender->setPosition({12.0F, 56.0F});
        row->addChild(sender);

        auto* body = makeChatText(message.text, 17.0F, Color32{28, 44, 57, 255}, language);
        body->setAutoSize(false);
        body->setTextAreaSize({row->getContentSize().width - 24.0F, 42.0F});
        body->setTextHorizontalAlignment(TextHAlignment::LEFT);
        body->setTextVerticalAlignment(TextVAlignment::CENTER);
        body->setAnchorPoint({0.0F, 0.5F});
        body->setPosition({12.0F, 25.0F});
        row->addChild(body);
        _chatMessageList->pushBackCustomItem(row);
    }
    _chatMessageList->jumpToBottom();
}

void LobbyLayer::updateChatStatus(std::string_view text, const ax::Color32& color)
{
    if (!_chatStatusText)
        return;
    _chatStatusText->setString(text);
    _chatStatusText->setTextColor(color);
}
}  // namespace cmc::client
