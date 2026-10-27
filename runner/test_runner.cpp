#include "libretro.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_W 640
#define MAX_H 480

static uint16_t g_fb[MAX_W * MAX_H];
static unsigned g_fb_w = 320;
static unsigned g_fb_h = 240;
static enum retro_pixel_format g_pix_fmt = RETRO_PIXEL_FORMAT_RGB565;

static uint16_t g_pad = 0;
static int g_frame = 0;
static int g_video_calls = 0;
static int64_t g_audio_frames = 0;
static int64_t g_audio_nonzero = 0;
static std::string g_base_dir = ".";

static void log_cb(enum retro_log_level level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* prefix = "[CORE]";
    if (level == RETRO_LOG_ERROR) prefix = "[CORE ERROR]";
    else if (level == RETRO_LOG_WARN) prefix = "[CORE WARN]";
    printf("%s ", prefix);
    vprintf(fmt, args);
    va_end(args);
}

static void video_cb(const void* data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;
    g_fb_w = width;
    g_fb_h = height;
    if (g_fb_w > MAX_W) g_fb_w = MAX_W;
    if (g_fb_h > MAX_H) g_fb_h = MAX_H;

    const uint8_t* src = static_cast<const uint8_t*>(data);
    if (g_pix_fmt == RETRO_PIXEL_FORMAT_RGB565) {
        for (unsigned y = 0; y < g_fb_h; ++y) {
            std::memcpy(&g_fb[y * g_fb_w], src + y * pitch, g_fb_w * sizeof(uint16_t));
        }
    } else if (g_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
        for (unsigned y = 0; y < g_fb_h; ++y) {
            const uint32_t* src32 = reinterpret_cast<const uint32_t*>(src + y * pitch);
            for (unsigned x = 0; x < g_fb_w; ++x) {
                uint32_t p = src32[x];
                uint16_t r = (p >> 19) & 0x1F;
                uint16_t g = (p >> 10) & 0x3F;
                uint16_t b = (p >> 3) & 0x1F;
                g_fb[y * g_fb_w + x] = (r << 11) | (g << 5) | b;
            }
        }
    } else if (g_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555) {
        for (unsigned y = 0; y < g_fb_h; ++y) {
            const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src + y * pitch);
            for (unsigned x = 0; x < g_fb_w; ++x) {
                uint16_t p = src16[x];
                uint16_t r = (p >> 10) & 0x1F;
                uint16_t g = (p >> 5) & 0x1F;
                uint16_t b = p & 0x1F;
                g_fb[y * g_fb_w + x] = (r << 11) | (g << 6) | b;
            }
        }
    }
    g_video_calls++;
}

static void audio_sample_cb(int16_t /*l*/, int16_t /*r*/) {
}

static size_t audio_batch_cb(const int16_t* data, size_t frames) {
    g_audio_frames += static_cast<int64_t>(frames);
    if (data) {
        for (size_t i = 0; i < frames * 2; ++i) {
            if (data[i] != 0)
                g_audio_nonzero++;
        }
    }
    return frames;
}

static void input_poll_cb(void) {
}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)index;
    if (device == RETRO_DEVICE_JOYPAD) {
        return (g_pad & (1u << id)) ? 1 : 0;
    }
    return 0;
}

