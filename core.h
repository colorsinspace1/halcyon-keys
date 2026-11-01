#pragma once
#include <opxy/audio.hpp>
#include "halcyon_core.h"

namespace hkeys {
// A white key plays one Halcyon voice; a black key plays a chord of four, and
// when its song moves on, the notes that change ring out while the new ones
// swell in: up to eight voices at once (four sounding, four ringing out, so a
// whole chord can fade while the next comes in; more give way, the faintest first).
const unsigned chord_notes = 4, slots = 8;
// One key: its Halcyon voices, each one's note (a Q16 pitch ratio from the key
// pressed) and level, and which of them the current chord's notes are.
struct Voice {
    hktone::Voice tone[slots];
    opxy::u32 ratio[slots];
    opxy::i32 level[slots];          // Q31
    opxy::u32 state[slots];          // 0 free, 1 sounding in the current chord, 2 ringing out, 3 giving way (a fast fade)
    opxy::i32 gain[slots];           // each voice's level in the mix, fixed when it starts
    opxy::u32 current[chord_notes];  // the slots of the current chord's notes (slots: waiting for one)
    opxy::u32 want[chord_notes];     // a waiting note's pitch ratio
    opxy::i32 key;        // the key pressed (MIDI note)
    opxy::u32 blocks;     // blocks since the key went down: a song's clock
    opxy::i32 song[8];    // a black key's song (degrees), made for one Root and Scale
    opxy::u32 song_length, song_for;   // its length, and which Root and Scale (0: not made)
    opxy::u32 chord_at;   // which chord of the song is sounding, plus one (0: none yet)
    opxy::u32 count;      // 0 until the key is placed, then 1 or chord_notes
    opxy::u32 owner;
};
struct Shared { hktone::Shared halcyon; };
}
