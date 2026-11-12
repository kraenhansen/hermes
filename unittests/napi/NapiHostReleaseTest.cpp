/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes_napi.h"

#include "hermes/VM/Runtime.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using namespace hermes::vm;

/// Creates a small Runtime for env tests.
std::shared_ptr<Runtime> makeRuntime() {
  return Runtime::create(RuntimeConfig::Builder()
                             .withGCConfig(GCConfig::Builder()
                                               .withInitHeapSize(1 << 16)
                                               .withMaxHeapSize(1 << 19)
                                               .build())
                             .build());
}

/// A host whose release callback counts the envs that are done with it.
class CountingHost {
 public:
  /// The host passed to hermes_napi_create_env.
  hermes_napi_host host{};
  /// Number of release calls received.
  int releases = 0;

  /// Points host.data and host.release at this object.
  CountingHost() {
    host.data = this;
    host.release = [](void *data) {
      ++static_cast<CountingHost *>(data)->releases;
    };
  }
};

TEST(NapiHostReleaseTest, CalledOnceWhenRuntimeIsDestroyed) {
  CountingHost counting;
  auto rt = makeRuntime();
  ASSERT_NE(nullptr, hermes_napi_create_env(&*rt, &counting.host));

  EXPECT_EQ(0, counting.releases);
  rt.reset();
  EXPECT_EQ(1, counting.releases);
}

TEST(NapiHostReleaseTest, CalledOncePerEnvSharingTheHost) {
  CountingHost counting;
  auto rt = makeRuntime();
  ASSERT_NE(nullptr, hermes_napi_create_env(&*rt, &counting.host));
  ASSERT_NE(nullptr, hermes_napi_create_env(&*rt, &counting.host));

  EXPECT_EQ(0, counting.releases);
  rt.reset();
  EXPECT_EQ(2, counting.releases);
}

TEST(NapiHostReleaseTest, OptionalForHostsThatOutliveTheRuntime) {
  hermes_napi_host host{};
  auto rt = makeRuntime();
  ASSERT_NE(nullptr, hermes_napi_create_env(&*rt, &host));
  rt.reset();
}

/// A heap-allocated host that frees itself in release, and records the
/// order in which the env uses it during teardown.
class SelfFreeingHost {
 public:
  /// The host passed to hermes_napi_create_env.
  hermes_napi_host host{};
  /// Where the callbacks record their names, in call order.
  std::vector<std::string> *events;

  /// Points every callback of host at this object.
  explicit SelfFreeingHost(std::vector<std::string> *events) : events(events) {
    host.data = this;
    host.post_task = [](void *data, void *, void (*)(void *)) {
      static_cast<SelfFreeingHost *>(data)->events->push_back("post_task");
    };
    host.ref_loop = [](void *data) {
      static_cast<SelfFreeingHost *>(data)->events->push_back("ref_loop");
    };
    host.unref_loop = [](void *data) {
      static_cast<SelfFreeingHost *>(data)->events->push_back("unref_loop");
    };
    host.fatal_exception = [](void *data, napi_env, napi_value) {
      static_cast<SelfFreeingHost *>(data)->events->push_back(
          "fatal_exception");
    };
    host.release = [](void *data) {
      std::unique_ptr<SelfFreeingHost> self(
          static_cast<SelfFreeingHost *>(data));
      self->events->push_back("release");
    };
  }
};

TEST(NapiHostReleaseTest, IsTheEnvsLastUseOfTheHost) {
  std::vector<std::string> events;
  auto rt = makeRuntime();
  auto host = std::make_unique<SelfFreeingHost>(&events);
  napi_env env = hermes_napi_create_env(&*rt, &host->host);
  ASSERT_NE(nullptr, env);
  // The env owns the host from here: release frees it.
  host.release();

  // A cleanup hook that uses the host while the env tears down.
  ASSERT_EQ(
      napi_ok,
      napi_add_env_cleanup_hook(
          env,
          [](void *arg) {
            auto e = static_cast<napi_env>(arg);
            napi_handle_scope scope = nullptr;
            ASSERT_EQ(napi_ok, napi_open_handle_scope(e, &scope));
            napi_value error = nullptr;
            ASSERT_EQ(napi_ok, napi_get_undefined(e, &error));
            EXPECT_EQ(napi_ok, napi_fatal_exception(e, error));
            EXPECT_EQ(napi_ok, napi_close_handle_scope(e, scope));
          },
          env));

  // A referenced thread-safe function, released by the env's teardown,
  // which unrefs the loop.
  napi_handle_scope scope = nullptr;
  ASSERT_EQ(napi_ok, napi_open_handle_scope(env, &scope));
  napi_value name = nullptr;
  ASSERT_EQ(
      napi_ok, napi_create_string_utf8(env, "tsfn", NAPI_AUTO_LENGTH, &name));
  napi_threadsafe_function tsfn = nullptr;
  ASSERT_EQ(
      napi_ok,
      napi_create_threadsafe_function(
          env,
          nullptr,
          nullptr,
          name,
          0,
          1,
          nullptr,
          nullptr,
          nullptr,
          [](napi_env, napi_value, void *, void *) {},
          &tsfn));
  ASSERT_EQ(napi_ok, napi_close_handle_scope(env, scope));

  // ASan reports any use of the host after release frees it.
  rt.reset();

  std::vector<std::string> expected{
      "ref_loop", "fatal_exception", "unref_loop", "release"};
  EXPECT_EQ(expected, events);
}

} // namespace
