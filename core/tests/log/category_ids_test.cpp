#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "yax86_core.h"

using namespace std;

// Log categories are declared per-module rather than in a central enum, so
// that modules do not need to know about one another. Nothing in the core
// therefore enforces that their IDs are unique - this test does, since the
// test suite already depends on every module.
namespace {

const vector<const LogCategory*> kAllCategories = {
    &kLogCategoryPlatform, &kLogCategoryCPU,      &kLogCategoryPIC,
    &kLogCategoryPIT,      &kLogCategoryPPI,      &kLogCategoryFDC,
    &kLogCategoryDMA,      &kLogCategoryKeyboard, &kLogCategoryVideo,
};

}  // namespace

class LogCategoryIDTest : public ::testing::Test {};

TEST_F(LogCategoryIDTest, IDsAreUnique) {
  map<uint8_t, string> ids_to_names;
  for (const LogCategory* category : kAllCategories) {
    auto existing = ids_to_names.find(category->id);
    ASSERT_EQ(existing, ids_to_names.end())
        << "Log category ID " << static_cast<int>(category->id)
        << " is used by both " << existing->second << " and " << category->name;
    ids_to_names[category->id] = category->name;
  }
}

TEST_F(LogCategoryIDTest, IDsFitInCategoryMask) {
  for (const LogCategory* category : kAllCategories) {
    EXPECT_LT(category->id, kLogMaxCategories) << "category " << category->name;
  }
}

TEST_F(LogCategoryIDTest, NamesAreNonEmpty) {
  for (const LogCategory* category : kAllCategories) {
    ASSERT_NE(category->name, nullptr);
    EXPECT_FALSE(string(category->name).empty());
  }
}
