#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "chords.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "bsp/board.h"
#include "tusb.h"

/*
 do di re me mi fa fi so si la te ti do
 0  1  2  3  4  5  6  7  8  9  10 11 12
*/

static const int major_scale[7] = {0, 2, 4, 5, 7, 9, 11}; // whoa whoa hey whoa whoa whoa (hey)
static const int natural_minor_scale[7] = {0, 2, 3, 5, 7, 8, 10}; 
static const int melodic_minor_scale[7] = {0, 2, 3, 5, 7, 9, 11}; // ascending
static const int harmonic_minor_scale[7] = {0, 2, 3, 5, 7, 8, 11};

// Lookup table for note names

static const int frustrated_leading_tone_penalty = 5;
static const int jumpy_alto_or_tenor_penalty_multiplier = 2;

static int key = 48;
static bool major = true;

bool genChord(int numeral, int inversion, struct Chord prev, volatile struct Chord *tobechanged)
{
   static struct Chord best;
    // putting 69s so I know if best is not assigned... which would mean no combinations work
    best.notes[0].pitch = -69;

    best.score = 99; // lower is better
    static struct Note masterAvailable[4];
    bool isSeventhChord = false;
    int bassNote = 0;

    // identify seventh chords and inversion positions
    switch (inversion)
        {
            case 0:
                bassNote = 0;
                break;
            case 6:
                bassNote = 1;
                break;
            case 64:
                bassNote = 2;
                break;
            case 7:
                bassNote = 0;
                isSeventhChord = true;
                break;
            case 65:
                bassNote = 1;
                isSeventhChord = true;
                break;
            case 43:
                bassNote = 2;
                isSeventhChord = true;
                break;
            case 42:
                bassNote = 3;
                isSeventhChord = true;
                break;
        }

    // Making sure all boolean leadingTone and isSevenths are false
    // Maybe there's some way to initialize it to zero?
    for (int i = 0; i < 4; i++){
        masterAvailable[i].isLeadingTone = false;
        masterAvailable[i].isSeventh = false;
    }

    if (major)
    {
        masterAvailable[0].pitch = key + major_scale[numeral - 1]; // root 
        masterAvailable[1].pitch = key + major_scale[(numeral + 1) % 7]; // third
        masterAvailable[2].pitch = key + major_scale[(numeral + 3) % 7]; // fifth 

        if (isSeventhChord)
        {
            masterAvailable[3].pitch = key + major_scale[(numeral + 5) % 7]; // seventh
            masterAvailable[3].isSeventh = true;
        }
    else 
        {
            masterAvailable[3].pitch = -1; // so I know its an error
        }
    } else // minor
    {
        masterAvailable[0].pitch = key + harmonic_minor_scale[numeral - 1]; // root 
        masterAvailable[1].pitch = key + harmonic_minor_scale[(numeral + 1) % 7]; // third
        masterAvailable[2].pitch = key + harmonic_minor_scale[(numeral + 3) % 7]; // fifth 

        if (isSeventhChord)
        {
            masterAvailable[3].pitch = key + harmonic_minor_scale[(numeral + 5) % 7]; // seventh
            masterAvailable[3].isSeventh = true;
        }  
    }

    // Check for leading tones
    for (int i = 0; i < 4; i++)
        {
            if ((masterAvailable[i].pitch - key) % 12 == 11)
                masterAvailable[i].isLeadingTone = true;
        }

    // checking validity of masterAvailable
    // //printf("masterAvailable: %d %d %d %d\n", masterAvailable[0].pitch, masterAvailable[1].pitch, masterAvailable[2].pitch, masterAvailable[3].pitch);

    // Double notes
    for (int i = 0; i < 4; i++)
        {
            // making a temp copy of available
            struct Note available[4];
            for (int lcv = 0; lcv < 4; lcv++)
                available[lcv] = masterAvailable[lcv];

            // double a triad note
            if (i < 3)
            {
                if (numeral == 6){
                    available[3] = available[1]; // double third on deceptive cadence
                } else if (inversion == 64){
                    available[3] = available[2]; // double fifth on 64 triads
                } else if (!isSeventhChord && !available[i].isLeadingTone){
                    available[3] = available[i];
                } else if (available[i].isLeadingTone)
                    continue;
            } else if (best.notes[0].pitch == -69 && !available[0].isLeadingTone && inversion == 0){
                available[2] = available[0]; // in worst scenario, replace fifth with root and triple root 
                available[3] = available[0];
            } else {
                break;
            }

            // Set bass note at the first spot in available (makes manual permutations easier)
            struct Note temp = available[0];
            available[0] = available[bassNote];
            available[bassNote] = temp;
            // //printf("Available after bass swap: %d %d %d %d\n", available[0].pitch, available[1].pitch, available[2].pitch, available[3].pitch);


            // //printf("Available after double: %d %d %d %d\n", available[0].pitch, available[1].pitch, available[2].pitch, available[3].pitch);
            // permutations here
            static struct Chord guess;
            guess.score = 999;
            //doing it manually 6 times because I don't understand the recusive algorithm to do it
            for (int j = 0; j < 3; j++)
                {
                    // //printf("ROUND %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",j);
                    int bassDownShift = 0;
                    if (j == 0)
                        bassDownShift = 12; // sometimes helps with preventing voice crossing

                    // On the third round, lower the tenor before adjusting spacing IF THE PREVIOUS TWO ROUNDS HAVE NOT WORKED
                    int tenorTweak = 0;
                    if (j == 2 && best.notes[0].pitch == -69)
                    {
                        //printf("Tweaking tenor\n");
                        tenorTweak = 12;
                    }
                else if (j == 2)
                    {
                        // //printf("ROUND 3 BREAKING!!!!!!!!!!!!!!!!!!!!!!\n");
                        break;
                    }

                    // TODO: limit the range of the voices
                    guess.notes[0] = available[0];
                    guess.notes[1] = available[1];
                    guess.notes[2] = available[2];
                    guess.notes[3] = available[3];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;

                    guess.notes[0] = available[0];
                    guess.notes[1] = available[1];
                    guess.notes[2] = available[3];
                    guess.notes[3] = available[2];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;

                    guess.notes[0] = available[0];
                    guess.notes[1] = available[2];
                    guess.notes[2] = available[1];
                    guess.notes[3] = available[3];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;

                    guess.notes[0] = available[0];
                    guess.notes[1] = available[2];
                    guess.notes[2] = available[3];
                    guess.notes[3] = available[1];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;

                    guess.notes[0] = available[0];
                    guess.notes[1] = available[3];
                    guess.notes[2] = available[1];
                    guess.notes[3] = available[2];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;

                    guess.notes[0] = available[0];
                    guess.notes[1] = available[3];
                    guess.notes[2] = available[2];
                    guess.notes[3] = available[1];
                    guess.notes[0].pitch -= tenorTweak;
                    guess.notes[1].pitch -= tenorTweak;
                    adjustSpacing(&guess);
                    guess.notes[0].pitch -= bassDownShift;
                    guess.score = evalChord(prev, guess); 
                    if (guess.score != -1 && guess.score < best.score)
                        best = guess;
                }
        }

    // Check if best has been assigned anything. Look for -69
    if (best.notes[0].pitch == -69)
    {
        // if none work, make a generic chord with doubled root and return false
        best.notes[0] = masterAvailable[0];
        best.notes[1] = masterAvailable[1];
        best.notes[2] = masterAvailable[2];
        if (isSeventhChord)
            best.notes[3] = masterAvailable[3];
        else
            best.notes[3] = masterAvailable[0];

        struct Note tmp = best.notes[0];
        best.notes[0] = best.notes[bassNote];
        best.notes[bassNote] = tmp;

        adjustSpacing(&best);
        /*
        board_led_write(0);
        sleep_ms(2000);
        blink(30);
        board_led_write(1);
        sleep_ms(5000);
        */
        *tobechanged = best;
        return false; //what's here?????? TODO supposed to be false
    }

    *tobechanged = best;
    return true;
}


