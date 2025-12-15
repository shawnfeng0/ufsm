#include <gtest/gtest.h>

#include <ufsm/ufsm.hpp>

#include <vector>

namespace {

struct EvShutterFull : ufsm::Event<EvShutterFull> {};
struct EvInFocus : ufsm::Event<EvInFocus> {};
struct EvMarker : ufsm::Event<EvMarker> {};

struct Focusing;
struct Focused;

struct Sm : ufsm::StateMachine<Sm, Focusing> {
    std::vector<int> seq;
};

struct Focusing : ufsm::State<Focusing, Sm> {
    using reactions = ufsm::List<ufsm::Deferral<EvShutterFull>, ufsm::Reaction<EvInFocus>>;

    using ufsm::State<Focusing, Sm>::State;

    ufsm::Result React(const EvInFocus&) {
        // Post a marker event so we can verify that deferred events are moved
        // to the *front* of the posted queue when this state exits.
        PostEvent(EvMarker{});
        return Transit<Focused>();
    }
};

struct Focused : ufsm::State<Focused, Sm> {
    using reactions = ufsm::List<ufsm::Reaction<EvShutterFull>, ufsm::Reaction<EvMarker>>;

    using ufsm::State<Focused, Sm>::State;

    ufsm::Result React(const EvShutterFull&) {
        OutermostContext().seq.push_back(1);
           return consume_event();
    }

    ufsm::Result React(const EvMarker&) {
        OutermostContext().seq.push_back(2);
           return consume_event();
    }
};

TEST(deferral_behavior, deferred_event_moves_to_front_of_posted_on_state_exit) {
    Sm sm;
    sm.Initiate();

    // While in Focusing, EvShutterFull is deferred.
    sm.ProcessEvent(EvShutterFull{});
    EXPECT_TRUE(sm.seq.empty());

    // Leaving Focusing causes its deferred events to be moved to the front of
    // the posted queue. After the transition completes, the posted queue drains.
    sm.ProcessEvent(EvInFocus{});

    ASSERT_EQ(sm.seq.size(), 2u);
    EXPECT_EQ(sm.seq[0], 1); // deferred shutter event
    EXPECT_EQ(sm.seq[1], 2); // marker posted before transit
}

struct FocusingAny;
struct FocusedAny;

struct SmAny : ufsm::StateMachine<SmAny, FocusingAny> {
    std::vector<int> seq;
};

struct FocusingAny : ufsm::State<FocusingAny, SmAny> {
    using reactions = ufsm::List<ufsm::Reaction<EvInFocus>, ufsm::Deferral<ufsm::detail::EventBase>>;

    using ufsm::State<FocusingAny, SmAny>::State;

    ufsm::Result React(const EvInFocus&) {
        PostEvent(EvMarker{});
        return Transit<FocusedAny>();
    }
};

struct FocusedAny : ufsm::State<FocusedAny, SmAny> {
    using reactions = ufsm::List<ufsm::Reaction<EvShutterFull>, ufsm::Reaction<EvMarker>>;

    using ufsm::State<FocusedAny, SmAny>::State;

    ufsm::Result React(const EvShutterFull&) {
        OutermostContext().seq.push_back(1);
           return consume_event();
    }

    ufsm::Result React(const EvMarker&) {
        OutermostContext().seq.push_back(2);
           return consume_event();
    }
};

TEST(deferral_behavior, event_base_deferral_defers_any_event) {
    SmAny sm;
    sm.Initiate();

    // No matter the concrete event type, FocusingAny defers it.
    sm.ProcessEvent(EvShutterFull{});
    EXPECT_TRUE(sm.seq.empty());

    sm.ProcessEvent(EvInFocus{});

    ASSERT_EQ(sm.seq.size(), 2u);
    EXPECT_EQ(sm.seq[0], 1);
    EXPECT_EQ(sm.seq[1], 2);
}

struct EvGo : ufsm::Event<EvGo> {};
struct EvPayload : ufsm::Event<EvPayload> {};

struct DeferSm;
struct DeferA;
struct DeferB;

struct DeferSm : ufsm::StateMachine<DeferSm, DeferA> {
    std::vector<int> seq;
};

struct DeferA : ufsm::State<DeferA, DeferSm> {
    using reactions = ufsm::List<ufsm::Reaction<EvPayload>, ufsm::Reaction<EvGo>>;
    using ufsm::State<DeferA, DeferSm>::State;

    ufsm::Result React(const EvPayload& ev) {
        (void)ev;
        return defer_event();
    }

    ufsm::Result React(const EvGo&) {
        return Transit<DeferB>();
    }
};

struct DeferB : ufsm::State<DeferB, DeferSm> {
    using reactions = ufsm::List<ufsm::Reaction<EvPayload>>;
    using ufsm::State<DeferB, DeferSm>::State;

    ufsm::Result React(const EvPayload&) {
        OutermostContext().seq.push_back(42);
           return consume_event();
    }
};

TEST(deferral_behavior, defer_event_helper_defers_current_event_until_state_exit) {
    DeferSm sm;
    sm.Initiate();

    sm.ProcessEvent(EvPayload{});
    EXPECT_TRUE(sm.seq.empty());

    sm.ProcessEvent(EvGo{});
    ASSERT_EQ(sm.seq.size(), 1u);
    EXPECT_EQ(sm.seq[0], 42);
}

} // namespace
