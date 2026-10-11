/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* BELL: a soft two-operator FM bell (Pelucco). A sine carrier phase-modulated by a sine whose pitch is a fixed
 * RATIO of the note's; the modulator's index (TONE) is what makes it a bell: a high one is a metallic clang, a low
 * one a round, nearly pure tone, and ENV lets the index follow the envelope, so a strike is bright and the tone
 * mellows as it rings out. DTN adds a second carrier a few cents off (the same modulator), which beats slowly
 * against the first: the shimmer of a real bell. The amplitude is the voice's ADSR: a bell is a long DECAY with
 * SUSTAIN 0.
 * GENERATIVE: with DENS above 0 the track stops reading its steps and plays four loops of unequal length (13, 17, 23
 * and 29 steps: seq.c gen_step) that drift against each other and repeat only every 148,000 steps. DENS is how many of
 * their slots sound, EVOLVE how fast the loops change as they come round, RANGE the octaves they roam, SEED which set
 * of loops: the same SEED is the same bells. The notes are snapped to the track's ROOT and SCALE.
 * The idea of this bell (an inharmonic ratio, an index that falls with the note) is AMB-1's, Charles Vestal,
 * GPL-3.0 (github.com/charlesvestal/fm1-amb); this is a fixed-point rewrite for Felucca's voices. */
static const char *const N_BL_RANGE[] = {"1 OCT", "2 OCT", "3 OCT", "4 OCT"};
static const char *const N_BL_RATIO[] = {"1", "1.5", "2", "2.76", "3", "3.5", "4.77"};
static const uint16_t BL_RATIO_Q8[7] = {256, 384, 512, 707, 768, 896, 1221};   /* x256 */

static void bell_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0;                /* carrier, modulator, second carrier: every strike starts the same */
    v->ph[1] = 0;
    v->ph[2] = 0;
}

static void bell_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t ratio = (uint32_t)p[P_E0] % 7u, i, k;
    int32_t det = p[P_E3], tone;
    uint32_t inc = m->inc, minc = (inc >> 8) * BL_RATIO_Q8[ratio], inc2 = det ? cents_inc(m->pitch16, det, m->fine) : 0;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    /* the index: TONE, the matrix's CUT / SHP, and ENV of the envelope on top (as PHASE's DCW) */
    tone = (p[P_E1] << 8) + m->cutoff + mulq15(m->envq15, p[P_E2] * 256) + (m->shape - (64 << 8));
    tone = clamp(tone, 0, 127 << 8);
    /* tone / 16 = the index in 1/256 radian (7.9 rad at the top); a phase offset of s (Q15) * index / 2pi turns is
     * s * (index / 256) * 81.5 in 1 / 2^32 turn: unsigned, so it wraps as a phase does */
    k = (uint32_t)(tone >> 4) * 81u;
    for (i = 0; i < n; i++) {
        uint32_t pm = (uint32_t)osc_sine(ph1) * k;
        int32_t s = osc_sine(ph0 + pm);
        if (det) {
            s = (s + osc_sine(ph2 + pm)) >> 1;
            ph2 += inc2;
        }
        ph0 += inc;
        ph1 += minc;
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
}

static const preset_t BELL_PRESETS[] = {
    /* name, {RATIO, TONE, ENV, DTN, -, -, -, -}, {A D S R}, fenv, mono */
    {"BELL", {4, 40, 60, 6, 0, 0, 0, 0}, {0, 100, 0, 80}, 0, 0, FX(0, 20, 30, 70), PAT(7)},
    {"GLASS", {6, 55, 90, 10, 0, 0, 0, 0}, {0, 90, 0, 60}, 0, 0, FX(0, 10, 30, 75), PAT(7)},
    {"CHIME", {5, 70, 80, 14, 0, 0, 0, 0}, {0, 105, 0, 90}, 0, 0, FX(0, 25, 35, 75), PAT(7)},
    {"MUSBOX", {2, 30, 100, 0, 0, 0, 0, 0}, {0, 70, 0, 40}, 0, 0, FX(0, 0, 40, 60), PAT(3)},
    {"TUBULAR", {1, 85, 70, 5, 0, 0, 0, 0}, {0, 120, 0, 110}, 0, 0, FX(0, 15, 25, 80), PAT(7)},
    {"GONG", {3, 95, 60, 20, 0, 0, 0, 0}, {2, 125, 0, 120}, 0, 0, FX(10, 20, 20, 90), PAT(8)},
    {"SHIMMER", {4, 25, 40, 25, 0, 0, 0, 0}, {0, 110, 0, 100}, 0, 0, FX(0, 40, 40, 85), PAT(5)},
    {"PURE", {0, 20, 50, 4, 0, 0, 0, 0}, {0, 90, 0, 70}, 0, 0, FX(0, 10, 30, 60), PAT(3)},
    /* generative: DENS EVOLVE RANGE SEED (a long AMBI reverb and a slow DIV make them) */
    {"RAIN", {4, 40, 60, 6, 30, 25, 1, 3}, {0, 100, 0, 90}, 0, 0, FX(0, 20, 45, 85), PAT(7)},
    {"TEMPLE", {1, 85, 70, 5, 18, 15, 2, 11}, {0, 120, 0, 115}, 0, 0, FX(0, 15, 40, 95), PAT(8)},
    {"SNOWFALL", {6, 55, 90, 10, 45, 40, 1, 27}, {0, 90, 0, 75}, 0, 0, FX(0, 10, 50, 90), PAT(5)},
};

static const engine_t ENG_BELL = {
    .name = "BELL",
    .page_title = {"BELL", "GEN"},
    .edit = {
        {"RATIO", F_ENUM, 0, 6, 4, N_BL_RATIO, 0},
        {"TONE", F_PCT, 0, 127, 40, 0, 0},
        {"ENV", F_PCT, 0, 127, 60, 0, 0},
        {"DTN", F_INT, 0, 50, 6, 0, "ct"},
        {"DENS", F_INT, 0, 100, 0, 0, "%"},
        {"EVOLVE", F_INT, 0, 100, 20, 0, "%"},
        {"RANGE", F_ENUM, 0, 3, 1, N_BL_RANGE, 0},
        {"SEED", F_INT, 0, 127, 0, 0, 0},
    },
    .presets = BELL_PRESETS,
    .npresets = NELEM(BELL_PRESETS),
    .note_on = bell_note_on,
    .render = bell_render,
    .knob = {P_E1, P_E4, P_E5, P_REL},
};
