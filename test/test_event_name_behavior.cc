#include <gtest/gtest.h>

#include <string_view>
#include <ufsm/ufsm.h>

FSM_EVENT(NameEventPing){};

TEST(EventNameBehaviorTest, EventProvidesReadableName) {
  NameEventPing ev;
  const char* name = ev.Name();

  ASSERT_NE(name, nullptr);
  std::string_view sv{name};
  EXPECT_FALSE(sv.empty());

  // Best-effort: ensure the emitted name contains the type identifier.
  EXPECT_NE(sv.find("NameEventPing"), std::string_view::npos);
}
