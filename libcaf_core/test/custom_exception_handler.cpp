/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE custom_exception_handler

#include "caf/all.hpp"

#include "core-test.hpp"

using namespace caf;

#ifndef CAF_ENABLE_EXCEPTIONS
#  error "building unit test for exception handlers in no-exceptions build"
#endif

class exception_testee : public event_based_actor {
public:
  ~exception_testee() override;
  exception_testee(actor_config& cfg) : event_based_actor(cfg) {
    set_exception_handler([](std::exception_ptr&) -> error {
      return exit_reason::remote_link_unreachable;
    });
  }

  behavior make_behavior() override {
    return {
      [](const std::string&) {
        throw std::runtime_error("whatever");
      }
    };
  }
};

exception_testee::~exception_testee() {
  // avoid weak-vtables warning
}

CAF_TEST(test_custom_exception_handler) {
  actor_system_config cfg;
  actor_system system{cfg};
  auto handler = [](std::exception_ptr& eptr) -> error {
    try {
      std::rethrow_exception(eptr);
    }
    catch (std::runtime_error&) {
      return exit_reason::normal;
    }
    catch (...) {
      // "fall through"
    }
    return sec::runtime_error;
  };
  scoped_actor self{system};
  auto testee1 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::runtime_error("ping");
  });
  auto testee2 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::logic_error("pong");
  });
  auto testee3 = self->spawn<exception_testee, monitored>();
  self->send(testee3, "foo");
  // receive all down messages
  self->wait_for(testee1, testee2, testee3);
}
