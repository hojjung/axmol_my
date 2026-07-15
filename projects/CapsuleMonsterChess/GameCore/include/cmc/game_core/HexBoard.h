#pragma once

#include "cmc/game_core/BattleTypes.h"

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
    [[nodiscard]] std::vector<std::int32_t> findPath(std::int32_t start, std::int32_t goal) const;
    void addNeighbor(std::vector<std::int32_t>& results, std::int32_t x, std::int32_t z) const;

    std::int32_t width_;
    std::int32_t height_;
    std::vector<std::uint8_t> blocked_;
    std::vector<std::int32_t> occupiedByUnit_;
};
}  // namespace cmc::game_core
