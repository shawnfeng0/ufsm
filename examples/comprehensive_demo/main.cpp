#include <iostream>
#include <string>
#include "ufsm/ufsm.hpp"

// --------------------------------------------------------------------------
// 1. Events Definition
// --------------------------------------------------------------------------
FSM_EVENT(EvStart){};
FSM_EVENT(EvStop){};
FSM_EVENT(EvObstacle){};
FSM_EVENT(EvClear){};
FSM_EVENT(EvLowBattery){};
FSM_EVENT(EvCharged){};
FSM_EVENT(EvPing){}; // For unhandled event demonstration

// --------------------------------------------------------------------------
// 2. Forward Declarations
// --------------------------------------------------------------------------
struct Robot;
struct Idle;
struct Active;
struct Moving;
struct Processing;
struct LowBattery;

// --------------------------------------------------------------------------
// 3. State Machine Definition
// --------------------------------------------------------------------------
FSM_STATE_MACHINE(Robot, Idle) {
    int battery_level = 100;

    // Unhandled event hook
    void OnUnhandledEvent(const ufsm::detail::EventBase& event) {
        std::cout << "[Robot] Warning: Unhandled event '" << event.Name() << "' in current state.\n";
    }

    // Trace hook
    void OnEventProcessed(const ufsm::detail::StateBase* leaf_state, const ufsm::detail::EventBase& event,
                          ufsm::Result result) {
        const char* state_name = leaf_state ? leaf_state->Name() : "None";
        const char* result_str = "Unknown";
        switch (result) {
            case ufsm::Result::kNoReaction: result_str = "NoReaction"; break;
            case ufsm::Result::kForwardEvent: result_str = "ForwardEvent"; break;
            case ufsm::Result::kDiscardEvent: result_str = "DiscardEvent"; break;
            case ufsm::Result::kDeferEvent: result_str = "DeferEvent"; break;
            case ufsm::Result::kConsumed: result_str = "Consumed"; break;
        }
        std::cout << "[Trace] State: " << state_name << ", Event: " << event.Name() << ", Result: " << result_str
                  << "\n";
    }
};

// --------------------------------------------------------------------------
// 4. States Definition
// --------------------------------------------------------------------------

// --- Idle State ---
// Initial state of the robot.
FSM_STATE(Idle, Robot) {
    Idle() { std::cout << "[Idle] Enter (System Ready)\n"; }
    ~Idle() { std::cout << "[Idle] Exit\n"; }

    using reactions = ufsm::List<
        ufsm::Transition<EvStart, Active>,
        ufsm::Transition<EvLowBattery, LowBattery>
    >;
};

// --- Active State (Composite) ---
// Parent state for Moving and Processing.
// Has an initial substate: Moving.
FSM_STATE(Active, Robot, Moving) {
    Active() { std::cout << "[Active] Enter (System Running)\n"; }
    ~Active() { std::cout << "[Active] Exit\n"; }

    using reactions = ufsm::List<
        // High-level interrupt: Stop command works in any substate
        ufsm::Transition<EvStop, Idle>,
        // High-level interrupt: Low battery works in any substate
        ufsm::Transition<EvLowBattery, LowBattery>
    >;
};

// --- Moving State ---
// Substate of Active.
FSM_STATE(Moving, Active) {
    Moving() { std::cout << "  [Moving] Enter (Motors On)\n"; }
    ~Moving() { std::cout << "  [Moving] Exit (Motors Off)\n"; }

    // Action functor for transition
    struct AvoidAction {
        void operator()() { std::cout << "  [Action] Obstacle detected! Initiating avoidance protocol...\n"; }
    };

    using reactions = ufsm::List<
        // Sibling transition with action
        ufsm::Transition<EvObstacle, Processing, AvoidAction>
    >;
};

// --- Processing State ---
// Substate of Active.
FSM_STATE(Processing, Active) {
    Processing() { std::cout << "  [Processing] Enter (Calculating Path)\n"; }
    ~Processing() { std::cout << "  [Processing] Exit\n"; }

    // Custom React method allows for complex logic (guards, context access, etc.)
    ufsm::Result React(const EvClear&) {
        // Accessing shared context (Robot)
        int& battery = Context<Robot>().battery_level;
        battery -= 5; // Consuming energy
        std::cout << "  [Processing] Path cleared. Battery at " << battery << "%.\n";

        // Guard condition simulation
        if (battery < 20) {
             std::cout << "  [Processing] Battery too low to continue!\n";
             return Transit<LowBattery>();
        }

        return Transit<Moving>();
    }

    using reactions = ufsm::List<
        ufsm::Reaction<EvClear>
    >;
};

// --- LowBattery State ---
// Error/Recovery state.
FSM_STATE(LowBattery, Robot) {
    LowBattery() { std::cout << "[LowBattery] Enter (System Suspended)\n"; }
    ~LowBattery() { std::cout << "[LowBattery] Exit\n"; }

    using reactions = ufsm::List<
        // Defer Start command until we are charged
        ufsm::Deferral<EvStart>,
        ufsm::Transition<EvCharged, Idle>
    >;
};

// --------------------------------------------------------------------------
// 5. Main Execution
// --------------------------------------------------------------------------
int main() {
    std::cout << "=== UFSM Comprehensive Demo: Robot Controller ===\n\n";

    Robot robot;
    robot.Initiate();

    std::cout << "\n--- 1. Normal Operation (Idle -> Active -> Moving) ---\n";
    robot.ProcessEvent(EvStart{});

    std::cout << "\n--- 2. Sibling Transition with Action (Moving -> Processing) ---\n";
    robot.ProcessEvent(EvObstacle{});

    std::cout << "\n--- 3. Custom Logic & Context Access (Processing -> Moving) ---\n";
    robot.ProcessEvent(EvClear{});

    std::cout << "\n--- 4. Hierarchical Transition / Drill-up (Moving -> Active -> Idle) ---\n";
    robot.ProcessEvent(EvStop{});

    std::cout << "\n--- 5. Deferral Logic (Idle -> LowBattery) ---\n";
    robot.ProcessEvent(EvLowBattery{});

    std::cout << "\n[User] Sending EvStart (should be deferred)...\n";
    robot.ProcessEvent(EvStart{});

    std::cout << "\n[User] Sending EvCharged (should transition to Idle and AUTO-PROCESS deferred Start)...\n";
    robot.ProcessEvent(EvCharged{});
    // Expected flow:
    // 1. LowBattery -> Idle (via EvCharged)
    // 2. Idle -> Active -> Moving (via deferred EvStart)

    std::cout << "\n--- 6. Unhandled Event Hook ---\n";
    robot.ProcessEvent(EvPing{});

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}
