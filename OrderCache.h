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

class OrderCache : public OrderCacheInterface {
public:
    OrderCache() = default;

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

    void cancelOrder(const std::string& orderId) override {
        std::unique_lock lock(mutex_);
        removeOrderInternal(orderId);
    }

    void cancelOrdersForUser(const std::string& user) override {
        std::unique_lock lock(mutex_);
        auto uit = userIndex_.find(user);
        if (uit == userIndex_.end()) return;

        std::vector<Order*> toRemove(uit->second.begin(), uit->second.end());
        for (auto ptr : toRemove) removeOrderInternal(ptr->orderId());
    }

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

    unsigned int getMatchingSizeForSecurity(const std::string& securityId) override {
        std::shared_lock lock(mutex_);
        auto sit = secIndex_.find(securityId);
        if (sit == secIndex_.end()) return 0;

        auto& buys = sit->second["Buy"];
        auto& sells = sit->second["Sell"];

        if (buys.empty() || sells.empty()) return 0;

        // Avoid repeated allocations
        std::unordered_map<Order*, unsigned int> buyQty;
        std::unordered_map<Order*, unsigned int> sellQty;

        for (auto ptr : buys) buyQty[ptr] = ptr->qty();
        for (auto ptr : sells) sellQty[ptr] = ptr->qty();

        unsigned int totalMatched = 0;

        for (auto buyPtr : buys) {
            unsigned int& buyRemaining = buyQty[buyPtr];
            if (buyRemaining == 0) continue;

            for (auto sellPtr : sells) {
                unsigned int& sellRemaining = sellQty[sellPtr];
                if (sellRemaining == 0) continue;
                if (buyPtr->company() == sellPtr->company()) continue;

                unsigned int matchedQty = std::min(buyRemaining, sellRemaining);
                buyRemaining -= matchedQty;
                sellRemaining -= matchedQty;
                totalMatched += matchedQty;

                if (buyRemaining == 0) break;
            }
        }

        return totalMatched;
    }

    std::vector<Order> getAllOrders() const override {
        std::shared_lock lock(mutex_);
        std::vector<Order> result;
        result.reserve(orders_.size());
        for (auto& kv : orders_) result.push_back(kv.second);
        return result;
    }

private:
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

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Order> orders_;
    std::unordered_map<std::string, std::unordered_set<Order*>> userIndex_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<Order*>>> secIndex_;
    std::unordered_map<std::string, unsigned int> securityQty_;
};
