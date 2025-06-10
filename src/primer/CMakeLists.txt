add_library(
  bustub_primer
  OBJECT
  hyperloglog.cpp
  hyperloglog_presto.cpp
  orset.cpp
  orset_driver.cpp
  skiplist.cpp
  trie.cpp
  trie_store.cpp)

# 👇 Add this line to fix include path issue
target_include_directories(bustub_primer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    /home/kris/CMU15-445/CMU15-445-Database-system/third_party/fmt/include
)



set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:bustub_primer>
  PARENT_SCOPE)
