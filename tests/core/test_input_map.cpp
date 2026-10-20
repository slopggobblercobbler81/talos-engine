// Unit tests for Input key tokens and InputMap: action bindings and .talos persistence
#include "tests/test_framework.h"
#include "core/input.h"
#include "core/input_map.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Talos::Core;
namespace fs = std::filesystem;

namespace {

    fs::path TempDir() {
        fs::path dir = fs::temp_directory_path() / "talos_input_map_tests";
        fs::create_directories(dir);
        return dir;
    }

    void WriteText(const fs::path& path, const std::string& text) {
        std::ofstream out(path, std::ios::trunc);
        out << text;
    }

}

TEST(KeyTokensAndNames_RoundTripAllSupportedKeys) {
    const Key allKeys[] = {
        Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H, Key::I,
        Key::J, Key::K, Key::L, Key::M, Key::N, Key::O, Key::P, Key::Q, Key::R,
        Key::S, Key::T, Key::U, Key::V, Key::W, Key::X, Key::Y, Key::Z,
        Key::D0, Key::D1, Key::D2, Key::D3, Key::D4,
        Key::D5, Key::D6, Key::D7, Key::D8, Key::D9,
        Key::Up, Key::Down, Key::Left, Key::Right,
        Key::Space, Key::Enter, Key::Escape, Key::Tab, Key::Backspace, Key::Delete,
        Key::LeftShift, Key::RightShift, Key::LeftControl, Key::RightControl,
        Key::LeftAlt, Key::RightAlt,
        Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6,
        Key::F7, Key::F8, Key::F9, Key::F10, Key::F11, Key::F12
    };

    for (Key k : allKeys) {
        const char* token = Input::GetKeyToken(k);
        const char* displayName = Input::GetKeyName(k);
        CHECK(token != nullptr);
        CHECK(displayName != nullptr);
        CHECK(std::string(token) != "Unknown");
        CHECK(std::string(displayName) != "Unknown");

        Key parsedFromToken = Key::Unknown;
        CHECK(Input::TryParseKeyToken(token, parsedFromToken));
        CHECK(parsedFromToken == k);

        Key parsedFromDisplay = Key::Unknown;
        CHECK(Input::TryParseKeyToken(displayName, parsedFromDisplay));
        CHECK(parsedFromDisplay == k);
    }

    // Whitespace-padded token parsing
    Key padded = Key::Unknown;
    CHECK(Input::TryParseKeyToken("   LeftShift  \t\r\n", padded));
    CHECK(padded == Key::LeftShift);

    // Unknown key fallback
    CHECK_EQ(std::string(Input::GetKeyToken(Key::Unknown)), std::string("Unknown"));
    CHECK_EQ(std::string(Input::GetKeyName(Key::Unknown)), std::string("Unknown"));

    Key invalid = Key::Space;
    CHECK(!Input::TryParseKeyToken("NonExistentKey", invalid));
    CHECK(invalid == Key::Unknown);
    CHECK(!Input::TryParseKeyToken("   ", invalid));
    CHECK(invalid == Key::Unknown);
}

TEST(InitDefaults_PopulatesStandardBuiltInActions) {
    InputMap::InitDefaults();

    const auto& actions = InputMap::GetActions();
    CHECK_EQ(actions.size(), static_cast<size_t>(15));
    CHECK(InputMap::HasAction("move_left"));
    CHECK(InputMap::HasAction("move_right"));
    CHECK(InputMap::HasAction("move_up"));
    CHECK(InputMap::HasAction("move_down"));
    CHECK(InputMap::HasAction("jump"));
    CHECK(InputMap::HasAction("attack"));
    CHECK(InputMap::HasAction("interact"));
    CHECK(InputMap::HasAction("sprint"));
    CHECK(InputMap::HasAction("crouch"));
    CHECK(InputMap::HasAction("dash"));
    CHECK(InputMap::HasAction("reload"));
    CHECK(InputMap::HasAction("inventory"));
    CHECK(InputMap::HasAction("pause"));
    CHECK(InputMap::HasAction("ui_accept"));
    CHECK(InputMap::HasAction("ui_cancel"));
    CHECK(!InputMap::HasAction("non_existent_action"));

    const InputAction* moveLeft = InputMap::FindAction("move_left");
    CHECK(moveLeft != nullptr);
    CHECK_EQ(moveLeft->keys.size(), static_cast<size_t>(2));
    CHECK(moveLeft->keys[0] == Key::A);
    CHECK(moveLeft->keys[1] == Key::Left);
}

TEST(ActionLifecycle_AddFindSanitizeAndRemove) {
    InputMap::InitDefaults();

    // Reject empty or whitespace-only names
    CHECK(!InputMap::AddAction(""));
    CHECK(!InputMap::AddAction("   \t\n "));

    // Trim outer spaces and normalize internal spaces to underscores
    CHECK(InputMap::AddAction("  special attack  "));
    CHECK(InputMap::HasAction("special_attack"));
    const InputAction* added = InputMap::FindAction("special_attack");
    CHECK(added != nullptr);
    CHECK(added->keys.empty());

    // Reject duplicate action names
    CHECK(!InputMap::AddAction("special_attack"));
    CHECK(!InputMap::AddAction("special attack"));

    // Remove existing action and verify it is gone
    CHECK(InputMap::RemoveAction("special_attack"));
    CHECK(!InputMap::HasAction("special_attack"));
    CHECK(!InputMap::RemoveAction("special_attack"));
}

