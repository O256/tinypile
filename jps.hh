#pragma once

/*
公共域的Jump Point Search实现--非常快速的统一代价网格寻路。
向下滚动查看编译配置、使用提示和示例代码。

License:
  公共域、WTFPL、CC0或您所在国家可用的任何宽松许可证。

依赖:
  libc (stdlib.h, math.h) 默认情况下，通过下面的定义修改使用您自己的函数。(realloc(), free(), sqrt())
  编译为C++98，不需要C++11或STL。
  不抛出异常，不使用RTTI，不包含任何虚方法。

Thread safety:
  无全局状态。Searcher实例不是线程安全的。Grid模板类由您决定。
  如果在寻路时网格访问是只读的，您可以让多个线程同时计算路径，
  每个线程使用自己的Searcher实例。

Background:
  如果您想在具有以下特性的地图上生成路径：
  - 您有一个2D网格（恰好两个维度！），每个格子恰好有8个邻居（上下左右+对角线）
  - 没有“成本”——一个格子要么是可行走的，要么是不可行走的。
  那么您可能想避免使用完整的A*，转而使用Jump Point Search（这个库）。
  JPS通常比普通的A*更快，只要您的格子可行走检查函数足够快。

Origin:
  https://github.com/fgenesis/tinypile/blob/master/jps.hh

Based on my older implementation:
  https://github.com/fgenesis/jps/blob/master/JPS.h
  (For changes compared to that version go to the end of this file)

Inspired by:
  http://users.cecs.anu.edu.au/~dharabor/data/papers/harabor-grastien-aaai11.pdf (The original paper)
  https://github.com/Yonaba/Jumper
  https://github.com/qiao/PathFinding.js

Usage:
    1. 定义一个重载`operator()(x, y) const`的类，返回一个可以被视为布尔值的值。
    2. 您需要负责边界检查！
    3. 您希望您的operator()尽可能快且小，因为它会被频繁调用。
    4. 请您的编译器强制内联它，如果可能的话。

// --- 开始示例代码 ---

struct MyGrid
{
    inline bool operator()(unsigned x, unsigned y) const // coordinates must be unsigned; method must be const
    {
        if(x < width && y < height) // Unsigned will wrap if < 0
            ... return true if terrain at (x, y) is walkable.
        // return false if terrain is not walkable or out-of-bounds.
    }
    unsigned width, height;
};

// 然后你可以检索路径：

MyGrid grid(... set grid width, height, map data, whatever);

const unsigned step = 0; // 0 将路径尽可能压缩并只记录路径点。
                         // 设置为1，如果您想要一个详细的单步路径
                         // (例如，如果您计划进一步修改路径)，
                         // 或任何其他更高的值以输出每N个位置。
                         // (waypoints总是会被输出，无论步长是多少。)

JPS::PathVector path; // 结果路径将在这里。
                      // 您也可以使用std::vector或任何其他类型，只要您的vector类型
                      // 具有push_back(), begin(), end(), resize()方法（语义与std::vector相同）。
                      // 注意，路径不会包括起始位置！
                      // --> 如果调用时start == end，它将报告找到路径，
                      //    但结果路径向量将为空！

// 单次调用接口：
// (有关此函数的更多详细信息，请参阅文件底部。)
// 注意，路径向量不会被清除！新路径点附加在末尾。
bool found = JPS::findPath(path, grid, startx, starty, endx, endy, step);


// --- 或者，如果您想要更多的控制和效率，用于重复路径查找： ---

// 使用Searcher实例（可以是类成员、堆栈上的对象，等等）
// 确保传递的grid引用在整个searcher的生命周期内保持有效。
// 如果您需要控制内存分配，您可以传递一个额外的指针，该指针将
// 转发到您自己的JPS_realloc & JPS_free（如果您设置了这些）。否则，它将被忽略。
JPS::Searcher<MyGrid> search(grid, userPtr = NULL);

// 从waypoints构建路径：
JPS::Position a, b, c, d = <...>; // 设置一些waypoints
if (search.findPath(path, a, b)
 && search.findPath(path, b, c)
 && search.findPath(path, c, d))
{
    // found path: a->b->c->d
}

// 保持重用现有的pathfinder实例
while(whatever)
{
    // Set startx, starty, endx, endy = <...>
    if(!search.findPath(path, JPS::Pos(startx, starty), JPS::Pos(endx, endy), step))
    {
        // ...handle failure...
    }
}

// 如果需要，您可以释放内部内存——这从来不是必需的；无论是为了性能，还是为了正确性。
// 如果您在释放内存后进行路径查找，它将分配新的内存。
// 注意，释放内存会中止任何当前正在进行的路径查找。
search.freeMemory();

// 如果您需要知道searcher内部分配了多少内存：
unsigned bytes = search.getTotalMemoryInUse();


// -------------------------------
// --- 增量路径查找 ---
// -------------------------------

调用JPS::findPath()或Searcher<>::findPath()总是计算整个路径或返回失败。
如果路径较长或成本较高，并且您有严格的CPU预算，您可能希望增量地进行路径查找，
跨多个帧进行。

首先，调用
  ### JPS_Result res = search.findPathInit(Position start, Position end) ###
不要忘记检查返回值，因为它可能返回：
- JPS_NO_PATH如果一个或两个点被阻挡
- JPS_EMPTY_PATH如果点相等且未被阻挡
- JPS_FOUND_PATH如果初始贪婪启发式可以快速找到路径。
- JPS_OUT_OF_MEMORY如果...好吧，是的。
If it returns JPS_NEED_MORE_STEPS then the next part can start.

重复调用
  ### JPS_Result res = search.findPathStep(int limit) ###
直到它返回JPS_NO_PATH或JPS_FOUND_PATH，或JPS_OUT_OF_MEMORY。
为了保持一致性，您将希望确保网格在后续调用之间不会发生变化；
如果网格发生变化，部分路径可能会穿过现在被阻挡的区域，或者可能不再是最优的。
如果limit为0，它将一次性执行路径查找。值> 0暂停搜索
尽可能快地超过步骤数，返回NEED_MORE_STEPS。
使用search.getStepsDone()在某些测试运行后找到limit的好值。

在获得JPS_FOUND_PATH后，通过
尽可能快地超过步骤数，返回NEED_MORE_STEPS。
使用search.getStepsDone()在某些测试运行后找到limit的好值。

在获得JPS_FOUND_PATH后，通过
  ### JPS_Result res = search.findPathFinish(PathVector& path, unsigned step = 0) ###
如上所述，路径点附加，步长参数可以调整粒度。
返回JPS_FOUND_PATH如果路径成功构建并附加到路径向量。
返回JPS_NO_PATH如果路径查找未完成或生成路径失败。
如果路径向量必须调整大小但分配失败，则可能返回JPS_OUT_OF_MEMORY。

如果findPathInit()或findPathStep()返回JPS_OUT_OF_MEMORY，则当前searcher进度未定义。
要恢复，在其他地方释放一些内存，并调用findPathInit()再次尝试。

如果findPathFinish()返回内存不足但先前步骤成功完成，
则找到的路径仍对生成路径向量有效。
在这种情况下，您可以在释放一些内存后再次调用findPathFinish()。

如果您不担心内存，将JPS_OUT_OF_MEMORY视为JPS_NO_PATH。

您可以传递JPS::PathVector、std::vector或您自己的findPathFinish()。
注意，如果传递的路径向量类型在分配失败时抛出异常（例如std::vector），
您将获得该异常，并且路径向量将处于成功插入最后一个元素时的状态。
如果未抛出异常（即您使用JPS::PathVector），则失败情况不会修改路径向量。

您可以随时通过findPathInit()、freeMemory()或销毁searcher实例来中止搜索。
中止或开始搜索将reset()返回的值。getStepsDone()和.getNodesExpanded()到0。

*/

