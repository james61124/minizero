#include "havannah.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <iostream>
#include <queue>
#include <string>
#include <vector>

namespace minizero::env::havannah {

using namespace minizero::utils;

int HavannahEnv::getPattern(int i, int j) const
{
    // Corner
    if (i == 0 && j == inner_size_ - 1) return 0x001;
    if (i == 0 && j == board_size_ - 1) return 0x002;
    if (i == board_size_ - inner_size_ && j == board_size_ - 1) return 0x004;
    if (i == board_size_ - 1 && j == board_size_ - inner_size_) return 0x008;
    if (i == board_size_ - 1 && j == 0) return 0x010;
    if (i == inner_size_ - 1 && j == 0) return 0x020;

    // Edge
    if (i + j == inner_size_ - 1) return 0x040;
    if (i == 0) return 0x080;
    if (j == board_size_ - 1) return 0x100;
    if (i + j == board_size_ * 2 - inner_size_ - 1) return 0x200;
    if (i == board_size_ - 1) return 0x400;
    if (j == 0) return 0x800;

    return 0; // others
}

void HavannahEnv::reset()
{
    winner_ = Player::kPlayerNone;
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.resize(board_size_ * board_size_);
    fill(board_.begin(), board_.end(), Player::kPlayerNone);
    inner_size_ = (board_size_ + 1) / 2;
    legal_actions_.resize(board_size_ * board_size_);
    parents.resize(board_size_ * board_size_);
    patterns.resize(board_size_ * board_size_);
    ranks.resize(board_size_ * board_size_);
    empty_counter_ = 0;
    int t = 0;
    for (int i = 0; i < board_size_; i++) {
        for (int j = 0; j < board_size_; j++) {
            bool legal = isOnBoard(i, j);
            legal_actions_[t] = legal;
            empty_counter_ += static_cast<int>(legal);
            ranks[t] = 0;
            parents[t] = t;
            patterns[t++] = getPattern(i, j);
        }
    }
}

bool HavannahEnv::act(const HavannahAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);

    int action_id = action.getActionID();

    if (config::env_hex_use_swap_rule) {
        // Check if it's the second move and the chosen action is same as the first action
        if (actions_.size() == 2 && action_id == actions_[0].getActionID()) {
            // Player 2 has chosen to swap
            // Clear original move
            board_[actions_[0].getActionID()] = Player::kPlayerNone;
        }
    }

    board_[action_id] = action.getPlayer();
    legal_actions_[action_id] = false;
    empty_counter_--;
    if (actions_.size() <= 2 && config::env_hex_use_swap_rule) {
        if (actions_.size() == 1) {
            legal_actions_[action_id] = true;
        } else if (actions_.size() == 2) {
            int first_action_id = actions_[0].getActionID();
            if (first_action_id == action_id) {
                empty_counter_++;
            } else {
                legal_actions_[first_action_id] = false;
            }
        }
    }

    winner_ = updateWinner(action_id);
    turn_ = action.nextPlayer();

    return true;
}

bool HavannahEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(HavannahAction(action_string_args));
}

