#include <iostream>
#include <iomanip>
#include <cassert>
#include "OrderCache.h"

void printOrders(const OrderCache& cache) {
    auto orders = cache.getAllOrders();
    std::cout << std::left << std::setw(8) << "OrderId"
              << std::setw(8) << "SecId"
              << std::setw(6) << "Side"
              << std::setw(6) << "Qty"
              << std::setw(8) << "User"
              << std::setw(10) << "Company" << "\n";
    std::cout << "--------------------------------------------------\n";
    for (const auto& ord : orders) {
        std::cout << std::setw(8) << ord.orderId()
                  << std::setw(8) << ord.securityId()
                  << std::setw(6) << ord.side()
                  << std::setw(6) << ord.qty()
                  << std::setw(8) << ord.user()
                  << std::setw(10) << ord.company() << "\n";
    }
}

void printMatchingSizes(OrderCache& cache, const std::vector<std::string>& secIds) {
    std::cout << "\nMatching sizes:\n";
    for (const auto& secId : secIds) {
        std::cout << secId << ": " << cache.getMatchingSizeForSecurity(secId) << "\n";
    }
}

void testExample(const std::string& name, const std::vector<Order>& orders,
                 const std::vector<std::pair<std::string, unsigned int>>& expectedMatches) {
    OrderCache cache;
    std::cout << "\nTest: " << name;

    for (const auto& ord : orders) cache.addOrder(ord);

    printOrders(cache);

    std::vector<std::string> secIds;
    for (const auto& [secId, _] : expectedMatches) secIds.push_back(secId);

    printMatchingSizes(cache, secIds);

    // Assertions
    for (const auto& [secId, expected] : expectedMatches) {
        assert(cache.getMatchingSizeForSecurity(secId) == expected);
    }
    std::cout << name << " passed all matching size checks\n";
}

int main() {
    testExample("Order Matching Example",
        {
            Order("OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"),
            Order("OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"),
            Order("OrdId3", "SecId1", "Sell", 500, "User3", "CompanyA"),
            Order("OrdId4", "SecId2", "Buy", 600, "User4", "CompanyC"),
            Order("OrdId5", "SecId2", "Buy", 100, "User5", "CompanyB"),
            Order("OrdId6", "SecId3", "Buy", 1000, "User6", "CompanyD"),
            Order("OrdId7", "SecId2", "Buy", 2000, "User7", "CompanyE"),
            Order("OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE")
        },
        {
            {"SecId1", 0},
            {"SecId2", 2700},
            {"SecId3", 0}
        }
    );

    testExample("Example 1",
        {
            Order("OrdId1", "SecId1", "Sell", 100, "User10", "Company2"),
            Order("OrdId2", "SecId3", "Sell", 200, "User8", "Company2"),
            Order("OrdId3", "SecId1", "Buy", 300, "User13", "Company2"),
            Order("OrdId4", "SecId2", "Sell", 400, "User12", "Company2"),
            Order("OrdId5", "SecId3", "Sell", 500, "User7", "Company2"),
            Order("OrdId6", "SecId3", "Buy", 600, "User3", "Company1"),
            Order("OrdId7", "SecId1", "Sell", 700, "User10", "Company2"),
            Order("OrdId8", "SecId1", "Sell", 800, "User2", "Company1"),
            Order("OrdId9", "SecId2", "Buy", 900, "User6", "Company2"),
            Order("OrdId10", "SecId2", "Sell", 1000, "User5", "Company1"),
            Order("OrdId11", "SecId1", "Sell", 1100, "User13", "Company2"),
            Order("OrdId12", "SecId2", "Buy", 1200, "User9", "Company2"),
            Order("OrdId13", "SecId1", "Sell", 1300, "User1", "Company")
        },
        {
            {"SecId1", 300},
            {"SecId2", 1000},
            {"SecId3", 600}
        }
    );

    testExample("Example 2",
        {
            Order("OrdId1", "SecId3", "Sell", 100, "User1", "Company1"),
            Order("OrdId2", "SecId3", "Sell", 200, "User3", "Company2"),
            Order("OrdId3", "SecId1", "Buy", 300, "User2", "Company1"),
            Order("OrdId4", "SecId3", "Sell", 400, "User5", "Company2"),
            Order("OrdId5", "SecId2", "Sell", 500, "User2", "Company1"),
            Order("OrdId6", "SecId2", "Buy", 600, "User3", "Company2"),
            Order("OrdId7", "SecId2", "Sell", 700, "User1", "Company1"),
            Order("OrdId8", "SecId1", "Sell", 800, "User2", "Company1"),
            Order("OrdId9", "SecId1", "Buy", 900, "User5", "Company2"),
            Order("OrdId10", "SecId1", "Sell", 1000, "User1", "Company1"),
            Order("OrdId11", "SecId2", "Sell", 1100, "User6", "Company2")
        },
        {
            {"SecId1", 900},
            {"SecId2", 600},
            {"SecId3", 0}
        }
    );

    std::cout << "\nAll examples completed successfully\n";
    return 0;
}