// ============================
// ====== COMPILE CONFIG ======
// ============================

// 如果您想避免sqrt()或浮点数，请定义此项。
// 在某些测试中，这比使用sqrt()快12%，因此它是默认值。
#define JPS_NO_FLOAT

// ------------------------------------------------

#include <stddef.h>  // for size_t (needed for operator new)

// Assertions
#ifndef JPS_ASSERT
#ifdef _DEBUG
#include <assert.h>
#define JPS_ASSERT(cond) assert(cond)
#else
#define JPS_ASSERT(cond)
#endif
#endif

// 默认分配器使用realloc(), free()。如果需要，请更改。
// 您将获得您传递给findPath()或Searcher ctor的user指针。
#if !defined(JPS_realloc) || !defined(JPS_free)
#include <stdlib.h>  // for realloc, free
#ifndef JPS_realloc
#define JPS_realloc(p, newsize, oldsize, user) realloc(p, newsize)
#endif
#ifndef JPS_free
#define JPS_free(p, oldsize, user) free(p)
#endif
#endif

#ifdef JPS_NO_FLOAT
#define JPS_HEURISTIC_ACCURATE(a, b) (Heuristic::Chebyshev(a, b))
#else
#ifndef JPS_sqrt
// for Euclidean heuristic.
#include <math.h>
#define JPS_sqrt(x) sqrtf(float(x))  // float cast here avoids a warning about implicit int->float cast
#endif
#endif

// 使用哪种启发式。
// 基本属性：距离估计，返回值>= 0。越小越好。
// 准确的启发式应该总是返回小于或等于估计启发式的值，
// 否则生成的路径可能不是最优的。
// （经验法则是估计值快速但可能高估）
// 有关启发式实现的更多信息，请向下滚动。
#ifndef JPS_HEURISTIC_ACCURATE
#define JPS_HEURISTIC_ACCURATE(a, b) (Heuristic::Euclidean(a, b))
#endif

#ifndef JPS_HEURISTIC_ESTIMATE
#define JPS_HEURISTIC_ESTIMATE(a, b) (Heuristic::Manhattan(a, b))
#endif

// --- 数据类型 ---
namespace JPS {

// 无符号整数类型，足够存储一个网格轴上的位置。
// 注意，在x86上，u32实际上比u16更快。
typedef unsigned PosType;

// 启发式的结果。也可以是(unsigned) int，但默认使用float，因为sqrtf()返回float，
// 我们不需要通过float->int来转换。如果使用整数启发式，请更改。
// （使用sqrt()的欧几里得启发式即使转换为int也能正常工作。您的选择真的。）
#ifdef JPS_NO_FLOAT
typedef int ScoreType;
#else
typedef float ScoreType;
#endif

// 大小类型；用于内部向量和类似结构。您可以将其设置为size_t，但如果需要，请更改。32位足够了。
typedef unsigned SizeT;

}  // end namespace JPS

// ================================
// ====== COMPILE CONFIG END ======
// ================================
// ----------------------------------------------------------------------------------------

typedef unsigned JPS_Flags;

enum JPS_Flags_ {
    // 无特殊行为
    JPS_Flag_Default = 0x00,

    // 如果定义了此项，则禁用贪婪的直接短路径检查，该检查避免了JPS的大面积扫描。
    // 这只是性能调整。在不断重新规划短路径且没有障碍物时，可能会节省大量CPU
    // （例如，一个实体紧跟在另一个实体后面）。
    // 不改变结果的优化性。如果您在开始路径查找之前执行自己的线视检查，
    // 则可以禁用贪婪，因为检查两次是不必要的，
    // 但通常最好保持启用它。
    JPS_Flag_NoGreedy = 0x01,

    // 如果定义了此项，则使用标准的A*而不是JPS（例如，如果您想在您的场景中比较性能）。
    // 在大多数情况下，这将慢得多，但可能有益于您的网格查找
    // 是慢的（即比O(1)慢或超过几个内联指令），
    // 因为它避免了JPS算法的大面积扫描。
    // （也增加了内存使用量，因为每个检查的位置都扩展为一个节点。）
    JPS_Flag_AStarOnly = 0x02,

    // 不要检查起始位置是否可行走。
    // 这使得起始位置总是可行走，即使地图数据说不是。
    JPS_Flag_NoStartCheck = 0x04,

    // 不要检查目标位置是否可行走。
    JPS_Flag_NoEndCheck = 0x08,
};

enum JPS_Result {
    JPS_NO_PATH,          // 没有找到路径
    JPS_FOUND_PATH,       // 找到路径
    JPS_NEED_MORE_STEPS,  // 需要更多的步骤
    JPS_EMPTY_PATH,       // 路径为空
    JPS_OUT_OF_MEMORY     // 内存不足
};

// operator new() without #include <new>
// 不幸的是，标准要求使用size_t，所以我们需要stddef.h至少。
// 通过https://github.com/ocornut/imgui的技巧
// "定义一个带有虚拟参数的自定义placement new()允许我们绕过包括<new>
// 这在某些平台上会抱怨，当用户禁用异常时。"
struct JPS__NewDummy {};

inline void* operator new(size_t, JPS__NewDummy, void* ptr) {
    return ptr;
}

