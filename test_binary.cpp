#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

#include "OrderCache.h"
#include "Order.h"

void printMatchingSize(OrderCache& cache, const std::string& secId) {
    std::cout << "Matching size for " << secId << " = "
              << cache.getMatchingSizeForSecurity(secId) << "\n";
}

void testDuplicateOrderIds() {
    std::cout << "=== Test: Duplicate Order IDs ===\n";
    OrderCache cache;

    cache.addOrder(Order("Ord1", "Sec1", "Buy", 100, "User1", "CompanyA"));
    try {
        cache.addOrder(Order("Ord1", "Sec1", "Sell", 50, "User2", "CompanyB"));
        assert(false && "Expected exception for duplicate Order ID");
    } catch (const std::exception& e) {
        std::cout << "Caught expected exception for duplicate Order ID.\n";
    }
    assert(cache.getAllOrders().size() == 1);
}

void testRemoveNonExistingOrder() {
    std::cout << "=== Test: Remove Non-existing Order ===\n";
    OrderCache cache;

    cache.addOrder(Order("Ord1", "Sec1", "Buy", 100, "User1", "CompanyA"));
    cache.cancelOrder("NonExistOrder"); // should do nothing, not throw
    assert(cache.getAllOrders().size() == 1);
    std::cout << "Remove non-existing order test passed.\n";
}

void testMatchingRules() {
    std::cout << "=== Test: Matching Rules ===\n";
    OrderCache cache;

    // Buy and Sell orders for same security
    cache.addOrder(Order("Ord1", "Sec1", "Buy", 1000, "User1", "CompanyA"));
    cache.addOrder(Order("Ord2", "Sec1", "Sell", 400, "User2", "CompanyB"));
    cache.addOrder(Order("Ord3", "Sec1", "Sell", 700, "User3", "CompanyA")); // same company

    assert(cache.getMatchingSizeForSecurity("Sec1") == 400);
    printMatchingSize(cache, "Sec1");
}

void testMultipleSameCompanyOrders() {
    std::cout << "=== Test: Multiple Same Company Orders ===\n";
    OrderCache cache;

    cache.addOrder(Order("Ord1", "Sec1", "Buy", 100, "User1", "CompanyA"));
    cache.addOrder(Order("Ord2", "Sec1", "Sell", 100, "User2", "CompanyA")); // same company
    assert(cache.getMatchingSizeForSecurity("Sec1") == 0);
}

void testCancelOrdersForUser() {
    std::cout << "=== Test: Cancel Orders for User ===\n";
    OrderCache cache;

    cache.addOrder(Order("Ord1", "Sec1", "Buy", 100, "User1", "CompanyA"));
    cache.addOrder(Order("Ord2", "Sec1", "Sell", 200, "User1", "CompanyB"));
    cache.addOrder(Order("Ord3", "Sec2", "Buy", 300, "User2", "CompanyC"));

    cache.cancelOrdersForUser("User1");
    assert(cache.getAllOrders().size() == 1);
}

void testCancelOrdersForSecIdWithMinQty() {
    std::cout << "=== Test: Cancel Orders for SecId with MinQty ===\n";
    OrderCache cache;

    cache.addOrder(Order("Ord1", "Sec1", "Buy", 100, "User1", "CompanyA"));
    cache.addOrder(Order("Ord2", "Sec1", "Sell", 150, "User2", "CompanyB"));
    cache.addOrder(Order("Ord3", "Sec1", "Buy", 200, "User3", "CompanyC"));

    cache.cancelOrdersForSecIdWithMinimumQty("Sec1", 150);
    assert(cache.getAllOrders().size() == 1);
}

void testLargeDataset() {
    std::cout << "=== Test: Large Dataset ===\n";
    OrderCache cache;
    constexpr int N = 100000;

    for (int i = 0; i < N; i++) {
        cache.addOrder(Order("Ord" + std::to_string(i),
                             "Sec" + std::to_string(i % 10),
                             i % 2 ? "Buy" : "Sell",
                             100 + i,
                             "User" + std::to_string(i),
                             "Company" + std::to_string(i % 5)));
    }

    assert(cache.getAllOrders().size() == N);
}

void testMultithreadedStress() {
    std::cout << "=== Test: Multithreaded Stress ===\n";
    OrderCache cache;
    constexpr int numThreads = 8;
    constexpr int ordersPerThread = 1000;

    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < ordersPerThread; i++) {
                std::string id = "T" + std::to_string(t) + "_O" + std::to_string(i);
                std::string sec = "Sec" + std::to_string(i % 10);
                std::string side = (i % 2 == 0 ? "Buy" : "Sell");
                try {
                    cache.addOrder(Order(id, sec, side, 100 + i, "User" + std::to_string(t), "Company" + std::to_string(i % 3)));
                } catch (...) {}
            }
        });
    }

    threads.emplace_back([&cache]() {
        for (int i = 0; i < 50; i++) {
            for (int s = 0; s < 10; s++) {
                cache.getMatchingSizeForSecurity("Sec" + std::to_string(s));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    for (auto& th : threads) th.join();
}

int main() {
    testDuplicateOrderIds();
    testRemoveNonExistingOrder();
    testMatchingRules();
    testMultipleSameCompanyOrders();
    testCancelOrdersForUser();
    testCancelOrdersForSecIdWithMinQty();
    testLargeDataset();
    testMultithreadedStress();

    std::cout << "\nAll tests completed successfully.\n";
    return 0;
}
