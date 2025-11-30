#include "ps2kbd.hpp"

#include "interrupts/isr.hpp"
#include "keyboard.hpp"
#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::devices::ps2kbd {
    constexpr uint16_t DATA = 0x60;
    constexpr uint16_t STATUS = 0x64;
    constexpr uint16_t COMMAND = 0x64;

    struct Configuration {
        uint8_t raw;

#define FIELD(name, index)                                                                                                                 \
    [[nodiscard]]                                                                                                                          \
    bool name() const {                                                                                                                    \
        return raw & (1 << index);                                                                                                         \
    }                                                                                                                                      \
    void name(const bool value) {                                                                                                          \
        if (value)                                                                                                                         \
            raw |= 1 << index;                                                                                                             \
        else                                                                                                                               \
            raw &= ~(1 << index);                                                                                                          \
    }

        FIELD(first_interrupt_enable, 0)
        FIELD(second_interrupt_enable, 1)
        FIELD(system, 2)
        FIELD(first_clock_disable, 4)
        FIELD(second_clock_disable, 5)
        FIELD(first_translation_enable, 6)

#undef FIELD
    };

    void wait_send() {
        for (auto i = 0; i < 8; i++) {
            utils::wait();
            if ((utils::byte_in(STATUS) & 0b10) == 0) return;
        }

        utils::panic(nullptr, "[ps2kbd] Failed to send data");
    }

    void send_controller_cmd(const uint8_t cmd) {
        wait_send();
        utils::byte_out(COMMAND, cmd);
    }

    void send_controller_cmd(const uint8_t cmd, const uint8_t data) {
        wait_send();
        utils::byte_out(COMMAND, cmd);

        wait_send();
        utils::byte_out(DATA, data);
    }

    uint8_t recv_data() {
        for (auto i = 0; i < 8; i++) {
            utils::wait();

            if ((utils::byte_in(STATUS) & 0b1) == 1) {
                return utils::byte_in(DATA);
            }
        }

        utils::panic(nullptr, "[ps2kbd] Failed to receive data");
    }

    void send_device_cmd(const uint8_t cmd) {
        wait_send();
        utils::byte_out(DATA, cmd);

        auto i = 0;

        for (;;) {
            const auto response = recv_data();
            if (response == 0xFA) break;

            if (response == 0xFE) {
                if (i >= 8) {
                    utils::panic(nullptr, "[ps2kbd] Failed to receive response");
                }

                wait_send();
                utils::byte_out(DATA, cmd);

                i++;
                continue;
            }

            serial::printf("[ps2kbd] Invalid response to a device command, 0x%X\n", response);
        }
    }

    constexpr uint8_t SCAN_RELEASE = 0x80;
    constexpr uint8_t SCAN_CTRL = 0x1D;
    constexpr uint8_t SCAN_NUM_LOCK = 0x45;
    constexpr uint8_t SCAN_EXT0 = 0xE0;
    constexpr uint8_t SCAN_EXT1 = 0xE1;

    static uint8_t state = 0;
    static keyboard::Key normal_key_map[128];
    static keyboard::Key extended_key_map[128];

    void on_data([[maybe_unused]] isr::InterruptInfo* info) {
        const auto data = utils::byte_in(DATA);

        const auto press = !(data & SCAN_RELEASE) ? true : false;
        const auto index = data & ~SCAN_RELEASE;

        keyboard::Key key;

        switch (state) {
        case 1:
            key = index < static_cast<int>(sizeof(extended_key_map)) ? extended_key_map[index] : keyboard::Key::Unknown;
            break;

        case 2:
            state = (index == SCAN_CTRL) ? 3 : 0;
            return;

        case 3:
            if (index == SCAN_NUM_LOCK) {
                key = keyboard::Key::Pause;
                break;
            }

            // fallthrough

        default:
            switch (data) {
            case SCAN_EXT0:
                state = 1;
                return;
            case SCAN_EXT1:
                state = 2;
                return;
            default:
                key = index < static_cast<int>(sizeof(normal_key_map)) ? normal_key_map[index] : keyboard::Key::Unknown;
                break;
            }
            break;
        }

        if (key != keyboard::Key::Unknown) {
            keyboard::add_event({ key, press });
        }

        state = 0;
    }

    void init_normal_key_map();
    void init_extended_key_map();

    void init() {
        init_normal_key_map();
        init_extended_key_map();

        // Disable both PS2 ports
        send_controller_cmd(0xAD);
        send_controller_cmd(0xA7);

        // Flush output buffer
        while (utils::byte_in(STATUS) & 0b1) {
            utils::byte_in(DATA);
        }

        // Set initial configuration byte
        send_controller_cmd(0x20);

        auto config = Configuration(recv_data());
        config.first_interrupt_enable(false);
        config.second_interrupt_enable(false);
        config.first_translation_enable(true);
        config.first_clock_disable(false);
        config.second_clock_disable(true);

        send_controller_cmd(0x60, config.raw);

        // Flush output buffer
        while (utils::byte_in(STATUS) & 0b1) {
            utils::byte_in(DATA);
        }

        // Perform self test
        send_controller_cmd(0xAA);
        const auto self_test_response = recv_data();

        if (self_test_response != 0x55) {
            utils::panic(nullptr, "[ps2kbd] Controller self test failed, 0x%X\n", self_test_response);
        }

        send_controller_cmd(0x60, config.raw);

        // Test first port
        send_controller_cmd(0xAB);
        const auto test_response = recv_data();

        if (test_response != 0x00) {
            utils::panic(nullptr, "[ps2kbd] First port test failed, 0x%X\n", test_response);
        }

        // Enable first port
        send_controller_cmd(0xAE);

        // Reset keyboard
        send_device_cmd(0xFF);
        const auto reset_response = recv_data();

        if (reset_response != 0xAA) {
            utils::panic(nullptr, "[ps2kbd] Failed to reset keyboard, 0x%X\n", reset_response);
        }

        // Enable interrupts for first port
        config.first_interrupt_enable(true);
        send_controller_cmd(0x60, config.raw);

        cosmos::isr::set(1, on_data);
    }

    void init_normal_key_map() {
        utils::memset(normal_key_map, 0, sizeof(normal_key_map));

        normal_key_map[0x01] = keyboard::Key::Escape;
        normal_key_map[0x02] = keyboard::Key::Key1;
        normal_key_map[0x03] = keyboard::Key::Key2;
        normal_key_map[0x04] = keyboard::Key::Key3;
        normal_key_map[0x05] = keyboard::Key::Key4;
        normal_key_map[0x06] = keyboard::Key::Key5;
        normal_key_map[0x07] = keyboard::Key::Key6;
        normal_key_map[0x08] = keyboard::Key::Key7;
        normal_key_map[0x09] = keyboard::Key::Key8;
        normal_key_map[0x0A] = keyboard::Key::Key9;
        normal_key_map[0x0B] = keyboard::Key::Key0;
        normal_key_map[0x0C] = keyboard::Key::Dash;
        normal_key_map[0x0D] = keyboard::Key::Equal;
        normal_key_map[0x0E] = keyboard::Key::Backspace;
        normal_key_map[0x0F] = keyboard::Key::Tab;
        normal_key_map[0x10] = keyboard::Key::Q;
        normal_key_map[0x11] = keyboard::Key::W;
        normal_key_map[0x12] = keyboard::Key::E;
        normal_key_map[0x13] = keyboard::Key::R;
        normal_key_map[0x14] = keyboard::Key::T;
        normal_key_map[0x15] = keyboard::Key::Y;
        normal_key_map[0x16] = keyboard::Key::U;
        normal_key_map[0x17] = keyboard::Key::I;
        normal_key_map[0x18] = keyboard::Key::O;
        normal_key_map[0x19] = keyboard::Key::P;
        normal_key_map[0x1A] = keyboard::Key::OpenBracket;
        normal_key_map[0x1B] = keyboard::Key::CloseBracket;
        normal_key_map[0x1C] = keyboard::Key::Enter;
        normal_key_map[0x1D] = keyboard::Key::LeftCtrl;
        normal_key_map[0x1E] = keyboard::Key::A;
        normal_key_map[0x1F] = keyboard::Key::S;
        normal_key_map[0x20] = keyboard::Key::D;
        normal_key_map[0x21] = keyboard::Key::F;
        normal_key_map[0x22] = keyboard::Key::G;
        normal_key_map[0x23] = keyboard::Key::H;
        normal_key_map[0x24] = keyboard::Key::J;
        normal_key_map[0x25] = keyboard::Key::K;
        normal_key_map[0x26] = keyboard::Key::L;
        normal_key_map[0x27] = keyboard::Key::Semicolon;
        normal_key_map[0x28] = keyboard::Key::Apostrophe;
        normal_key_map[0x29] = keyboard::Key::GraveAccent;
        normal_key_map[0x2A] = keyboard::Key::LeftShift;
        normal_key_map[0x2B] = keyboard::Key::Backslash;
        normal_key_map[0x2C] = keyboard::Key::Z;
        normal_key_map[0x2D] = keyboard::Key::X;
        normal_key_map[0x2E] = keyboard::Key::C;
        normal_key_map[0x2F] = keyboard::Key::V;
        normal_key_map[0x30] = keyboard::Key::B;
        normal_key_map[0x31] = keyboard::Key::N;
        normal_key_map[0x32] = keyboard::Key::M;
        normal_key_map[0x33] = keyboard::Key::Comma;
        normal_key_map[0x34] = keyboard::Key::Period;
        normal_key_map[0x35] = keyboard::Key::Slash;
        normal_key_map[0x36] = keyboard::Key::RightShift;
        normal_key_map[0x37] = keyboard::Key::NumStar;
        normal_key_map[0x38] = keyboard::Key::LeftAlt;
        normal_key_map[0x39] = keyboard::Key::Space;
        normal_key_map[0x3A] = keyboard::Key::CapsLock;
        normal_key_map[0x3B] = keyboard::Key::F1;
        normal_key_map[0x3C] = keyboard::Key::F2;
        normal_key_map[0x3D] = keyboard::Key::F3;
        normal_key_map[0x3E] = keyboard::Key::F4;
        normal_key_map[0x3F] = keyboard::Key::F5;
        normal_key_map[0x40] = keyboard::Key::F6;
        normal_key_map[0x41] = keyboard::Key::F7;
        normal_key_map[0x42] = keyboard::Key::F8;
        normal_key_map[0x43] = keyboard::Key::F9;
        normal_key_map[0x44] = keyboard::Key::F10;
        normal_key_map[0x45] = keyboard::Key::NumLock;
        normal_key_map[0x46] = keyboard::Key::ScrollLock;
        normal_key_map[0x47] = keyboard::Key::Num7;
        normal_key_map[0x48] = keyboard::Key::Num8;
        normal_key_map[0x49] = keyboard::Key::Num9;
        normal_key_map[0x4A] = keyboard::Key::NumDash;
        normal_key_map[0x4B] = keyboard::Key::Num4;
        normal_key_map[0x4C] = keyboard::Key::Num5;
        normal_key_map[0x4D] = keyboard::Key::Num6;
        normal_key_map[0x4E] = keyboard::Key::NumPlus;
        normal_key_map[0x4F] = keyboard::Key::Num1;
        normal_key_map[0x50] = keyboard::Key::Num2;
        normal_key_map[0x51] = keyboard::Key::Num3;
        normal_key_map[0x52] = keyboard::Key::Num0;
        normal_key_map[0x53] = keyboard::Key::NumPeriod;
        normal_key_map[0x54] = keyboard::Key::Unknown; // keyboard::Key::SYSREQ;
        normal_key_map[0x56] = keyboard::Key::Unknown; // keyboard::Key::EUROPE_2;
        normal_key_map[0x57] = keyboard::Key::F11;
        normal_key_map[0x58] = keyboard::Key::F12;
        normal_key_map[0x59] = keyboard::Key::NumEqual;
        normal_key_map[0x5C] = keyboard::Key::Unknown; // keyboard::Key::I10L_6;
        normal_key_map[0x64] = keyboard::Key::F13;
        normal_key_map[0x65] = keyboard::Key::F14;
        normal_key_map[0x66] = keyboard::Key::F15;
        normal_key_map[0x67] = keyboard::Key::F16;
        normal_key_map[0x68] = keyboard::Key::F17;
        normal_key_map[0x69] = keyboard::Key::F18;
        normal_key_map[0x6A] = keyboard::Key::F19;
        normal_key_map[0x6B] = keyboard::Key::F20;
        normal_key_map[0x6C] = keyboard::Key::F21;
        normal_key_map[0x6D] = keyboard::Key::F22;
        normal_key_map[0x6E] = keyboard::Key::F23;
        normal_key_map[0x70] = keyboard::Key::Unknown; // keyboard::Key::I10L_2;

        /* The following two keys (0x71, 0x72) are release-only. */
        normal_key_map[0x71] = keyboard::Key::Unknown; // keyboard::Key::LANG_2;
        normal_key_map[0x72] = keyboard::Key::Unknown; // keyboard::Key::LANG_1;
        normal_key_map[0x73] = keyboard::Key::Unknown; // keyboard::Key::I10L_1;

        /* The following key (0x76) can be either F24 or LANG_5. */
        normal_key_map[0x76] = keyboard::Key::F24;
        normal_key_map[0x77] = keyboard::Key::Unknown; // keyboard::Key::LANG_4;
        normal_key_map[0x78] = keyboard::Key::Unknown; // keyboard::Key::LANG_3;
        normal_key_map[0x79] = keyboard::Key::Unknown; // keyboard::Key::I10L_4;
        normal_key_map[0x7B] = keyboard::Key::Unknown; // keyboard::Key::I10L_5;
        normal_key_map[0x7D] = keyboard::Key::Unknown; // keyboard::Key::I10L_3;
        normal_key_map[0x7E] = keyboard::Key::Unknown; // keyboard::Key::EQUAL_SIGN;
    }

    void init_extended_key_map() {
        utils::memset(extended_key_map, 0, sizeof(extended_key_map));

        extended_key_map[0x10] = keyboard::Key::Unknown; // keyboard::Key::_SCAN_PREVIOUS_TRACK;
        extended_key_map[0x19] = keyboard::Key::Unknown; // keyboard::Key::_SCAN_NEXT_TRACK;
        extended_key_map[0x1C] = keyboard::Key::NumEnter;
        extended_key_map[0x1D] = keyboard::Key::RightCtrl;
        extended_key_map[0x20] = keyboard::Key::Unknown; // keyboard::Key::_MUTE;
        extended_key_map[0x21] = keyboard::Key::Unknown; // keyboard::Key::_AL_CALCULATOR;
        extended_key_map[0x22] = keyboard::Key::Unknown; // keyboard::Key::_PLAY_PAUSE;
        extended_key_map[0x24] = keyboard::Key::Unknown; // keyboard::Key::_STOP;
        extended_key_map[0x2E] = keyboard::Key::Unknown; // keyboard::Key::_VOLUME_DOWN;
        extended_key_map[0x30] = keyboard::Key::Unknown; // keyboard::Key::_VOLUME_UP;
        extended_key_map[0x32] = keyboard::Key::Unknown; // keyboard::Key::_AC_HOME;
        extended_key_map[0x35] = keyboard::Key::NumSlash;
        extended_key_map[0x37] = keyboard::Key::PrintScreen;
        extended_key_map[0x38] = keyboard::Key::RightAlt;
        extended_key_map[0x46] = keyboard::Key::Pause;
        extended_key_map[0x47] = keyboard::Key::Home;
        extended_key_map[0x48] = keyboard::Key::Up;
        extended_key_map[0x49] = keyboard::Key::PageUp;
        extended_key_map[0x4B] = keyboard::Key::Left;
        extended_key_map[0x4D] = keyboard::Key::Right;
        extended_key_map[0x4F] = keyboard::Key::End;
        extended_key_map[0x50] = keyboard::Key::Down;
        extended_key_map[0x51] = keyboard::Key::PageDown;
        extended_key_map[0x52] = keyboard::Key::Insert;
        extended_key_map[0x53] = keyboard::Key::Delete;
        extended_key_map[0x5B] = keyboard::Key::LeftSuper;
        extended_key_map[0x5C] = keyboard::Key::RightSuper;
        extended_key_map[0x5D] = keyboard::Key::Unknown; // keyboard::Key::APPLICATION;

        /* The following extended key (0x5E) may also be INPUT_KEY_POWER. */
        extended_key_map[0x5E] = keyboard::Key::Unknown; // keyboard::Key::YSTEM_POWER_DOWN;
        extended_key_map[0x5F] = keyboard::Key::Unknown; // keyboard::Key::YSTEM_SLEEP;
        extended_key_map[0x63] = keyboard::Key::Unknown; // keyboard::Key::YSTEM_WAKE_UP;
        extended_key_map[0x65] = keyboard::Key::Unknown; // keyboard::Key::_AC_SEARCH;
        extended_key_map[0x66] = keyboard::Key::Unknown; // keyboard::Key::_AC_BOOKMARKS;
        extended_key_map[0x67] = keyboard::Key::Unknown; // keyboard::Key::_AC_REFRESH;
        extended_key_map[0x68] = keyboard::Key::Unknown; // keyboard::Key::_AC_STOP;
        extended_key_map[0x69] = keyboard::Key::Unknown; // keyboard::Key::_AC_FORWARD;
        extended_key_map[0x6A] = keyboard::Key::Unknown; // keyboard::Key::_AC_BACK;
        extended_key_map[0x6B] = keyboard::Key::Unknown; // keyboard::Key::_AL_LOCAL_BROWSER;
        extended_key_map[0x6C] = keyboard::Key::Unknown; // keyboard::Key::_AL_EMAIL_READER;
        extended_key_map[0x6D] = keyboard::Key::Unknown; // keyboard::Key::_AL_MEDIA_SELECT;
    }
} // namespace cosmos::devices::ps2kbd
