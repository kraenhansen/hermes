/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <hermes/hermes.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include "jsi/jsi.h"

using namespace facebook::hermes;
using namespace facebook::jsi;
using namespace std::filesystem;

class CommonModule : public HostObject {
  Value get(Runtime &runtime, const PropNameID &name) override {
    if (name.utf8(runtime) == "buildType") {
      return String::createFromAscii(runtime, "Debug");
    } else {
      return Value::undefined();
    }
  }
};

class AssertModule : public HostObject {
  Value get(Runtime &runtime, const PropNameID &name) override {
    return Value::undefined();
  }
};

class ExternalNodeAPITest : public ::testing::TestWithParam<std::string> {
 public:
  ExternalNodeAPITest()
      : jsPath(GetParam()),
        rt(makeHermesRuntime()),
        assertExports(std::make_shared<AssertModule>()),
        commonExports(std::make_shared<CommonModule>()) {}

  void SetUp() override {
    // TODO: Inject globals
    rt->global().setProperty(
        *rt,
        "require",
        Function::createFromHostFunction(
            *rt,
            PropNameID::forAscii(*rt, "require"),
            1,
            [=](Runtime &runtime,
                const Value &thisValue,
                const Value *args,
                size_t count) -> Value {
              if (count < 1 || !args[0].isString()) {
                throw JSError(
                    runtime, "require() expects a single string argument");
              }
              auto moduleName = args[0].getString(runtime).utf8(runtime);
              return require(moduleName);
            }));
  }

  void TearDown() override {
    // Clean up the test environment, if needed
  }

  /// Loads the file into a string and evaluates it.
  void evaluate() {
    std::ifstream file(jsPath);
    std::string script(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    rt->evaluateJavaScript(std::make_shared<StringBuffer>(script), jsPath);
  }

  Object require(std::string moduleName) {
    printf("Loading module: %s\n", moduleName.c_str());
    if (moduleName == "assert") {
      return Object::createFromHostObject(*rt, assertExports);
    } else if (moduleName == "../../common") {
      return Object::createFromHostObject(*rt, commonExports);
    } else {
      throw JSError(*rt, "Cannot find module '" + moduleName + "'");
    }
  }

 protected:
  std::string jsPath;
  std::unique_ptr<HermesRuntime> rt;
  std::shared_ptr<AssertModule> assertExports;
  std::shared_ptr<CommonModule> commonExports;
};

struct JsFileIterator {
  std::vector<std::string> jsFiles;
  size_t index = 0;

  JsFileIterator(const path &directory) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(directory)) {
      if (entry.path().extension() == ".js") {
        jsFiles.push_back(entry.path().string());
      }
    }
    std::sort(jsFiles.begin(), jsFiles.end());
  }

  operator ::testing::internal::ParamGenerator<std::string>() const {
    return ::testing::ValuesIn(jsFiles);
  }
};

TEST_P(ExternalNodeAPITest, Runs) {
  printf("Running test for %s\n", jsPath.c_str());
  evaluate();
}

INSTANTIATE_TEST_CASE_P(
    ExternalNodeAPITests,
    ExternalNodeAPITest,
    JsFileIterator(std::filesystem::path(JS_NATIVE_API_PATH)),
    [](const testing::TestParamInfo<std::string> &info) {
      auto path = std::filesystem::path(info.param);
      auto formattedName =
          path.parent_path().filename().string() + path.filename().string();
      formattedName.erase(
          std::remove_if(
              formattedName.begin(),
              formattedName.end(),
              [](unsigned char c) { return !std::isalnum(c); }),
          formattedName.end());
      return formattedName;
    });
