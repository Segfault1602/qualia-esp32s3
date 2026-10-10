#include "modules/inputs.h"
#include "modules/module.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

namespace
{
using namespace std::chrono_literals;
using modules::Input;

struct FakeButtons
{
    static inline bool initialized = true;
    static inline std::array<bool, 2> levels{};
    static inline bool fail = false;

    static void reset()
    {
        initialized = true;
        levels = {};
        fail = false;
    }
    static bool is_initialized()
    {
        return initialized;
    }
    static std::expected<bool, esp_err_t> level(Input which)
    {
        if (fail)
        {
            return std::unexpected(ESP_ERR_TIMEOUT);
        }
        return levels[static_cast<std::size_t>(which)];
    }
};

using TestInputs = modules::Inputs<FakeButtons>;
static_assert(modules::Module<TestInputs>);

std::vector<Input> g_edges;

void record(Input which)
{
    g_edges.push_back(which);
}

void reset()
{
    FakeButtons::reset();
    g_edges.clear();
}

void steps(TestInputs& inputs, int count)
{
    for (int i = 0; i < count; ++i)
    {
        inputs.step(0us);
    }
}

void init_tests()
{
    {
        reset();
        FakeButtons::initialized = false;
        TestInputs inputs(record);
        CHECK(inputs.init() == ESP_ERR_INVALID_STATE);
        CHECK(!inputs.is_initialized());
    }
    {
        reset();
        TestInputs inputs(nullptr);
        CHECK(inputs.init() == ESP_ERR_INVALID_ARG);
    }
    {
        reset();
        TestInputs inputs(record);
        CHECK(inputs.init() == ESP_OK);
        CHECK(inputs.is_initialized());
    }
}

void debounce_tests()
{
    reset();
    TestInputs inputs(record);
    CHECK(inputs.init() == ESP_OK);

    FakeButtons::levels = {true, true};
    steps(inputs, 3);
    FakeButtons::levels = {false, true};
    steps(inputs, 6);
    CHECK(g_edges.empty());
    // The seventh low step after a high one reports the edge once.
    steps(inputs, 1);
    CHECK(g_edges.size() == 1);
    CHECK(g_edges[0] == Input::Up);
    steps(inputs, 20);
    CHECK(g_edges.size() == 1);

    // A bounce inside the window restarts it.
    FakeButtons::levels = {true, true};
    steps(inputs, 1);
    FakeButtons::levels = {false, true};
    steps(inputs, 4);
    FakeButtons::levels = {true, true};
    steps(inputs, 1);
    FakeButtons::levels = {false, true};
    steps(inputs, 6);
    CHECK(g_edges.size() == 1);
    steps(inputs, 1);
    CHECK(g_edges.size() == 2);
}

void read_failure_tests()
{
    reset();
    TestInputs inputs(record);
    CHECK(inputs.init() == ESP_OK);

    FakeButtons::levels = {true, true};
    steps(inputs, 1);
    FakeButtons::levels = {true, false};
    steps(inputs, 6);
    // Failed steps do not count toward the debounce window.
    FakeButtons::fail = true;
    steps(inputs, 5);
    CHECK(g_edges.empty());
    FakeButtons::fail = false;
    steps(inputs, 1);
    CHECK(g_edges.size() == 1);
    CHECK(g_edges[0] == Input::Down);
}
} // namespace

int main()
{
    init_tests();
    debounce_tests();
    read_failure_tests();
    std::puts("inputs module tests passed");
    return EXIT_SUCCESS;
}
