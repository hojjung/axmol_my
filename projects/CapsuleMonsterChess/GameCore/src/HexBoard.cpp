#include "cmc/game_core/HexBoard.h"

#include <cstdlib>
#include <limits>

namespace cmc::game_core
{
namespace
{
[[nodiscard]] bool isSupportedBoardSize(std::int32_t width, std::int32_t height) noexcept
{
    if (width <= 0 || height <= 0 || width > MAX_BOARD_WIDTH || height > MAX_BOARD_HEIGHT)
        return false;
    return static_cast<std::int64_t>(width) * static_cast<std::int64_t>(height) <= MAX_BOARD_CELLS;
}
}  // namespace

HexBoard::HexBoard(std::int32_t width, std::int32_t height, const std::vector<std::int32_t>& blockedCells)
    : width_(width <= 0 ? DEFAULT_BOARD_WIDTH : width), height_(height <= 0 ? DEFAULT_BOARD_HEIGHT : height)
{
    if (!isSupportedBoardSize(width_, height_))
    {
        width_  = DEFAULT_BOARD_WIDTH;
        height_ = DEFAULT_BOARD_HEIGHT;
    }

    occupiedByUnit_.fill(-1);
    for (const auto cell : blockedCells)
    {
        if (isValidIndex(cell))
            blocked_[static_cast<std::size_t>(cell)] = 1U;
    }
}

std::int32_t HexBoard::toIndex(std::int32_t x, std::int32_t z) const noexcept
{
    if (x < 0 || x >= width_ || z < 0 || z >= height_)
        return -1;
    return x + z * width_;
}

HexCoord HexBoard::toCoord(std::int32_t index) const noexcept
{
    return {index % width_, index / width_};
}

bool HexBoard::isValidIndex(std::int32_t index) const noexcept
{
    return index >= 0 && index < cellCount();
}

bool HexBoard::isBlocked(std::int32_t index) const noexcept
{
    return !isValidIndex(index) || blocked_[static_cast<std::size_t>(index)] != 0U;
}

bool HexBoard::isOccupied(std::int32_t index) const noexcept
{
    return isValidIndex(index) && occupiedByUnit_[static_cast<std::size_t>(index)] >= 0;
}

std::int32_t HexBoard::occupant(std::int32_t index) const noexcept
{
    return isValidIndex(index) ? occupiedByUnit_[static_cast<std::size_t>(index)] : -1;
}

bool HexBoard::canStand(std::int32_t index) const noexcept
{
    return isValidIndex(index) && blocked_[static_cast<std::size_t>(index)] == 0U &&
           occupiedByUnit_[static_cast<std::size_t>(index)] < 0;
}

void HexBoard::occupy(std::int32_t index, std::int32_t unitId) noexcept
{
    if (isValidIndex(index))
        occupiedByUnit_[static_cast<std::size_t>(index)] = unitId;
}

void HexBoard::clearOccupant(std::int32_t index, std::int32_t unitId) noexcept
{
    if (isValidIndex(index) && occupiedByUnit_[static_cast<std::size_t>(index)] == unitId)
        occupiedByUnit_[static_cast<std::size_t>(index)] = -1;
}

bool HexBoard::moveOccupant(std::int32_t from, std::int32_t to, std::int32_t unitId) noexcept
{
    if (!canStand(to) || !isValidIndex(from) || occupiedByUnit_[static_cast<std::size_t>(from)] != unitId)
        return false;

    occupiedByUnit_[static_cast<std::size_t>(from)] = -1;
    occupiedByUnit_[static_cast<std::size_t>(to)]   = unitId;
    return true;
}

std::int32_t HexBoard::distance(std::int32_t a, std::int32_t b) const noexcept
{
    if (!isValidIndex(a) || !isValidIndex(b))
        return std::numeric_limits<std::int32_t>::max();

    const auto [ax, az] = toCoord(a);
    const auto [bx, bz] = toCoord(b);
    const auto aq       = ax - (az - (az & 1)) / 2;
    const auto ar       = az;
    const auto bq       = bx - (bz - (bz & 1)) / 2;
    const auto br       = bz;
    const auto as       = -aq - ar;
    const auto bs       = -bq - br;
    return (std::abs(aq - bq) + std::abs(ar - br) + std::abs(as - bs)) / 2;
}

void HexBoard::getNeighbors(std::int32_t index, std::vector<std::int32_t>& results) const
{
    results.clear();
    const auto list = neighbors(index);
    if (list.count == 0)
        return;
    results.reserve(MAX_NEIGHBORS);
    results.insert(results.end(), list.begin(), list.end());
}

bool HexBoard::tryFindNextStep(std::int32_t from,
                               std::int32_t target,
                               std::int32_t range,
                               std::int32_t& nextStep,
                               std::int32_t& targetDistance) const
{
    nextStep = -1;
    if (!isValidIndex(from) || !isValidIndex(target) || range < 0)
    {
        targetDistance = std::numeric_limits<std::int32_t>::max();
        return false;
    }
    targetDistance = distance(from, target);
    if (targetDistance <= range)
        return true;

    NeighborList candidates;
    for (const auto cell : neighbors(target))
    {
        if (distance(cell, target) <= range && (canStand(cell) || cell == from))
            candidates.cells[candidates.count++] = cell;
    }

    std::size_t bestLength = std::numeric_limits<std::size_t>::max();
    std::int32_t bestStep   = -1;
    for (const auto candidate : candidates)
    {
        std::int32_t firstStep = -1;
        std::size_t pathLength = 0;
        if (findPathFirstStep(from, candidate, firstStep, pathLength) && pathLength > 1U && pathLength < bestLength)
        {
            bestLength = pathLength;
            bestStep   = firstStep;
        }
    }

    if (bestStep < 0)
        return false;
    nextStep = bestStep;
    return true;
}

std::int32_t HexBoard::getBestEmptyNeighborNear(std::int32_t target, std::int32_t from) const
{
    std::int32_t best         = -1;
    std::int32_t bestDistance = std::numeric_limits<std::int32_t>::max();
    for (const auto cell : neighbors(target))
    {
        if (!canStand(cell))
            continue;
        const auto candidateDistance = distance(from, cell);
        if (candidateDistance < bestDistance)
        {
            bestDistance = candidateDistance;
            best         = cell;
        }
    }
    return best;
}

std::int32_t HexBoard::getKnockbackCell(std::int32_t actorCell, std::int32_t targetCell) const
{
    std::int32_t best         = -1;
    std::int32_t bestDistance = distance(actorCell, targetCell);
    for (const auto cell : neighbors(targetCell))
    {
        if (!canStand(cell))
            continue;
        const auto candidateDistance = distance(actorCell, cell);
        if (candidateDistance > bestDistance)
        {
            bestDistance = candidateDistance;
            best         = cell;
        }
    }
    return best;
}

HexBoard::NeighborList HexBoard::neighbors(std::int32_t index) const noexcept
{
    NeighborList results;
    if (!isValidIndex(index))
        return results;

    const auto [x, z] = toCoord(index);
    const bool odd    = (z & 1) == 1;
    addNeighbor(results, x + 1, z);
    addNeighbor(results, x - 1, z);
    addNeighbor(results, x + (odd ? 1 : 0), z + 1);
    addNeighbor(results, x + (odd ? 0 : -1), z + 1);
    addNeighbor(results, x + (odd ? 1 : 0), z - 1);
    addNeighbor(results, x + (odd ? 0 : -1), z - 1);
    return results;
}

void HexBoard::beginPathSearch() const noexcept
{
    ++searchStamp_;
    if (searchStamp_ == 0)
    {
        cellSearchStamp_.fill(0);
        searchStamp_ = 1;
    }
    openCount_ = 0;
}

bool HexBoard::findPathFirstStep(std::int32_t start,
                                 std::int32_t goal,
                                 std::int32_t& firstStep,
                                 std::size_t& pathLength) const noexcept
{
    firstStep  = -1;
    pathLength = 0;
    if (!isValidIndex(start) || !isValidIndex(goal))
        return false;
    if (start == goal)
    {
        pathLength = 1;
        return true;
    }

    beginPathSearch();
    const auto startIndex          = static_cast<std::size_t>(start);
    cellSearchStamp_[startIndex]   = searchStamp_;
    searchState_[startIndex]       = SearchState::Open;
    cameFrom_[startIndex]          = -1;
    gScore_[startIndex]            = 0;
    open_[openCount_++]            = start;

    while (openCount_ != 0)
    {
        std::size_t currentPosition = 0U;
        auto current                = open_[0];
        auto currentF               = gScore_[static_cast<std::size_t>(current)] + distance(current, goal);
        for (std::size_t i = 1U; i < openCount_; ++i)
        {
            const auto candidate = open_[i];
            const auto f         = gScore_[static_cast<std::size_t>(candidate)] + distance(candidate, goal);
            if (f < currentF || (f == currentF && candidate < current))
            {
                currentPosition = i;
                current         = candidate;
                currentF        = f;
            }
        }

        for (auto i = currentPosition + 1U; i < openCount_; ++i)
            open_[i - 1U] = open_[i];
        --openCount_;

        if (current == goal)
        {
            pathLength = 1;
            auto cursor = goal;
            while (cursor != start)
            {
                const auto parent = cameFrom_[static_cast<std::size_t>(cursor)];
                if (parent < 0)
                    return false;
                ++pathLength;
                if (parent == start)
                    firstStep = cursor;
                cursor = parent;
            }
            return true;
        }

        searchState_[static_cast<std::size_t>(current)] = SearchState::Closed;
        for (const auto next : neighbors(current))
        {
            const auto nextIndex = static_cast<std::size_t>(next);
            const bool seen      = cellSearchStamp_[nextIndex] == searchStamp_;
            if ((seen && searchState_[nextIndex] == SearchState::Closed) || isBlocked(next) ||
                (isOccupied(next) && next != goal))
                continue;

            const auto tentative = gScore_[static_cast<std::size_t>(current)] + 1;
            if (seen && tentative >= gScore_[nextIndex])
                continue;

            cameFrom_[nextIndex] = current;
            gScore_[nextIndex]   = tentative;
            if (!seen)
            {
                cellSearchStamp_[nextIndex] = searchStamp_;
                searchState_[nextIndex]     = SearchState::Open;
                open_[openCount_++]          = next;
            }
        }
    }
    return false;
}

void HexBoard::addNeighbor(NeighborList& results, std::int32_t x, std::int32_t z) const noexcept
{
    const auto index = toIndex(x, z);
    if (index >= 0)
        results.cells[results.count++] = index;
}
}  // namespace cmc::game_core
