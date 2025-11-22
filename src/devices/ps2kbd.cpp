#include "ps2kbd.hpp"

#include "interrupts/isr.hpp"
#include "scheduler/scheduler.hpp"
#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::devices::ps2kbd {
    constexpr uint16_t DATA = 0x60;
    constexpr uint16_t STATUS = 0x64;
    constexpr uint16_t COMMAND = 0x64;

    struct Configuration {
        uint8_t raw;

#define FIELD(name, index)                                                                                                                 \
    [[nodiscard]] bool name() const {                                                                                                      \
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

        serial::printf("[ps2kbd] Failed to send data\n");
        utils::halt();
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

        serial::printf("[ps2kbd] Failed to receive data\n");
        utils::halt();
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
                    serial::printf("[ps2kbd] Failed to receive response\n");
                    utils::halt();
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
    static Key normal_key_map[128];
    static Key extended_key_map[128];

    constexpr uint32_t BUFFER_SIZE = 16;
    static Event buffer[BUFFER_SIZE];
    static uint32_t buffer_write_index = 0;
    static uint32_t buffer_read_index = 0;

    static scheduler::ProcessId waiting_process = 0;

    void on_data([[maybe_unused]] isr::InterruptInfo* info) {
        const auto data = utils::byte_in(DATA);

        const auto press = !(data & SCAN_RELEASE) ? true : false;
        const auto index = data & ~SCAN_RELEASE;

        Key key;

        switch (state) {
        case 1:
            key = index < static_cast<int>(sizeof(extended_key_map)) ? extended_key_map[index] : Key::Unknown;
            break;

        case 2:
            state = (index == SCAN_CTRL) ? 3 : 0;
            return;

        case 3:
            if (index == SCAN_NUM_LOCK) {
                key = Key::Pause;
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
                key = index < static_cast<int>(sizeof(normal_key_map)) ? normal_key_map[index] : Key::Unknown;
                break;
            }
            break;
        }

        if (key != Key::Unknown) {
            const auto next_write_index = (buffer_write_index + 1) % BUFFER_SIZE;

            if (next_write_index != buffer_read_index) {
                buffer[buffer_write_index] = {
                    .key = key,
                    .press = press,
                };

                buffer_write_index = next_write_index;

                // Resume waiting process
                if (waiting_process != 0) {
                    scheduler::resume(waiting_process);
                    waiting_process = 0;
                }
            }
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
            serial::printf("[ps2kbd] Controller self test failed, 0x%X\n", self_test_response);
            utils::halt();
        }

        send_controller_cmd(0x60, config.raw);

        // Test first port
        send_controller_cmd(0xAB);
        const auto test_response = recv_data();

        if (test_response != 0x00) {
            serial::printf("[ps2kbd] First port test failed, 0x%X\n", test_response);
            utils::halt();
        }

        // Enable first port
        send_controller_cmd(0xAE);

        // Reset keyboard
        send_device_cmd(0xFF);
        const auto reset_response = recv_data();

        if (reset_response != 0xAA) {
            serial::printf("[ps2kbd] Failed to reset keyboard, 0x%X\n", reset_response);
            utils::halt();
        }

        // Enable interrupts for first port
        config.first_interrupt_enable(true);
        send_controller_cmd(0x60, config.raw);

        cosmos::isr::set(1, on_data);
    }

    void reset_buffer() {
        asm volatile("cli");

        buffer_write_index = 0;
        buffer_read_index = 0;

        asm volatile("sti");
    }

    bool try_get_event(Event& event) {
        asm volatile("cli" ::: "memory");

        if (buffer_read_index != buffer_write_index) {
            event = buffer[buffer_read_index];
            buffer_read_index = (buffer_read_index + 1) % BUFFER_SIZE;

            asm volatile("sti" ::: "memory");
            return true;
        }

        asm volatile("sti" ::: "memory");
        return false;
    }

    void resume_on_event() {
        waiting_process = scheduler::get_current_process();
    }

    Event wait_for_event() {
        for (;;) {
            Event event;
            if (try_get_event(event)) return event;

            resume_on_event();
            scheduler::suspend();
        }
    }

    void init_normal_key_map() {
        utils::memset(normal_key_map, 0, sizeof(normal_key_map));

        normal_key_map[0x01] = Key::Escape;
        normal_key_map[0x02] = Key::Key1;
        normal_key_map[0x03] = Key::Key2;
        normal_key_map[0x04] = Key::Key3;
        normal_key_map[0x05] = Key::Key4;
        normal_key_map[0x06] = Key::Key5;
        normal_key_map[0x07] = Key::Key6;
        normal_key_map[0x08] = Key::Key7;
        normal_key_map[0x09] = Key::Key8;
        normal_key_map[0x0A] = Key::Key9;
        normal_key_map[0x0B] = Key::Key0;
        normal_key_map[0x0C] = Key::Dash;
        normal_key_map[0x0D] = Key::Equal;
        normal_key_map[0x0E] = Key::Backspace;
        normal_key_map[0x0F] = Key::Tab;
        normal_key_map[0x10] = Key::Q;
        normal_key_map[0x11] = Key::W;
        normal_key_map[0x12] = Key::E;
        normal_key_map[0x13] = Key::R;
        normal_key_map[0x14] = Key::T;
        normal_key_map[0x15] = Key::Y;
        normal_key_map[0x16] = Key::U;
        normal_key_map[0x17] = Key::I;
        normal_key_map[0x18] = Key::O;
        normal_key_map[0x19] = Key::P;
        normal_key_map[0x1A] = Key::OpenBracket;
        normal_key_map[0x1B] = Key::CloseBracket;
        normal_key_map[0x1C] = Key::Enter;
        normal_key_map[0x1D] = Key::LeftCtrl;
        normal_key_map[0x1E] = Key::A;
        normal_key_map[0x1F] = Key::S;
        normal_key_map[0x20] = Key::D;
        normal_key_map[0x21] = Key::F;
        normal_key_map[0x22] = Key::G;
        normal_key_map[0x23] = Key::H;
        normal_key_map[0x24] = Key::J;
        normal_key_map[0x25] = Key::K;
        normal_key_map[0x26] = Key::L;
        normal_key_map[0x27] = Key::Semicolon;
        normal_key_map[0x28] = Key::Apostrophe;
        normal_key_map[0x29] = Key::GraveAccent;
        normal_key_map[0x2A] = Key::LeftShift;
        normal_key_map[0x2B] = Key::Backslash;
        normal_key_map[0x2C] = Key::Z;
        normal_key_map[0x2D] = Key::X;
        normal_key_map[0x2E] = Key::C;
        normal_key_map[0x2F] = Key::V;
        normal_key_map[0x30] = Key::B;
        normal_key_map[0x31] = Key::N;
        normal_key_map[0x32] = Key::M;
        normal_key_map[0x33] = Key::Comma;
        normal_key_map[0x34] = Key::Period;
        normal_key_map[0x35] = Key::Slash;
        normal_key_map[0x36] = Key::RightShift;
        normal_key_map[0x37] = Key::NumStar;
        normal_key_map[0x38] = Key::LeftAlt;
        normal_key_map[0x39] = Key::Space;
        normal_key_map[0x3A] = Key::CapsLock;
        normal_key_map[0x3B] = Key::F1;
        normal_key_map[0x3C] = Key::F2;
        normal_key_map[0x3D] = Key::F3;
        normal_key_map[0x3E] = Key::F4;
        normal_key_map[0x3F] = Key::F5;
        normal_key_map[0x40] = Key::F6;
        normal_key_map[0x41] = Key::F7;
        normal_key_map[0x42] = Key::F8;
        normal_key_map[0x43] = Key::F9;
        normal_key_map[0x44] = Key::F10;
        normal_key_map[0x45] = Key::NumLock;
        normal_key_map[0x46] = Key::ScrollLock;
        normal_key_map[0x47] = Key::Num7;
        normal_key_map[0x48] = Key::Num8;
        normal_key_map[0x49] = Key::Num9;
        normal_key_map[0x4A] = Key::NumDash;
        normal_key_map[0x4B] = Key::Num4;
        normal_key_map[0x4C] = Key::Num5;
        normal_key_map[0x4D] = Key::Num6;
        normal_key_map[0x4E] = Key::NumPlus;
        normal_key_map[0x4F] = Key::Num1;
        normal_key_map[0x50] = Key::Num2;
        normal_key_map[0x51] = Key::Num3;
        normal_key_map[0x52] = Key::Num0;
        normal_key_map[0x53] = Key::NumPeriod;
        normal_key_map[0x54] = Key::Unknown; // Key::SYSREQ;
        normal_key_map[0x56] = Key::Unknown; // Key::EUROPE_2;
        normal_key_map[0x57] = Key::F11;
        normal_key_map[0x58] = Key::F12;
        normal_key_map[0x59] = Key::NumEqual;
        normal_key_map[0x5C] = Key::Unknown; // Key::I10L_6;
        normal_key_map[0x64] = Key::F13;
        normal_key_map[0x65] = Key::F14;
        normal_key_map[0x66] = Key::F15;
        normal_key_map[0x67] = Key::F16;
        normal_key_map[0x68] = Key::F17;
        normal_key_map[0x69] = Key::F18;
        normal_key_map[0x6A] = Key::F19;
        normal_key_map[0x6B] = Key::F20;
        normal_key_map[0x6C] = Key::F21;
        normal_key_map[0x6D] = Key::F22;
        normal_key_map[0x6E] = Key::F23;
        normal_key_map[0x70] = Key::Unknown; // Key::I10L_2;

        /* The following two keys (0x71, 0x72) are release-only. */
        normal_key_map[0x71] = Key::Unknown; // Key::LANG_2;
        normal_key_map[0x72] = Key::Unknown; // Key::LANG_1;
        normal_key_map[0x73] = Key::Unknown; // Key::I10L_1;

        /* The following key (0x76) can be either F24 or LANG_5. */
        normal_key_map[0x76] = Key::F24;
        normal_key_map[0x77] = Key::Unknown; // Key::LANG_4;
        normal_key_map[0x78] = Key::Unknown; // Key::LANG_3;
        normal_key_map[0x79] = Key::Unknown; // Key::I10L_4;
        normal_key_map[0x7B] = Key::Unknown; // Key::I10L_5;
        normal_key_map[0x7D] = Key::Unknown; // Key::I10L_3;
        normal_key_map[0x7E] = Key::Unknown; // Key::EQUAL_SIGN;
    }

    void init_extended_key_map() {
        utils::memset(extended_key_map, 0, sizeof(extended_key_map));

        extended_key_map[0x10] = Key::Unknown; // Key::_SCAN_PREVIOUS_TRACK;
        extended_key_map[0x19] = Key::Unknown; // Key::_SCAN_NEXT_TRACK;
        extended_key_map[0x1C] = Key::NumEnter;
        extended_key_map[0x1D] = Key::RightCtrl;
        extended_key_map[0x20] = Key::Unknown; // Key::_MUTE;
        extended_key_map[0x21] = Key::Unknown; // Key::_AL_CALCULATOR;
        extended_key_map[0x22] = Key::Unknown; // Key::_PLAY_PAUSE;
        extended_key_map[0x24] = Key::Unknown; // Key::_STOP;
        extended_key_map[0x2E] = Key::Unknown; // Key::_VOLUME_DOWN;
        extended_key_map[0x30] = Key::Unknown; // Key::_VOLUME_UP;
        extended_key_map[0x32] = Key::Unknown; // Key::_AC_HOME;
        extended_key_map[0x35] = Key::NumSlash;
        extended_key_map[0x37] = Key::PrintScreen;
        extended_key_map[0x38] = Key::RightAlt;
        extended_key_map[0x46] = Key::Pause;
        extended_key_map[0x47] = Key::Home;
        extended_key_map[0x48] = Key::Up;
        extended_key_map[0x49] = Key::PageUp;
        extended_key_map[0x4B] = Key::Left;
        extended_key_map[0x4D] = Key::Right;
        extended_key_map[0x4F] = Key::End;
        extended_key_map[0x50] = Key::Down;
        extended_key_map[0x51] = Key::PageDown;
        extended_key_map[0x52] = Key::Insert;
        extended_key_map[0x53] = Key::Delete;
        extended_key_map[0x5B] = Key::LeftSuper;
        extended_key_map[0x5C] = Key::RightSuper;
        extended_key_map[0x5D] = Key::Unknown; // Key::APPLICATION;

        /* The following extended key (0x5E) may also be INPUT_KEY_POWER. */
        extended_key_map[0x5E] = Key::Unknown; // Key::YSTEM_POWER_DOWN;
        extended_key_map[0x5F] = Key::Unknown; // Key::YSTEM_SLEEP;
        extended_key_map[0x63] = Key::Unknown; // Key::YSTEM_WAKE_UP;
        extended_key_map[0x65] = Key::Unknown; // Key::_AC_SEARCH;
        extended_key_map[0x66] = Key::Unknown; // Key::_AC_BOOKMARKS;
        extended_key_map[0x67] = Key::Unknown; // Key::_AC_REFRESH;
        extended_key_map[0x68] = Key::Unknown; // Key::_AC_STOP;
        extended_key_map[0x69] = Key::Unknown; // Key::_AC_FORWARD;
        extended_key_map[0x6A] = Key::Unknown; // Key::_AC_BACK;
        extended_key_map[0x6B] = Key::Unknown; // Key::_AL_LOCAL_BROWSER;
        extended_key_map[0x6C] = Key::Unknown; // Key::_AL_EMAIL_READER;
        extended_key_map[0x6D] = Key::Unknown; // Key::_AL_MEDIA_SELECT;
    }
} // namespace cosmos::devices::ps2kbd