static bool environ_cb(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format fmt = *reinterpret_cast<enum retro_pixel_format*>(data);
            if (fmt == RETRO_PIXEL_FORMAT_RGB565 || fmt == RETRO_PIXEL_FORMAT_XRGB8888 || fmt == RETRO_PIXEL_FORMAT_0RGB1555) {
                g_pix_fmt = fmt;
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback* cb = reinterpret_cast<struct retro_log_callback*>(data);
            cb->log = log_cb;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            *reinterpret_cast<const char**>(data) = g_base_dir.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            *reinterpret_cast<bool*>(data) = true;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable* var = reinterpret_cast<struct retro_variable*>(data);
            if (!var || !var->key) return false;
            if (strcmp(var->key, "scummvm_pointer_device") == 0) {
                var->value = "retropad";
                return true;
            }
            if (strcmp(var->key, "scummvm_gamepad_cursor_speed") == 0) {
                var->value = "1.5";
                return true;
            }
            if (strcmp(var->key, "scummvm_gamepad_cursor_acceleration_time") == 0) {
                var->value = "0.05";
                return true;
            }
            if (strcmp(var->key, "scummvm_mapper_up") == 0) { var->value = "RETROKE_UP"; return true; }
            if (strcmp(var->key, "scummvm_mapper_down") == 0) { var->value = "RETROKE_DOWN"; return true; }
            if (strcmp(var->key, "scummvm_mapper_left") == 0) { var->value = "RETROKE_LEFT"; return true; }
            if (strcmp(var->key, "scummvm_mapper_right") == 0) { var->value = "RETROKE_RIGHT"; return true; }
            if (strcmp(var->key, "scummvm_mapper_a") == 0) { var->value = "RETROKE_LEFT_BUTTON"; return true; }
            if (strcmp(var->key, "scummvm_mapper_b") == 0) { var->value = "RETROKE_RIGHT_BUTTON"; return true; }
            if (strcmp(var->key, "scummvm_mapper_x") == 0) { var->value = "RETROK_F5"; return true; }
            if (strcmp(var->key, "scummvm_mapper_y") == 0) { var->value = "RETROK_ESCAPE"; return true; }
            if (strcmp(var->key, "scummvm_mapper_start") == 0) { var->value = "RETROKE_SCUMMVM_GUI"; return true; }
            if (strcmp(var->key, "scummvm_mapper_select") == 0) { var->value = "RETROK_SPACE"; return true; }
            if (strcmp(var->key, "scummvm_video_hw_acceleration") == 0) { var->value = "disabled"; return true; }
            return false;
        }
        case RETRO_ENVIRONMENT_SET_MESSAGE:
        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
            return true;
        default:
            return false;
    }
}

static void save_ppm(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", g_fb_w, g_fb_h);
    std::vector<uint8_t> rgb(g_fb_w * 3);
    for (unsigned y = 0; y < g_fb_h; ++y) {
        for (unsigned x = 0; x < g_fb_w; ++x) {
            uint16_t p = g_fb[y * g_fb_w + x];
            rgb[x * 3 + 0] = ((p >> 11) & 0x1F) << 3;
            rgb[x * 3 + 1] = ((p >> 5) & 0x3F) << 2;
            rgb[x * 3 + 2] = (p & 0x1F) << 3;
        }
        fwrite(rgb.data(), 1, rgb.size(), f);
    }
    fclose(f);
}

static int count_nonblack_pixels() {
    int count = 0;
    int total = g_fb_w * g_fb_h;
    for (int i = 0; i < total; ++i) {
        if (g_fb[i] != 0)
            count++;
    }
    return count;
}

