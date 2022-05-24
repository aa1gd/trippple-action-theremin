#pragma once

struct Note
    {
        int pitch; 
        bool isSeventh;
        bool isLeadingTone;
        // TODO: bool heldSeventh; or i could just use isSeventh here
    };

// index 0 is s, index 1 is a, index 2 is t, index 3 is b
struct Chord
    {
        struct Note notes[4];
        int score;
    };

bool genChord(int key, bool major, int numeral, int inversion, struct Chord prev, volatile struct Chord *tobechanged);
int evalChord(int key, struct Chord previous, struct Chord next);
void adjustSpacing(struct Chord *c);
int inversionConversion(int level);
int figBassToNumeral(int key, bool major, int bass, int inversion);
