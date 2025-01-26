/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HERMES_HERMES_NODE_API_H
#define HERMES_HERMES_NODE_API_H

#include <jsi/jsi.h>
#include "js_native_api.h"

EXTERN_C_START

NAPI_EXTERN napi_env jsi_create_napi_env(facebook::jsi::Runtime&);

EXTERN_C_END

#endif // !HERMES_HERMES_NODE_API_H
