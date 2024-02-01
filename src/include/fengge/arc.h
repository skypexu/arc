/*
 *  Copyright (c) 2024 Xu Yifeng
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#ifndef SRC_INCLUDE_FENGGE_ARC_H_
#define SRC_INCLUDE_FENGGE_ARC_H_

#include <fengge/cache_traits.h>

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <iostream>

namespace fengge {

struct ARCSizeInfo {
    size_t b1, t1;
    size_t b2, t2;

    ARCSizeInfo() : b1(0), t1(0), b2(0), t2(0) {}
    ARCSizeInfo(size_t _b1, size_t _t1, size_t _b2, size_t _t2)
        : b1(_b1), t1(_t1), b2(_b2), t2(_t2) {}
    size_t BSize() const {
        return b1 + b2;
    }
    size_t TSize() const {
        return t1 + t2;
    }
};

enum class ARCQId { B1, T1, B2, T2 };

template <typename K, typename V, typename KeyTraits = CacheTraits<K>,
          typename ValueTraits = CacheTraits<V>>
class ARC {
 public:
    using EvictionCB = std::function<void(const K&, V&&)>;

    ARC(size_t max_count)
     : c_(max_count), p_(0), cached_bytes_(0), cache_hit_(0), cache_miss_(0)
    {}

    void Put(const K& key, const V& value);
    void Put(const K& key, const V& value, const EvictionCB& cb);
    bool Get(const K& key, V* value);
    void Remove(const K& key);
    void Clear();
    size_t Size() const;
    size_t Capacity() const;
    ARCSizeInfo ARCSize() const;
    size_t CachedByteCount() const;
    uint64_t HitCount() const;
    uint64_t MissCount() const;

    // for test purpose
    std::vector<K> GetKeysOfQ(ARCQId q) const;
    std::vector<V> GetValuesOfQ(ARCQId q) const;

 private:
    struct B;
    struct T;
    struct BMapVal;
    struct TMapVal;
    struct BListVal;
    struct TListVal;
    typedef std::unordered_map<K, BMapVal> BMap;
    typedef typename BMap::iterator BMapIter;
    typedef std::unordered_map<K, TMapVal> TMap;
    typedef typename TMap::iterator TMapIter;

    typedef typename std::list<BListVal> BList;
    typedef typename BList::iterator BListIter;
    typedef typename std::list<TListVal> TList;
    typedef typename TList::iterator TListIter;

    ARC(const ARC&) = delete;
    void operator=(const ARC&) = delete;

    void Replace(const K& k, const EvictionCB& evict_cb);
    bool Move_T_B(T* t, B* b, const EvictionCB& evict_cb);
    bool IsCacheFull() const;
    void IncreaseP(size_t delta);
    void DecreaseP(size_t delta);
    void OnCacheHit();
    void OnCacheMiss();
    void UpdateRemoveFromCacheBytes(size_t bytes);
    void UpdateAddToCacheBytes(size_t bytes);

    struct BMapVal {
        BListIter list_iter;

        BMapVal() = default;
        BMapVal(const BMapVal& o) = default;
        BMapVal& operator=(const BMapVal& o) = default;
        BMapVal(const BListIter& iter)  // NOLINT
            : list_iter(iter) {}
    };
    struct TMapVal {
        TListIter list_iter;

        TMapVal() = default;
        TMapVal(const TMapVal& o) = default;
        TMapVal& operator=(const TMapVal& o) = default;
        TMapVal(const TListIter& iter)  // NOLINT
            : list_iter(iter) {}
    };
    struct BListVal {
        BMapIter map_iter;

        BListVal() = default;
        BListVal(const BListVal& o) = default;
        BListVal& operator=(const BListVal& o) = default;
        BListVal(const BMapIter& iter)  // NOLINT
            :map_iter(iter) {}
    };
    struct TListVal {
        TMapIter map_iter;
        V value;

        TListVal() = default;
        TListVal(const TListVal& o) = default;
        TListVal(const TMapIter& iter, const V& v)
            : map_iter(iter), value(v) {}
        TListVal& operator=(const TListVal& o) = default;
    };

    struct B {
        BMap map_;
        BList list_;

        bool Find(const K& k, BMapIter* iter);
        void Insert(const K& k);
        void Remove(BMapIter&& map_iter, ARC* cache);
        void RemoveLRU(ARC* cache);
        size_t Count() const { return map_.size(); }
        void Clear() { list_.clear(); map_.clear(); }
    };

    struct T {
        TMap map_;
        TList list_;

        void Insert(const K& k, const V& v, ARC* cache);
        void Move(const K& k, T* other, TMapIter&& other_it, const V *v,
                  ARC *cache);
        bool Find(const K& k, TMapIter* map_iter);
        void Remove(TMapIter&& map_iter, ARC* cache);
        bool RemoveLRU(const EvictionCB& evict_cb, ARC* cache);
        void Touch(const TMapIter& map_iter, const V* v, ARC* cache);
        size_t Count() const { return map_.size(); }
        TMapIter GetLRU() const { return list_.begin()->map_iter; }
        void Clear() { list_.clear(); map_.clear(); }
    };

    size_t c_;
    size_t p_;
    B b1_;
    B b2_;
    T t1_;
    T t2_;
    size_t cached_bytes_;
    uint64_t cache_hit_;
    uint64_t cache_miss_;
};

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::B::Find(const K& k,
    BMapIter *iter) {
    auto it = map_.find(k);
    if (it != map_.end()) {
        if (iter != nullptr) *iter = it;
        return true;
    }
    return false;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::B::Insert(const K& k) {
    auto r = map_.insert({k, BListIter{}});
    assert(r.second);
    list_.push_back(r.first);
    r.first->second.list_iter = --list_.end();
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::B::Remove(BMapIter&& map_iter,
    ARC* cache) {
    if (cache) {
        cache->UpdateRemoveFromCacheBytes(
            KeyTraits::CountBytes(map_iter->first));
    }
    list_.erase(map_iter->second.list_iter);
    map_.erase(map_iter);
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::B::RemoveLRU(ARC* cache) {
    if (list_.empty()) return;
    if (cache) {
        cache->UpdateRemoveFromCacheBytes(
            KeyTraits::CountBytes(list_.front().map_iter->first));
    }
    map_.erase(list_.front().map_iter);
    list_.pop_front();
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::T::Insert(const K& k, const V& v,
    ARC* cache) {
    auto r = map_.insert({k, TListIter{}});
    assert(r.second);
    list_.emplace_back(r.first, v);
    r.first->second.list_iter = --list_.end();
    if (cache != nullptr) {
        cache->UpdateAddToCacheBytes(KeyTraits::CountBytes(k) +
                ValueTraits::CountBytes(v));
    }
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::T::Move(const K& k, T* other,
    TMapIter&& other_it, const V *v, ARC *cache) {
    // Move key-value from another to this. For C++17 and later,
    // we use extract() and insert() to improve performance.
    list_.splice(list_.end(), other->list_, other_it->second.list_iter);
    if (v != nullptr) {
        if (cache != nullptr) {
            size_t oldSize =
                ValueTraits::CountBytes(list_.back().value);
            size_t newSize = ValueTraits::CountBytes(*v);
            if (oldSize != newSize) {
                cache->UpdateRemoveFromCacheBytes(oldSize);
                cache->UpdateAddToCacheBytes(newSize);
            }
        }
        list_.back().value = *v;
    }
#if __cplusplus >= 201703L
    auto node = other->map_.extract(other_it);
    auto r = map_.insert(std::move(node));
    assert(r.inserted);
    list_.back().map_iter = r.position;
    r.position->second.list_iter = --list_.end();
#else
    auto r = map_.insert({k, TListIter()});
    assert(r.second);
    list_.back().map_iter = r.first;
    r.first->second.list_iter = --list_.end();
    other->map_.erase(other_it);
#endif
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::T::Find(const K& k,
    TMapIter* map_iter) {
    auto it = map_.find(k);
    if (it != map_.end()) {
        if (map_iter != nullptr) *map_iter = it;
        return true;
    }

    return false;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::T::Remove(TMapIter&& map_iter,
    ARC* cache) {
    if (cache != nullptr) {
        cache->UpdateRemoveFromCacheBytes(
                KeyTraits::CountBytes(map_iter->first) +
                ValueTraits::CountBytes(map_iter->second.list_iter->value));
    }
    list_.erase(map_iter->second.list_iter);
    map_.erase(map_iter);
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::T::RemoveLRU(
    const EvictionCB& evict_cb, ARC* cache) {
    if (list_.empty()) return false;
    if (evict_cb) {
        auto &list_val = list_.front();
        evict_cb(list_val.map_iter->first, std::move(list_val.value));
    }
    if (cache != nullptr) {
        cache->UpdateRemoveFromCacheBytes(
                KeyTraits::CountBytes(list_.front().map_iter->first) +
                ValueTraits::CountBytes(list_.front().value));
    }
    map_.erase(list_.front().map_iter);
    list_.pop_front();
    return true;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::T::Touch(const TMapIter& map_iter,
    const V* v, ARC* cache) {
    auto newest_iter = --list_.end();
    if (v && cache) {
        size_t oldSize =
            ValueTraits::CountBytes(map_iter->second.list_iter->value);
        size_t newSize = ValueTraits::CountBytes(*v);
        if (oldSize != newSize) {
            cache->UpdateRemoveFromCacheBytes(oldSize);
            cache->UpdateAddToCacheBytes(newSize);
        }
    }
    if (newest_iter == map_iter->second.list_iter) {
        if (v != nullptr) newest_iter->value = *v;
        return;
    }

    list_.splice(list_.end(), list_, map_iter->second.list_iter);
    if (v != nullptr) {
        list_.back().value = *v;
    }
    map_iter->second.list_iter = --list_.end();
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::IsCacheFull() const {
    return t1_.Count() + t2_.Count() == c_;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::IncreaseP(size_t delta) {
    if (!IsCacheFull())
        return;
    if (delta > c_ - p_)
        p_ = c_;
    else
        p_ += delta;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::DecreaseP(size_t delta) {
    if (!IsCacheFull())
        return;
    if (delta > p_)
        p_ = 0;
    else
        p_ -= delta;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::Get(const K& key, V* value) {
    TMapIter it;

    if (t1_.Find(key, &it)) {
        if (value) *value = it->second.list_iter->value;
        t2_.Move(key, &t1_, std::move(it), nullptr, this);
        OnCacheHit();
        return true;
    }
    if (t2_.Find(key, &it)) {
        if (value) *value = it->second.list_iter->value;
        t2_.Touch(it, nullptr, nullptr);
        OnCacheHit();
        return true;
    }
    OnCacheMiss();
    return false;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::Put(const K& key, const V& value) {
    static EvictionCB cb(nullptr);
    Put(key, value, cb);
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::Put(const K& key, const V& value,
    const EvictionCB& evict_cb) {
    TMapIter it;

    if (t1_.Find(key, &it)) {
        t2_.Move(key, &t1_, std::move(it), &value, this);
        OnCacheHit();
        return;
    }
    if (t2_.Find(key, &it)) {
        t2_.Touch(it, &value, this);
        OnCacheHit();
        return;
    }

    BMapIter it2;
    if (b1_.Find(key, &it2)) {
        size_t delta = std::min((size_t)1, b2_.Count() / b1_.Count());
        IncreaseP(delta);

        Replace(key, evict_cb);
        b1_.Remove(std::move(it2), this);
        t2_.Insert(key, value, this);
        return;
    }

    if (b2_.Find(key, &it2)) {
        size_t delta = std::max((size_t)1, b1_.Count() / b2_.Count());
        DecreaseP(delta);

        Replace(key, evict_cb);
        b2_.Remove(std::move(it2), this);
        t2_.Insert(key, value, this);
        return;
    }

    if (IsCacheFull() && t1_.Count() + b1_.Count() == c_) {
        if (t1_.Count() < c_) {
            b1_.RemoveLRU(this);
            Replace(key, evict_cb);
        } else {
            t1_.RemoveLRU(evict_cb, this);
        }
    } else if (t1_.Count() + b1_.Count() < c_) {
        auto total = t1_.Count() + b1_.Count() + t2_.Count() + b2_.Count();
        if (total >= c_) {
            if (total == 2 * c_) {
                if (b2_.Count() > 0) {
                    b2_.RemoveLRU(this);
                } else {
                    b1_.RemoveLRU(this);
                }
            }
            Replace(key, evict_cb);
        }
    }
    t1_.Insert(key, value, this);
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::Replace(const K& k,
        const EvictionCB& evict_cb) {
    if (!IsCacheFull()) {
        return;
    }
    if (t1_.Count() != 0 &&
        ((t1_.Count() > p_) || (b2_.Find(k, nullptr) && t1_.Count() == p_))) {
        Move_T_B(&t1_, &b1_, evict_cb);
    } else if (t2_.Count() > 0) {
        Move_T_B(&t2_, &b2_, evict_cb);
    } else {
        Move_T_B(&t1_, &b1_, evict_cb);
    }
}

// This operation detach the key from cache
template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::Remove(const K& key) {
    T* ts[] {&t1_, &t2_};
    for (auto t : ts) {
        TMapIter it;
        if (t->Find(key, &it)) {
            t->Remove(std::move(it), this);
            return;
        }
    }
    B* bs[] {&b1_, &b2_};
    for (auto b : bs) {
        BMapIter it;
        if (b->Find(key, &it)) {
            b->Remove(std::move(it), this);
            return;
        }
    }
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
bool ARC<K, V, KeyTraits, ValueTraits>::Move_T_B(T* t, B* b,
    const EvictionCB &evict_cb) {
    // move t's LRU item to b as MRU item
    if (t->Count() == 0) return false;

    auto map_iter = t->GetLRU();
    UpdateRemoveFromCacheBytes(
        ValueTraits::CountBytes(map_iter->second.list_iter->value));
    if (evict_cb) {
        evict_cb(map_iter->first, std::move(map_iter->second.list_iter->value));
    }
    b->Insert(map_iter->first);
    t->Remove(std::move(map_iter), nullptr);
    return true;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::Clear() {
    b1_.clear();
    t1_.clear();
    b2_.clear();
    t2_.clear();

    p_ = 0;
    cached_bytes_ = 0;
    cache_hit_ = 0;
    cache_miss_ = 0;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
size_t ARC<K, V, KeyTraits, ValueTraits>::Size() const {
    return t1_.Count() + t2_.Count();
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
size_t ARC<K, V, KeyTraits, ValueTraits>::Capacity() const {
    return c_;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
ARCSizeInfo ARC<K, V, KeyTraits, ValueTraits>::ARCSize() const {
    return {b1_.Count(), t1_.Count(), b2_.Count(), t2_.Count()};
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
size_t ARC<K, V, KeyTraits, ValueTraits>::CachedByteCount() const {
    return cached_bytes_;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
uint64_t ARC<K, V, KeyTraits, ValueTraits>::HitCount() const {
    return cache_hit_;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
uint64_t ARC<K, V, KeyTraits, ValueTraits>::MissCount() const {
    return cache_miss_;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::UpdateRemoveFromCacheBytes(
    size_t bytes) {
    cached_bytes_ -= bytes;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::UpdateAddToCacheBytes(
    size_t bytes) {
    cached_bytes_ += bytes;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::OnCacheHit() {
    cache_hit_++;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
void ARC<K, V, KeyTraits, ValueTraits>::OnCacheMiss() {
    cache_miss_++;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
std::vector<K> ARC<K, V, KeyTraits, ValueTraits>::GetKeysOfQ(ARCQId q) const {
    std::vector<K> v;

    switch (q) {
    case ARCQId::B1:
        {
            for (auto &item : b1_.list_)
                v.push_back(item.map_iter->first);
        }
        break;
    case ARCQId::B2:
        {
            for (auto &item : b2_.list_)
                v.push_back(item.map_iter->first);
        }
        break;
    case ARCQId::T1:
        {
            for (auto &item : t1_.list_)
                v.push_back(item.map_iter->first);
        }
        break;
    case ARCQId::T2:
        {
            for (auto &item : t2_.list_)
                v.push_back(item.map_iter->first);
        }
        break;
    }
    return v;
}

template <typename K, typename V, typename KeyTraits, typename ValueTraits>
std::vector<V> ARC<K, V, KeyTraits, ValueTraits>::GetValuesOfQ(ARCQId q) const {
    std::vector<V> v;

    switch (q) {
    case ARCQId::B1:
        break;
    case ARCQId::B2:
        break;
    case ARCQId::T1:
        {
            for (auto &item : t1_.list_)
                v.push_back(item.value);
        }
        break;
    case ARCQId::T2:
        {
            for (auto &item : t2_.list_)
                v.push_back(item.value);
        }
        break;
    }
    return v;
}

}  // namespace fengge

#endif  // SRC_INCLUDE_FENGGE_ARC_H_
