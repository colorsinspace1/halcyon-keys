#include "core.h"
#include "halcyon_engine.h"
#include <opxy/q31.hpp>

// Halcyon Keys: Halcyon, played in a scale. The white keys are the scale: from
// middle C, each white key is the next note of the chosen Scale from the chosen
// Root (pentatonic scales, five notes an octave, climb a little faster than the
// keyboard). The ten black keys of each two octaves are ten songs: holding one
// plays its own chord progression, four to eight chords long, built by rules
// from the scale's own notes, different for every Root and Scale (960 songs),
// all in the same register. Each chord is a low
// root with its third, fifth and ninth above (the octave where the ninth would
// be flat), voiced close to the key;
// a chord the scale makes diminished gives way to the chord a third below.
// Progress sets how long each chord lasts, from 30 s down to 4 s (off: the
// first chord only). Each key is its own note, so the OP-XY's envelopes,
// filter and LFO work on every note as usual.
// Page 1: Root, Scale, Warmth, Swell. Page 2: Drift, Air, Progress, Age.
// Hiss, Ensemble and Halcyon's Chord are fixed.
namespace hkeys {
namespace {
using opxy::i32;
using opxy::u32;

const u32 scale_count = 8;
const u32 scale_size[scale_count] = {7, 7, 7, 7, 7, 7, 5, 5};
const u32 scale_notes[scale_count][7] = {
    {0, 2, 3, 5, 7, 8, 10},   // minor
    {0, 2, 3, 5, 7, 9, 10},   // dorian
    {0, 2, 3, 5, 7, 8, 11},   // harmonic minor
    {0, 1, 3, 5, 7, 8, 10},   // phrygian
    {0, 2, 4, 6, 7, 9, 11},   // lydian
    {0, 2, 4, 5, 7, 9, 11},   // major
    {0, 2, 4, 7, 9, 0, 0},    // major pentatonic
    {0, 3, 5, 7, 10, 0, 0},   // minor pentatonic
};
// Each white key's place in the octave, and each black key's (-1: a white key).
const i32 white[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
const i32 black[12] = {-1, 0, -1, 1, -1, -1, 2, -1, 3, -1, 4, -1};
// Each scale's colour: the chords it leans on besides home, and the chords a
// song ends on to lead back home (as degrees: in minor 0 is i, 5 VI, 6 VII).
const i32 colour[scale_count][3] = {
    {5, 6, 3},   // minor: VI, VII, iv
    {3, 6, 2},   // dorian: its bright IV, VII, III
    {5, 3, 6},   // harmonic minor: VI, iv, and VII (taken as VI where it is diminished)
    {1, 6, 3},   // phrygian: the dark II a half step up, vii, iv
    {1, 4, 5},   // lydian: its floating II, V, vi
    {3, 5, 4},   // major: IV, vi, V
    {1, 3, 4},   // major pentatonic
    {2, 3, 4},   // minor pentatonic
};
const i32 ending[scale_count][2] = {{6, 3}, {3, 6}, {4, 3}, {1, 6}, {1, 4}, {4, 3}, {4, 3}, {4, 3}};
// A chord's notes ring out when it lets go, 60 dB down over half the chord's
// length (at least 2 s), per block, Q31, by seconds 0..30.
const i32 ring_out[31] = {2140319550, 2136746468, 2136746468, 2136746468, 2136746468, 2138889601, 2140319550, 2141341527, 2142108331, 2142704923, 2143182317, 2143572991, 2143898607, 2144174166, 2144410389, 2144615136, 2144794305, 2144952409, 2145092955, 2145218714, 2145331904, 2145434319, 2145527428, 2145612444, 2145690379, 2145762081, 2145828269, 2145889557, 2145946468, 2145999456, 2146048913};
const i32 silent = 0x0044ce6c;   // -50 dB: a ringing note is let go below this
// The last stage: peaks above 0.45 are rounded off softly towards a ceiling
// of 0.6, never clipped; ordinary playing stays below the knee, untouched.
inline i32 soften(i32 sum) {
    const u32 knee = 0x39999999u, room = 0x0ccccccdu;                // 0.45, and 0.15 more
    const bool down = sum < 0;
    const u32 a = down ? static_cast<u32>(-(sum + 1)) : static_cast<u32>(sum);
    if (a <= knee) return sum;
    const u32 E = (a - knee) >> 16, D = room >> 16;                   // 32-bit: no 64-bit helpers
    const i32 y = static_cast<i32>(knee + ((E * D / (E + D)) << 16));
    return down ? -y : y;
}
// Progress: off (the first chord only), or seconds a chord, 30 down to 4.
inline u32 chord_seconds(i32 control) {
    const u32 a = control <= 0 ? 0u : static_cast<u32>(control) >> 16;    // 0..32767
    if (a < 1638u) return 0;
    return 30u - (a - 1638u) * 26u / (32767u - 1638u);
}

// The OP-XY's phase increment for each MIDI note (2^32 * hertz / 44100).
const u32 note_increment[128] = {796254,843601,893765,946911,1003217,1062871,1126073,1193033,1263974,1339134,1418763,1503127,1592507,1687203,1787529,1893821,2006434,2125742,2252146,2386065,2527948,2678268,2837526,3006254,3185015,3374406,3575058,3787642,4012867,4251485,4504291,4772130,5055896,5356535,5675051,6012507,6370030,6748811,7150117,7575285,8025735,8502970,9008582,9544261,10111792,10713070,11350103,12025015,12740059,13497623,14300233,15150569,16051469,17005939,18017165,19088521,20223584,21426141,22700205,24050030,25480119,26995246,28600467,30301139,32102938,34011878,36034330,38177043,40447168,42852281,45400411,48100060,50960238,53990491,57200933,60602278,64205876,68023757,72068660,76354085,80894335,85704563,90800821,96200119,101920476,107980983,114401866,121204555,128411753,136047513,144137319,152708170,161788671,171409126,181601643,192400238,203840952,215961966,228803732,242409110,256823506,272095026,288274639,305416341,323577341,342818251,363203285,384800477,407681904,431923931,457607465,484818220,513647012,544190053,576549277,610832681,647154683,685636503,726406571,769600953,815363807,863847862,915214929,969636441,1027294024,1088380105,1153098554,1221665363};
// 2^(k/12), Q16, for k = -64..64.
const u32 semitones[129] = {1625,1722,1825,1933,2048,2170,2299,2435,2580,2734,2896,3069,3251,3444,3649,3866,4096,4340,4598,4871,5161,5468,5793,6137,6502,6889,7298,7732,8192,8679,9195,9742,10321,10935,11585,12274,13004,13777,14596,15464,16384,17358,18390,19484,20643,21870,23170,24548,26008,27554,29193,30929,32768,34716,36781,38968,41285,43740,46341,49097,52016,55109,58386,61858,65536,69433,73562,77936,82570,87480,92682,98193,104032,110218,116772,123715,131072,138866,147123,155872,165140,174960,185364,196386,208064,220436,233544,247431,262144,277732,294247,311744,330281,349920,370728,392772,416128,440872,467088,494862,524288,555464,588493,623487,660561,699841,741455,785544,832255,881744,934175,989724,1048576,1110928,1176987,1246974,1321123,1399681,1482910,1571089,1664511,1763488,1868350,1979448,2097152,2221855,2353974,2493948,2642246};
const i32 hiss = 60740000;         // about 0.028: the same faint tape hiss as before Halcyon's new Hiss curve, no particles
const i32 ensemble = 0x20000000;   // 0.25

inline u32 mix(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

inline u32 choice(i32 v, u32 count) {
    const u32 c = v <= 0 ? 0u : ((static_cast<u32>(v) >> 16) * count) >> 15;
    return c >= count ? count - 1 : c;
}
// (a * b) >> 16 in 32-bit pieces: no 64-bit helpers on the device.
inline u32 mul16(u32 a, u32 b) {
    const u32 al = a & 0xffffu, ah = a >> 16, bl = b & 0xffffu, bh = b >> 16;
    const u32 ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    const u32 mid = (ll >> 16) + (lh & 0xffffu) + (hl & 0xffffu);
    return ((hh + (lh >> 16) + (hl >> 16) + (mid >> 16)) << 16) | (mid & 0xffffu);
}
// The nearest MIDI note to a phase increment.
inline i32 nearest_note(u32 increment) {
    i32 low = 0, high = 127;
    while (low < high) {
        const i32 mid = (low + high) >> 1;
        if (note_increment[mid] < increment) low = mid + 1; else high = mid;
    }
    if (low > 0 && increment - note_increment[low - 1] < note_increment[low] - increment) --low;
    return low;
}
// The scale's note at a step from middle C's root, as a MIDI note.
inline i32 step_note(i32 degree, u32 root, u32 scale) {
    const i32 size = static_cast<i32>(scale_size[scale]);
    i32 wrap = degree / size, step = degree % size;
    if (step < 0) { step += size; --wrap; }
    return 60 + static_cast<i32>(root) + static_cast<i32>(scale_notes[scale][step]) + 12 * wrap;
}
// A note's pitch class placed at or above `low`, within an octave.
inline i32 place(i32 note, i32 low) { return low + ((note - low) % 12 + 12) % 12; }
// Whether a chord on a degree sounds settled: its fifth neither diminished nor augmented.
inline bool usable(i32 degree, u32 root, u32 scale) {
    const i32 fifth = ((step_note(degree + 4, root, scale) - step_note(degree, root, scale)) % 12 + 12) % 12;
    return fifth != 6 && fifth != 8;
}
// A song: its chords as degrees of the scale, four to eight long, made by rules
// from the root, scale and key, so the same three always give the same song.
// Most start home, some on a colour chord; each leans on the scale's own
// colour, never repeats a chord, and ends on a chord that leads back home.
inline u32 make_song(u32 root, u32 scale, u32 song, u32 salt, i32 (&chords)[8]) {
    const i32 size = static_cast<i32>(scale_size[scale]);
    const u32 seed = mix(0x9e3779b9u * (root * scale_count + scale + 1u) ^ (song + 1u) * 0x85ebca6bu ^ salt * 0x27d4eb2fu);
    const u32 lengths[4] = {4, 6, 8, 5};
    const u32 length = lengths[seed & 3u];
    // Most songs start home; some open on one of the scale's colour chords.
    chords[0] = (seed >> 8) % 5u < 2u && usable(colour[scale][(seed >> 12) % 3u], root, scale) ? colour[scale][(seed >> 12) % 3u] : 0;
    for (u32 n = 1; n + 1 < length; ++n) {
        u32 r = mix(seed + n * 0x632be5abu);
        i32 pick = colour[scale][0];
        for (u32 tries = 0; tries < 8; ++tries, r = mix(r)) {
            const i32 c = r % 3u == 0 ? colour[scale][(r >> 4) % 3u] : static_cast<i32>((r >> 8) % static_cast<u32>(size));
            if (c != chords[n - 1] && usable(c, root, scale)) { pick = c; break; }
        }
        chords[n] = pick;
    }
    i32 last = ending[scale][seed >> 31];
    if (last == chords[length - 2] || !usable(last, root, scale)) last = ending[scale][1 - (seed >> 31)];
    // A song that opened away from home comes home at its end instead.
    if (chords[0] != 0 && chords[length - 2] != 0) last = 0;
    if (last == chords[0]) last = ending[scale][0] == chords[0] ? ending[scale][1] : ending[scale][0];
    chords[length - 1] = last;
    return length;
}
// Two songs the same as they loop (over eight chords)?
inline bool same_song(const i32* a, u32 la, const i32* b, u32 lb) {
    for (u32 n = 0; n < 8; ++n) if (a[n % la] != b[n % lb]) return false;
    return true;
}
// The song a black key plays: made by make_song, and made again with a new salt
// while it matches a song on a black key before it, so all ten differ.
inline u32 song_chords(u32 root, u32 scale, u32 song, i32 (&chords)[8]) {
    i32 made[10][8];
    u32 lengths[10];
    for (u32 s = 0; s <= song && s < 10; ++s) {
        for (u32 salt = 0; salt < 16; ++salt) {
            lengths[s] = make_song(root, scale, s, salt, made[s]);
            bool repeat = false;
            for (u32 t = 0; t < s && !repeat; ++t) repeat = same_song(made[s], lengths[s], made[t], lengths[t]);
            if (!repeat) break;
        }
    }
    for (u32 n = 0; n < 8; ++n) chords[n] = made[song][n];
    return lengths[song];
}
// The ten voicings, one for each song, kept through it so each song has its own
// sound: four notes as (step of the scale above the chord's degree, register:
// 0 bass from A2, 1 middle from A3, 2 high from A4). In D minor:
const i32 voicings[10][chord_notes][2] = {
    {{0, 0}, {2, 1}, {4, 1}, {8, 1}},   // open: D, then F A E
    {{0, 0}, {0, 1}, {2, 1}, {4, 1}},   // close: D, then D F A
    {{0, 0}, {4, 1}, {8, 1}, {7, 2}},   // wide: D, then A E, D high
    {{0, 0}, {1, 1}, {4, 1}, {7, 2}},   // sus2: D, then E A, D high
    {{0, 0}, {3, 1}, {4, 1}, {7, 2}},   // add 11: D, then G A, D high
    {{0, 0}, {2, 1}, {1, 1}, {4, 1}},   // minor add 9, close: D, then F E A
    {{0, 0}, {4, 1}, {7, 1}, {2, 2}},   // third on top: D, then A D, F high
    {{0, 0}, {4, 0}, {2, 1}, {7, 1}},   // low and dark: D A, then F D
    {{0, 0}, {4, 0}, {0, 1}, {4, 1}},   // fifths: D A, then D A, a wide drone
    {{0, 1}, {2, 2}, {4, 2}, {8, 2}},   // high shimmer: D, then F A E an octave up
};
// A chord on a degree in a song's voicing, in the register every song shares.
inline void chord_on(i32 degree, u32 root, u32 scale, i32 (&notes)[chord_notes], u32 voicing = 0) {
    // A diminished chord (its fifth six semitones up) gives way to the chord a third below.
    if (!usable(degree, root, scale)) degree -= 2;
    const i32 base = step_note(degree, root, scale);
    for (u32 n = 0; n < chord_notes; ++n) {
        i32 step = voicings[voicing % 10][n][0];
        // A flat ninth or second (a semitone above the root) is too sharp a rub: the octave instead.
        if ((step == 1 || step == 8) && ((step_note(degree + step, root, scale) - base) % 12 + 12) % 12 == 1) step = 7;
        notes[n] = place(step_note(degree + step, root, scale), 45 + 12 * voicings[voicing % 10][n][1]);   // A2, A3, A4
    }
}
// Which of the ten songs a black key plays (-1: a white key).
inline i32 song_of(i32 key) {
    if (black[key % 12] < 0) return -1;
    const i32 octave = key / 12 - 5;                          // middle C (60) is octave 0
    const i32 pair = octave >= 0 ? octave / 2 : (octave - 1) / 2;   // two octaves of black keys
    return (octave - pair * 2) * 5 + black[key % 12];
}
// The notes a key plays: a white key its scale note, a black key the chord its
// song is on at `chord` (a count of chords since the key went down).
inline u32 key_notes(i32 key, u32 root, u32 scale, i32 (&notes)[chord_notes], u32 chord = 0) {
    const i32 song = song_of(key);
    if (song < 0) {
        notes[0] = step_note((key / 12 - 5) * 7 + white[key % 12], root, scale);
        return 1;
    }
    i32 chords[8];
    const u32 length = song_chords(root, scale, static_cast<u32>(song), chords);
    chord_on(chords[chord % length], root, scale, notes, static_cast<u32>(song));
    return chord_notes;
}
}

inline void initialize(Voice& v, Shared& shared, u32 owner) {
    v = Voice();
    v.owner = owner;
    for (u32 n = 0; n < slots; ++n) hktone::initialize(v.tone[n], shared.halcyon, owner * slots + n);
}

inline void start(Voice& v, Shared&, u32) {
    // A new note on a voice still sounding: what it was playing gives way with
    // a fast fade instead of stopping dead, which clicked.
    v.count = 0;
    for (u32 n = 0; n < slots; ++n) if (v.state[n]) v.state[n] = 3;
    for (u32 n = 0; n < chord_notes; ++n) v.current[n] = slots;
}

inline void destroy(Voice&, Shared&, u32) {}

namespace {
// A free slot for a new note (slots: none).
inline u32 free_slot(Voice& v) {
    for (u32 n = 0; n < slots; ++n) if (!v.state[n]) return n;
    return slots;
}
inline void begin(Voice& v, Shared& shared, u32 slot, u32 ratio) {
    hktone::start(v.tone[slot], shared.halcyon, v.owner);   // swells in with Halcyon's Swell
    v.ratio[slot] = ratio;
    v.level[slot] = 0x7fffffff;
    v.gain[slot] = v.count > 1 ? 0x23333333 : 0x5999999a;   // a chord's note 0.275, a single note 0.7, doubled below
    v.state[slot] = 1;
}
// Notes waiting for a slot take free ones. While none is free, the faintest
// notes ringing out give way with a fast fade (about 10 ms) to make room, so a
// note is never cut off: the new one starts a few milliseconds later instead.
inline void place(Voice& v, Shared& shared) {
    u32 waiting = 0;
    for (u32 n = 0; n < v.count; ++n) {
        if (v.current[n] != slots) continue;
        const u32 slot = free_slot(v);
        if (slot == slots) { ++waiting; continue; }
        v.current[n] = slot;
        begin(v, shared, slot, v.want[n]);
    }
    u32 going = 0;
    for (u32 n = 0; n < slots; ++n) if (v.state[n] == 3) ++going;
    while (going < waiting) {
        u32 pick = slots;
        i32 faintest = 0x7fffffff;
        for (u32 n = 0; n < slots; ++n)
            if (v.state[n] == 2 && v.level[n] < faintest) { faintest = v.level[n]; pick = n; }
        if (pick == slots) break;
        v.state[pick] = 3;
        ++going;
    }
}
}

inline void render(Voice& v, Shared& shared, u32, opxy::Output output, opxy::Parameters p, opxy::Pitch pitch) {
    // Where the key's notes belong now: a white key's scale note, or a black
    // key's chord at this point of its song (the key's own clock, from 0 at the press).
    const bool first = !v.count;
    if (first) { v.key = nearest_note(pitch[0]); v.blocks = 0; v.song_for = 0; v.chord_at = 0; }
    const u32 seconds = chord_seconds(p[6]);
    if (first || black[v.key % 12] >= 0) {
        const u32 root = choice(p[0], 12), scale = choice(p[1], scale_count);
        const u32 chord = seconds ? v.blocks / (seconds * 689u) : 0u;
        i32 notes[chord_notes];
        const i32 song = song_of(v.key);
        if (song < 0) {
            v.count = key_notes(v.key, root, scale, notes);
            if (first) {
                i32 k = notes[0] - v.key;
                if (k < -64) k = -64;
                if (k > 64) k = 64;
                v.want[0] = semitones[k + 64];
                v.current[0] = slots;
            }
        } else {
            // A song plays like hands on keys: each chord is held for three
            // quarters of its time, then let go to ring out, and the next one
            // comes in as the last one fades.
            const u32 tag = root * scale_count + scale + 1u;
            if (v.song_for != tag) { v.song_length = song_chords(root, scale, static_cast<u32>(song), v.song); v.song_for = tag; }
            v.count = chord_notes;
            const u32 length = seconds * 689u;
            const bool held = !seconds || v.blocks % length < length - length / 4;
            if (chord + 1u != v.chord_at) {
                // The next chord: four new notes swell in (Halcyon's Swell).
                chord_on(v.song[chord % v.song_length], root, scale, notes, static_cast<u32>(song));
                for (u32 n = 0; n < chord_notes; ++n) {
                    if (v.current[n] < slots && v.state[v.current[n]] == 1) v.state[v.current[n]] = 2;
                    i32 k = notes[n] - v.key;
                    if (k < -64) k = -64;
                    if (k > 64) k = 64;
                    v.want[n] = semitones[k + 64];
                    v.current[n] = slots;
                }
                v.chord_at = chord + 1u;
            } else if (!held) {
                for (u32 n = 0; n < chord_notes; ++n)
                    if (v.current[n] < slots && v.state[v.current[n]] == 1) v.state[v.current[n]] = 2;   // let go: rings out
            }
        }
    }
    place(v, shared);
    if (v.blocks < 0xfffffff0u) ++v.blocks;
    // Halcyon's controls: Warmth, Swell, Drift, Air, Hiss, Chord, Ensemble, Age.
    const i32 controls[8] = {p[2], p[3], p[4], p[5], hiss, 0, ensemble, p[7]};
    // A note ringing out drops Halcyon's costly layers (Air's octaves, Hiss,
    // Ensemble's double, Age) as it fades, for about half the work.
    const i32 ringing[8] = {p[2], p[3], p[4], 0, 0, 0, 0, 0};
    i32 tone[opxy::block_frames];
    u32 played[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] = 0;
    // A single note at 1.4 times Halcyon's own level, a chord's notes at 0.55
    // each (Halcyon is a quarter scale at most), so a chord, even one ringing
    // into the next, sits with the white keys; soften() catches rare peaks.
    bool steady = true;
    for (unsigned i = 1; i < opxy::block_frames; ++i) if (pitch[i] != pitch[0]) { steady = false; break; }
    for (u32 n = 0; n < slots; ++n) {
        if (!v.state[n]) continue;
        // At most a quarter of the sample rate: above `cap` the product would pass it.
        const u32 cap = v.ratio[n] <= 65536u ? 0xffffffffu : (0x3fffffffu / v.ratio[n]) << 16;
        if (steady) {
            // The usual case, a held pitch: one product for the whole block.
            const u32 r = pitch[0] >= cap ? 0x3fffffffu : mul16(pitch[0], v.ratio[n]);
            const u32 q = r > 0x3fffffffu ? 0x3fffffffu : r;
            for (unsigned i = 0; i < opxy::block_frames; ++i) played[i] = q;
        } else {
            for (unsigned i = 0; i < opxy::block_frames; ++i) {
                const u32 r = pitch[i] >= cap ? 0x3fffffffu : mul16(pitch[i], v.ratio[n]);
                played[i] = r > 0x3fffffffu ? 0x3fffffffu : r;
            }
        }
        // The note's level (falling while it rings out or gives way) and its
        // gain, handed to Halcyon to fold into its own level ramp.
        if (v.state[n] == 2) v.level[n] = opxy::q31::multiply(v.level[n], ring_out[seconds]);
        else if (v.state[n] == 3) v.level[n] = opxy::q31::multiply(v.level[n], 0x33333333);   // -8 dB a block
        hktone::render(v.tone[n], shared.halcyon, v.owner, tone, v.state[n] == 1 ? controls : ringing, played,
                       opxy::q31::multiply(v.level[n], v.gain[n]));
        for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] += tone[i] << 1;
        if (v.state[n] >= 2 && v.level[n] < silent) v.state[n] = 0;
    }
    for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] = soften(output[i]);
}
}
