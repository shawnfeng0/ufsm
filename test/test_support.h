#pragma once

#include <gtest/gtest.h>

#include <string>
#include <vector>

template <class Tag>
class LifecycleLog {
public:
    static void RecordConstruction(const char* name) {
        ConstructionStorage().emplace_back(name);
    }

    static void RecordDestruction(const char* name) {
        DestructionStorage().emplace_back(name);
    }

    static void Clear() {
        ConstructionStorage().clear();
        DestructionStorage().clear();
    }

    static const std::vector<std::string>& Construction() {
        return ConstructionStorage();
    }

    static const std::vector<std::string>& Destruction() {
        return DestructionStorage();
    }

private:
    static std::vector<std::string>& ConstructionStorage() {
        static std::vector<std::string> construction;
        return construction;
    }

    static std::vector<std::string>& DestructionStorage() {
        static std::vector<std::string> destruction;
        return destruction;
    }
};

inline void ExpectSeq(const std::vector<std::string>& actual, const std::vector<std::string>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(actual[i], expected[i]);
    }
}
