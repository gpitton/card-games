#pragma once

// Implementation of the Monte Carlo game search engine.
// We need to include boost/json/src.hpp to import boost/json
// as header-only library and avoid linking.
#include "boost/json/fwd.hpp"
#include "boost/json/value_to.hpp"
#include <bits/ranges_algo.h>
#include <bits/ranges_algobase.h>
#include <boost/json/src.hpp>
//#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/message.hpp>
#include <algorithm>
#include <array>
#include <bitset>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include <iostream>

namespace http = boost::beast::http;

class GameState
{
public:
    template <class Body, class Fields>
    static GameState fromJson(const http::request<Body, Fields>& req)
    {
        boost::system::error_code ec;
        boost::json::value v = boost::json::parse(req.body(), ec);
        if (ec)
        {
            throw std::runtime_error(ec.what());
        }
        return GameState{std::move(v)};
    }

    // TODO replace with static vector/array.
    // Returns a vector with the cards still in the deck, including
    // those that could be in the opponent's hand. The order is not
    // important as we do not know what are the cards outstanding,
    // except for the last card in the deck which is known.
    std::vector<int> deckCards() const
    {
        std::vector<int> cards;
        cards.reserve(40);
        for(int i = 0; i < 40; ++i)
        {
            if ((i != m_trump_card)
                && (!m_spent_cards.test(i))
                && std::ranges::find(m_hand, i) == m_hand.end())
            {
                cards.push_back(i);
            }
        }
        // Put the trump card at the end of the deck.
        cards.push_back(m_trump_card);
        return cards;
    }

    // Returns how many hands (or turns) are left to play.
    int nHandsLeft() const
    {
        return (40 - 2 * m_hand.size() - m_spent_cards.count()) / 2;
    }

    const std::vector<int>& playerHand() const
    {
        return m_hand;
    }

    int firstPlayer() const
    {
        return m_first_player;
    }

    int trumpCard() const
    {
        return m_trump_card;
    }

    // Returns the number of points of the first player.
    int points() const
    {
        return m_points.at(0);
    }

private:
    GameState(boost::json::value&& v)
        :
        m_points{boost::json::value_to<std::array<int, 2>>(v.at(s_key_points))},
        m_hand{boost::json::value_to<std::vector<int>>(v.at(s_key_hand))},
        m_first_player(v.at(s_key_first_player).as_int64()),
        m_trump_card(v.at(s_key_trump_card).as_int64())
    {
        for (const auto& i : v.at(s_key_spent_cards).as_array())
        {
            // We need to subtract 1 because the javascript implementation
            // counts from 1.
            m_spent_cards.set(i.as_int64() - 1);
        }
    }

    // Expected keys for the Json dictionary.
    static constexpr std::string_view s_key_points {"points"};
    static constexpr std::string_view s_key_hand {"hand"};
    static constexpr std::string_view s_key_first_player {"first_to_play"};
    static constexpr std::string_view s_key_trump_card {"trump_card"};
    static constexpr std::string_view s_key_spent_cards {"spent_cards"};

    std::array<int, 2> m_points;
    std::vector<int> m_hand;
    std::bitset<40> m_spent_cards;
    int m_first_player;
    int m_trump_card;
};


// Returns the points obtained by player 0 if it plays the
// sequence of cards `sequence0` when the game is in a state
// given by `game` and the deck is in a state given by `deck`
// and the opponent plays the cards of `sequence1`.
class Evaluator
{
public:
    static int evaluatePlay(
        const GameState& game,
        const std::vector<int>& deck,
        const std::vector<int>& sequence0,
        const std::vector<int>& sequence1
    );

private:
    // Calculates if the first player won the hand and the number of
    // points of the hand.
    static std::pair<bool, int> evaluateHand(
        int c0, int c1, int trump_card
    );

    // Strength of each card, from ace to king.
    static constexpr int strength[]{9, 0, 8, 1, 2, 3, 4, 5, 6, 7};
    // Value of each card, from ace to king.
    static constexpr int value[]{11, 0, 10, 0, 0, 0, 0, 2, 3, 4};
};


/* static */ int Evaluator::evaluatePlay(
    const GameState& game,
    const std::vector<int>& deck,
    const std::vector<int>& sequence0,
    const std::vector<int>& sequence1
)
{
    std::array<int, 3> hand0;
    std::ranges::copy_n(game.playerHand().cbegin(), 3, hand0.begin());
    // We choose to use the first three cards of deck as the
    // cards in opponent's hand.
    // TODO what if deck has < 3 cards?
    std::array<int, 3> hand1{deck.at(0), deck.at(1), deck.at(2)};
    int first_player = game.firstPlayer();
    auto it_next_card = deck.cbegin() + 3;
    auto it_p0 = sequence0.cbegin();
    auto it_p1 = sequence1.cbegin();
    int pts = game.points();
    std::pair<bool, int> hand_res;
    while (it_p0 != sequence0.end())
    {
        const int c0 = hand0.at(*it_p0);
        const int c1 = hand1.at(*it_p1);
        if (first_player == 0)
        {
            hand_res = evaluateHand(c0, c1, game.trumpCard());
            first_player = static_cast<int>(hand_res.first);
        }
        else
        {
            hand_res = evaluateHand(c1, c0, game.trumpCard());
            first_player = static_cast<int>(!hand_res.first);
        }
        if (first_player == 0)
        {
            // Hand won. Increment points.
            pts += hand_res.second;
        }
        // Update cards in hand.
        hand0.at(*it_p0) = *it_next_card++;
        hand1.at(*it_p1) = *it_next_card++;
        // Update iterators.
        ++it_p0;
        ++it_p1;
    }
    return pts;
}