inline void operator delete(void*, JPS__NewDummy, void*) {
}

#define JPS_PLACEMENT_NEW(p) new (JPS__NewDummy(), p)

namespace JPS {

struct Position {
    PosType x, y;  // 位置

    inline bool operator==(const Position& p) const  // 相等
    {
        return x == p.x && y == p.y;
    }

    inline bool operator!=(const Position& p) const  // 不相等
    {
        return x != p.x || y != p.y;
    }

    inline bool isValid() const {
        return x != PosType(-1);
    }
};

// 无效位置。用于内部标记不可行走的点。
static const Position npos = {PosType(-1), PosType(-1)};
static const SizeT noidx = SizeT(-1);

// ctor函数，以保持Position是一个真正的POD结构。
inline static Position Pos(PosType x, PosType y) {
    Position p;
    p.x = x;
    p.y = y;
    return p;
}

template <typename T>
inline static T Max(T a, T b) {
    return a < b ? b : a;
}

template <typename T>
inline static T Min(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
inline static T Abs(T a) {
    return a < T(0) ? -a : a;
}

template <typename T>
inline static int Sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

// 启发式。如果需要，请添加新的启发式。
namespace Heuristic {
// 曼哈顿距离
inline ScoreType Manhattan(const Position& a, const Position& b) {
    const int dx = Abs(int(a.x - b.x));
    const int dy = Abs(int(a.y - b.y));
    return static_cast<ScoreType>(dx + dy);
}

// 切比雪夫距离
inline ScoreType Chebyshev(const Position& a, const Position& b) {
    const int dx = Abs(int(a.x - b.x));
    const int dy = Abs(int(a.y - b.y));
    return static_cast<ScoreType>(Max(dx, dy));
}
#ifdef JPS_sqrt
// 欧几里得距离
inline ScoreType Euclidean(const Position& a, const Position& b) {
    const int dx = (int(a.x - b.x));
    const int dy = (int(a.y - b.y));
    return static_cast<ScoreType>(JPS_sqrt(dx * dx + dy * dy));
}
#endif
}  // namespace Heuristic

// --- 开始基础设施，数据结构 ---

namespace Internal {

// 永远不会分配在PodVec<Node>之外 --> 所有节点在内存中是线性相邻的。
struct Node {
    ScoreType f, g;   // 启发式距离
    Position pos;     // 位置
    int parentOffs;   // 没有父节点如果为0
    unsigned _flags;  // 标志

    inline int hasParent() const {
        return parentOffs;
    }

    inline void setOpen() {
        _flags |= 1;
    }  // 设置为开放

    inline void setClosed() {
        _flags |= 2;
    }  // 设置为封闭

    inline unsigned isOpen() const {
        return _flags & 1;
    }  // 是否开放

    inline unsigned isClosed() const {
        return _flags & 2;
    }  // 是否封闭

    // 我们知道节点在内存中是顺序分配的，所以这是可以的。
    inline Node& getParent() {
        JPS_ASSERT(parentOffs);
        return this[parentOffs];
    }  // 获取父节点

    inline const Node& getParent() const {
        JPS_ASSERT(parentOffs);
        return this[parentOffs];
    }  // 获取父节点

    inline const Node* getParentOpt() const {
        return parentOffs ? this + parentOffs : 0;
    }  // 获取父节点

    inline void setParent(const Node& p) {
        JPS_ASSERT(&p != this);
        parentOffs = static_cast<int>(&p - this);
    }  // 设置父节点
};

// 模板类，用于管理节点
template <typename T>
class PodVec {
public:
    PodVec(void* user = 0) : _data(0), used(0), cap(0), _user(user) {
    }

    ~PodVec() {
        dealloc();
    }

    inline void clear() {
        used = 0;
    }

    void dealloc() {
        JPS_free(_data, cap * sizeof(T), _user);
        _data = 0;
        used = 0;
        cap = 0;
    }

    T* alloc() {
        T* e = 0;
        if (used < cap || _grow()) {
            e = _data + used;
            ++used;
        }
        return e;
    }

    inline void push_back(const T& e) {
        if (T* dst = alloc())  // yes, this silently fails when OOM. this is handled internally.
            *dst = e;
    }

    inline void pop_back() {
        JPS_ASSERT(used);
        --used;
    }

    inline T& back() {
        JPS_ASSERT(used);
        return _data[used - 1];
    }

    inline SizeT size() const {
        return used;
    }

    inline bool empty() const {
        return !used;
    }

    inline T* data() {
        return _data;
    }

    inline const T* data() const {
        return _data;
    }

    inline T& operator[](size_t idx) const {
        JPS_ASSERT(idx < used);
        return _data[idx];
    }

    inline SizeT getindex(const T* e) const {
        JPS_ASSERT(e && _data <= e && e < _data + used);
        return static_cast<SizeT>(e - _data);
    }

    void* _reserve(SizeT newcap)  // 内部使用
    {
        return cap < newcap ? _grow(newcap) : _data;
    }

    void resize(SizeT sz) {
        if (_reserve(sz))
            used = sz;
    }

    SizeT _getMemSize() const {
        return cap * sizeof(T);
    }

    // 最小迭代器接口
    typedef T* iterator;              // 迭代器
    typedef const T* const_iterator;  // 常量迭代器
    typedef SizeT size_type;          // 大小类型
    typedef T value_type;             // 值类型

    inline iterator begin() {
        return data();
    }  // 开始

    inline iterator end() {
        return data() + size();
    }  // 结束

    inline const_iterator cbegin() const {
        return data();
    }  // 常量开始

    inline const_iterator cend() const {
        return data() + size();
    }

private:
    void* _grow(SizeT newcap)  // 增长
    {
        void* p = JPS_realloc(_data, newcap * sizeof(T), cap * sizeof(T), _user);
        if (p) {
            _data = (T*)p;
            cap = newcap;
        }
        return p;
    }

    void* _grow() {
        const SizeT newcap = cap + (cap / 2) + 32;
        return _grow(newcap);
    }

    T* _data;         // 数据
    SizeT used, cap;  // 使用，容量

public:
    void* const _user;

private:
    // 禁止操作
    PodVec<T>& operator=(const PodVec<T>&);
    PodVec(const PodVec<T>&);
};

template <typename T>
inline static void Swap(T& a, T& b) {
    const T tmp = a;
    a = b;
    b = tmp;
}

template <typename IT>
inline static void Reverse(IT first, IT last) {
    while ((first != last) && (first != --last)) {
        Swap(*first, *last);
        ++first;
    }
}

typedef PodVec<Node> Storage;

// 节点映射
class NodeMap {
private:
    static const unsigned LOAD_FACTOR = 8;       // 估计：{CPU缓存行大小(64)} / sizeof(HashLoc)
    static const unsigned INITIAL_BUCKETS = 16;  // 必须大于1且为2的幂

    struct HashLoc {
        unsigned hash2;  // 仅用于早期检查
        SizeT idx;       // 在中央存储中的索引
    };

    typedef PodVec<HashLoc> Bucket;  // 桶

    // 哈希函数，用于确定桶。仅使用低几位。应该很好地混淆低几位。
    static inline unsigned Hash(PosType x, PosType y) {
        return x ^ y;
    }

    // 哈希函数，设计为尽可能少地丢失数据。用于早期检查。所有位都使用。
    static inline unsigned Hash2(PosType x, PosType y) {
        return (y << 16) ^ x;
    }

public:
    NodeMap(Storage& storage) : _storageRef(storage), _buckets(storage._user) {
    }

    ~NodeMap() {
        dealloc();
    }

    void dealloc() {
        for (SizeT i = 0; i < _buckets.size(); ++i)
            _buckets[i].~Bucket();
        _buckets.dealloc();
    }

    void clear() {
        // 清除桶，但*不*清除桶向量
        for (SizeT i = 0; i < _buckets.size(); ++i)
            _buckets[i].clear();
    }

    Node* operator()(PosType x, PosType y) {
        const unsigned h = Hash(x, y);
        const unsigned h2 = Hash2(x, y);
        const SizeT ksz = _buckets.size();  // 已知为2的幂
        Bucket* b = 0;                      // MSVC /W4 抱怨这个未初始化且使用，所以我们初始化它...
        if (ksz) {
            b = &_buckets[h & (ksz - 1)];
            const SizeT bsz = b->size();
            const HashLoc* const bdata = b->data();
            for (SizeT i = 0; i < bsz; ++i) {
                // 这是唯一使用HashLoc::hash2的地方；它可以被移除，这意味着：
                // - 两次空间用于索引每缓存行
                // - 但也有更高的机会出现缓存未命中，因为对于每个桶中的条目，我们仍然需要检查节点的X/Y坐标，
                //   并且我们最终可能会在RAM的随机位置结束。
                // 快速基准测试表明，*使用*hash2检查，它几乎无法测量（小于1%）更快。
                if (bdata[i].hash2 == h2) {
                    Node& n = _storageRef[bdata[i].idx];
                    if (n.pos.x == x && n.pos.y == y)
                        return &n;
                }
            }
        }

        // 如果需要，扩大哈希图；如果需要，修复桶
        SizeT newbsz = _enlarge();
        if (newbsz > 1)
            b = &_buckets[h & (newbsz - 1)];
        else if (newbsz == 1)  // 错误情况
            return 0;

        HashLoc* loc = b->alloc();  // ... see above. b is always initialized here. when ksz==0, _enlarge() will do its
                                    // initial allocation, so it can never return 0.

        if (!loc)
            return 0;

        loc->hash2 = h2;
        loc->idx = _storageRef.size();

        // 没有节点在(x, y)，创建新节点
        Node* n = _storageRef.alloc();
        if (n) {
            n->f = 0;
            n->g = 0;
            n->pos.x = x;
            n->pos.y = y;
            n->parentOffs = 0;
            n->_flags = 0;
        }
        return n;
    }

    SizeT _getMemSize() const {
        SizeT sum = _buckets._getMemSize();
        for (Buckets::const_iterator it = _buckets.cbegin(); it != _buckets.cend(); ++it)
            sum += it->_getMemSize();
        return sum;
    }

private:
    // 返回值：0 = 没有要做的；1 = 错误；>1：内部存储被扩大到这个桶的数量
    SizeT _enlarge() {
        const SizeT n = _storageRef.size();
        const SizeT oldsz = _buckets.size();
        if (n < oldsz * LOAD_FACTOR)
            return 0;

        // 预先分配桶存储，我们即将使用
        const SizeT newsz = oldsz ? oldsz * 2 : INITIAL_BUCKETS;  // 保持为2的幂

        if (!_buckets._reserve(newsz))
            return 0;  // 如果realloc失败，则提前退出；这不是问题，我们可以继续。

        // 忘记所有内容
        for (SizeT i = 0; i < oldsz; ++i)
            _buckets[i].clear();

        // 调整大小并初始化
        for (SizeT i = oldsz; i < newsz; ++i) {
            void* p = _buckets.alloc();  // 不能失败，因为空间已经预留
            JPS_PLACEMENT_NEW(p) PodVec<HashLoc>(_buckets._user);
        }

        const SizeT mask = _buckets.size() - 1;
        for (SizeT i = 0; i < n; ++i) {
            const Position p = _storageRef[i].pos;
            HashLoc* loc = _buckets[Hash(p.x, p.y) & mask].alloc();
            if (!loc)
                return 1;  // error case

            loc->hash2 = Hash2(p.x, p.y);
            loc->idx = i;
        }
        return newsz;
    }

    Storage& _storageRef;
    typedef PodVec<Bucket> Buckets;
    Buckets _buckets;
};

// 开放列表
class OpenList {
private:
    const Storage& _storageRef;
    PodVec<SizeT> idxHeap;

public:
    OpenList(const Storage& storage) : _storageRef(storage), idxHeap(storage._user) {
    }

    inline void pushNode(Node* n) {
        _heapPushIdx(_storageRef.getindex(n));
    }

    inline Node& popNode() {
        return _storageRef[_popIdx()];
    }

    // 重新堆化，因为节点改变了它的顺序
    inline void fixNode(const Node& n) {
        const unsigned ni = _storageRef.getindex(&n);
        const unsigned sz = idxHeap.size();
        unsigned* p = idxHeap.data();      // 获取堆数据
        for (unsigned i = 0; i < sz; ++i)  // TODO: 如果这成为性能瓶颈：使每个节点都知道它的堆索引
            if (p[i] == ni) {
                _fixIdx(i);  // 重新堆化
                return;
            }
        JPS_ASSERT(false);  // 期望节点被找到
    }

    inline void dealloc() {
        idxHeap.dealloc();
    }

    inline void clear() {
        idxHeap.clear();
    }

    inline bool empty() const {
        return idxHeap.empty();
    }

    inline SizeT _getMemSize() const {
        return idxHeap._getMemSize();
    }

private:
    inline bool _heapLess(SizeT a, SizeT b) {
        return _storageRef[idxHeap[a]].f > _storageRef[idxHeap[b]].f;
    }

    inline bool _heapLessIdx(SizeT a, SizeT idx) {
        return _storageRef[idxHeap[a]].f > _storageRef[idx].f;
    }

    // 向上堆化
    void _percolateUp(SizeT i) {
        const SizeT idx = idxHeap[i]; // 获取堆索引
        SizeT p;
        goto start;
        do {
            idxHeap[i] = idxHeap[p];  // 父节点更小，移动它
            i = p;                    // 继续使用父节点
        start:
            p = (i - 1) >> 1;
        } while (i && _heapLessIdx(p, idx));
        idxHeap[i] = idx;  // 找到正确的位置
    }

    // 向下堆化
    void _percolateDown(SizeT i) {
        const SizeT idx = idxHeap[i];  // 获取堆索引
        const SizeT sz = idxHeap.size();
        SizeT child;
        goto start;
        do {
            // 如果存在且更大或相等，则选择右兄弟
            if (child + 1 < sz && !_heapLess(child + 1, child))
                ++child;
            idxHeap[i] = idxHeap[child];
            i = child;
        start:
            child = (i << 1) + 1;
        } while (child < sz);
        idxHeap[i] = idx;
        _percolateUp(i);
    }

    void _heapPushIdx(SizeT idx) {
        SizeT i = idxHeap.size();
        idxHeap.push_back(idx);
        _percolateUp(i);
    }

    SizeT _popIdx() {
        SizeT sz = idxHeap.size();
        JPS_ASSERT(sz);
        const SizeT root = idxHeap[0];
        idxHeap[0] = idxHeap[--sz];
        idxHeap.pop_back();
        if (sz > 1)
            _percolateDown(0);
        return root;
    }

    // 重新堆化索引为i的节点
    inline void _fixIdx(SizeT i) {
        _percolateDown(i);
        _percolateUp(i);
    }
};

#undef JPS_PLACEMENT_NEW

// --- 结束基础设施，数据结构 ---

// 那些不依赖于模板参数的东西...
class SearcherBase {
protected:
    Storage storage;  // 存储
    OpenList open;    // 开放列表
    NodeMap nodemap;  // 节点映射

    Position endPos;
    SizeT endNodeIdx;
    JPS_Flags flags;
    int stepsRemain;
    SizeT stepsDone;

    SearcherBase(void* user)
        : storage(user),
          open(storage),
          nodemap(storage),
          endPos(npos),
          endNodeIdx(noidx),
          flags(0),
          stepsRemain(0),
          stepsDone(0) {
    }

    void clear() {
        open.clear();
        nodemap.clear();
        storage.clear();
        endNodeIdx = noidx;
        stepsDone = 0;
    }

    // 扩展节点，思路是：
    // 1. 计算额外代价
    // 2. 计算新代价
    // 3. 如果节点不在开放列表中，或者新代价小于节点代价，则更新节点
    // 4. 如果节点不在开放列表中，则将节点加入开放列表
    // 5. 如果节点在开放列表中，则更新节点
    // 参数：
    // jp: 目标位置
    // jn: 目标节点
    // parent: 父节点
    void _expandNode(const Position jp, Node& jn, const Node& parent) {
        JPS_ASSERT(jn.pos == jp);                                   // 确保节点是正确的
        ScoreType extraG = JPS_HEURISTIC_ACCURATE(jp, parent.pos);  // 计算额外代价
        ScoreType newG = parent.g + extraG;                         // 计算新代价
        if (!jn.isOpen() || newG < jn.g) {  // 如果节点不在开放列表中，或者新代价小于节点代价，则更新节点
            jn.g = newG;                    // 更新节点代价
            jn.f = jn.g + JPS_HEURISTIC_ESTIMATE(jp, endPos);  // 计算新f值
            jn.setParent(parent);                              // 设置父节点
            if (!jn.isOpen()) {      // 如果节点不在开放列表中，则将节点加入开放列表
                open.pushNode(&jn);  // 将节点加入开放列表
                jn.setOpen();        // 设置节点为开放
            } else
                open.fixNode(jn);  // 如果节点在开放列表中，则更新节点
        }
    }

public:
    template <typename PV>
    JPS_Result generatePath(PV& path, unsigned step) const;

    void freeMemory() {
        open.dealloc();
        nodemap.dealloc();
        storage.dealloc();
        endNodeIdx = noidx;
    }

    // --- Statistics ---

    inline SizeT getStepsDone() const {
        return stepsDone;
    }

    inline SizeT getNodesExpanded() const {
        return storage.size();
    }

    SizeT getTotalMemoryInUse() const {
        return storage._getMemSize() + nodemap._getMemSize() + open._getMemSize();
    }
};

template <typename GRID>
class Searcher : public SearcherBase {
public:
    Searcher(const GRID& g, void* user = 0) : SearcherBase(user), grid(g) {
    }

    // 单次调用
    template <typename PV>
    bool findPath(PV& path, Position start, Position end, unsigned step, JPS_Flags flags = JPS_Flag_Default);

    // 增量路径查找
    JPS_Result findPathInit(Position start, Position end, JPS_Flags flags = JPS_Flag_Default);
    JPS_Result findPathStep(int limit);
    // 生成路径，在找到路径后
    template <typename PV>
    JPS_Result findPathFinish(PV& path, unsigned step) const;

private:
    const GRID& grid;

    Node* getNode(const Position& pos);
    bool identifySuccessors(const Node& n);

    bool findPathGreedy(Node* start, Node* end);

    unsigned findNeighborsAStar(const Node& n, Position* wptr);

    unsigned findNeighborsJPS(const Node& n, Position* wptr) const;
    Position jumpP(const Position& p, const Position& src);
    Position jumpD(Position p, int dx, int dy);
    Position jumpX(Position p, int dx);
    Position jumpY(Position p, int dy);

    // 禁止任何操作
    Searcher& operator=(const Searcher<GRID>&);
    Searcher(const Searcher<GRID>&);
};

// -----------------------------------------------------------------------

template <typename PV>
JPS_Result SearcherBase::generatePath(PV& path, unsigned step) const {
    if (endNodeIdx == noidx)
        return JPS_NO_PATH;
    const SizeT offset = path.size();
    SizeT added = 0;
    const Node& endNode = storage[endNodeIdx];
    const Node* next = &endNode;
    if (!next->hasParent())
        return JPS_NO_PATH;
    if (step) {
        const Node* prev = endNode.getParentOpt();
        if (!prev)
            return JPS_NO_PATH;
        do {
            const unsigned x = next->pos.x, y = next->pos.y;
            int dx = int(prev->pos.x - x);
            int dy = int(prev->pos.y - y);
            const int adx = Abs(dx);
            const int ady = Abs(dy);
            JPS_ASSERT(!dx || !dy || adx == ady);  // known to be straight, if diagonal
            const int steps = Max(adx, ady);
            dx = int(step) * Sgn(dx);
            dy = int(step) * Sgn(dy);
            int dxa = 0, dya = 0;
            for (int i = 0; i < steps; i += step) {
                path.push_back(Pos(x + dxa, y + dya));
                ++added;
                dxa += dx;
                dya += dy;
            }
            next = prev;
            prev = prev->getParentOpt();
        } while (prev);
    } else {
        do {
            JPS_ASSERT(next != &next->getParent());
            path.push_back(next->pos);
            ++added;
            next = &next->getParent();
        } while (next->hasParent());
    }

    // JPS::PathVector默默地丢弃push_back()，当内存分配失败时；
    // 检测这种情况并回滚。
    if (path.size() != offset + added) {
        path.resize(offset);
        return JPS_OUT_OF_MEMORY;
    }

    // Nodes were traversed backwards, fix that
    Reverse(path.begin() + offset, path.end());
    return JPS_FOUND_PATH;
}

//-----------------------------------------

template <typename GRID>
inline Node* Searcher<GRID>::getNode(const Position& pos) {
    JPS_ASSERT(grid(pos.x, pos.y));
    return nodemap(pos.x, pos.y);
}

template <typename GRID>
Position Searcher<GRID>::jumpP(const Position& p, const Position& src) {
    JPS_ASSERT(grid(p.x, p.y));

    int dx = int(p.x - src.x);
    int dy = int(p.y - src.y);
    JPS_ASSERT(dx || dy);

    if (dx && dy)
        return jumpD(p, dx, dy);
    else if (dx)
        return jumpX(p, dx);
    else if (dy)
        return jumpY(p, dy);

    // not reached
    JPS_ASSERT(false);
    return npos;
}

template <typename GRID>
Position Searcher<GRID>::jumpD(Position p, int dx, int dy) {
    JPS_ASSERT(grid(p.x, p.y));
    JPS_ASSERT(dx && dy);

    const Position endpos = endPos;
    unsigned steps = 0;

    while (true) {
        if (p == endpos)
            break;

        ++steps;
        const PosType x = p.x;
        const PosType y = p.y;

        if ((grid(x - dx, y + dy) && !grid(x - dx, y)) || (grid(x + dx, y - dy) && !grid(x, y - dy)))
            break;

        const bool gdx = !!grid(x + dx, y);
        const bool gdy = !!grid(x, y + dy);

        if (gdx && jumpX(Pos(x + dx, y), dx).isValid())
            break;

        if (gdy && jumpY(Pos(x, y + dy), dy).isValid())
            break;

        if ((gdx || gdy) && grid(x + dx, y + dy)) {
            p.x += dx;
            p.y += dy;
        } else {
            p = npos;
            break;
        }
    }
    stepsDone += steps;
    stepsRemain -= steps;
    return p;
}

template <typename GRID>
inline Position Searcher<GRID>::jumpX(Position p, int dx) {
    JPS_ASSERT(dx);
    JPS_ASSERT(grid(p.x, p.y));

    const PosType y = p.y;
    const Position endpos = endPos;
    unsigned steps = 0;

    unsigned a = ~((!!grid(p.x, y + 1)) | ((!!grid(p.x, y - 1)) << 1));

    while (true) {
        const unsigned xx = p.x + dx;
        const unsigned b = (!!grid(xx, y + 1)) | ((!!grid(xx, y - 1)) << 1);

        if ((b & a) || p == endpos)
            break;
        if (!grid(xx, y)) {
            p = npos;
            break;
        }

        p.x += dx;
        a = ~b;
        ++steps;
    }

    stepsDone += steps;
    stepsRemain -= steps;
    return p;
}

template <typename GRID>
inline Position Searcher<GRID>::jumpY(Position p, int dy) {
    JPS_ASSERT(dy);
    JPS_ASSERT(grid(p.x, p.y));

    const PosType x = p.x;
    const Position endpos = endPos;
    unsigned steps = 0;

    unsigned a = ~((!!grid(x + 1, p.y)) | ((!!grid(x - 1, p.y)) << 1));

    while (true) {
        const unsigned yy = p.y + dy;
        const unsigned b = (!!grid(x + 1, yy)) | ((!!grid(x - 1, yy)) << 1);

        if ((a & b) || p == endpos)
            break;
        if (!grid(x, yy)) {
            p = npos;
            break;
        }

        p.y += dy;
        a = ~b;
        ++steps;
    }

    stepsDone += steps;
    stepsRemain -= steps;
    return p;
}

#define JPS_CHECKGRID(dx, dy) (grid(x + (dx), y + (dy)))
#define JPS_ADDPOS(dx, dy)              \
    do {                                \
        *w++ = Pos(x + (dx), y + (dy)); \
    } while (0)
#define JPS_ADDPOS_CHECK(dx, dy)   \
    do {                           \
        if (JPS_CHECKGRID(dx, dy)) \
            JPS_ADDPOS(dx, dy);    \
    } while (0)
#define JPS_ADDPOS_NO_TUNNEL(dx, dy)                \
    do {                                            \
        if (grid(x + (dx), y) || grid(x, y + (dy))) \
            JPS_ADDPOS_CHECK(dx, dy);               \
    } while (0)

template <typename GRID>
unsigned Searcher<GRID>::findNeighborsJPS(const Node& n, Position* wptr) const {
    Position* w = wptr;
    const unsigned x = n.pos.x;
    const unsigned y = n.pos.y;

    if (!n.hasParent()) {
        // straight moves
        JPS_ADDPOS_CHECK(-1, 0);
        JPS_ADDPOS_CHECK(0, -1);
        JPS_ADDPOS_CHECK(0, 1);
        JPS_ADDPOS_CHECK(1, 0);

        // diagonal moves + prevent tunneling
        JPS_ADDPOS_NO_TUNNEL(-1, -1);
        JPS_ADDPOS_NO_TUNNEL(-1, 1);
        JPS_ADDPOS_NO_TUNNEL(1, -1);
        JPS_ADDPOS_NO_TUNNEL(1, 1);

        return unsigned(w - wptr);
    }
    const Node& p = n.getParent();
    // jump directions (both -1, 0, or 1)
    const int dx = Sgn<int>(x - p.pos.x);
    const int dy = Sgn<int>(y - p.pos.y);

    if (dx && dy) {
        // diagonal
        // natural neighbors
        const bool walkX = !!grid(x + dx, y);
        if (walkX)
            *w++ = Pos(x + dx, y);
        const bool walkY = !!grid(x, y + dy);
        if (walkY)
            *w++ = Pos(x, y + dy);

        if (walkX || walkY)
            JPS_ADDPOS_CHECK(dx, dy);

        // forced neighbors
        if (walkY && !JPS_CHECKGRID(-dx, 0))
            JPS_ADDPOS_CHECK(-dx, dy);

        if (walkX && !JPS_CHECKGRID(0, -dy))
            JPS_ADDPOS_CHECK(dx, -dy);
    } else if (dx) {
        // along X axis
        if (JPS_CHECKGRID(dx, 0)) {
            JPS_ADDPOS(dx, 0);

            // Forced neighbors (+ prevent tunneling)
            if (!JPS_CHECKGRID(0, 1))
                JPS_ADDPOS_CHECK(dx, 1);
            if (!JPS_CHECKGRID(0, -1))
                JPS_ADDPOS_CHECK(dx, -1);
        }
    } else if (dy) {
        // along Y axis
        if (JPS_CHECKGRID(0, dy)) {
            JPS_ADDPOS(0, dy);

            // Forced neighbors (+ prevent tunneling)
            if (!JPS_CHECKGRID(1, 0))
                JPS_ADDPOS_CHECK(1, dy);
            if (!JPS_CHECKGRID(-1, 0))
                JPS_ADDPOS_CHECK(-1, dy);
        }
    }

    return unsigned(w - wptr);
}

//-------------- Plain old A* search ----------------
template <typename GRID>
unsigned Searcher<GRID>::findNeighborsAStar(const Node& n, Position* wptr) {
    Position* w = wptr;
    const int x = n.pos.x;
    const int y = n.pos.y;
    const int d = 1;
    JPS_ADDPOS_NO_TUNNEL(-d, -d);
    JPS_ADDPOS_CHECK(0, -d);
    JPS_ADDPOS_NO_TUNNEL(+d, -d);
    JPS_ADDPOS_CHECK(-d, 0);
    JPS_ADDPOS_CHECK(+d, 0);
    JPS_ADDPOS_NO_TUNNEL(-d, +d);
    JPS_ADDPOS_CHECK(0, +d);
    JPS_ADDPOS_NO_TUNNEL(+d, +d);
    stepsDone += 8;
    return unsigned(w - wptr);
}

//-------------------------------------------------
#undef JPS_ADDPOS
#undef JPS_ADDPOS_CHECK
#undef JPS_ADDPOS_NO_TUNNEL
#undef JPS_CHECKGRID

// 识别后继节点
template <typename GRID>
bool Searcher<GRID>::identifySuccessors(const Node& n_) {
    const SizeT nidx = storage.getindex(&n_);
    const Position np = n_.pos;
    Position buf[8];

    const int num = (flags & JPS_Flag_AStarOnly) ? findNeighborsAStar(n_, &buf[0]) : findNeighborsJPS(n_, &buf[0]);

    for (int i = num - 1; i >= 0; --i) {
        // 不变性：一个节点只有在对应的网格位置是可行走的时才是有效的邻居（在jumpP中被断言）
        Position jp;
        if (flags & JPS_Flag_AStarOnly)
            jp = buf[i];
        else {
            jp = jumpP(buf[i], np);
            if (!jp.isValid())
                continue;
        }
        // 现在网格位置肯定是有效的跳点，我们必须创建实际的节点。
        Node* jn = getNode(jp);  // 这可能会重新分配存储
        if (!jn)
            return false;  // 内存不足

        Node& n = storage[nidx];  // 在重新分配的情况下获取有效的引用
        JPS_ASSERT(jn != &n);
        if (!jn->isClosed())
            _expandNode(jp, *jn, n);
    }
    return true;
}

template <typename GRID>
template <typename PV>
bool Searcher<GRID>::findPath(PV& path, Position start, Position end, unsigned step, JPS_Flags flags) {
    JPS_Result res = findPathInit(start, end, flags);

    // 如果这是真的，结果路径是空的（findPathFinish()会失败，所以这需要在检查之前）
    if (res == JPS_EMPTY_PATH)
        return true;

    while (true) {
        switch (res) {
            case JPS_NEED_MORE_STEPS:
                res = findPathStep(0);
                break;  // 开关

            case JPS_FOUND_PATH:
                return findPathFinish(path, step) == JPS_FOUND_PATH;

            case JPS_EMPTY_PATH:
                JPS_ASSERT(false);  // can't happen
                // fall through
            case JPS_NO_PATH:
            case JPS_OUT_OF_MEMORY:
                return false;
        }
    }
}

template <typename GRID>
JPS_Result Searcher<GRID>::findPathInit(Position start, Position end, JPS_Flags flags) {
    // 这仅重置几个计数器；容器内存未触及
    this->clear();

    this->flags = flags;
    endPos = end;

    // FIXME: 检查这个
    if (start == end && !(flags & (JPS_Flag_NoStartCheck | JPS_Flag_NoEndCheck))) {
        // 只有当这个单个位置是可行走的时才有路径。
        // 但由于起始位置在输出中被省略，这里没有什么可做的。
        return grid(end.x, end.y) ? JPS_EMPTY_PATH : JPS_NO_PATH;
    }

    if (!(flags & JPS_Flag_NoStartCheck))
        if (!grid(start.x, start.y))
            return JPS_NO_PATH;

    if (!(flags & JPS_Flag_NoEndCheck))
        if (!grid(end.x, end.y))
            return JPS_NO_PATH;

    Node* endNode = getNode(end);  // this might realloc the internal storage...
    if (!endNode)
        return JPS_OUT_OF_MEMORY;
    endNodeIdx = storage.getindex(endNode);  // .. so we keep this for later

    Node* startNode = getNode(start);  // this might also realloc
    if (!startNode)
        return JPS_OUT_OF_MEMORY;
    endNode = &storage[endNodeIdx];  // startNode is valid, make sure that endNode is valid too in case we reallocated

    if (!(flags & JPS_Flag_NoGreedy)) {
        // Try the quick way out first
        if (findPathGreedy(startNode, endNode))
            return JPS_FOUND_PATH;
    }

    open.pushNode(startNode);

    return JPS_NEED_MORE_STEPS;
}

template <typename GRID>
JPS_Result Searcher<GRID>::findPathStep(int limit) {
    stepsRemain = limit;
    do {
        if (open.empty())
            return JPS_NO_PATH;
        Node& n = open.popNode();
        n.setClosed();
        if (n.pos == endPos)
            return JPS_FOUND_PATH;
        if (!identifySuccessors(n))
            return JPS_OUT_OF_MEMORY;
    } while (stepsRemain >= 0);
    return JPS_NEED_MORE_STEPS;
}

template <typename GRID>
template <typename PV>
JPS_Result Searcher<GRID>::findPathFinish(PV& path, unsigned step) const {
    return this->generatePath(path, step);
}

template <typename GRID>
bool Searcher<GRID>::findPathGreedy(Node* n, Node* endnode) {
    Position midpos = npos;
    PosType x = n->pos.x;
    PosType y = n->pos.y;
    const Position endpos = endnode->pos;

    JPS_ASSERT(x != endpos.x || y != endpos.y);  // must not be called when start==end
    JPS_ASSERT(n != endnode);

    int dx = int(endpos.x - x);
    int dy = int(endpos.y - y);
    const int adx = Abs(dx);
    const int ady = Abs(dy);
    dx = Sgn(dx);
    dy = Sgn(dy);

    // go diagonally first
    if (x != endpos.x && y != endpos.y) {
        JPS_ASSERT(dx && dy);
        const int minlen = Min(adx, ady);
        const PosType tx = x + dx * minlen;
        while (x != tx) {
            if (grid(x, y) && (grid(x + dx, y) || grid(x, y + dy)))  // prevent tunneling as well
            {
                x += dx;
                y += dy;
            } else
                return false;
        }

        if (!grid(x, y))
            return false;

        midpos = Pos(x, y);
    }

    // at this point, we're aligned to at least one axis
    JPS_ASSERT(x == endpos.x || y == endpos.y);

    if (!(x == endpos.x && y == endpos.y)) {
        while (x != endpos.x)
            if (!grid(x += dx, y))
                return false;

        while (y != endpos.y)
            if (!grid(x, y += dy))
                return false;

        JPS_ASSERT(x == endpos.x && y == endpos.y);
    }

    if (midpos.isValid()) {
        const unsigned nidx = storage.getindex(n);
        Node* mid = getNode(midpos);  // this might invalidate n, endnode
        if (!mid)
            return false;
        n = &storage[nidx];  // reload pointers
        endnode = &storage[endNodeIdx];
        JPS_ASSERT(mid && mid != n);
        mid->setParent(*n);
        if (mid != endnode)
            endnode->setParent(*mid);
    } else
        endnode->setParent(*n);

    return true;
}

#undef JPS_ASSERT
#undef JPS_realloc
#undef JPS_free
#undef JPS_sqrt
#undef JPS_HEURISTIC_ACCURATE
#undef JPS_HEURISTIC_ESTIMATE

}  // end namespace Internal

using Internal::Searcher;

typedef Internal::PodVec<Position> PathVector;

// 单次调用便利函数。为了效率，不要在需要重复计算路径时使用这个函数。
//
// 返回：0如果失败或无法找到路径，否则为步数。
//
// path: 如果函数返回成功，路径将被附加到这个向量中。
//       路径不包含起始位置，即如果起始位置和结束位置相同，结果路径将没有元素。
//       向量不必为空。函数不清除它；相反，新路径位置被附加到末尾。
//       这允许增量构建路径。
//
// grid: 仿函数，预期重载operator()(x, y)，如果位置可行走，返回true，否则返回false。
//
// step: 如果为0，仅返回路径点。
//       如果为1，创建详尽的步进路径。
//       如果为N，将N个块的距离或当到达路径点时放入一个位置。
//       所有返回的位置都保证在一条直线上（垂直、水平或对角线），并且任何两个连续位置之间没有障碍。
//       注意，此参数不会影响路径搜索；它仅控制输出路径的粗细。
template <typename GRID, typename PV>
SizeT findPath(PV& path, const GRID& grid, PosType startx, PosType starty, PosType endx, PosType endy,
               unsigned step = 0,  // optional
               JPS_Flags flags = JPS_Flag_Default,
               void* user = 0)  // memory allocation userdata
{
    Searcher<GRID> search(grid, user);
    if (!search.findPath(path, Pos(startx, starty), Pos(endx, endy), step, flags))
        return 0;
    const SizeT done = search.getStepsDone();
    return done + !done;  // report at least 1 step; as 0 would indicate failure
}

}  // end namespace JPS

/*
    与旧版JPS.h相比的更改：

- 显式释放内存不再必要。freeMemory()方法仍然存在，但它什么也不做（丢弃所有内部存储），但你永远不需要显式调用它。
  与旧版本不同，如果你不定期释放内存，性能不会下降。实际上，如果你释放内存并再次路径搜索，它必须重新分配内部数据结构，这会稍微慢一些。

- Searcher::getNodesExpanded() 在开始搜索时重置为0。

- 添加了可选的JPS_Flags参数，以控制搜索行为。编译时#defines已删除。

- 删除了skip参数。在我看来，这个参数只是增加了混乱，并没有实际好处。
  如果你出于某种原因想要它，请告诉我，打开一个issue，或者随便你。

- 将JPS::Result重命名为JPS_Result。枚举值获得了JPS_前缀，因此JPS::NO_PATH现在是JPS_NO_PATH，依此类推。

- 添加了一个新的JPS_Result值：JPS_OUT_OF_MEMORY。请参阅上面的信息块，了解如何处理此问题。

- 将Searcher<>::findPathFinish()的签名更改为返回JPS_Result（以前是bool）。
  这更符合另外2个方法，因为它现在可以返回JPS_OUT_OF_MEMORY。

- 将JPS::findPath()的签名更改为返回JPS_Result（以前是bool）。非零返回仍然是成功。输出状态的指针已删除。
  如果你需要详细信息，请使用Searcher实例。

- 这个版本不再依赖于C++ STL：<algorithm>、<vector>、<map>、operator new()，所有这些都已删除。
  这使得事情更内存和缓存友好，并且相当快。

- 规范文件名现在是"jps.hh"，而不是"JPS.h"
*/

/*
待办事项：
- 将 int 改为 DirType
- 使得即使在初始化时返回 JPS_EMPTY_PATH 也可以调用 findPathStep()/findPathFinish()（简化 switch-case）
- 让节点知道其堆索引
- 可选的对角线（设为运行时参数）
*/
