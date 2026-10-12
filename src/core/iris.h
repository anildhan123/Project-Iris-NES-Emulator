#ifndef IRIS_H
#define IRIS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NES core. One caller thread owns a handle. Pointers from iris_frame and iris_audio
   stay valid until the next load, unload, run, or destroy on that handle. */
typedef struct IrisSystem IrisSystem;

IrisSystem* iris_create(void);
void iris_destroy(IrisSystem* system);

int iris_load_bytes(
    IrisSystem* system,
    const uint8_t* data,
    size_t size,
    const char* name,
    char* error,
    size_t error_size);
/* path uses the host filesystem encoding. */
int iris_load_file(IrisSystem* system, const char* path, char* error, size_t error_size);
void iris_unload(IrisSystem* system);
int iris_loaded(const IrisSystem* system);
void iris_reset(IrisSystem* system);
void iris_set_paused(IrisSystem* system, int paused);
void iris_run_frame(IrisSystem* system);
/* A 0x01, B 0x02, Select 0x04, Start 0x08, Up 0x10, Down 0x20, Left 0x40, Right 0x80. */
void iris_set_controller(IrisSystem* system, int port, uint8_t buttons);

const uint8_t* iris_frame(const IrisSystem* system, int* width, int* height);
const float* iris_audio(const IrisSystem* system, int* count, int* sample_rate);
int64_t iris_frame_period_ns(const IrisSystem* system);

/* out receives a 64-character hex id and a null terminator. Returns 0 when the buffer is shorter than 65. */
int iris_content_id(const IrisSystem* system, char* out, size_t out_size);

/* Pass a null buffer to read the required size. Returns 0 when the buffer is too small or the state is refused. */
int iris_save_state(
    IrisSystem* system,
    uint8_t* out,
    size_t* size,
    const char* description,
    char* error,
    size_t error_size);
int iris_load_state(IrisSystem* system, const uint8_t* data, size_t size, char* error, size_t error_size);

/* Same size rules as iris_save_state. Returns 0 when the cartridge has no battery-backed RAM. */
int iris_copy_battery(const IrisSystem* system, uint8_t* out, size_t* size, char* error, size_t error_size);
int iris_restore_battery(IrisSystem* system, const uint8_t* data, size_t size, char* error, size_t error_size);

const char* iris_version(void);

#ifdef __cplusplus
}
#endif

#endif
