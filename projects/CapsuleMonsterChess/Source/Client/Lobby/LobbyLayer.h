#pragma once

#include "Client/Battle/BattlePresentationRequest.h"
#include "Client/Chat/ChatClient.h"
#include "Client/Content/MonsterCatalog.h"
#include "Client/Content/ProgressionCatalog.h"
#include "Client/Content/StageCatalog.h"
#include "Client/UserData/UserDataSource.h"
#include "axmol/axmol.h"
#include "axmol/ui/CocosGUI.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cmc::client
{
class LobbyLayer final : public ax::Layer
{
public:
    CREATE_FUNC(LobbyLayer);

    ~LobbyLayer() override;
    bool init() override;
    bool setUserDataSource(std::unique_ptr<IUserDataSource> source, std::string& error);
    void setWorldVisibilityCallback(std::function<void(bool)> callback);
    void setBattleLaunchCallback(std::function<void(const BattlePresentationRequest&)> callback);

private:
    enum class ScreenId
    {
        MainLobby,
        Monsters,
        Stages,
        Shop,
        Placeholder,
        Ranking,
    };

    enum class UnitDetailTab
    {
        Stats,
        Equipment,
        Skills,
    };

    enum class QuestTab
    {
        Daily,
        Career,
        Season,
    };

    ax::Node& replaceScreen();
    ax::Node& createOverlay();
    void closeOverlay();
    bool saveUserProfile();
    void showMainLobby();
    void showMonsterCatalog();
    void showStageSelect();
    void showShop();
    void showPlaceholder();
    void showRanking();
    void showChatOverlay();
    void showGachaReveal();
    void showGachaResult();
    void finishGachaReveal();
    void showUnitDetail(int unitId, UnitDetailTab tab);
    void showSubMenuOverlay();
    void showQuestOverlay(QuestTab tab);
    void showBattlePass();
    void claimQuestReward(int questId);
    void launchSelectedStage(cmc::BattleExecutionMode mode);
    void buildBottomNavigation(ax::Node& root, ScreenId selectedScreen);
    void buildDeckPanel(ax::Node& root);
    void buildMonsterGrid(ax::Node& root);
    void selectDeckPreset(std::size_t presetIndex);
    void toggleUnitInSelectedDeck(int unitId);
    void showLoadError(ax::Node& root, std::string_view message);
    void initializeChat();
    void reconnectChat();
    void selectChatChannel(ChatChannel channel);
    void sendChatMessage();
    void refreshChatMessages();
    void updateChatStatus(std::string_view text, const ax::Color32& color);

    ax::Rect _safeArea;
    MonsterCatalog _monsterCatalog;
    ProgressionCatalog _progressionCatalog;
    StageCatalog _stageCatalog;
    std::unique_ptr<IUserDataSource> _userDataSource;
    std::unique_ptr<ChatClient> _chatClient;
    UserProfile _userProfile;
    std::string _monsterLoadError;
    std::string _progressionLoadError;
    std::string _stageLoadError;
    std::string _userDataError;
    ax::Node* _screenRoot              = nullptr;
    ax::Node* _overlayRoot             = nullptr;
    ax::ui::Text* _stageSelectionText  = nullptr;
    ax::ui::Text* _stageStartText      = nullptr;
    ax::ui::Layout* _selectedStageCard = nullptr;
    ax::ui::ListView* _chatMessageList = nullptr;
    ax::ui::EditBox* _chatInput        = nullptr;
    ax::ui::EditBox* _directTarget     = nullptr;
    ax::ui::Text* _chatStatusText      = nullptr;
    ax::Color32 _selectedStageBaseColor{0, 0, 0, 255};
    std::function<void(bool)> _worldVisibilityCallback;
    std::function<void(const BattlePresentationRequest&)> _battleLaunchCallback;
    std::unordered_map<std::string, std::vector<ChatMessage>> _chatHistory;
    ChatChannel _chatChannel;
    std::string _chatUrl;
    std::string _chatLastError;
    std::size_t _stagePage         = 0;
    QuestTab _questTab             = QuestTab::Career;
    std::size_t _clearedStageCount = 2;
    int _selectedStageId           = 0;
    int _selectedStageNumber       = 0;
    int _pendingRewardUnitId       = 0;
    bool _pendingRewardWasOwned    = false;
    bool _pendingRewardApplied     = false;
    bool _chatReconnectScheduled   = false;
};
}  // namespace cmc::client
