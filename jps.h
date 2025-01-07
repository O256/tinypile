#ifndef JPS_H
#define JPS_H

#include <cmath>
#include <queue>
#include <unordered_set>
#include <vector>

struct Position {
    int x, y;

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }

    Position operator+(const Position& other) const {
        return {x + other.x, y + other.y};
    }
};

// 为Position提供哈希函数以用于unordered_set
struct PositionHash {
    std::size_t operator()(const Position& pos) const {
        return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 1);  // 哈希函数, 将x和y的哈希值进行异或运算
    }
};

struct Node {
    Position pos;
    Node* parent;
    float f, g, h;

    Node(Position p, Node* par = nullptr) : pos(p), parent(par), f(0), g(0), h(0) {
    }
};

// 用于优先队列的比较器
struct CompareNode {
    bool operator()(const Node* a, const Node* b) const {
        return a->f > b->f;
    }
};

class JPS {
public:
    JPS(const std::vector<std::vector<bool>>& grid);
    ~JPS();

    std::vector<Position> findPath(Position start, Position goal);

private:
    std::vector<std::vector<bool>> grid_;
    int width_, height_;
    Position goal_;

    // 方向数组：直线方向和对角线方向
    static const std::vector<Position> DIRECTIONS;

    bool isValid(const Position& pos) const;
    bool isWalkable(const Position& pos) const;
    float getHeuristic(const Position& pos) const;
    std::vector<Position> identifySuccessors(Node* current);
    Position jump(Position current, Position direction);
    std::vector<Position> findNeighbors(Node* current);
    bool hasForceNeighbor(const Position& pos, const Position& direction);
    std::vector<Position> reconstructPath(Node* endNode);
    void pruneNeighbors(const Position& pos, std::vector<Position>& neighbors);
};

#endif  // JPS_H