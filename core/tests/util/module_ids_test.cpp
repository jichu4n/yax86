#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "yax86_core.h"

using namespace std;

// Each module declares its own LogModule in its own public header rather than
// registering in a central enum, so that modules do not need to know about one
// another. Nothing in the core therefore enforces that their IDs are unique -
// this test does, since the test suite already depends on every module.
namespace {

const vector<const LogModule*> kAllModules = {
    &kLogModulePlatform, &kLogModuleCPU, &kLogModulePIC, &kLogModulePIT,
    &kLogModulePPI,      &kLogModuleFDC, &kLogModuleDMA, &kLogModuleKeyboard,
    &kLogModuleVideo,    &kLogModuleHDC,
};

}  // namespace

class LogModuleIDTest : public ::testing::Test {};

TEST_F(LogModuleIDTest, IDsAreUnique) {
  map<uint8_t, string> ids_to_names;
  for (const LogModule* module : kAllModules) {
    auto existing = ids_to_names.find(module->id);
    ASSERT_EQ(existing, ids_to_names.end())
        << "Log module ID " << static_cast<int>(module->id)
        << " is used by both " << existing->second << " and " << module->name;
    ids_to_names[module->id] = module->name;
  }
}

TEST_F(LogModuleIDTest, IDsFitInModuleMask) {
  for (const LogModule* module : kAllModules) {
    EXPECT_LT(module->id, kLogMaxModules) << "module " << module->name;
  }
}

TEST_F(LogModuleIDTest, NamesAreNonEmpty) {
  for (const LogModule* module : kAllModules) {
    ASSERT_NE(module->name, nullptr);
    EXPECT_FALSE(string(module->name).empty());
  }
}
