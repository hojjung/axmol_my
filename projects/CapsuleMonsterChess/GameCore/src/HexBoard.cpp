#include "cmc/game_core/HexBoard.h"

#include <algorithm>
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

    const auto count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    blocked_.assign(count, 0U);
    occupiedByUnit_.assign(count, -1);
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
    if (!isValidIndex(index))
        return;

    results.reserve(6);
    const auto [x, z] = toCoord(index);
    const bool odd    = (z & 1) == 1;
    addNeighbor(results, x + 1, z);
    addNeighbor(results, x - 1, z);
    addNeighbor(results, x + (odd ? 1 : 0), z + 1);
    addNeighbor(results, x + (odd ? 0 : -1), z + 1);
    addNeighbor(results, x + (odd ? 1 : 0), z - 1);
    addNeighbor(results, x + (odd ? 0 : -1), z - 1);
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

    std::vector<std::int32_t> candidates;
    std::vector<std::int32_t> neighbors;
    candidates.reserve(6);
    getNeighbors(target, neighbors);
    for (const auto cell : neighbors)
    {
        if (distance(cell, target) <= range && (canStand(cell) || cell == from))
            candidates.push_back(cell);
    }

    std::size_t bestLength = std::numeric_limits<std::size_t>::max();
    std::vector<std::int32_t> bestPath;
    for (const auto candidate : candidates)
    {
        auto path = findPath(from, candidate);
        if (path.size() > 1U && path.size() < bestLength)
        {
            bestLength = path.size();
            bestPath   = std::move(path);
        }
    }

    if (bestPath.empty())
        return false;
    nextStep = bestPath[1];
    return true;
}

std::int32_t HexBoard::getBestEmptyNeighborNear(std::int32_t target, std::int32_t from) const
{
    std::vector<std::int32_t> neighbors;
    getNeighbors(target, neighbors);
    std::int32_t best         = -1;
    std::int32_t bestDistance = std::numeric_limits<std::int32_t>::max();
    for (const auto cell : neighbors)
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
    std::vector<std::int32_t> neighbors;
    getNeighbors(targetCell, neighbors);
    std::int32_t best         = -1;
    std::int32_t bestDistance = distance(actorCell, targetCell);
    for (const auto cell : neighbors)
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

std::vector<std::int32_t> HexBoard::findPath(std::int32_t start, std::int32_t goal) const
{
    if (!isValidIndex(start) || !isValidIndex(goal))
        return {};
    if (start == goal)
        return {start};

    const auto count = static_cast<std::size_t>(cellCount());
    std::vector<std::uint8_t> closed(count, 0U);
    std::vector<std::int32_t> cameFrom(count, -1);
    std::vector<std::int32_t> gScore(count, std::numeric_limits<std::int32_t>::max());
    std::vector<std::int32_t> open{start};
    std::vector<std::int32_t> neighbors;
    open.reserve(count);
    gScore[static_cast<std::size_t>(start)] = 0;

    while (!open.empty())
    {
        std::size_t currentPosition = 0U;
        auto current                = open[0];
        auto currentF               = gScore[static_cast<std::size_t>(current)] + distance(current, goal);
        for (std::size_t i = 1U; i < open.size(); ++i)
        {
            const auto candidate = open[i];
            const auto f         = gScore[static_cast<std::size_t>(candidate)] + distance(candidate, goal);
            if (f < currentF || (f == currentF && candidate < current))
            {
                currentPosition = i;
                current         = candidate;
                currentF        = f;
            }
        }

        open.erase(open.begin() + static_cast<std::ptrdiff_t>(currentPosition));
        if (current == goal)
        {
            std::vector<std::int32_t> path{current};
            while (cameFrom[static_cast<std::size_t>(current)] >= 0)
            {
                current = cameFrom[static_cast<std::size_t>(current)];
                path.push_back(current);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        closed[static_cast<std::size_t>(current)] = 1U;
        getNeighbors(current, neighbors);
        for (const auto next : neighbors)
        {
            const auto nextIndex = static_cast<std::size_t>(next);
            if (closed[nextIndex] != 0U || isBlocked(next) || (isOccupied(next) && next != goal))
                continue;

            const auto tentative = gScore[static_cast<std::size_t>(current)] + 1;
            if (tentative >= gScore[nextIndex])
                continue;

            cameFrom[nextIndex] = current;
            gScore[nextIndex]   = tentative;
            if (std::find(open.begin(), open.end(), next) == open.end())
                open.push_back(next);
        }
    }
    return {};
}

void HexBoard::addNeighbor(std::vector<std::int32_t>& results, std::int32_t x, std::int32_t z) const
{
    const auto index = toIndex(x, z);
    if (index >= 0)
        results.push_back(index);
}
}  // namespace cmc::game_core
