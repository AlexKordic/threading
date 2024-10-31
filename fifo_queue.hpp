//
// Copyright 2024 (c) Perun Software Ltd.
// This unpublished material is proprietary to Perun Software Ltd. All rights reserved.
// The methods and techniques described herein are considered trade secrets and/or confidential.
// Reproduction or distribution, in whole or in part, is forbidden except by express written permission of Perun Software Ltd.
//

#ifndef _PERUN_QUEUE_HPP_
#define _PERUN_QUEUE_HPP_

#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <functional>

namespace Perun {

enum class FifoError {
  OK,
  Destroyed,
  Full,
  Empty,
  NotFound,
  Timeouted,
};


// Implements asynchronous message exchange queue.
// Use .try_push() & .try_pop() for non-blocking operation
// Use .push() & .pop() for blocking operation 
// .pop(timeout) overload allows waiting for message for specified time
// Use .close() to deny further operations on the queue and unblock all suspended threads
template <typename T>
class FifoQueue {
public:
  FifoQueue(int64_t max_size = std::numeric_limits<int64_t>::max())
      : max_size(max_size) { }

  virtual ~FifoQueue() = default;
  
  FifoError try_push(T const& value) {
    {
      std::lock_guard guard(_lock);
      if(!_is_valid) return FifoError::Destroyed;
      if(_storage.size() >= max_size) return FifoError::Full;
      _storage.push_back(value);
    }
    _c_pop_blocking_on.notify_one();
    return FifoError::OK;
  }

  FifoError try_push(T&& value) {
    {
      std::lock_guard guard(_lock);
      if(!_is_valid) return FifoError::Destroyed;
      if(_storage.size() >= max_size) return FifoError::Full;
      _storage.emplace_back(std::move(value));
    }
    _c_pop_blocking_on.notify_one();
    return FifoError::OK;
  }

  FifoError push(T const& value) {
    {
      std::unique_lock guard(_lock);
      if(!_is_valid) return FifoError::Destroyed;
      while(_storage.size() >= max_size) {
        // Block until queue is consumed
        _c_push_blockin_on.wait(guard);
        if(!_is_valid) return FifoError::Destroyed;
      }
      _storage.push_back(value);
    }
    _c_pop_blocking_on.notify_one();
    return FifoError::OK;
  }

  FifoError push(T&& value) {
    {
      std::unique_lock guard(_lock);
      if(!_is_valid) return FifoError::Destroyed;
      while(_storage.size() >= max_size) {
        // Block until queue is consumed
        _c_push_blockin_on.wait(guard);
        if(!_is_valid) return FifoError::Destroyed;
      }
      _storage.push_back(std::move(value));
    }
    _c_pop_blocking_on.notify_one();
    return FifoError::OK;
  }

  [[nodiscard]] FifoError try_pop(T& value) {
    {
      std::lock_guard guard(_lock);
      if(!_is_valid) return FifoError::Destroyed;
      if(_storage.empty()) return FifoError::Empty;
      value = std::move(_storage.front());
      _storage.pop_front();
    }
    _c_push_blockin_on.notify_one();
    return FifoError::OK;
  }

  [[nodiscard]] FifoError pop(T& value) {
    {
      std::unique_lock guard(_lock);
      while(_storage.empty()) {
        if(!_is_valid) return FifoError::Destroyed;
        // will release lock in process of waiting and acquire again before return:
        _c_pop_blocking_on.wait(guard);
      }
      if(!_is_valid) return FifoError::Destroyed;
      value = std::move(_storage.front());
      _storage.pop_front();
    }
    _c_push_blockin_on.notify_one();
    return FifoError::OK;
  }

  [[nodiscard]] FifoError
  pop(T& value, std::chrono::milliseconds timeout) {
    {
      auto             deadline = std::chrono::steady_clock::now() + timeout;
      std::unique_lock guard(_lock);
      while(_storage.empty()) {
        if(!_is_valid) return FifoError::Destroyed;
        // will release lock in process of waiting and acquire again before return:
        if(std::cv_status::timeout == _c_pop_blocking_on.wait_until(guard, deadline)) {
          return FifoError::Timeouted;
        }
      }
      if(!_is_valid) return FifoError::Destroyed;
      value = std::move(_storage.front());
      _storage.pop_front();
    }
    _c_push_blockin_on.notify_one();
    return FifoError::OK;
  }

  // After invoking this method put raises exception and get raises exception on empty _storage
  void close() {
    {
      std::lock_guard guard(_lock);
      _is_valid = false;
    }
    _c_pop_blocking_on.notify_all();
    _c_push_blockin_on.notify_all();
  }

  bool running() {
    std::lock_guard guard(_lock);
    return _is_valid;
  }

  void erase_if(std::function<bool(const T&)> f) {
    std::lock_guard guard(_lock);
    std::erase_if(_storage, f);
  }

  int64_t size() {
    std::lock_guard guard(_lock);
    return _storage.size();
  }

  [[nodiscard]] FifoError get(T& value, std::function<bool(const T&)> f) {
    std::lock_guard guard(_lock);
    if(!_is_valid) return FifoError::Destroyed;
    // for (const T& el : _storage) {
    for (auto it = _storage.begin(); it != _storage.end(); ++it) {
      if(f(*it)) {
        value = std::move(*it);
        _storage.erase(it);
        return FifoError::OK;
      }
    }
    return FifoError::NotFound;
  }

  const int64_t max_size;

protected:
  std::mutex              _lock;
  std::condition_variable _c_pop_blocking_on, _c_push_blockin_on;
  std::deque<T>           _storage;
  volatile bool           _is_valid = true;
};

} // namespace Perun

#endif // _PERUN_QUEUE_HPP_
