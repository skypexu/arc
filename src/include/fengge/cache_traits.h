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

#ifndef FENGGE_SRC_INCLUDE_FENGGE_CACHE_TRAITS_H_
#define FENGGE_SRC_INCLUDE_FENGGE_CACHE_TRAITS_H_

#include <string>

namespace fengge {

template<class T>
struct CacheTraits {
    static size_t CountBytes(const T &) {
        return sizeof(T);
    }
};

template<>
struct CacheTraits<std::string> {
    static size_t CountBytes(const std::string &v) {
        return v.size();
    }
};

}  // namespace arc_cache

#endif  // FENGGE_SRC_INCLUDE_FENGGE_CACHE_TRAITS_H_
