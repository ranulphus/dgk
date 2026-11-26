// midi_ymfm.cpp - dgk/midi.h's FM chip off DOS: an OPL3 (YMF262) emulated by ymfm (third_party/ymfm, BSD-3),
// written by GLOS's FM driver (midi.c) and mixed by dgk (dgk_mix_stream). The driver's writes are kept in the
// game's thread and handed over in one go (dgk__ymfm_commit) to the audio thread, which applies them before the
// samples it makes next; the chip's 49,716 Hz is taken to the mix's rate by straight-line interpolation.
#include "ymfm_opl.h"

extern "C" {
#include "dgk/mix.h"
int dgk__ymfm_open(void);
void dgk__ymfm_write(unsigned short reg, unsigned char v);
void dgk__ymfm_commit(void);
void dgk__ymfm_close(void);
}

namespace {

struct chip_if : ymfm::ymfm_interface {};

struct write_t {
    uint16_t reg;
    uint8_t v;
};

const uint32_t OPL3_CLOCK = 14318181;           // a Sound Blaster's: 49,716 samples a second
const unsigned RING = 16384;                    // writes handed to the audio thread, not yet applied

chip_if intf;
ymfm::ymf262 *chip;
write_t pending[RING];                          // the game's thread's, since the last commit
unsigned npending;
write_t ring[RING];                             // the audio thread's (under the mix's lock)
unsigned rhead, rtail;
uint32_t step;                                  // the chip's samples to one of the mix's, 16.16
uint32_t phase;
int32_t prev, cur;

void apply(const write_t &w)
{
    if (w.reg & 0x100)
        chip->write_address_hi((uint8_t)w.reg);
    else
        chip->write_address((uint8_t)w.reg);
    chip->write_data(w.v);
}

int32_t next_sample()
{
    ymfm::ymf262::output_data o;
    chip->generate(&o, 1);
    return (o.data[0] + o.data[1]) / 2;         // the OPL3's left and right (the driver sends to both)
}

void render(int32_t *acc, int frames, void *)
{
    if (!chip)
        return;
    while (rhead != rtail) {
        apply(ring[rhead]);
        rhead = (rhead + 1) % RING;
    }
    for (int f = 0; f < frames; f++) {
        while (phase >= 65536) {
            prev = cur;
            cur = next_sample();
            phase -= 65536;
        }
        acc[f] += prev + (int32_t)(((int64_t)(cur - prev) * (int32_t)phase) >> 16);
        phase += step;
    }
}

}

extern "C" int dgk__ymfm_open(void)
{
    if (!chip) {
        chip = new ymfm::ymf262(intf);
        chip->reset();
    }
    step = (uint32_t)(((uint64_t)chip->sample_rate(OPL3_CLOCK) << 16) / dgk_mix_rate());
    phase = 0, prev = cur = 0, npending = 0, rhead = rtail = 0;
    dgk_mix_stream(render, nullptr);
    return 1;
}

extern "C" void dgk__ymfm_write(unsigned short reg, unsigned char v)
{
    if (npending < RING)
        pending[npending++] = write_t{ reg, v };
}

extern "C" void dgk__ymfm_commit(void)
{
    if (!npending)
        return;
    dgk_mix_lock();
    for (unsigned i = 0; i < npending && (rtail + 1) % RING != rhead; i++) {
        ring[rtail] = pending[i];
        rtail = (rtail + 1) % RING;
    }
    dgk_mix_unlock();
    npending = 0;
}

extern "C" void dgk__ymfm_close(void)
{
    dgk_mix_stream(nullptr, nullptr);
    dgk_mix_lock();
    while (rhead != rtail) {                    // (the driver's last words: every key off)
        apply(ring[rhead]);
        rhead = (rhead + 1) % RING;
    }
    dgk_mix_unlock();
}
