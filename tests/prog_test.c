/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The Pad's chord progression (Pelucco; seq.c prog_step): with DENS above 0 track 1 plays I, IV, V and vi of its ROOT /
 * SCALE by itself instead of its steps. Checks: DENS 0 plays the steps as ever; the chords are the diatonic triads of the
 * scale (major and minor), only on I IV V vi, I the most often; each is struck once and held for BARS x 16 steps; DENS
 * thins the bars; the same chord is seldom repeated. Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define NSTEPS 2000u
#define CHK(c) do { if (!(c)) { printf("prog: FAIL line %d: %s\n", __LINE__, #c); bad++; } } while (0)

static uint8_t at_step[NSTEPS], cn[NSTEPS][4];       /* per step: notes started on it (count, the notes) */
static uint32_t play(uint32_t dens, uint32_t bars, uint32_t scale, uint32_t root)
{
    track_t *t;
    uint32_t b, steps = 0, tot = 0;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->engine = t->eng_req = 0;
    t->p[P_VOICE] = V_POLY; t->p[P_VOIC] = VC_CLOSE; t->p[P_CHRD] = 0;
    t->p[P_SLEN] = 16; t->p[P_SGATE] = 90; t->p[P_SSWING] = 0;
    t->p[P_PDEN] = (int16_t)dens; t->p[P_PLEN] = (int16_t)bars;
    t->p[P_SCALE] = (int16_t)scale; t->p[P_ROOT] = (int16_t)root; t->p[P_TRANS] = 0;
    song.octave = 0;
    memset(at_step, 0, sizeof at_step); memset(cn, 0, sizeof cn);
    seq_start();
    for (b = 0; steps < NSTEPS && b < 40000000u; b++) {
        uint32_t v0 = vage, before = prog_c[0];
        seq_tick(t, CTL);
        if (prog_c[0] != before && steps < NSTEPS) steps++;
        if (v0 != vage && steps && steps <= NSTEPS) {
            uint32_t i;
            at_step[steps - 1] = (uint8_t)t->seq_n;
            for (i = 0; i < t->seq_n && i < 4u; i++) cn[steps - 1][i] = t->seq_notes[i];
            tot += t->seq_n;
        }
    }
    return tot;
}
static uint32_t chords(void) { uint32_t i, n = 0; for (i = 0; i < NSTEPS; i++) n += at_step[i] != 0; return n; }
static uint32_t pcset(uint32_t i) { uint32_t k, m = 0; for (k = 0; k < at_step[i]; k++) m |= 1u << (cn[i][k] % 12u); return m; }

int main(void)
{
    int bad = 0;
    uint32_t i, n, deg[4] = {0, 0, 0, 0}, rep = 0, prev = 99, last = 0;
    /* the diatonic triads as pitch-class sets: C major I IV V vi, D minor i iv v VI */
    static const uint32_t MAJ[4] = {1u | 1u << 4 | 1u << 7, 1u << 5 | 1u << 9 | 1u, 1u << 7 | 1u << 11 | 1u << 2, 1u << 9 | 1u | 1u << 4};
    static const uint32_t MIN[4] = {1u << 2 | 1u << 5 | 1u << 9, 1u << 7 | 1u << 10 | 1u << 2, 1u << 9 | 1u << 0 | 1u << 4, 1u << 10 | 1u << 2 | 1u << 5};
    n = play(0, 1, 1, 0);
    CHK(n == 0);                                         /* DENS 0: the empty steps, nothing */
    play(100, 1, 1, 0);
    CHK(chords() >= NSTEPS / 16u - 1u);                  /* a chord every bar */
    for (i = 0; i < NSTEPS; i++) {
        if (!at_step[i]) continue;
        CHK(i % 16u == 0 && at_step[i] == 3u);           /* struck on a bar, a triad */
        {
            uint32_t s = pcset(i), d;
            for (d = 0; d < 4u && s != MAJ[d]; d++) ;
            CHK(d < 4u);                                 /* one of I IV V vi of C major */
            if (d < 4u) { deg[d]++; if (d == prev) rep++; prev = d; }
        }
    }
    CHK(deg[0] > deg[1] && deg[1] > deg[3] && deg[2] > deg[3] && deg[3] > 5u);   /* I most, all four heard, vi the least */
    printf("prog: chords %u I %u IV %u V %u vi %u repeats %u\n", chords(), deg[0], deg[1], deg[2], deg[3], rep);
    CHK(rep < chords() / 5u);                            /* seldom the same chord twice */
    play(100, 2, 1, 0);
    for (i = 0; i < NSTEPS; i++) if (at_step[i]) { CHK(i % 32u == 0); last = i; }
    CHK(chords() >= NSTEPS / 32u - 1u && last > 0);      /* BARS 2: a chord every 32 steps */
    play(50, 1, 1, 0);
    n = chords();
    CHK(n > NSTEPS / 16u / 4u && n < NSTEPS / 16u * 3u / 4u);   /* DENS 50: about half the bars */
    play(100, 1, 2, 2);                                  /* D natural minor */
    for (i = 0; i < NSTEPS; i++) {
        uint32_t s, d;
        if (!at_step[i]) continue;
        s = pcset(i);
        for (d = 0; d < 4u && s != MIN[d]; d++) ;
        CHK(d < 4u);
    }
    if (bad) { printf("PROG TEST FAILED\n"); return 1; }
    printf("progression test passed\n");
    return 0;
}
