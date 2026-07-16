#pragma once

#include "cmc/game_core/BattleTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmc::game_core
{
class HexBoard final
{
public:
    HexBoard(std::int32_t width, std::int32_t height, const std::vector<std::int32_t>& blockedCells);
    explicit HexBoard(const BattleBoardConfig& config) : HexBoard(config.width, config.height, config.blockedCells) {}

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }
    [[nodiscard]] std::int32_t height() const noexcept { return height_; }
    [[nodiscard]] std::int32_t cellCount() const noexcept { return width_ * height_; }

    [[nodiscard]] std::int32_t toIndex(std::int32_t x, std::int32_t z) const noexcept;
    [[nodiscard]] std::int32_t toIndex(HexCoord coord) const noexcept { return toIndex(coord.x, coord.z); }
    [[nodiscard]] HexCoord toCoord(std::int32_t index) const noexcept;
    [[nodiscard]] bool isValidIndex(std::int32_t index) const noexcept;
    [[nodiscard]] bool isBlocked(std::int32_t index) const noexcept;
    [[nodiscard]] bool isOccupied(std::int32_t index) const noexcept;
    [[nodiscard]] std::int32_t occupant(std::int32_t index) const noexcept;
    [[nodiscard]] bool canStand(std::int32_t index) const noexcept;

    void occupy(std::int32_t index, std::int32_t unitId) noexcept;
    void clearOccupant(std::int32_t index, std::int32_t unitId) noexcept;
    [[nodiscard]] bool moveOccupant(std::int32_t from, std::int32_t to, std::int32_t unitId) noexcept;

    [[nodiscard]] std::int32_t distance(std::int32_t a, std::int32_t b) const noexcept;
    void getNeighbors(std::int32_t index, std::vector<std::int32_t>& results) const;
    [[nodiscard]] bool tryFindNextStep(std::int32_t from,
                                       std::int32_t target,
                                       std::int32_t range,
                                       std::int32_t& nextStep,
                                       std::int32_t& targetDistance) const;
    [[nodiscard]] std::int32_t getBestEmptyNeighborNear(std::int32_t target, std::int32_t from) const;
    [[nodiscard]] std::int32_t getKnockbackCell(std::int32_t actorCell, std::int32_t targetCell) const;

private:
    static constexpr std::size_t MAX_NEIGHBORS = 6;

    struct NeighborList final
    {
        std::array<std::int32_t, MAX_NEIGHBORS> cells{};
        std::size_t count = 0;

        [[nodiscard]] const std::int32_t* begin() const noexcept { return cells.data(); }
        [[nodiscard]] const std::int32_t* end() const noexcept { return cells.data() + count; }
    };

    enum class SearchState : std::uint8_t
    {
        Open,
        Closed,
    };

    [[nodiscard]] NeighborList neighbors(std::int32_t index) const noexcept;
    void addNeighbor(NeighborList& results, std::int32_t x, std::int32_t z) const noexcept;
    void beginPathSearch() const noexcept;
    [[nodiscard]] bool findPathFirstStep(std::int32_t start,
                                         std::int32_t goal,
                                         std::int32_t& firstStep,
                                         std::size_t& pathLength) const noexcept;

    std::int32_t width_;
    std::int32_t height_;
    std::array<std::uint8_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> blocked_{};
    std::array<std::int32_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> occupiedByUnit_{};

    mutable std::uint32_t searchStamp_ = 0;
    mutable std::array<std::uint32_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> cellSearchStamp_{};
    mutable std::array<SearchState, static_cast<std::size_t>(MAX_BOARD_CELLS)> searchState_{};
    mutable std::array<std::int32_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> cameFrom_{};
    mutable std::array<std::int32_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> gScore_{};
    mutable std::array<std::int32_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> open_{};
    mutable std::size_t openCount_ = 0;
};
}  // namespace cmc::game_core