std::vector<HavannahAction> HavannahEnv::getLegalActions() const
{
    std::vector<HavannahAction> actions;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        HavannahAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool HavannahEnv::isLegalAction(const HavannahAction& action) const
{
    int action_id = action.getActionID();
    Player player = action.getPlayer();

    assert(0 <= action_id && action_id < board_size_ * board_size_);
    assert(player == Player::kPlayer1 || player == Player::kPlayer2);

    return player == turn_ && legal_actions_[action_id];
}

bool HavannahEnv::isTerminal() const
{
    return winner_ != Player::kPlayerNone || empty_counter_ == 0;
}

float HavannahEnv::getEvalScore(bool is_resign /* = false */) const
{
    if (is_resign) {
        return turn_ == Player::kPlayer1 ? -1. : 1.;
    }
    switch (winner_) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

bool HavannahEnv::on_virtual_bridge(int action, Player player, int bridgeType) const {
    static const std::vector<std::vector<int>> offsets = {
        // own_pos_1, own_pos_2, empty_pos_1, empty_pos_2
        {-board_size_, board_size_ + 1, 0, 1},
        {-board_size_ - 1, 1, 0, -board_size_},
        {1, board_size_, 0, board_size_ + 1},
        {board_size_, -board_size_ - 1, 0, -1},
        {board_size_ + 1, -1, 0, board_size_},
        {-1, -board_size_, 0, -board_size_ - 1}
    };

    return (board_[action + offsets[bridgeType][0]] == static_cast<Player>(player))
        && (board_[action + offsets[bridgeType][1]] == static_cast<Player>(player))
        && (board_[action + offsets[bridgeType][2]] == Player::kPlayerNone)
        && (board_[action + offsets[bridgeType][3]] == Player::kPlayerNone);
}

bool HavannahEnv::make_virtual_bridge(int action, Player player, int bridgeType) const {
    static const std::vector<std::vector<int>> offsets = {
        // own_pos_1, empty_pos_1, empty_pos_2
        {-2 * board_size_ - 1, -board_size_ - 1, -board_size_},
        {-board_size_ + 1, -board_size_, 1},
        {board_size_ + 2, 1, board_size_ + 1},
        {2 * board_size_ + 1, board_size_ + 1, board_size_},
        {board_size_ - 1, board_size_, -1},
        {-board_size_ - 2, -1, -board_size_ - 1}
    };

    return (board_[action + offsets[bridgeType][0]] == static_cast<Player>(player))
        && (board_[action + offsets[bridgeType][1]] == Player::kPlayerNone)
        && (board_[action + offsets[bridgeType][2]] == Player::kPlayerNone);
}

std::vector<float> HavannahEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 4 channels:
        0~1. own/opponent's position
        2. Black's turn
        3. White's turn
        4~9: on own virtual bridge
        10~15: on opponent's virtual bridge
        16~21: this position make player a virtual bridge
        22~27: this position make opponent a virtual bridge
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < getNumInputChannels(); ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = pos;
            switch (channel) {
                case 0:
                    vFeatures.push_back((board_[rotation_pos] == turn_ ? 1.0f : 0.0f));
                    break;
                case 1:
                    vFeatures.push_back((board_[rotation_pos] == getNextPlayer(turn_, kHavannahNumPlayer) ? 1.0f : 0.0f));
                    break;
                case 2:
                    vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
                    break;
                case 3:
                    vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
                    break;
                default:
                    break;
            }
            if (channel >= 4 && channel <= 9) {
                vFeatures.push_back((on_virtual_bridge(rotation_pos, turn_, channel - 4) ? 1.0f : 0.0f));
            }
            if (channel >= 10 && channel <= 15) {
                vFeatures.push_back((on_virtual_bridge(rotation_pos, getNextPlayer(turn_, kHavannahNumPlayer), channel - 4) ? 1.0f : 0.0f));
            }
            if (channel >= 16 && channel <= 21) {
                vFeatures.push_back((make_virtual_bridge(rotation_pos, turn_, channel - 10) ? 1.0f : 0.0f));
            }
            if (channel >= 22 && channel <= 27) {
                vFeatures.push_back((make_virtual_bridge(rotation_pos, getNextPlayer(turn_, kHavannahNumPlayer), channel - 10) ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> HavannahEnv::getActionFeatures(const HavannahAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    action_features[action.getActionID()] = 1.0f;
    return action_features;
}

std::string HavannahEnv::toString() const
{
    // Declare the two new color variables
    auto color_player_1 = minizero::utils::TextColor::kRed;
    auto color_player_2 = minizero::utils::TextColor::kBlue;

    std::vector<char> rr{};

    // Printing the alphabets on top
    rr.push_back(' ');
    rr.push_back(' ');
    rr.push_back(' ');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        std::string colored{minizero::utils::getColorText(
            " " + std::string(1, ii + 97 + (ii > 7 ? 1 : 0)), minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_2)};
        rr.insert(rr.end(), colored.begin(), colored.end());
    }
    rr.push_back('\n');

    for (size_t ii = board_size_; ii-- > 0;) {
        // Adding leading spaces
        rr.push_back(' ');
        for (size_t jj = 0; jj < board_size_ - ii - 1; jj++) {
            rr.push_back(' ');
        }

        // Printing row number
        std::string row_num = std::to_string(ii + 1);
        std::string colored_row_num{minizero::utils::getColorText(
            row_num, minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_1)};
        rr.insert(rr.end(), colored_row_num.begin(), colored_row_num.end());

        // Adding extra space to compensate for larger row numbers
        if (ii < 9) rr.push_back(' ');

        rr.push_back('\\');

        // Printing board cells
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            auto cell = board_[jj + static_cast<size_t>(board_size_) * ii];
            if (cell == Player::kPlayer1) {
                std::string colored{minizero::utils::getColorText(
                    "B ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
                    color_player_1)};
                rr.insert(rr.end(), colored.begin(), colored.end());
            } else if (cell == Player::kPlayer2) {
                std::string colored{minizero::utils::getColorText(
                    "W ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
                    color_player_2)};
                rr.insert(rr.end(), colored.begin(), colored.end());
            } else {
                rr.push_back('[');
                rr.push_back(']');
            }
        }

        rr.push_back('\\');
        rr.insert(rr.end(), colored_row_num.begin(), colored_row_num.end());

        // Adding extra space to compensate for larger row numbers
        if (ii < 9) rr.push_back(' ');

        rr.push_back('\n');
    }

    // Adding leading spaces
    rr.push_back(' ');
    for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
        rr.push_back(' ');
    }

    // Printing the alphabets on the bottom
    rr.push_back(' ');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        std::string colored{minizero::utils::getColorText(
            " " + std::string(1, ii + 97 + (ii > 7 ? 1 : 0)), minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_2)};
        rr.insert(rr.end(), colored.begin(), colored.end());
    }
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::string HavannahEnv::toStringDebug() const
{
    std::vector<char> rr{};

    // Print the first line (the alphabet characters).
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(' ');
        rr.push_back(' ');
        rr.push_back(ii + 97 + (ii > 7 ? 1 : 0));
    }
    rr.push_back('\n');

    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        // Indentation.
        for (size_t jj = 0; jj < ii; jj++) {
            rr.push_back(' ');
        }

        // Row number at the start.
        rr.push_back(static_cast<char>(ii + 1 + '0'));
        rr.push_back('\\');
        rr.push_back(' ');

        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            auto cell = board_[jj + static_cast<size_t>(board_size_) * (static_cast<size_t>(board_size_) - ii - 1)];
            if (cell == Player::kPlayer1) {
                rr.push_back('B');
            } else if (cell == Player::kPlayer2) {
                rr.push_back('W');
            } else {
                rr.push_back('.');
            }
            rr.push_back(' ');
            rr.push_back(' ');
        }

        // Row number at the end.
        rr.push_back('\\');
        rr.push_back(static_cast<char>(ii + 1 + '0'));

        rr.push_back('\n');
    }

    // Print the last line (the alphabet characters).
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(' ');
        rr.push_back(' ');
        rr.push_back(ii + 97 + (ii > 8 ? 1 : 0));
    }
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::vector<int> HavannahEnv::getWinningStonesPosition() const
{
    // TODO: Show the winning pattern
    if (winner_ == Player::kPlayerNone) { return {}; }
    std::vector<int> winning_stones{};
    for (size_t ii = 0; ii < board_size_ * board_size_; ii++) {
        if ((winner_ == Player::kPlayer1) || (winner_ == Player::kPlayer2)) {
            winning_stones.push_back(ii);
        }
    }
    return winning_stones;
}

