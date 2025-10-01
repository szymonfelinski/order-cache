#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

#include "OrderCacheInterface.h"

// OrderCache maintains a thread-safe collection of orders with efficient lookup by user, security, and side
// Supports matching of buy/sell orders while preventing matches between orders from the same company
class OrderCache : public OrderCacheInterface {
public:
    OrderCache() = default;

    // Adds a new order to the cache and updates all indices
    // Throws if an order with the same ID already exists
    // Thread-safe: uses unique_lock
    void addOrder(Order order) override {
        std::unique_lock lock(mutex_);

        const std::string id = order.orderId();
        if (orders_.find(id) != orders_.end()) {
            throw std::runtime_error("Order with ID already exists: " + id);
        }

        auto orderPtr = &orders_.emplace(id, std::move(order)).first->second;

        userIndex_[orderPtr->user()].insert(orderPtr);
        secIndex_[orderPtr->securityId()][orderPtr->side()].insert(orderPtr);
        securityQty_[orderPtr->securityId()] += orderPtr->qty();
    }

    // Cancels a single order by its ID
    // No-op if order doesn't exist
    // Thread-safe: uses unique_lock
    void cancelOrder(const std::string& orderId) override {
        std::unique_lock lock(mutex_);
        removeOrderInternal(orderId);
    }

    // Cancels all orders belonging to a specific user
    // No-op if user has no orders
    // Thread-safe: uses unique_lock
    void cancelOrdersForUser(const std::string& user) override {
        std::unique_lock lock(mutex_);
        auto uit = userIndex_.find(user);
        if (uit == userIndex_.end()) return;

        std::vector<Order*> toRemove(uit->second.begin(), uit->second.end());
        for (auto ptr : toRemove) removeOrderInternal(ptr->orderId());
    }

    // Cancels all orders for a security ID where quantity >= minQty
    // No-op if security ID doesn't exist or no orders meet the quantity threshold
    // Thread-safe: uses unique_lock
    void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) override {
        std::unique_lock lock(mutex_);
        auto sit = secIndex_.find(securityId);
        if (sit == secIndex_.end()) return;

        std::vector<Order*> toRemove;
        for (auto& [side, ordersSet] : sit->second) {
            for (auto ptr : ordersSet) {
                if (ptr->qty() >= minQty) toRemove.push_back(ptr);
            }
        }

        for (auto ptr : toRemove) removeOrderInternal(ptr->orderId());
    }

    // Calculates the total quantity that can be matched between buy and sell orders
    // for a given security, excluding matches between orders from the same company
    // Thread-safe: uses shared_lock for concurrent reads
    // Returns 0 if security doesn't exist or no matches are possible
    unsigned int getMatchingSizeForSecurity(const std::string& securityId) override {
        std::shared_lock lock(mutex_);
        auto sit = secIndex_.find(securityId);
        if (sit == secIndex_.end()) return 0;

        auto itBuy = sit->second.find("Buy");
        auto itSell = sit->second.find("Sell");
        if (itBuy == sit->second.end() || itSell == sit->second.end()) return 0;

        const auto& buysSet = itBuy->second;
        const auto& sellsSet = itSell->second;
        if (buysSet.empty() || sellsSet.empty()) return 0;

        // Copy the pointers into vectors to avoid repeated hash table allocations
        std::vector<Order*> buys(buysSet.begin(), buysSet.end());
        std::vector<Order*> sells(sellsSet.begin(), sellsSet.end());
        
        // Create vectors holding the remaining quantities
        std::vector<unsigned int> buyRemaining(buys.size());
        std::vector<unsigned int> sellRemaining(sells.size());
        
        for (size_t i = 0; i < buys.size(); ++i) {
            buyRemaining[i] = buys[i]->qty();
        }
        for (size_t i = 0; i < sells.size(); ++i) {
            sellRemaining[i] = sells[i]->qty();
        }

        unsigned int totalMatched = 0;
        
        // Greedy matching algorithm between buys and sells
        for (size_t i = 0; i < buys.size(); ++i) {
            if (buyRemaining[i] == 0) continue;
            for (size_t j = 0; j < sells.size(); ++j) {
                if (sellRemaining[j] == 0) continue;
                // Skip matching if companies are the same
                if (buys[i]->company() == sells[j]->company()) continue;
                
                unsigned int match = std::min(buyRemaining[i], sellRemaining[j]);
                buyRemaining[i] -= match;
                sellRemaining[j] -= match;
                totalMatched += match;
                
                if (buyRemaining[i] == 0) break;
            }
        }
        
        return totalMatched;
    }

    // Returns a copy of all orders currently in the cache
    // Thread-safe: uses shared_lock for concurrent reads
    std::vector<Order> getAllOrders() const override {
        std::shared_lock lock(mutex_);
        std::vector<Order> result;
        result.reserve(orders_.size());
        for (auto& kv : orders_) result.push_back(kv.second);
        return result;
    }

private:
    // Helper method to remove an order and update all indices
    // Caller must hold unique_lock
    // No-op if order doesn't exist
    void removeOrderInternal(const std::string& orderId) {
        auto oit = orders_.find(orderId);
        if (oit == orders_.end()) return;

        Order* ordPtr = &oit->second;
        const std::string& user = ordPtr->user();
        const std::string& sec = ordPtr->securityId();
        const std::string& side = ordPtr->side();
        unsigned int qty = ordPtr->qty();

        auto sqit = securityQty_.find(sec);
        if (sqit != securityQty_.end()) {
            if (sqit->second <= qty) securityQty_.erase(sqit);
            else sqit->second -= qty;
        }

        auto sit = secIndex_.find(sec);
        if (sit != secIndex_.end()) {
            auto& sideSet = sit->second[side];
            sideSet.erase(ordPtr);
            if (sideSet.empty()) sit->second.erase(side);
            if (sit->second.empty()) secIndex_.erase(sit);
        }

        auto uit = userIndex_.find(user);
        if (uit != userIndex_.end()) {
            uit->second.erase(ordPtr);
            if (uit->second.empty()) userIndex_.erase(uit);
        }

        orders_.erase(oit);
    }

    // Mutex for thread-safe access to the cache
    mutable std::shared_mutex mutex_;
    // Primary storage for all orders, indexed by order ID
    std::unordered_map<std::string, Order> orders_;
    // Index for quick lookup of orders by user
    std::unordered_map<std::string, std::unordered_set<Order*>> userIndex_;
    // Nested index for quick lookup of orders by security ID and side (Buy/Sell)
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<Order*>>> secIndex_;
    // Track total quantity for each security
    std::unordered_map<std::string, unsigned int> securityQty_;
};
