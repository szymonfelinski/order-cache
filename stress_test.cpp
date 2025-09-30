#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

#include "OrderCache.h"

void addOrders(OrderCache& cache, int threadId, int count) {
    for (int i = 0; i < count; ++i) {
        std::string orderId = "T" + std::to_string(threadId) + "_Order" + std::to_string(i);
        std::string secId = "Sec" + std::to_string(i % 5);
        std::string side = (i % 2 == 0) ? "Buy" : "Sell";
        unsigned int qty = (i + 1) * 10;
        std::string user = "User" + std::to_string(threadId);
        std::string company = "Company" + std::to_string(i % 3);

        cache.addOrder(Order(orderId, secId, side, qty, user, company));
    }
}

void cancelOrders(OrderCache& cache, const std::vector<std::string>& orderIds) {
    for (const auto& id : orderIds) {
        cache.cancelOrder(id);
    }
}

void readMatchingSizes(OrderCache& cache, const std::vector<std::string>& secIds) {
    for (const auto& secId : secIds) {
        unsigned int size = cache.getMatchingSizeForSecurity(secId);
        std::cout << "[Reader] Matching size for " << secId << " = " << size << "\n";
    }
}

int main() {
    OrderCache cache;

    constexpr int numThreads = 8;
    constexpr int ordersPerThread = 100;

    // Thread pool
    std::vector<std::thread> threads;

    // Launch add threads
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(addOrders, std::ref(cache), t, ordersPerThread);
    }

    // Launch read threads
    threads.emplace_back([&]() {
        std::vector<std::string> secs = {"Sec0", "Sec1", "Sec2", "Sec3", "Sec4"};
        for (int i = 0; i < 50; ++i) {
            readMatchingSizes(cache, secs);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Launch cancel threads
    threads.emplace_back([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto orders = cache.getAllOrders();
        std::vector<std::string> toCancel;
        for (size_t i = 0; i < orders.size(); i += 3) {
            toCancel.push_back(orders[i].orderId());
        }
        cancelOrders(cache, toCancel);
    });

    // Join all threads
    for (auto& th : threads) {
        th.join();
    }

    // Final state
    std::cout << "\nFinal orders in cache: " << cache.getAllOrders().size() << "\n";
    for (const auto& sec : {"Sec0", "Sec1", "Sec2", "Sec3", "Sec4"}) {
        std::cout << "Final matching size for " << sec << ": "
                  << cache.getMatchingSizeForSecurity(sec) << "\n";
    }

    std::cout << "Multithreaded stress test complete.\n";
    return 0;
}
