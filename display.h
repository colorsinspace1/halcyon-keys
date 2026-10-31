#pragma once
#include <opxy/page_header.hpp>

namespace hkeys {
// Fog lake. Node layout (panel.svg.fragment; scene nodes authored around the origin):
//   0..7   the two-page header's stepped bar
//   8, 9   the sun's outer and inner halo     10     the sun's disc
//   11..14 four mist bands                    15..20 the sun's reflection, six bars
//   21..83 three ripples of 21 rings each, radii 4 to 84 in steps of 4
//   84..89 the reflection's ensemble double   90..101 dust on the water
const opxy::u32 node_header = 0, node_halo = 8, node_disc = 10, node_mist = 11, node_reflection = 15,
                node_ring = 21, node_double = 84, node_dust = 90;
const opxy::u32 mist_count = 4, reflection_count = 6, ripple_count = 3, ring_sizes = 21, dust_count = 12;
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, 102, 16, 12>;

struct Ripple {
    opxy::i32 x, y;        // where the note fell, pixels
    opxy::i32 age;         // milliseconds since it fell; < 0: this slot is free
    opxy::i32 speed;       // how fast its rings spread, pixels per second (slower with Swell)
    opxy::i32 wait;        // milliseconds until the late octaves' rings follow
    opxy::u32 octaves;     // 0: none, 1: the octave above, 2: above and below
    opxy::u32 chords;      // 0: none, 1: the fifth, 2: fifth and ninth (their rings follow the note's)
};

struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;      // last note-start count seen
    opxy::u32 random;
    opxy::u32 lane;        // last lane a ripple fell in, so the next falls elsewhere
    opxy::u32 drawn;       // 1 once the first frame is set
    opxy::i32 activity;    // Q10: rises while notes are held, at the Swell rate; falls on release
    opxy::i32 motion;      // milliseconds of movement so far: mist and reflection move only while it runs
    Ripple ripples[ripple_count];
};
}