void printChord(struct Chord c)
{
    bool useLetters = true;
    if (key % 12 == 0 && major && useLetters) // if key is C major
    {
        for (int i = 3; i >= 0; i--)
            {
                int cut = c.notes[i].pitch % 12; 
                int whichOctave = c.notes[i].pitch / 12 - 1;
                switch (cut)
                    {
                        case 0:
                            printf("C");
                            break;

                        case 2:
                            printf("D");
                            break;

                        case 4:
                            printf("E");
                            break;

                        case 5:
                            printf("F");
                            break;

                        case 7:
                            printf("G");
                            break;

                        case 9:
                            printf("A");
                            break;

                        case 11:
                            printf("B");
                            break;

                        default:
                            printf("unrecognized ");
                            break;
                    }
            }
        printf("\n");
        return;
    }

    for (int i = 0; i < 4; i++)
        {
            printf("%d ", c.notes[i].pitch);
        }
    printf("\n");
}


int evalChord(struct Chord previous, struct Chord next)
{
    //printf("Started eval\n");
    //printChord(next);
    int score = 0;

    // check spacing
    if (!(next.notes[3].pitch >= next.notes[2].pitch && next.notes[2].pitch >= next.notes[1].pitch && next.notes[1].pitch >= next.notes[0].pitch))
    {
        //printf("Voice Crossing\n");
        return -1;
    }
    if (next.notes[3].pitch - next.notes[2].pitch > 12 || next.notes[2].pitch - next.notes[1].pitch > 12)
    {
        //printf("Notes more than octave apart\n");
        return -1;
    }

    // Incentivize spacing out the chord, to prevent future voice crossings
    score += 24 - (next.notes[3].pitch - next.notes[1].pitch);

    // Penalize bass moving out of G2-C4 range
    if (next.notes[0].pitch < 43 || next.notes[0].pitch > 60)
        score += 5; // TODO: make penalty constant for this

    // Penalize soprano moving above G5
    if (next.notes[3].pitch > 79)
        score += 5;

    // the following criteria depend on previous chord. If no previous chord, return score now.
    if (previous.notes[0].pitch == 0)
    {
        //printf("No previous chord\n");
        //printf("score: %d\n\n", score);
        return score;
    }

    // check voice crossings
    if (next.notes[0].pitch >= previous.notes[1].pitch ||
        next.notes[1].pitch <= previous.notes[0].pitch || next.notes[1].pitch >= previous.notes[2].pitch ||
        next.notes[2].pitch <= previous.notes[1].pitch || next.notes[2].pitch >= previous.notes[3].pitch ||
        next.notes[3].pitch <= previous.notes[2].pitch)
    {
        //printf("Voices overlap\n");
        return -1;
    }

    // check leading tone resolutions
    int leadingToneIndex = -1; // s a t or b
    bool foundLeadingTone = false;
    for (int i = 0; i < 4; i++)
        {
            if (previous.notes[i].isLeadingTone)
            {
                if (foundLeadingTone){
                    //printf("Two leading tones\n");
                    return -1;
                }
                leadingToneIndex = i;
                foundLeadingTone = true;
            }
        }

    if (next.notes[leadingToneIndex].isLeadingTone) // if ti goes to ti
        score += 0;
    else if ((next.notes[leadingToneIndex].pitch - key) % 12 == 0) // if ti goes to do
            score += 0;
        else if ((next.notes[leadingToneIndex].pitch - key) % 12 == 7) // if ti goes to so (frustrated leading tone)
                score += frustrated_leading_tone_penalty; // slightly punished
            else
                {
                    //printf("Leading tone didn't resolve \n");
                    return -1;
                }
    // check seventh resolutions
    int seventhIndex = -1; // s a t or b
    for (int i = 0; i < 4; i++)
        {
            if (previous.notes[i].isSeventh)
                seventhIndex = i;
        }
    if (seventhIndex != -1)
    {
        int seventhDifference = previous.notes[seventhIndex].pitch - next.notes[seventhIndex].pitch;
        //printf("Seventh Difference: %d\n", seventhDifference);
        if (!(seventhDifference <= 2 && seventhDifference > 0)) // diff of zero means holding... TODO add a seventh holding bool variable 
        {
            //printf("Seventh didn't resolve\n");
            return -1;
        }
    }

    // TODO: else if difference == 0, mark the seventhIndex of next as a seventh

    // check parallel fifths and octaves
    for (int i = 0; i < 4; i++)
        {
            for (int j = i; j < 4; j++)
                {
                    int difference = previous.notes[j].pitch - previous.notes[i].pitch;
                    if (difference == 0)
                        continue;
                    // check that same interval on the next chord
                    int nextDiff = next.notes[j].pitch - next.notes[i].pitch;
                    // oblique motion is fine
                    if (previous.notes[j].pitch == next.notes[j].pitch || previous.notes[i].pitch == next.notes[i].pitch)
                        continue;

                    //printf("Diff %d and nextDiff %d\n", difference, nextDiff);
                    nextDiff %= 12;
                    difference %= 12;
                    //printf("After modulo: Diff %d and nextDiff %d\n", difference, nextDiff);
                    if (nextDiff < 0)
                        nextDiff += 12;
                    if (difference < 0)
                        difference += 12;

                    //printf("After adding: Diff %d and nextDiff %d\n", difference, nextDiff);

                    if (difference == 7 && difference == nextDiff)
                    {
                        //printf("Found P5\n");
                        return -1;
                    }

                    if (difference == 0 && difference == nextDiff)
                    {
                        //printf("Found P8\n");
                        return -1;
                    }
                }
        }

    // evaluate alto and tenor smoothness. Rougher lines mean higher points, which is less desirable
    // unacceptable if jumping more than a fifth
    int tenorChange = abs(next.notes[1].pitch - previous.notes[1].pitch);
    int altoChange = abs(next.notes[2].pitch - previous.notes[2].pitch);
    if (tenorChange > 7 || altoChange > 7)
    {
        //printf("too much tenor or alto change\n");
        return -1;
    }

    if (tenorChange > 2)
        score += tenorChange * jumpy_alto_or_tenor_penalty_multiplier;
    if (altoChange > 2)
        score += altoChange * jumpy_alto_or_tenor_penalty_multiplier;

    // TODO: soprano over-jerkiness prevention?

    //printf("score: %d\n\n", score);
    return score;
}

