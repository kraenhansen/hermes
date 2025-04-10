# Node-API tests

The test suite is copied directly from the Node.js project: https://github.com/nodejs/node/tree/main/test/js-native-api

To enable building the Node-API tests, configure the project with `HERMES_BUILD_NODE_API_TESTS` enabled:

```
-DHERMES_BUILD_NODE_API_TESTS:BOOL=TRUE
```

This directory has a CMake configuration, which calls a python script to transform .gyp files into CMake configuration files.

If your CMake configure call fails, with an error complaining about missing Python modules, you might need to install some dependencies first:

```
python3 -m pip install json5
```

## Updating the tests

Delete any existing `js-native-api` directory in this directory.

```
rm -rf hermes/node-api-tests/js-native-api
```

Perform a local checkout of the Node.js repository.

```
git clone --depth 1 git@github.com:nodejs/node.git
```

Copy over the runtime agnostic native tests:

```
cp -r node/test/js-native-api hermes/node-api-tests/js-native-api
```
