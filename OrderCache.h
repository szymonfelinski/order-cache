#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

#include "OrderCacheInterface.h"

class OrderCache : public OrderCacheInterface
{
public:
    OrderCache() = default;

    // add order to the cache
    void addOrder(Order order) override
    {
        std::unique_lock lock(mutex_); // exclusive lock

        const std::string id = order.orderId();
        auto it = orders_.find(id);
        if (it != orders_.end()) {
            removeOrderInternal(id);
        }

        unsigned int qty = order.qty();
        const std::string& sec = order.securityId();
        const std::string& user = order.user();

        orders_.emplace(id, std::move(order));
        userIndex_[user].insert(id);
        secIndex_[sec].insert(id);
        securityQty_[sec] += qty;
    }

    // remove order with this unique order id from the cache
    void cancelOrder(const std::string& orderId) override
    {
        std::unique_lock lock(mutex_);

        auto it = orders_.find(orderId);
        if (it == orders_.end()) return;
        removeOrderInternal(orderId);
    }

    // remove all orders in the cache for this user
    void cancelOrdersForUser(const std::string& user) override
    {
        std::unique_lock lock(mutex_);

        auto uit = userIndex_.find(user);
        if (uit == userIndex_.end()) return;

        std::vector<std::string> toRemove;
        toRemove.reserve(uit->second.size());
        for (const auto& id : uit->second) toRemove.push_back(id);

        for (const auto& id : toRemove) removeOrderInternal(id);
    }

    // remove all orders in the cache for this security with qty >= minQty
    void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) override
    {
        std::unique_lock lock(mutex_);

        auto sit = secIndex_.find(securityId);
        if (sit == secIndex_.end()) return;

        std::vector<std::string> toRemove;
        toRemove.reserve(sit->second.size());
        for (const auto& id : sit->second) {
            auto oit = orders_.find(id);
            if (oit != orders_.end() && oit->second.qty() >= minQty) {
                toRemove.push_back(id);
            }
        }

        for (const auto& id : toRemove) removeOrderInternal(id);
    }

    // return the total qty that can match for the security id
    unsigned int getMatchingSizeForSecurity(const std::string& securityId) override
    {
        std::shared_lock lock(mutex_); // shared lock for reads

        unsigned int totalMatched = 0;
        std::vector<Order> buys;
        std::vector<Order> sells;

        auto sit = secIndex_.find(securityId);
        if (sit == secIndex_.end()) return 0;

        for (const auto& orderId : sit->second) {
            auto oit = orders_.find(orderId);
            if (oit == orders_.end()) continue;
            const Order& ord = oit->second;
            if (ord.side() == "Buy") buys.push_back(ord);
            else if (ord.side() == "Sell") sells.push_back(ord);
        }

        std::unordered_map<std::string, unsigned int> buyQty;
        std::unordered_map<std::string, unsigned int> sellQty;
        for (const auto& b : buys) buyQty[b.orderId()] = b.qty();
        for (const auto& s : sells) sellQty[s.orderId()] = s.qty();

        for (auto& buy : buys) {
            unsigned int& buyRemaining = buyQty[buy.orderId()];
            for (auto& sell : sells) {
                unsigned int& sellRemaining = sellQty[sell.orderId()];
                if (buy.company() == sell.company()) continue;
                if (buyRemaining == 0 || sellRemaining == 0) continue;

                unsigned int matchedQty = std::min(buyRemaining, sellRemaining);
                buyRemaining -= matchedQty;
                sellRemaining -= matchedQty;
                totalMatched += matchedQty;
            }
        }

        return totalMatched;
    }

    // return all orders in cache in a vector
    std::vector<Order> getAllOrders() const override
    {
        std::shared_lock lock(mutex_);

        std::vector<Order> result;
        result.reserve(orders_.size());
        for (const auto& kv : orders_) {
            result.push_back(kv.second);
        }
        return result;
    }

private:
    void removeOrderInternal(const std::string& orderId)
    {
        auto oit = orders_.find(orderId);
        if (oit == orders_.end()) return;

        const Order& ord = oit->second;
        const std::string& user = ord.user();
        const std::string& sec = ord.securityId();
        unsigned int qty = ord.qty();

        auto sqit = securityQty_.find(sec);
        if (sqit != securityQty_.end()) {
            if (sqit->second <= qty) {
                securityQty_.erase(sqit);
            } else {
                sqit->second -= qty;
            }
        }

        auto sit = secIndex_.find(sec);
        if (sit != secIndex_.end()) {
            sit->second.erase(orderId);
            if (sit->second.empty()) secIndex_.erase(sit);
        }

        auto uit = userIndex_.find(user);
        if (uit != userIndex_.end()) {
            uit->second.erase(orderId);
            if (uit->second.empty()) userIndex_.erase(uit);
        }

        orders_.erase(oit);
    }

    mutable std::shared_mutex mutex_; // protects all members

    std::unordered_map<std::string, Order> orders_;
    std::unordered_map<std::string, std::unordered_set<std::string>> userIndex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> secIndex_;
    std::unordered_map<std::string, unsigned int> securityQty_;
};
