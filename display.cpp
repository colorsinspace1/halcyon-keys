#include "display.h"

// Fog lake: a sun low behind bands of mist over still water. Each note drops
// a ripple whose rings spread across the water and fade, the late octaves'
// rings following fainter. Warmth warms the sun, Drift moves the mist and the
// reflection, Swell slows the rings and the sun's glow. Page 2: Hiss thickens
// the mist, Chord adds rings, Ensemble doubles the reflection, Age scatters
// dust on the water. Still when nothing plays.
namespace hkeys {
namespace {
using opxy::i32;
using opxy::u32;

// Ring brightness from newest to oldest: palette entries, white down to the faintest grey.
const u32 ring_shades[7] = {7, 6, 5, 4, 3, 2, 1};
const i32 lane_x[5] = {96, 152, 208, 264, 392};  // the widest ring stays on screen
const i32 lane_y[3] = {168, 180, 192};
const i32 mist_x[4] = {348, 296, 376, 252}, mist_y[4] = {105, 113, 121, 129};
const i32 mist_period[4] = {18000, 23000, 15000, 27000};  // milliseconds per sway
const u32 reflection_shades[6] = {5, 4, 4, 3, 3, 2};
const i32 sun_x = 336, sun_y = 84;
// Dust on the water: fixed specks, filled in this order as Age rises, clear of the reflection.
const i32 dust_x[12] = {58, 412, 186, 268, 452, 120, 228, 376, 30, 290, 158, 436};
const i32 dust_y[12] = {150, 162, 206, 146, 196, 184, 172, 212, 202, 190, 158, 144};

inline u32 next_random(DisplayState& s) {
    u32 x = s.random ? s.random : 0x2545f491u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s.random = x;
    return x;
}

void drop(DisplayState& s, i32 swell, i32 air, i32 chord) {
    // The free slot, else the oldest ripple.
    u32 slot = 0;
    i32 oldest = -2;
    for (u32 i = 0; i < ripple_count; ++i) {
        if (s.ripples[i].age < 0) { slot = i; break; }
        if (s.ripples[i].age > oldest) { oldest = s.ripples[i].age; slot = i; }
    }
    // The lane farthest from the ripples still spreading, so rings never pile up;
    // a little chance decides between lanes that are nearly as clear.
    u32 best = 0;
    i32 clearest = -1;
    const u32 jitter = next_random(s);
    for (u32 lane = 0; lane < 5u; ++lane) {
        i32 room = 1000;
        for (u32 i = 0; i < ripple_count; ++i) {
            if (i == slot || s.ripples[i].age < 0) continue;
            const i32 d = lane_x[lane] - s.ripples[i].x;
            const i32 gap = d < 0 ? -d : d;
            if (gap < room) room = gap;
        }
        if (lane == s.lane) room -= 40;                     // not the same spot twice running
        room += static_cast<i32>((jitter >> (lane * 4)) & 15u);
        if (room > clearest) { clearest = room; best = lane; }
    }
    s.lane = best;
    Ripple& r = s.ripples[slot];
    r.x = lane_x[s.lane] + static_cast<i32>(next_random(s) % 9u) - 4;
    r.y = lane_y[next_random(s) % 3u];
    r.age = 0;
    r.speed = 40 - (26 * swell >> 10);              // 40 px/s, or 14 at the slowest Swell
    r.wait = 150 + (1200 * swell >> 10);            // as the sound's late octaves
    r.octaves = air <= 0 ? 0u : air < 512 ? 1u : 2u;
    r.chords = chord <= 0 ? 0u : chord < 512 ? 1u : 2u;
}
}

inline void display_reset(DisplayState& state) {
    state.header.reset();
    state.notes.reset();
    state.serial = 0;
    state.random = 0x6d2b79f5u;
    state.lane = 0;
    state.drawn = 0;
    state.activity = 0;
    state.motion = 0;
    for (u32 i = 0; i < ripple_count; ++i) state.ripples[i].age = -1;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    const bool fresh = !state.drawn || state.notes.track != input.track;
    state.notes.update(input);
    const bool still = input.reduced_motion != 0;
    const bool held = input.held_notes != 0 && input.held_notes <= 128;
    const i32 ms = clamp(input.elapsed, 0, 102) * 1000 >> 10;
    // Halcyon Keys' slots: Root, Scale, Warmth, Swell, Drift, Air, Chord, Age;
    // Hiss and Ensemble are fixed (engine.cpp).
    const i32 warmth = input.slots[2] < 0 ? 0 : input.slots[2] >> 21;   // Q10
    const i32 swell = input.slots[3] < 0 ? 0 : input.slots[3] >> 21;
    const i32 drift = input.slots[4] < 0 ? 0 : input.slots[4] >> 21;
    const i32 air = input.slots[5] < 0 ? 0 : input.slots[5] >> 21;
    const i32 hiss = 102;
    const i32 chord = 0;                                             // Halcyon's Chord is fixed off
    const i32 ensemble = 256;
    const i32 age = input.slots[7] < 0 ? 0 : input.slots[7] >> 21;

    // The two-page header: the key, then the sound's amounts, 0 to 100.
    const char* const titles[8] = {"Root", "Scale", "Warmth", "Swell", "Drift", "Air", "Progress", "Age"};
    const char* const roots[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    const char* const scales[8] = {"min", "dor", "harm", "phry", "lyd", "maj", "Mpen", "mpen"};
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    const u32 root = input.slots[0] <= 0 ? 0u : ((static_cast<u32>(input.slots[0]) >> 16) * 12u) >> 15;
    const u32 scale = input.slots[1] <= 0 ? 0u : ((static_cast<u32>(input.slots[1]) >> 16) * 8u) >> 15;
    copy(values[0], header_capacity, roots[root > 11 ? 11 : root]);
    copy(values[1], header_capacity, scales[scale > 7 ? 7 : scale]);
    // Progress: off, or how many seconds each chord lasts ("12s"), as engine.cpp.
    const u32 walk = input.slots[6] <= 0 ? 0u : static_cast<u32>(input.slots[6]) >> 16;
    if (walk < 1638u) copy(values[6], header_capacity, "off");
    else {
        const u32 seconds = 30u - (walk - 1638u) * 26u / (32767u - 1638u);
        char* t = values[6];
        u32 at = 0;
        if (seconds >= 10) t[at++] = static_cast<char>('0' + seconds / 10);
        t[at++] = static_cast<char>('0' + seconds % 10); t[at++] = 's'; t[at] = 0;
    }
    page_header(state.header, input, frame, titles, values, node_header);

    // New notes drop ripples (at most two a frame; a chord is one gesture).
    u32 fallen = input.note_serial - state.serial;
    state.serial = input.note_serial;
    if (fresh || still) {
        fallen = 0;
        for (u32 i = 0; i < ripple_count; ++i) state.ripples[i].age = -1;
    }
    for (u32 n = 0; n < fallen && n < 2u; ++n) drop(state, swell, air, chord);

    // The glow rises while notes are held, as slowly as Swell; it fades over a second after.
    const i32 target = held ? unit : 0;
    if (still) {
        state.activity = target;
    } else if (state.activity != target) {
        i32 per_second = held ? 20480 >> (swell * 6 >> 10) : 900;      // Q10 per second
        i32 k = clamp(ms * per_second / 1000, 1, unit);
        state.activity += (target - state.activity) * k >> 10;
        if (!held && state.activity < 8) state.activity = 0;
        if (held && unit - state.activity < 8) state.activity = unit;
    }

    // Ripples age; a ripple is done once its last ring has spread past the edge.
    bool rippling = false;
    bool used[ripple_count * ring_sizes] = {};
    // How many newer ripples each one has: an older ripple draws a step fainter for each.
    u32 newer[ripple_count] = {};
    for (u32 i = 0; i < ripple_count; ++i)
        for (u32 j = 0; j < ripple_count; ++j)
            if (state.ripples[i].age >= 0 && state.ripples[j].age >= 0 && state.ripples[j].age < state.ripples[i].age) ++newer[i];
    for (u32 i = 0; i < ripple_count; ++i) {
        Ripple& r = state.ripples[i];
        if (r.age < 0) continue;
        r.age += ms;
        const i32 gap = 12000 / r.speed;                                // 12 px between rings
        // The chord's rings follow the note's; the octaves' rings at least a gap behind them all.
        const i32 last = (2 + static_cast<i32>(r.chords)) * gap;
        const i32 up = r.wait > last + gap ? r.wait : last + gap;
        const i32 down = r.wait * 3 / 2 > up + gap ? r.wait * 3 / 2 : up + gap;
        i32 offsets[7];
        u32 kinds[7];                                                 // 0: the note's, 1: a chord tone's, 2: an octave's
        u32 fronts = 0;
        for (u32 k = 0; k < 3u + r.chords; ++k) { offsets[fronts] = static_cast<i32>(k) * gap; kinds[fronts++] = k < 3u ? 0u : 1u; }
        if (r.octaves > 0) { offsets[fronts] = up; kinds[fronts++] = 2u; }
        if (r.octaves > 1) { offsets[fronts] = down; kinds[fronts++] = 2u; }
        bool alive = false;
        for (u32 f = 0; f < fronts; ++f) {
            const i32 a = r.age - offsets[f];
            if (a < 0) { alive = true; continue; }
            const i32 radius = 6 * 256 + r.speed * a * 256 / 1000;      // Q8 pixels: each ring starts 6 px wide
            u32 size = static_cast<u32>(radius >> 10);                    // ring k covers 4k .. 4k+4 px
            if (size >= ring_sizes) continue;
            alive = true;
            if (size < 1) size = 1;
            const u32 node_index = i * ring_sizes + size - 1;
            if (used[node_index]) continue;
            used[node_index] = true;
            const i32 scale = clamp(radius / static_cast<i32>(size), 96, 2 * unit);  // Q10: radius / (4 size)
            const i32 percent = radius * 100 / (84 * 256);
            u32 step = percent < 10 ? 0u : percent < 22 ? 1u : percent < 36 ? 2u : percent < 52 ? 3u
                     : percent < 68 ? 4u : percent < 84 ? 5u : 6u;
            step += kinds[f];                                             // chord tones a step fainter, octaves two
            step += newer[i];                                             // older ripples step back
            if (step > 6u) step = 6u;
            place(frame, node_ring + node_index, r.x * unit, r.y * unit, scale, ring_shades[step], stroke);
        }
        if (!alive) r.age = -1;
        rippling = rippling || alive;
    }
    for (u32 n = 0; n < ripple_count * ring_sizes; ++n)
        if (!used[n]) hide(frame, node_ring + n);

    // Mist and reflection move only while the lake is stirred.
    if (!still && (state.activity || rippling)) state.motion = (state.motion + ms) % 37260000;
    const i32 sway = 6 + (26 * drift >> 10);                            // pixels
    for (u32 b = 0; b < mist_count; ++b) {
        const i32 turn = (state.motion % mist_period[b]) * 3600 / mist_period[b] + 900 * static_cast<i32>(b);
        const i32 x = mist_x[b] * unit + sway * sine(turn);
        place(frame, node_mist + b, x, mist_y[b] * unit, unit, hiss > 512 ? 3u : 2u, fill_and_stroke);
    }
    const i32 shimmer = 3 * drift;                                      // Q10 pixels, up to 3
    for (u32 b = 0; b < reflection_count; ++b) {
        const i32 turn = (state.motion % 7000) * 3600 / 7000 + 600 * static_cast<i32>(b);
        const i32 x = sun_x * unit + (shimmer * sine(turn) >> 10);
        place(frame, node_reflection + b, x, (142 + 4 * static_cast<i32>(b)) * unit + unit / 2, unit,
              reflection_shades[b], stroke);
        // Ensemble: a second reflection beside the first, moving the other way.
        if (ensemble > 0) {
            const i32 apart = (6 + (12 * ensemble >> 10)) * unit;                // 6 to 18 px beside the first
            place(frame, node_double + b, x + apart - (shimmer * sine(turn) >> 9), (142 + 4 * static_cast<i32>(b)) * unit + unit / 2,
                  unit, reflection_shades[b] > 3u ? reflection_shades[b] - 2u : 1u, stroke);
        } else {
            hide(frame, node_double + b);
        }
    }

    // Age: dust settles on the water, more of it as Age rises.
    const u32 specks = static_cast<u32>(age * static_cast<i32>(dust_count) >> 10);
    for (u32 d = 0; d < dust_count; ++d) {
        if (d < specks) place(frame, node_dust + d, dust_x[d] * unit, dust_y[d] * unit + unit / 2, unit, d & 1u ? 4u : 5u, stroke);
        else hide(frame, node_dust + d);
    }

    // The sun: Warmth sets its tone; the glow brightens its rim and halos while notes sound.
    u32 disc = 8u + static_cast<u32>(warmth * 3 >> 10) + (state.activity > 512 ? 1u : 0u);
    if (disc > 11u) disc = 11u;
    const bool glowing = state.activity > 256;
    place(frame, node_halo, sun_x * unit, sun_y * unit, unit, glowing ? 2u : 1u, stroke);
    place(frame, node_halo + 1, sun_x * unit, sun_y * unit, unit, glowing ? 3u : 2u, stroke);
    place(frame, node_disc, sun_x * unit, sun_y * unit, unit, disc, fill_and_stroke);
    state.drawn = 1;
}
}
