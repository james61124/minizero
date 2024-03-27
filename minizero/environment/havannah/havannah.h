#pragma once

#include "base_env.h"
#include "configuration.h"
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::havannah {

const std::string kHavannahName = "havannah";
const int kHavannahNumPlayer = 2;
const int kMaxHavannahBoardSize = 19;

typedef BaseBoardAction<kHavannahNumPlayer> HavannahAction;

inline int popcount(int n)
{
    int count = 0;
    while (n) {
        count++;
        n &= (n - 1);
    }
    return count;
}

class HavannahEnv : public BaseBoardEnv<HavannahAction> {
public:
    HavannahEnv()
    {
        assert(getBoardSize() <= kMaxHavannahBoardSize);
        assert(getBoardSize() % 2 == 1);
        reset();
    }

    void reset() override;
    bool act(const HavannahAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<HavannahAction> getLegalActions() const override;
    bool isLegalAction(const HavannahAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const HavannahAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 4; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    std::string toStringDebug() const;
    inline std::string name() const override { return kHavannahName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kHavannahNumPlayer; }
    inline Player getWinner() const { return winner_; }
    inline const std::vector<Player>& getBoard() const { return board_; }
    std::vector<int> getWinningStonesPosition() const;
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

private:
    Player updateWinner(int actionID);
    inline bool isOnBoard(int i, int j) const
    {
        if (i < 0 || i >= board_size_) return false;
        if (j < 0 || j >= board_size_) return false;
        int sum = i + j;
        return inner_size_ - 1 <= sum && sum <= getBoardSize() * 2 - inner_size_ - 1;
    }
    int getPattern(int i, int j) const;
    // Disjoint set funtion
    inline int find(int x)
    {
        if (x != parents[x]) parents[x] = find(parents[x]);
        return parents[x];
    }
    inline void link(int x, int y)
    {
        int px = find(x);
        int py = find(y);
        if (ranks[px] < ranks[py]) {
            std::swap(px, py);
        } else if (ranks[px] == ranks[py]) {
            ranks[px]++;
        }
        parents[py] = px;
        patterns[px] |= patterns[py];

        int corners = popcount(patterns[px] & 0x3f);
        int edges = popcount(patterns[px] >> 6);
        if (corners >= 2 || edges >= 3) winner_ = turn_;
    }

    Player winner_;
    std::vector<Player> board_;
    std::vector<bool> legal_actions_;
    int inner_size_;
    int empty_counter_;
    // Disjoint set
    std::vector<int> parents;
    std::vector<int> patterns;
    std::vector<int> ranks;
};

class HavannahEnvLoader : public BaseBoardEnvLoader<HavannahAction, HavannahEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kHavannahName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::havannah