TEST(KeyBinding_BindUnbindDeduplicateAndClear) {
    InputMap::InitDefaults();
    CHECK(InputMap::AddAction("grapple"));

    // Reject Key::Unknown or non-existent actions
    CHECK(!InputMap::BindKey("grapple", Key::Unknown));
    CHECK(!InputMap::BindKey("missing_action", Key::G));

    // Bind distinct keys and reject duplicates on the same action
    CHECK(InputMap::BindKey("grapple", Key::G));
    CHECK(InputMap::BindKey("grapple", Key::Q));
    CHECK(!InputMap::BindKey("grapple", Key::G));

    const InputAction* grapple = InputMap::FindAction("grapple");
    CHECK(grapple != nullptr);
    CHECK_EQ(grapple->keys.size(), static_cast<size_t>(2));
    CHECK(grapple->keys[0] == Key::G);
    CHECK(grapple->keys[1] == Key::Q);

    // Unbind specific key
    CHECK(InputMap::UnbindKey("grapple", Key::G));
    CHECK(!InputMap::UnbindKey("grapple", Key::G));
    CHECK(!InputMap::UnbindKey("missing_action", Key::Q));
    CHECK_EQ(grapple->keys.size(), static_cast<size_t>(1));
    CHECK(grapple->keys[0] == Key::Q);

    // Clear all keys on the action without deleting the action itself
    CHECK(InputMap::ClearActionKeys("grapple"));
    CHECK(!InputMap::ClearActionKeys("missing_action"));
    CHECK(InputMap::HasAction("grapple"));
    CHECK(grapple->keys.empty());
}

TEST(Persistence_SaveAndLoadRoundTrip) {
    InputMap::GetActionsMutable().clear();
    CHECK(InputMap::AddAction("thrust"));
    CHECK(InputMap::BindKey("thrust", Key::W));
    CHECK(InputMap::BindKey("thrust", Key::Up));
    CHECK(InputMap::BindKey("thrust", Key::Space));

    CHECK(InputMap::AddAction("fire_laser"));
    CHECK(InputMap::BindKey("fire_laser", Key::J));
    CHECK(InputMap::BindKey("fire_laser", Key::LeftControl));

    CHECK(InputMap::AddAction("unbound_action"));

    fs::path filePath = TempDir() / "nested" / "custom_input_map.talos";
    CHECK(InputMap::Save(filePath.string()));
    CHECK(fs::exists(filePath));

    // Reset to defaults so memory differs from saved file
    InputMap::InitDefaults();
    CHECK(!InputMap::HasAction("thrust"));

    // Load back from disk
    CHECK(InputMap::Load(filePath.string()));
    const auto& loaded = InputMap::GetActions();
    CHECK_EQ(loaded.size(), static_cast<size_t>(3));

    CHECK_EQ(loaded[0].name, std::string("thrust"));
    CHECK_EQ(loaded[0].keys.size(), static_cast<size_t>(3));
    CHECK(loaded[0].keys[0] == Key::W);
    CHECK(loaded[0].keys[1] == Key::Up);
    CHECK(loaded[0].keys[2] == Key::Space);

    CHECK_EQ(loaded[1].name, std::string("fire_laser"));
    CHECK_EQ(loaded[1].keys.size(), static_cast<size_t>(2));
    CHECK(loaded[1].keys[0] == Key::J);
    CHECK(loaded[1].keys[1] == Key::LeftControl);

    CHECK_EQ(loaded[2].name, std::string("unbound_action"));
    CHECK(loaded[2].keys.empty());

    fs::remove_all(TempDir());
    InputMap::InitDefaults();
}

TEST(Persistence_LoadHandlesCommentsDuplicatesAndFallbacks) {
    fs::path dir = TempDir();
    fs::path customFile = dir / "handwritten.talos";

    WriteText(customFile,
        "# Comment header\n"
        "\n"
        "   # Indented comment\n"
        "invalid_line_without_equals\n"
        " = Space\n"
        "  jump_action  =  Space , Up Arrow , Space , InvalidKeyToken , W \n"
        "empty_keys = \n"
    );

    CHECK(InputMap::Load(customFile.string()));
    const auto& actions = InputMap::GetActions();
    CHECK_EQ(actions.size(), static_cast<size_t>(2));

    CHECK_EQ(actions[0].name, std::string("jump_action"));
    CHECK_EQ(actions[0].keys.size(), static_cast<size_t>(3));
    CHECK(actions[0].keys[0] == Key::Space);
    CHECK(actions[0].keys[1] == Key::Up);
    CHECK(actions[0].keys[2] == Key::W);

    CHECK_EQ(actions[1].name, std::string("empty_keys"));
    CHECK(actions[1].keys.empty());

    // Loading an empty/comment-only file falls back to InitDefaults() and returns false
    fs::path emptyFile = dir / "empty.talos";
    WriteText(emptyFile, "# Only comments\n\n");
    CHECK(!InputMap::Load(emptyFile.string()));
    CHECK_EQ(InputMap::GetActions().size(), static_cast<size_t>(15));

    // Loading a non-existent file falls back to InitDefaults() and returns false
    InputMap::GetActionsMutable().clear();
    CHECK(!InputMap::Load((dir / "does_not_exist.talos").string()));
    CHECK_EQ(InputMap::GetActions().size(), static_cast<size_t>(15));

    fs::remove_all(dir);
}

TEST_MAIN()
