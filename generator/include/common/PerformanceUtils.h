#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SCXML {
namespace Common {

/**
 * @brief Fast set implementation for state IDs with optimized lookup
 */
class FastStateSet {
private:
    std::unordered_set<std::string> set_;
    mutable bool sorted_cache_valid_ = false;
    mutable std::vector<std::string> sorted_cache_;

public:
    FastStateSet() {
        set_.reserve(64);  // Reserve space for typical usage
    }

    void insert(const std::string &stateId) {
        set_.insert(stateId);
        sorted_cache_valid_ = false;
    }

    void erase(const std::string &stateId) {
        set_.erase(stateId);
        sorted_cache_valid_ = false;
    }

    bool contains(const std::string &stateId) const {
        return set_.find(stateId) != set_.end();
    }

    size_t size() const {
        return set_.size();
    }

    bool empty() const {
        return set_.empty();
    }

    void clear() {
        set_.clear();
        sorted_cache_valid_ = false;
    }

    // Get sorted vector for iteration (cached)
    const std::vector<std::string> &getSorted() const {
        if (!sorted_cache_valid_) {
            sorted_cache_.clear();
            sorted_cache_.reserve(set_.size());
            sorted_cache_.assign(set_.begin(), set_.end());
            std::sort(sorted_cache_.begin(), sorted_cache_.end());
            sorted_cache_valid_ = true;
        }
        return sorted_cache_;
    }

    // Iterator support
    auto begin() const {
        return set_.begin();
    }

    auto end() const {
        return set_.end();
    }
};

/**
 * @brief Cache with LRU eviction policy
 */
template <typename Key, typename Value> class LRUCache {
private:
    struct Node {
        Key key;
        Value value;
        Node *prev = nullptr;
        Node *next = nullptr;
    };

    std::unordered_map<Key, std::unique_ptr<Node>> map_;
    Node *head_ = nullptr;
    Node *tail_ = nullptr;
    size_t capacity_;
    size_t size_ = 0;

    void moveToFront(Node *node) {
        if (node == head_) {
            return;
        }

        // Remove from current position
        if (node->prev) {
            node->prev->next = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        if (node == tail_) {
            tail_ = node->prev;
        }

        // Move to front
        node->prev = nullptr;
        node->next = head_;
        if (head_) {
            head_->prev = node;
        }
        head_ = node;
        if (!tail_) {
            tail_ = node;
        }
    }

    void removeLRU() {
        if (!tail_) {
            return;
        }

        Node *lru = tail_;
        tail_ = tail_->prev;
        if (tail_) {
            tail_->next = nullptr;
        } else {
            head_ = nullptr;
        }

        map_.erase(lru->key);
        size_--;
    }

public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        map_.reserve(capacity * 2);  // Reserve extra space to avoid rehashing
    }

    ~LRUCache() {
        clear();
    }

    void put(const Key &key, const Value &value) {
        auto it = map_.find(key);

        if (it != map_.end()) {
            // Update existing
            it->second->value = value;
            moveToFront(it->second.get());
            return;
        }

        // Add new
        if (size_ >= capacity_) {
            removeLRU();
        }

        auto node = std::make_unique<Node>();
        node->key = key;
        node->value = value;
        node->next = head_;
        if (head_) {
            head_->prev = node.get();
        }
        head_ = node.get();
        if (!tail_) {
            tail_ = node.get();
        }

        map_[key] = std::move(node);
        size_++;
    }

    std::optional<Value> get(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }

        moveToFront(it->second.get());
        return it->second->value;
    }

    bool contains(const Key &key) const {
        return map_.find(key) != map_.end();
    }

    void clear() {
        map_.clear();
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }
};

/**
 * @brief String pool to reduce memory allocation for commonly used strings
 */
class StringPool {
private:
    std::unordered_set<std::string> pool_;

public:
    const std::string &intern(const std::string &str) {
        auto [it, inserted] = pool_.insert(str);
        return *it;
    }

    const std::string &intern(std::string &&str) {
        auto [it, inserted] = pool_.insert(std::move(str));
        return *it;
    }

    void clear() {
        pool_.clear();
    }

    size_t size() const {
        return pool_.size();
    }
};

/**
 * @brief Efficient vector operations
 */
namespace VectorUtils {

template <typename T> void fastRemove(std::vector<T> &vec, const T &value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        // Swap with last element and pop (O(1) removal)
        if (it != vec.end() - 1) {
            *it = std::move(vec.back());
        }
        vec.pop_back();
    }
}

template <typename T> bool fastContains(const std::vector<T> &vec, const T &value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template <typename T> void removeDuplicates(std::vector<T> &vec) {
    std::sort(vec.begin(), vec.end());
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template <typename T> std::vector<T> intersection(const std::vector<T> &a, const std::vector<T> &b) {
    std::vector<T> result;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}
}  // namespace VectorUtils

/**
 * @brief Pattern matching utilities with caching
 */
class PatternMatcher {
private:
    LRUCache<std::string, bool> cache_{256};

public:
    bool matchWildcard(const std::string &text, const std::string &pattern) {
        std::string cacheKey = pattern + "|" + text;

        auto cached = cache_.get(cacheKey);
        if (cached.has_value()) {
            return *cached;
        }

        bool result = matchWildcardImpl(text, pattern);
        cache_.put(cacheKey, result);
        return result;
    }

private:
    bool matchWildcardImpl(const std::string &text, const std::string &pattern) {
        if (pattern == text) {
            return true;
        }
        if (pattern.empty()) {
            return text.empty();
        }
        if (text.empty()) {
            return pattern == "*";
        }

        // Simple wildcard matching
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.length() - 1);
            return text.length() >= prefix.length() && text.substr(0, prefix.length()) == prefix;
        }

        if (pattern.front() == '*') {
            std::string suffix = pattern.substr(1);
            return text.length() >= suffix.length() && text.substr(text.length() - suffix.length()) == suffix;
        }

        // Pattern with * in the middle
        size_t starPos = pattern.find('*');
        if (starPos != std::string::npos) {
            std::string prefix = pattern.substr(0, starPos);
            std::string suffix = pattern.substr(starPos + 1);

            return text.length() >= prefix.length() + suffix.length() && text.substr(0, prefix.length()) == prefix &&
                   text.substr(text.length() - suffix.length()) == suffix;
        }

        return false;
    }
};

}  // namespace Common
}  // namespace SCXML