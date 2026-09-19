// Copyright 2026 Ayush Sheta (https://github.com/Ayushsheta2005)
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>

namespace market_sim {

enum class Side {
    Buy,
    Sell
};

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using Timestamp = std::uint64_t;

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

struct Trade {
    OrderId resting_order_id;
    OrderId aggressive_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

inline std::string side_to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

}  // namespace market_sim
