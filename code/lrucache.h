/*
 * MIT License
 * 
 * Copyright (c) 2017 Andrew Kirmse
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * File:   lrucache.hpp
 * Author: Alexander Ponomarev
 *
 * Created on June 20, 2013, 5:09 PM
 */

#ifndef _LRUCACHE_H_
#define _LRUCACHE_H_

#include <unordered_map>
#include <list>
#include <cstddef>
#include <stdexcept>

// When an element is removed from the cache, it's deleted (i.e. cache owns its values)

template<typename key_t, typename value_t>
class lru_cache {
public:
  typedef typename std::pair<key_t, value_t> key_value_pair_t;
  typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

  lru_cache(size_t max_size) :
      _max_size(max_size) {
  }
  
  void put(const key_t& key, const value_t& value) {
    auto it = _cache_items_map.find(key);
    _cache_items_list.push_front(key_value_pair_t(key, value));
    if (it != _cache_items_map.end()) {
      _cache_items_list.erase(it->second);
      _cache_items_map.erase(it);
    }
    _cache_items_map[key] = _cache_items_list.begin();
    
    if (_cache_items_map.size() > _max_size) {
      auto last = _cache_items_list.end();
      last--;
      _cache_items_map.erase(last->first);
      delete last->second;
      _cache_items_list.pop_back();
    }
  }
  
  const value_t& get(const key_t& key) {
    auto it = _cache_items_map.find(key);
    if (it == _cache_items_map.end()) {
      throw std::range_error("There is no such key in cache");
    } else {
      _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
      return it->second->second;
    }
  }
  
  bool exists(const key_t& key) const {
    return _cache_items_map.find(key) != _cache_items_map.end();
  }
  
  size_t size() const {
    return _cache_items_map.size();
  }
  
private:
  std::list<key_value_pair_t> _cache_items_list;
  std::unordered_map<key_t, list_iterator_t> _cache_items_map;
  size_t _max_size;
};

#endif  // _LRUACCHE_H_
