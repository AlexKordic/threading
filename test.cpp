
#include "fifo_queue.hpp"

#include <memory>
#include <thread>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

using namespace Perun;

TEST_CASE("[fifo] non-blocking pop") {
  // 5. Implement non-blocking ‘pop’ method: in case message is not available, return an error
  FifoQueue<std::unique_ptr<int>> q(2);
  std::unique_ptr<int> result;

  FifoError e = q.try_pop(result);
  REQUIRE(e == FifoError::Empty);
}

TEST_CASE("[fifo] blocking pop with timeout") {
  // 5. `production` style implementation, ready to be used by application
  FifoQueue<std::unique_ptr<int>> q(2);
  std::unique_ptr<int> result;

  FifoError e = q.pop(result, std::chrono::milliseconds(20));
  REQUIRE(e == FifoError::Timeouted);
}

TEST_CASE("[fifo] blocking pop") {
  // 2. Queue ‘pop’ operation must be blocking
  FifoQueue<std::unique_ptr<int>> q(2);
  std::unique_ptr<int> result;

  result.reset();
  std::thread waiting_pop([&](){
    FifoError err = q.pop(result);
    CHECK(err == FifoError::OK);
  });
  // yield to waiting_pop thread
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // release waiting_pop thread
  {
    FifoError e = q.push(std::make_unique<int>(13));
    REQUIRE(e == FifoError::OK);
  }
  waiting_pop.join();
  REQUIRE(result.get() != nullptr);
  REQUIRE(*result == 13);
}

TEST_CASE("[fifo] non-blocking push") {
  // 3. Queue ‘push’ operation must be non-blocking - returns an error
  FifoQueue<std::unique_ptr<int>> q(2);

  q.push(std::make_unique<int>(1));
  q.push(std::make_unique<int>(2));
  FifoError e = q.try_push(std::make_unique<int>(3));
  REQUIRE(e == FifoError::Full);
}

TEST_CASE("[fifo] blocking push") {
  // 4. Implement blocking ‘push’ method: in case message addition is not possible, wait until it possible
  FifoQueue<std::unique_ptr<int>> q(2);
  std::unique_ptr<int> result;

  q.push(std::make_unique<int>(1));
  q.push(std::make_unique<int>(2));
  std::thread waiting_push([&](){
    FifoError err = q.push(std::make_unique<int>(7));
    CHECK(err == FifoError::OK);
  });
  // yield to waiting_push thread
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // release waiting_push thread
  {
    FifoError e = q.pop(result);
    REQUIRE(e == FifoError::OK);
    REQUIRE(result.get() != nullptr);
  }
  waiting_push.join();
};

TEST_CASE("[fifo] close") {
  // 1.  Implement ‘close’ function, that makes further pop/push impossible
  FifoQueue<std::unique_ptr<int>> q(2);
  std::unique_ptr<int> result;

  q.push(std::make_unique<int>(1));
  q.push(std::make_unique<int>(2));
  std::thread waiting_push([&](){
    FifoError err = q.push(std::make_unique<int>(9));
    CHECK(err == FifoError::Destroyed);
  });
  // yield to waiting_push thread
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // release waiting_push thread
  q.close();
  waiting_push.join();
  FifoError e = q.pop(result);
  REQUIRE(e == FifoError::Destroyed);
};

TEST_CASE("[fifo] get") {
  // 3. Implement get (PREDICATE) method, which accepts predicate, and returns first matching element from the queue
  FifoQueue<std::unique_ptr<int>> q;
  std::unique_ptr<int> result;

  q.push(std::make_unique<int>(1));
  q.push(std::make_unique<int>(2));
  q.push(std::make_unique<int>(3));
  q.push(std::make_unique<int>(4));
  q.push(std::make_unique<int>(5));
  REQUIRE(q.size() == 5);
  FifoError e = q.get(result, [](std::unique_ptr<int> const & item) -> bool {
    return item && *item == 3;
  });
  REQUIRE(e == FifoError::OK);
  REQUIRE(result.get() != nullptr);
  REQUIRE(*result == 3);
  REQUIRE(q.size() == 4);
};
