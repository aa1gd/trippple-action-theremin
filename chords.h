#pragma once

struct Note
    {
        int pitch; 
        bool isSeventh;
        bool isLeadingTone;
        // bool heldSeventh; or i could just use isSeventh here
    };

// index 0 is s, index 1 is a, index 2 is t, index 3 is b
struct Chord
    {
        struct Note notes[4];
        int score;
    };

bool genChord(int numeral, int inversion, struct Chord prev, volatile struct Chord *tobechanged);
int evalChord(struct Chord previous, struct Chord next);
void printChord(struct Chord c);
void adjustSpacing(struct Chord *c);
int inversionConversion(int level);
int figBassToNumeral(int bass, int inversion);
