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
#include <fengge/arc.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <initializer_list>

using fengge::ARC;
using fengge::ARCQId;

template <size_t N>
static void assert_keys(const ARC<int, int>& cache, ARCQId q,
    const int (&x)[N]) {
    auto v = cache.GetKeysOfQ(q);
    ASSERT_EQ(v.size(), N);
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(x[i], v[i]);
    }
}

template <size_t N>
static void assert_values(const ARC<int, int>& cache, ARCQId q,
    const int (&x)[N]) {
    auto v = cache.GetValuesOfQ(q);
    ASSERT_EQ(v.size(), N);
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(x[i], v[i]);
    }
}

static void assert_keys(const ARC<int, int>& cache, ARCQId q, int *x, int N) {
    auto v = cache.GetKeysOfQ(q);
    ASSERT_EQ(v.size(), N);
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(x[i], v[i]);
    }
}

static void assert_cache_metrics(const ARC<int, int>& cache) {
    const auto arcSize = cache.ARCSize();
    /* sizeof(key) + sizeof(value), yet sizeof(int) + sizeof(value) */
    const auto sizeofKey = sizeof(int);
    const auto sizeofValue = sizeof(int);
    ASSERT_EQ(arcSize.BSize() * sizeofKey +
            arcSize.TSize() * (sizeofKey + sizeofValue),
            cache.CachedByteCount());
}

TEST(ARCTest, cache_create) {
    const int maxCount = 5;
    ARC<int, int> cache(maxCount);

    ASSERT_EQ(cache.Capacity(), maxCount);
    ASSERT_EQ(cache.Size(), 0);
    ASSERT_EQ(cache.HitCount(), 0);
    ASSERT_EQ(cache.MissCount(), 0);
    ASSERT_EQ(cache.CachedByteCount(), 0);
}

TEST(ARCTest, cache_inspect) {
    const int maxCount = 3;
    ARC<int, int> cache(maxCount);

    for (auto a : {1, 2, 3}) {
        cache.Put(a, a);
    }

    // t1: [1,2,3]
    assert_keys(cache, ARCQId::B1, nullptr, 0);
    assert_keys(cache, ARCQId::T1, {1, 2, 3});
    assert_values(cache, ARCQId::T1, {1, 2, 3});
    assert_keys(cache, ARCQId::T2, nullptr, 0);
    assert_keys(cache, ARCQId::B2, nullptr, 0);

    ASSERT_TRUE(cache.Get(1, nullptr));
    // t1: [2,3], t2: [1]
    assert_keys(cache, ARCQId::B1, nullptr, 0);
    assert_keys(cache, ARCQId::T1, {2, 3});
    assert_values(cache, ARCQId::T1, {2, 3});
    assert_keys(cache, ARCQId::T2, {1});
    assert_values(cache, ARCQId::T2, {1});
    assert_keys(cache, ARCQId::B2, nullptr, 0);

    cache.Put(4, 4);
    // b1: [2], t1: [3, 4], t2: [1]
    assert_keys(cache, ARCQId::B1, {2});
    assert_keys(cache, ARCQId::T1, {3, 4});
    assert_values(cache, ARCQId::T1, {3, 4});
    assert_keys(cache, ARCQId::T2, {1});
    assert_values(cache, ARCQId::T2, {1});
    assert_keys(cache, ARCQId::B2, nullptr, 0);

    cache.Put(2, 2);
    // b1: [3], t1: [4], t2:[1,2]
    assert_keys(cache, ARCQId::B1, {3});
    assert_keys(cache, ARCQId::T1, {4});
    assert_values(cache, ARCQId::T1, {4});
    assert_keys(cache, ARCQId::T2, {1, 2});
    assert_values(cache, ARCQId::T2, {1, 2});
    assert_keys(cache, ARCQId::B2, nullptr, 0);

    cache.Get(4, nullptr);
    // b1: [3], t1: [], t2:[1,2,4]
    assert_keys(cache, ARCQId::B1, {3});
    assert_keys(cache, ARCQId::T1, nullptr, 0);
    assert_keys(cache, ARCQId::T2, {1, 2, 4});
    assert_values(cache, ARCQId::T2, {1, 2, 4});
    assert_keys(cache, ARCQId::B2, nullptr, 0);

    cache.Put(3, 3);
    // b1: [], t1: [], t2:[2,4,3], b2:[1]
    assert_keys(cache, ARCQId::B1, nullptr, 0);
    assert_keys(cache, ARCQId::T1, nullptr, 0);
    assert_keys(cache, ARCQId::T2, {2, 4, 3});
    assert_values(cache, ARCQId::T2, {2, 4, 3});
    assert_keys(cache, ARCQId::B2, {1});

    assert_cache_metrics(cache);
}

TEST(ARCTest, cache_evict) {
    const int maxCount = 5;
    ARC<int, int> cache(maxCount);

    for (int i = 0; i < maxCount; ++i) {
        cache.Put(i, i);
    }

    ASSERT_TRUE(cache.Size() == maxCount);

    for (int i = 0; i < maxCount; ++i) {
        int v;
        ASSERT_TRUE(cache.Get(i, &v));
        ASSERT_EQ(i, v);
    }

    std::vector<int> evicted;
    auto evict_cb = [&](const int &k, const int &&v) {
        ASSERT_EQ(k, v);
        evicted.push_back(k);
    };

    for (int i = maxCount; i < maxCount * 2; ++i) {
        cache.Put(i, i, evict_cb);
    }
    ASSERT_EQ(evicted.size(), maxCount);

    for (auto k : evicted) {
        int v;
        ASSERT_FALSE(cache.Get(k, &v));
    }
    assert_cache_metrics(cache);
}

TEST(ARCTest, cache_remove) {
    const int maxCount = 5;
    ARC<int, int> cache(maxCount);

    for (int i = 0; i < maxCount; ++i) {
        cache.Put(i, i);
    }

    for (int i = 0; i < maxCount; ++i) {
        cache.Remove(i);
        ASSERT_FALSE(cache.Get(i, nullptr));
    }

    assert_cache_metrics(cache);
}

TEST(ARCTest, cache_hitcount) {
    const int maxCount = 5;
    ARC<int, int> cache(maxCount);

    for (int i = 0; i < maxCount; ++i) {
        cache.Put(i, i);
    }
    for (int i = 0; i < maxCount; ++i) {
        ASSERT_TRUE(cache.Get(i, nullptr));
    }

    ASSERT_EQ(cache.HitCount(), maxCount);
    ASSERT_EQ(cache.MissCount(), 0);

    for (int i = maxCount; i < maxCount * 2; ++i) {
        ASSERT_FALSE(cache.Get(i, nullptr));
    }

    ASSERT_EQ(cache.HitCount(), maxCount);
    ASSERT_EQ(cache.MissCount(), maxCount);
}