// Calculates if the first player won the hand and the number of
// points of the hand.
/* static */ std::pair<bool, int> Evaluator::evaluateHand(
    const int c0, const int c1, const int trump_card
)
{
    const int suit0 = c0 / 10, suit1 = c1 / 10;
    // Face value
    const int fv0 = c0 % 10, fv1 = c1 % 10;
    // Strength
    const int s0 = strength[fv0], s1 = strength[fv1];
    // Point value
    const int v0 = value[fv0], v1 = value[fv1];
    // Same suit as the trump card?
    const bool is_trump_0 = (trump_card / 10) == suit0;
    const bool is_trump_1 = (trump_card / 10) == suit1;
    if (is_trump_0)
    {
        if (is_trump_1)
        {
            // Both cards are trumps. The highest one wins.
            return {s0 > s1, v0 + v1};
        }
        else
        {
            // First player wins.
            return {true, v0 + v1};
        }
    }
    else
    {
        if (is_trump_1)
        {
            // Second player wins.
            return {false, v0 + v1};
        }
        else if (suit0 == suit1)
        {
            // Same suit. Highest card wins.
            return {s0 > s1, v0 + v1};
        }
        else
        {
            // Different suit. First player wins.
            return {true, v0 + v1};
        }
    }
}


// The MonteCarloEngine accepts a game state and explores the
// space of possible games that can issue from the given state.
// At each iteration it generates a random shuffle of the deck
// and simulates a game by picking random cards for each player.
// For each card in the player's hand, it keeps track of the
// fraction of games that are won if that card is played first.
// TODO rename this class: SerialEngine, which implements a
// MonteCarloEngine interface.
class MonteCarloEngine
{
public:
    MonteCarloEngine(int n_games = 1'024)
      : m_ngames{n_games}
    {}

    void run(const GameState& game, double ps[3]);

private:
    // Generates a random playing strategy with n_cards cards.
    // Returns a vector of ints having values in [0, 1, 2] that
    // encode which card of the player's hand is played at each
    // turn.
    std::vector<int> randomPlay(int n_cards) const;

    // Number of games to simulate.
    int m_ngames;
    mutable std::mt19937 m_gen;
};


void MonteCarloEngine::run(const GameState& game, double ps[3])
{
    // Zero-out the memory buffer.
    ps[0] = 0.0;
    ps[1] = 0.0;
    ps[2] = 0.0;

    if (game.nHandsLeft() <= 1)
    {
        // Just play whatever card you have in hand.
        return;
    }

    // TODO offload to a pure function for parallel execution?
    std::vector<int> deck_cards = game.deckCards();
    // deck_cards are all cards except those already played
    // and those in player0's hand
    const std::size_t ncards = deck_cards.size();
    auto hidden_deck = std::ranges::take_view(deck_cards, ncards - 1);

    // Number of games played that started with card 0, 1, or 2.
    double n_games[] {0.0, 0.0, 0.0};

    // TODO replace with an algorithm?
    for (int i = 0; i < m_ngames; ++i)
    {
        std::ranges::shuffle(hidden_deck, m_gen);
        const std::vector<int> play0 = randomPlay(game.nHandsLeft());
        const std::vector<int> play1 = randomPlay(game.nHandsLeft());
        const int pts0 = Evaluator::evaluatePlay(game, deck_cards, play0, play1);
        // Index of the first card played. Must be 0, 1, or 2.
        const int idx = play0.front();
        // Count how many games started with card idx.
        n_games[idx] += 1.0;
        // Update the number of wins.
        if (pts0 >= 60)
        {
            ps[idx] += 1.0;
        }
    }
    // Convert number of wins to probabilities.
    ps[0] /= n_games[0];
    ps[1] /= n_games[1];
    ps[2] /= n_games[2];
}


// Generates a random playing strategy with n_cards cards.
// Returns a vector of ints having values in [0, 1, 2] that
// encode which card of the player's hand is played at each
// turn.
std::vector<int> MonteCarloEngine::randomPlay(const int n_hands) const
{
    std::vector<int> play(n_hands);
    std::uniform_int_distribution dist(0, 2), dist2(0, 1);
    for (int i = 0; i < n_hands - 2; ++i)
    {
        play.at(i) = dist(m_gen);
    }
    // Assumption: n_hands >= 2.
    play.at(n_hands - 2) = dist2(m_gen);
    play.back() = 0;
    return play;
}