std::vector<int> HavannahEnv::getNeighbors(int action) const
{
    std::vector<int> neighbors;
    int offsets[6] = {
        -1 - board_size_, 0 - board_size_,
        -1 - 0 * board_size_, 1 + 0 * board_size_,
        0 + board_size_, 1 + board_size_};
    for (int i = 0; i < 6; i++) {
        int col = action % board_size_;
        if (col == 0 && (i == 0 || i == 2)) continue;
        if (col == board_size_ - 1 && (i == 3 || i == 5)) continue;
        int neighbor = action + offsets[i];
        if (neighbor < 0 || neighbor >= board_size_ * board_size_) continue;
        if (!isOnBoard(neighbor / board_size_, neighbor % board_size_)) continue;
        neighbors.push_back(neighbor);
    }
    return neighbors;
}

bool HavannahEnv::hasRing(int action)
{
    int group = find(action);
    int num = 0;
    std::vector<bool> connection(board_size_ * board_size_, false);
    for (int i = 0; i < board_size_ * board_size_; i++) {
        if (!isOnBoard(i)) continue;
        if (find(i) == group) {
            connection[i] = true;
            num++;
        }
    }
    std::queue<int> checklist;
    std::vector<bool> in_list(board_size_ * board_size_, false);
    for (int i = 0; i < board_size_ * board_size_; i++) {
        if (!connection[i]) continue;
        std::vector<int> neighbors;
        for (auto neighbor : getNeighbors(i)) {
            if (connection[neighbor]) neighbors.push_back(neighbor);
        }
        if (neighbors.size() >= 3) continue;
        checklist.push(i);
        in_list[i] = true;
    }
    while (!checklist.empty() && num >= 6) {
        int target = checklist.front();
        checklist.pop();
        in_list[target] = false;

        std::vector<int> neighbors;
        for (auto neighbor : getNeighbors(target)) {
            if (connection[neighbor]) neighbors.push_back(neighbor);
        }
        int neighbor_size = neighbors.size();
        if (neighbor_size >= 3) continue;
        if (neighbor_size == 2) {
            bool neighbors_are_not_neighbors = true;
            for (auto neighbor : getNeighbors(neighbors[0])) {
                if (neighbor == neighbors[1]) neighbors_are_not_neighbors = false;
            }
            if (neighbors_are_not_neighbors) continue;
        }

        num--;
        connection[target] = false;
        for (auto neighbor : neighbors) {
            if (in_list[neighbor]) continue;
            checklist.push(neighbor);
            in_list[neighbor] = true;
        }
    }
    return num >= 6;
}

Player HavannahEnv::updateWinner(int action)
{
    // struct Coord{int x{}; int y{};};
    /* neighboorActionIds
      4 5
      |/
    2-C-3
     /|
    0 1
    */

    // Get neighbor cells.
    for (auto neighbor : getNeighbors(action)) {
        if (board_[neighbor] != turn_) continue;
        link(action, neighbor);
    }
    if (hasRing(action)) {
        winner_ = turn_;
    }
    return winner_;
}

std::vector<float> HavannahEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const HavannahAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(getBoardSize() * getBoardSize(), 0.0f);
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? action.getActionID() : utils::Random::randInt() % action_features.size());
    action_features[action_id] = 1.0f;
    return action_features;
}

} // namespace minizero::env::havannah