static uint32_t calc_checksum() {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(g_fb);
    size_t sz = g_fb_w * g_fb_h * sizeof(uint16_t);
    for (size_t i = 0; i < sz; ++i) {
        crc ^= p[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <game_path_or_file> [frames=400] [sshot_prefix=screenshots/frame] [mode=tour]\n", argv[0]);
        return 1;
    }

    const char* core_path = argv[1];
    const char* game_path = argv[2];
    int total_frames = (argc > 3) ? std::atoi(argv[3]) : 400;
    const char* sshot_prefix = (argc > 4) ? argv[4] : "screenshots/frame";
    const char* mode = (argc > 5) ? argv[5] : "tour";

    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    printf("=== ScummVM Libretro Test Runner ===\n");
    printf("Core: %s\n", core_path);
    printf("Game: %s\n", game_path);
    printf("Total frames: %d (mode: %s)\n", total_frames, mode);

    void* handle = dlopen(core_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "Failed to load core: %s\n", dlerror());
        return 1;
    }

    auto sym = [handle](const char* name) -> void* {
        void* s = dlsym(handle, name);
        if (!s) fprintf(stderr, "Missing symbol: %s\n", name);
        return s;
    };

    auto retro_set_environment = (void (*)(retro_environment_t))sym("retro_set_environment");
    auto retro_set_video_refresh = (void (*)(retro_video_refresh_t))sym("retro_set_video_refresh");
    auto retro_set_audio_sample = (void (*)(retro_audio_sample_t))sym("retro_set_audio_sample");
    auto retro_set_audio_sample_batch = (void (*)(retro_audio_sample_batch_t))sym("retro_set_audio_sample_batch");
    auto retro_set_input_poll = (void (*)(retro_input_poll_t))sym("retro_set_input_poll");
    auto retro_set_input_state = (void (*)(retro_input_state_t))sym("retro_set_input_state");
    auto retro_init = (void (*)(void))sym("retro_init");
    auto retro_deinit = (void (*)(void))sym("retro_deinit");
    auto retro_load_game = (bool (*)(const struct retro_game_info*))sym("retro_load_game");
    auto retro_unload_game = (void (*)(void))sym("retro_unload_game");
    auto retro_run = (void (*)(void))sym("retro_run");

    if (!retro_set_environment || !retro_load_game || !retro_run) {
        fprintf(stderr, "Essential symbols missing!\n");
        return 1;
    }

    // Determine base dir
    std::string gp = game_path;
    size_t last_slash = gp.find_last_of('/');
    if (last_slash != std::string::npos) {
        g_base_dir = gp.substr(0, last_slash);
    }

    retro_set_environment(environ_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);

    printf("Calling retro_init()...\n");
    retro_init();

    struct retro_game_info info;
    info.path = game_path;
    info.data = nullptr;
    info.size = 0;
    info.meta = nullptr;

    printf("Loading game: %s...\n", game_path);
    if (!retro_load_game(&info)) {
        fprintf(stderr, "retro_load_game failed!\n");
        return 1;
    }
    printf("Game loaded successfully!\n");

    mkdir("screenshots", 0777);

    for (g_frame = 0; g_frame < total_frames; ++g_frame) {
        g_pad = 0;

        // Scripted tour:
        // Frame 0-60: Boot / splash idle
        // Frame 61-90: Press Space/A (skip title / text)
        // Frame 91-180: Move D-Pad Right & Down (moving mouse pointer)
        // Frame 181-220: Press A (Left mouse click)
        // Frame 221-300: Move D-Pad Left & Up (moving mouse pointer)
        // Frame 301-340: Press A (Left mouse click)
        // Frame 341-450: Move around
        // Pulse Space, Return, and Left Click periodically to advance through intro into gameplay
        if (g_frame >= 40 && (g_frame % 25 < 6)) {
            g_pad |= (1u << RETRO_DEVICE_ID_JOYPAD_SELECT) | (1u << RETRO_DEVICE_ID_JOYPAD_A) | (1u << RETRO_DEVICE_ID_JOYPAD_B);
        }
        if (g_frame > 200) {
            // Move mouse cursor around with D-Pad
            if ((g_frame / 60) % 2 == 0)
                g_pad |= (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1u << RETRO_DEVICE_ID_JOYPAD_DOWN);
            else
                g_pad |= (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1u << RETRO_DEVICE_ID_JOYPAD_UP);
        }

        retro_run();

        // Save screenshots at checkpoints
        struct Shot { int frame; const char* name; };
        const Shot shots[] = {
            { 40, "01_boot" },
            { 120, "02_cursor_moved" },
            { 240, "03_clicked_action" },
            { 360, "04_gameplay" },
            { total_frames - 1, "05_final" }
        };

        for (const auto& s : shots) {
            if (g_frame == s.frame) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s_%s.ppm", sshot_prefix, s.name);
                save_ppm(buf);
                printf("[Frame %4d] Saved %s (%dx%d, nonblack: %d, crc: 0x%08X)\n",
                       g_frame, buf, g_fb_w, g_fb_h, count_nonblack_pixels(), calc_checksum());
            }
        }
    }

    printf("\n=== Execution Summary ===\n");
    printf("Total frames rendered: %d\n", g_frame);
    printf("Video refresh calls: %d\n", g_video_calls);
    printf("Audio frames generated: %lld (nonzero: %lld)\n",
           (long long)g_audio_frames, (long long)g_audio_nonzero);
    printf("Resolution: %dx%d\n", g_fb_w, g_fb_h);
    printf("Final nonblack pixels: %d / %d\n", count_nonblack_pixels(), g_fb_w * g_fb_h);
    printf("Final CRC32: 0x%08X\n", calc_checksum());

    printf("Unloading game...\n");
    retro_unload_game();
    retro_deinit();
    dlclose(handle);

    printf("Done!\n");
    return 0;
}
