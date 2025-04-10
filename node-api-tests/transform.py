import sys
import os
import json5 as json

namespaced_target_names = []
skipped_targets = [
  "test_finalizer", # Uses NAPI_EXPERIMENTAL, which is not implemented
]

def read_gyp_file(file_path):
  with open(file_path, "r") as gyp_file:
    content = json.load(gyp_file)
    # Validate the structure of the parsed GYP content
    if "targets" not in content or not isinstance(content["targets"], list):
      raise ValueError(f"Invalid GYP file structure in {file_path}: 'targets' must be a list.")

    for target in content["targets"]:
      if not isinstance(target, dict):
        raise ValueError(f"Invalid target in {file_path}: Each target must be a dictionary.")
      
      # Check required properties
      if "target_name" not in target or not isinstance(target["target_name"], str):
        raise ValueError(f"Invalid target in {file_path}: 'target_name' must be a string.")
      if "sources" not in target or not isinstance(target["sources"], list) or not all(isinstance(s, str) for s in target["sources"]):
        raise ValueError(f"Invalid target in {file_path}: 'sources' must be a list of strings.")
      
      # Check optional properties
      if "defines" in target and (not isinstance(target["defines"], list) or not all(isinstance(d, str) for d in target["defines"])):
        raise ValueError(f"Invalid target in {file_path}: 'defines', if present, must be a list of strings.")
      
      # Ensure no unexpected properties
      allowed_keys = {"target_name", "sources", "defines"}
      unexpected_keys = set(target.keys()) - allowed_keys
      if unexpected_keys:
        raise ValueError(f"Invalid target in {file_path}: Unexpected properties {unexpected_keys}.")
    return content


def transform_gyp_file(file_path):
  gyp = read_gyp_file(file_path)
  cmake_file_path = os.path.join(root, "CMakeLists.txt")
  # If validation passes, create an empty CMakeLists.txt file
  with open(cmake_file_path, "w") as cmake_file:
    for target in gyp["targets"]:
      if target["target_name"] in skipped_targets:
        print(f"Skipping target: {target['target_name']}")
        continue
      target_name = target["target_name"]
      directory_name = os.path.basename(root)
      # Need the directory_name prefix to avoid name collisions
      namespace_target_name = f"node-api-tests-addon-{directory_name}-{target['target_name']}"
      namespaced_target_names.append(namespace_target_name)
      sources = " ".join(target["sources"])
      cmake_file.write(f"add_library({namespace_target_name} SHARED {sources})\n")
      cmake_file.write(f"set_target_properties({namespace_target_name} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO PREFIX \"\" SUFFIX \".node\" OUTPUT_NAME \"{target_name}\" LIBRARY_OUTPUT_DIRECTORY \"${{CMAKE_CURRENT_SOURCE_DIR}}/build/$<CONFIG>\")\n")
      cmake_file.write(f"target_include_directories({namespace_target_name} PRIVATE ${{CMAKE_SOURCE_DIR}}/API/hermes_node_api/node_api)\n")
      if "defines" in target:
        defines = " ".join(target["defines"])
        cmake_file.write(f"target_compile_definitions({namespace_target_name} PRIVATE {defines})\n")
      # Link against the Hermes Node API
      cmake_file.write(f"target_link_libraries({namespace_target_name} PRIVATE hermesNodeApi)\n")
      # Copy .js files to the build directory
      cmake_file.write("file(GLOB JS_FILES \"${CMAKE_CURRENT_SOURCE_DIR}/*.js\")\n")
      cmake_file.write("file(COPY ${JS_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})\n")
  return gyp

print("Transforming binding.gyp files to CMakeLists.txt files...")

directories_with_gyp = []

for root, dirs, files in os.walk(os.getcwd()):
  for file in files:
    if file == "binding.gyp":
      file_path = os.path.join(root, file)
      print(f"Found: {file_path}")
      gyp = transform_gyp_file(file_path)
      directories_with_gyp.append(root)

# Create a CMakeLists.txt file in the js-native-api directory
js_native_api_dir = os.path.join(os.path.dirname(__file__), "js-native-api")
os.makedirs(js_native_api_dir, exist_ok=True)
cmake_file_path = os.path.join(js_native_api_dir, "CMakeLists.txt")

with open(cmake_file_path, "w") as cmake_file:
  for directory in directories_with_gyp:
    relative_path = os.path.relpath(directory, js_native_api_dir)
    cmake_file.write(f"add_subdirectory({relative_path})\n")
  cmake_file.write(f"add_custom_target(node-api-tests-addons ALL DEPENDS {" ".join(namespaced_target_names)})\n")

print(f"CMakeLists.txt created at {cmake_file_path}")
