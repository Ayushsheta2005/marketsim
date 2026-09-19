// Copyright 2026 Ayush Sheta (https://github.com/Ayushsheta2005)
// SPDX-License-Identifier: MIT

#include "MarketDataReplay.hpp"

#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Default to the bundled synthetic sample, but accept a path argument so
    // the same demo can run against a real LOBSTER message file (e.g. the
    // free sample downloadable from lobsterdata.com).
    std::string path = "data/sample_market_message.csv";
    if (argc > 1) {
        path = argv[1];
    }

    std::ifstream input(path);

    if (!input) {
        std::cerr << "Could not open " << path << "\n";
        return 1;
    }

    market_sim::OrderBook book;
    auto summary = market_sim::replay_lobster_messages(input, book);

    std::cout << "Market data replay: " << path << "\n";
    std::cout << "Events processed:    " << summary.events_processed << "\n";
    std::cout << "New limit orders:    " << summary.new_orders << "\n";
    std::cout << "Partial cancels:     " << summary.partial_cancels << "\n";
    std::cout << "Deletions:           " << summary.deletions << "\n";
    std::cout << "Executions (counted):" << summary.executions << "\n";
    std::cout << "Hidden executions:   " << summary.hidden_executions << "\n";
    std::cout << "Auxiliary events:    " << summary.auxiliary << "\n";
    std::cout << "Successful cancels:  " << summary.successful_cancels << "\n";
    std::cout << "Executed quantity:   " << summary.executed_quantity << "\n";
    std::cout << "Resting order count: " << book.order_count() << "\n";

    if (auto bid = book.best_bid()) {
        std::cout << "Best bid: " << *bid << "\n";
    } else {
        std::cout << "Best bid: empty\n";
    }

    if (auto ask = book.best_ask()) {
        std::cout << "Best ask: " << *ask << "\n";
    } else {
        std::cout << "Best ask: empty\n";
    }

    return 0;
}
