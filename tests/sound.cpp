// Halcyon Keys' own sound checks, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>

namespace {
opxy::i32 pick(unsigned index, unsigned count) { return static_cast<opxy::i32>((index * 2 + 1) * (0x7fffffffu / (count * 2))); }
bool in_scale(int note, unsigned root, unsigned scale) {
    const int degree = ((note - 60 - static_cast<int>(root)) % 12 + 12) % 12;
    for (unsigned s = 0; s < hkeys::scale_size[scale]; ++s)
        if (static_cast<int>(hkeys::scale_notes[scale][s]) == degree) return true;
    return false;
}
}

int main() {
    bool ok = true;
    // C minor: the white keys from middle C play C D Eb F G Ab Bb C.
    const int c_minor[8] = {60, 62, 63, 65, 67, 68, 70, 72};
    const int whites[8] = {60, 62, 64, 65, 67, 69, 71, 72};
    for (int i = 0; i < 8; ++i) {
        int notes[4];
        if (hkeys::key_notes(whites[i], 0, 0, notes) != 1 || notes[0] != c_minor[i]) {
            std::printf("FAIL white key %d plays %d, not %d\n", whites[i], notes[0], c_minor[i]); ok = false; }
    }
    // Every key, in every root and scale: white keys a scale note, black keys ten
    // different songs of four-note chords, every note in the scale.
    for (unsigned root = 0; root < 12; ++root)
        for (unsigned scale = 0; scale < 8; ++scale) {
            long songs[10][8];
            unsigned found = 0;
            for (int key = 24; key < 108; ++key) {
                int notes[4];
                if (hkeys::black[key % 12] < 0) {
                    hkeys::key_notes(key, root, scale, notes);
                    if (!in_scale(notes[0], root, scale)) { std::printf("FAIL key %d plays %d, outside the scale\n", key, notes[0]); ok = false; }
                    continue;
                }
                for (unsigned chord = 0; chord < 8; ++chord) {
                    if (hkeys::key_notes(key, root, scale, notes, chord) != 4) { std::puts("FAIL a black key is not a chord"); ok = false; }
                    for (int n = 0; n < 4; ++n)
                        if (!in_scale(notes[n], root, scale)) { std::printf("FAIL black key %d chord note %d outside the scale\n", key, notes[n]); ok = false; }
                    if (key >= 60 && key < 84) songs[found][chord] = notes[0] * 1000000L + notes[1] * 10000L + notes[2] * 100L + notes[3];
                }
                if (key >= 60 && key < 84) ++found;
            }
            if (found != 10) { std::printf("FAIL %u songs in two octaves, not 10\n", found); ok = false; continue; }
            for (unsigned a = 0; a < 10; ++a)
                for (unsigned b = a + 1; b < 10; ++b) {
                    bool same = true;
                    for (unsigned c = 0; c < 8; ++c) same = same && songs[a][c] - songs[a][0] == songs[b][c] - songs[b][0];
                    if (same && scale < 6) { std::printf("FAIL songs %u and %u are the same (root %u, scale %u)\n", a, b, root, scale); ok = false; }
                }
        }
    // A held black key plays its song: at the fastest Progress the chord changes after 4 s.
    {
        static hkeys::Voice voice;
        static hkeys::Shared shared;
        hkeys::initialize(voice, shared, 2);
        hkeys::start(voice, shared, 2);
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = hkeys::note_increment[61];
        opxy::i32 output[opxy::block_frames];
        const opxy::i32 p[8] = {pick(0, 12), pick(0, 8), 0x40000000, 0, 0, 0, 0x7fffffff, 0};
        hkeys::render(voice, shared, 2, output, p, pitch);
        opxy::u32 before[4];
        for (int n = 0; n < 4; ++n) before[n] = voice.ratio[voice.current[n]];
        for (unsigned b = 0; b < 689 * 9 / 2; ++b) hkeys::render(voice, shared, 2, output, p, pitch);
        bool moved = false;
        for (int n = 0; n < 4; ++n) moved = moved || voice.ratio[voice.current[n]] != before[n];
        if (!moved) { std::puts("FAIL a held black key does not move to its next chord"); ok = false; }
        // The chord it let go still rings out as the new one comes in.
        bool ringing = false;
        for (unsigned n = 0; n < hkeys::slots; ++n) ringing = ringing || voice.state[n] == 2;
        if (!ringing) { std::puts("FAIL the last chord does not ring out under the new one"); ok = false; }
    }
    if (ok) std::puts("PASS");
    return ok ? 0 : 1;
}
