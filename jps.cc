#include "jps.h"
#include <algorithm>

const std::vector<Position> JPS::DIRECTIONS = {
    {0, 1}, {1, 0},  {0, -1}, {-1, 0},  // 直线方向
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  // 对角线方向
};

JPS::JPS(const std::vector<std::vector<bool>>& grid) : grid_(grid), width_(grid[0].size()), height_(grid.size()) {
}

JPS::~JPS() {
}

bool JPS::isValid(const Position& pos) const {
    return pos.x >= 0 && pos.x < width_ && pos.y >= 0 && pos.y < height_;
}

bool JPS::isWalkable(const Position& pos) const {
    return isValid(pos) && grid_[pos.y][pos.x];
}

float JPS::getHeuristic(const Position& pos) const {
    // 使用曼哈顿距离
    return std::abs(pos.x - goal_.x) + std::abs(pos.y - goal_.y);
}

Position JPS::jump(Position current, Position direction) {
    Position next = {current.x + direction.x, current.y + direction.y};

    if (!isWalkable(next)) {
        return {-1, -1}; // 如果跳跃后的位置无效，则返回-1
    }

    if (next == goal_) {
        return next;
    }

    // 检查强制邻居
    if (hasForceNeighbor(next, direction)) {
        return next;
    }

    // 对角线移动
    if (direction.x != 0 && direction.y != 0) {
        // 检查水平和垂直方向
        Position horizJump = jump(next, {direction.x, 0});
        Position vertJump = jump(next, {0, direction.y});
        if (horizJump.x != -1 || vertJump.x != -1) {
            return next; // 如果水平或垂直方向有跳跃点，则返回跳跃点
        }
    }

    // 继续在当前方向跳跃
    return jump(next, direction);
}

bool JPS::hasForceNeighbor(const Position& pos, const Position& direction) {
    // 直线移动时检查强制邻居
    if (direction.x == 0 || direction.y == 0) {
        for (const auto& dir : DIRECTIONS) {

            // 如果方向相同，则跳过，或者方向相反，则跳过
            if (dir == direction || dir.x == -direction.x || dir.y == -direction.y) {
                continue;
            }

            // 检查强制邻居
            Position neighbor = pos + dir;
            Position blocked = {pos.x - direction.x, pos.y - direction.y}; // 检查障碍物
            if (!isWalkable(blocked) && isWalkable(neighbor)) {
                return true;
            }
        }
    }
    return false;
}

// 查找邻居
std::vector<Position> JPS::findNeighbors(Node* current) {
    std::vector<Position> neighbors;

    // 如果是起点，考虑所有方向
    if (!current->parent) {
        for (const auto& dir : DIRECTIONS) {
            Position next = current->pos + dir;
            if (isWalkable(next)) {
                neighbors.push_back(next); // 如果邻居是可行走的，则加入邻居列表
            }
        }
        return neighbors;
    }

    // 获取移动方向
    Position direction = {
        (current->pos.x - current->parent->pos.x) / std::max(1, std::abs(current->pos.x - current->parent->pos.x)),
        (current->pos.y - current->parent->pos.y) / std::max(1, std::abs(current->pos.y - current->parent->pos.y))};

    // 添加自然邻居
    Position next = current->pos + direction;
    if (isWalkable(next)) {
        neighbors.push_back(next); // 如果邻居是可行走的，则加入邻居列表
    }

    pruneNeighbors(current->pos, neighbors); // 移除不可行走的邻居
    return neighbors;
}

void JPS::pruneNeighbors(const Position& pos, std::vector<Position>& neighbors) {
    // 移除不可行走的邻居
    neighbors.erase(std::remove_if(neighbors.begin(), neighbors.end(),
                                   [this](const Position& p) {
                                       return !isWalkable(p);
                                   }),
                    neighbors.end());
}

std::vector<Position> JPS::identifySuccessors(Node* current) {
    std::vector<Position> successors;
    auto neighbors = findNeighbors(current);

    for (const auto& neighbor : neighbors) {
        Position direction = {(neighbor.x - current->pos.x) / std::max(1, std::abs(neighbor.x - current->pos.x)),
                              (neighbor.y - current->pos.y) / std::max(1, std::abs(neighbor.y - current->pos.y))};

        Position jumpPoint = jump(current->pos, direction);
        if (jumpPoint.x != -1) {
            successors.push_back(jumpPoint);
        }
    }

    return successors;
}

std::vector<Position> JPS::reconstructPath(Node* endNode) {
    std::vector<Position> path;
    Node* current = endNode;

    while (current != nullptr) {
        path.push_back(current->pos);
        current = current->parent;
    }

    std::reverse(path.begin(), path.end());
    return path;
}

// 查找路径
std::vector<Position> JPS::findPath(Position start, Position goal) {
    goal_ = goal; // 设置目标位置

    if (!isWalkable(start) || !isWalkable(goal)) {
        return {};
    }

    std::priority_queue<Node*, std::vector<Node*>, CompareNode> openList; // 优先队列
    std::unordered_set<Position, PositionHash> closedList;

    Node* startNode = new Node(start);
    startNode->h = getHeuristic(start); // 设置启发式值
    startNode->f = startNode->h; // 设置f值，f = g + h

    openList.push(startNode); // 将起点加入优先队列

    while (!openList.empty()) {
        Node* current = openList.top();
        openList.pop();

        if (current->pos == goal) {
            auto path = reconstructPath(current);
            // 清理内存
            while (!openList.empty()) {
                delete openList.top();
                openList.pop();
            }
            return path;
        }

        closedList.insert(current->pos);

        auto successors = identifySuccessors(current);
        for (const auto& succ : successors) {
            if (closedList.find(succ) != closedList.end()) {
                continue;
            }

            float newG =
                current->g + std::sqrt(std::pow(succ.x - current->pos.x, 2) + std::pow(succ.y - current->pos.y, 2)); // 计算新g值

            Node* successor = new Node(succ, current);
            successor->g = newG;
            successor->h = getHeuristic(succ); // 计算新启发式值
            successor->f = successor->g + successor->h; // 计算新f值

            openList.push(successor); // 将后继节点加入优先队列
        }
    }

    return {};
}