// recursively adjusts until spaced correctly
void adjustSpacing(struct Chord *c)
{
    // //printf("adjusting spacing: ");
    // printChord(*c);
    bool adjusted = false;
    if (c->notes[0].pitch > c->notes[1].pitch)
    {
        c->notes[1].pitch += 12;
        adjusted = true;
    }
    if (c->notes[1].pitch > c->notes[2].pitch)
    {
        c->notes[2].pitch += 12;
        adjusted = true;
    }
    if (c->notes[2].pitch > c->notes[3].pitch)
    {
        c->notes[3].pitch += 12;
        adjusted = true;
    }

    // catch errors
    if (abs(c->notes[0].pitch) > 120 || abs(c->notes[1].pitch) > 120 || abs(c->notes[2].pitch) > 120 || abs(c->notes[3].pitch) > 120)
    {
        //printf("Error while adjusting spacing.\n");
        return;
    }

    if (adjusted)
        adjustSpacing(c);
}

// Converts sensor level to 0, 664-765-4342
int inversionConversion(int level)
{
    switch (level)
    {
        case 0:
            return 0;

        case 1:
            return 6;
        
        case 2:
            return 64;

        case 3:
            return 7;

        case 4:
            return 65;

        case 5:
            return 43;
        
        case 6:
            return 42;
        
        default:
            return -1;
    }
}

int figBassToNumeral(int bass, int inversion)
{
    int numeral;
    int bassNote;

    switch (inversion)
        {
            case 0:
                bassNote = 0;
                break;
            case 6:
                bassNote = 1;
                break;
            case 64:
                bassNote = 2;
                break;
            case 7:
                bassNote = 0;
                break;
            case 65:
                bassNote = 1;
                break;
            case 43:
                bassNote = 2;
                break;
            case 42:
                bassNote = 3;
                break;
        }

    int pitch_relative_to_key = (bass - key) % 12;
    if (pitch_relative_to_key < 0)
        pitch_relative_to_key += 12;

    if (major)
    {
        for (int i = 0; i < 7; i++)
            {
                if (pitch_relative_to_key == major_scale[i])
                    numeral = i;
            }
    } else
    {
        for (int i = 0; i < 7; i++)
            {
                if (pitch_relative_to_key == harmonic_minor_scale[i])
                    numeral = i;
            }
    }

    return (numeral - 2 * bassNote + 7) % 7 + 1;
}
