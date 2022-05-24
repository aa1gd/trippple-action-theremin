/* Created by Godwin Duan May 17, 2022 - May 23, 2022 for Mr. Sato's 4th period AP Music Theory final project. */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "bsp/board.h"
#include "tusb.h"

#include "chords.h"
#include "hc-sr04.h"
#include "ssd1306.h"

#define PITCH_OFFSET 0

#define DISPLAY_SDA_PIN 0
#define DISPLAY_SCL_PIN 1

#define ROTARY_1_CLK_PIN 13
#define ROTARY_1_DT_PIN 14
#define ROTARY_1_SW_PIN 15
#define ROTARY_2_CLK_PIN 5
#define ROTARY_2_DT_PIN 6
#define ROTARY_2_SW_PIN 7
#define ROTARY_3_CLK_PIN 2
#define ROTARY_3_DT_PIN 3
#define ROTARY_3_SW_PIN 4

#define SENSOR_1_TRIG_PIN 8
#define SENSOR_1_ECHO_PIN 9
#define SENSOR_2_TRIG_PIN 16
#define SENSOR_2_ECHO_PIN 17

#define MUX_1_A_PIN 18
#define MUX_1_B_PIN 19
#define MUX_1_C_PIN 20
#define MUX_2_A_PIN 10
#define MUX_2_B_PIN 11
#define MUX_2_C_PIN 12
#define MUX_2_DISABLE_PIN 21

#define FOOT_PEDAL_PIN 22

#define MAX_RANGE_1 64
#define MAX_RANGE_2 64

#define BLINK_NOT_MOUNTED 250
#define BLINK_MOUNTED 500
#define BLINK_SUSPENDED 1000

static volatile uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static const uint LED_PIN = PICO_DEFAULT_LED_PIN;

void setup_gpios();
void led_blinking_task(void);
void midi_write_note(uint8_t note, uint8_t volume);
void midi_turn_off_note(uint8_t note);
void midi_reset_all();
void midi_play_chord(struct Chord previous, struct Chord next);

void tud_mount_cb(void);
void tud_umount_cb(void);
void tud_suspend_cb(bool remote_wakeup_en);
void tud_resume_cb(void);

void display_update(ssd1306_t *disp, char *line1, char *line2, char *line3, char *line4, char *line5);

void get_note_name(int pitch, char *name);

void setLeftMux(int light);
void setRightMux(int light);

void read_rotaries();
static bool prevStateCLK1;
static bool prevStateCLK2;
static bool prevStateCLK3;
static bool prevButtonState1;
static bool prevButtonState2;
static bool prevButtonState3;

// State variables for the 4 different operating modes
// Melody
static int prevMelodyNote = 0;
static int prevVolume = 0;

// Chords
// TODO: When I compiled, I had these two variables initialized as zero.
// Should have been something else as I chord cannot be played as the first chord
static int prevInversion = 0;
static int prevDegree = 0;
static volatile struct Chord second, third;

// Figured Bass
static int prevBassNote = 0;
static int prevFigBassInversion;

// Duet
static int prevDuetNote1 = 0;
static int prevDuetNote2 = 0;

static struct Note default_note; // is this even necessary?

static const int melodic_major_scale[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const int playable_natural_minor_scale[8] = {0, 2, 3, 5, 7, 8, 10, 12}; 

static const char note_names[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char alt_note_names[12][3] = {"", "Db", "", "Eb", "", "", "Gb", "", "Ab", "", "Bb", "B"};

static int key = 48;
static bool major = true;
static int operating_mode = 0; // 0 -> melodies, 1 -> chords, 2 -> figured bass, 3 -> melody harmonization
static int midi_channel = 0; // between 0 and 15

static ssd1306_t disp;

int main()
{
    // Neccesary?
    default_note.pitch = 0;
    default_note.isSeventh = false;
    default_note.isLeadingTone = false;

    third.notes[0] = default_note;
    third.notes[1] = default_note;
    third.notes[2] = default_note;
    third.notes[3] = default_note;
    third.score = -1;

    second = third;
    // end neccessary?

    setup_gpios();

    // for sensor diagnostics
    gpio_init(26);
    gpio_set_dir(26, GPIO_OUT);

    // Setup SSD1306 display
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
    ssd1306_clear(&disp);

    ssd1306_draw_string(&disp, 8, 24, 2, "Loading");
    ssd1306_show(&disp);

    // Get initial rotary readings
    prevStateCLK1 = gpio_get(ROTARY_1_CLK_PIN);
    prevStateCLK2 = gpio_get(ROTARY_2_CLK_PIN);
    prevStateCLK3 = gpio_get(ROTARY_3_CLK_PIN);

    prevButtonState1 = gpio_get(ROTARY_1_SW_PIN); // it's high when not pressed, low when pressed
    prevButtonState2 = gpio_get(ROTARY_2_SW_PIN);
    prevButtonState3 = gpio_get(ROTARY_3_SW_PIN);

    // initialize LEDs with a sweep
    for (int i = 0; i < 8; i++)
    {
        setLeftMux(i);
        setRightMux(i);
        busy_wait_ms(250);
    }
    // then turn them off
    setLeftMux(-1);
    setRightMux(-1);

    board_init();
    tusb_init();

    static bool twentysix = false;
    
    // Gives the computer some time to pick up the device
    absolute_time_t endstartup = delayed_by_ms(get_absolute_time(), 3000);
    while (absolute_time_diff_us(get_absolute_time(), endstartup) > 0)
        tud_task();

    ssd1306_clear(&disp);
    display_update(&disp, "", "", "", "", "");

    while (true)
        {
            gpio_put(26, twentysix);
            twentysix = !twentysix;

            led_blinking_task();

            uint8_t packet[4];
            while ( tud_midi_available() )
                tud_midi_packet_read(packet);

            // Check for rotary switch / button inputs
            read_rotaries();

            int vol, level, deg, pitch;

            if (operating_mode == 0) // single note melody mode
            {
                level = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_ECHO_PIN, MAX_RANGE_2, 8);               
                vol = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_ECHO_PIN, MAX_RANGE_1, 128);

                if (level == -1)
                {
                    if (prevMelodyNote != 0)
                        midi_turn_off_note(prevMelodyNote);
                    prevMelodyNote = 0;
                    continue;
                }

                if (vol == -1)
                    vol = 127;

                deg = level - PITCH_OFFSET;
                pitch = 0;

                if (deg >= 0)
                {
                    if (major)
                        pitch = melodic_major_scale[deg] + key;
                    else
                        pitch = playable_natural_minor_scale[deg] + key;
                } else
                {
                    deg += 7;
                    if (major)
                        pitch = melodic_major_scale[deg] + key - 12;
                    else
                        pitch = playable_natural_minor_scale[deg] + key - 12;
                }

                if (vol < 126)
                    setLeftMux(vol / 18);
                else
                    setLeftMux(6);
                setRightMux(deg);

                if (pitch != prevMelodyNote || vol != prevVolume)
                {
                    if (prevMelodyNote != 0)
                        midi_turn_off_note(prevMelodyNote);
                    midi_write_note(pitch, vol);
                    prevMelodyNote = pitch;
                    prevVolume = vol;
                }
            }
            else if (operating_mode == 1) // chords mode
            {
                int degree = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_ECHO_PIN, MAX_RANGE_2, 7);               
                int inversionLevel = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_ECHO_PIN, MAX_RANGE_1, 7);

                int inversion = inversionConversion(inversionLevel);

                // show LEDs with mux here
                setLeftMux(inversionLevel);
                setRightMux(degree);

                // active low foot pedal
                if ((degree != prevDegree || inversion != prevInversion) && !gpio_get(FOOT_PEDAL_PIN))
                {
                    genChord(key, major, degree + 1, inversion, second, &third);
                    midi_play_chord(second, third);
                    second = third;
                    prevDegree = degree;
                    prevInversion = inversion;
                    char line1[8] = "";
                    char line2[8] = "";
                    char line3[8] = "";
                    char line4[8] = "";
                    char line5[8] = "";

                    get_note_name(third.notes[3].pitch, line1);
                    get_note_name(third.notes[2].pitch, line2);
                    get_note_name(third.notes[1].pitch, line3);
                    get_note_name(third.notes[0].pitch, line4);

                    if (major)
                    {
                        switch (degree)
                        {
                        case 0:
                            sprintf(line5, "I");
                            break;

                        case 1:
                            sprintf(line5, "ii");
                            break;

                        case 2:
                            sprintf(line5, "iii");
                            break;

                        case 3:
                            sprintf(line5, "IV");
                            break;

                        case 4:
                            sprintf(line5, "V");
                            break;

                        case 5:
                            sprintf(line5, "vi");
                            break;

                        case 6:
                            sprintf(line5, "vii*");
                            break;
                        }
                    }
                    else
                    {
                        switch (degree)
                        {
                        case 0:
                            sprintf(line5, "i");
                            break;

                        case 1:
                            sprintf(line5, "ii*");
                            break;

                        case 2:
                            sprintf(line5, "III+");
                            break;

                        case 3:
                            sprintf(line5, "iv");
                            break;

                        case 4:
                            sprintf(line5, "V");
                            break;

                        case 5:
                            sprintf(line5, "VI");
                            break;

                        case 6:
                            sprintf(line5, "vii*");
                            break;
                        }
                    }

                    char wordInversion[3];
                    sprintf(wordInversion, "%2d", inversion);
                    if (inversion != 0)
                        strcat(line5, wordInversion);

                    display_update(&disp, line1, line2, line3, line4, line5);
                }
            }
            else if (operating_mode == 2) // figured bass mode
            {
                int bNote = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_ECHO_PIN, MAX_RANGE_2, 7);               
                int invLevel = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_ECHO_PIN, MAX_RANGE_1, 7);
                
                int inv = inversionConversion(invLevel);

                int actualNumeral = figBassToNumeral(key, major, bNote, inv);
                // Show mux leds here
                setLeftMux(invLevel);
                setRightMux(bNote);

                // Foot pedal active low
                if ((bNote != prevBassNote || inv != prevInversion) && !gpio_get(FOOT_PEDAL_PIN))
                {
                    genChord(key, major, actualNumeral + 1, inv, second, &third);
                    midi_play_chord(second, third);
                    second = third;
                    prevBassNote = bNote;
                    prevFigBassInversion = inv;

                    char line1[8] = "";
                    char line2[8] = "";
                    char line3[8] = "";
                    char line4[8] = "";
                    char line5[8] = "";

                    get_note_name(third.notes[3].pitch, line1);
                    get_note_name(third.notes[2].pitch, line2);
                    get_note_name(third.notes[1].pitch, line3);
                    get_note_name(third.notes[0].pitch, line4);

                    // TODO: add sharps when key is minor
                    switch (actualNumeral)
                    {
                    case 0:
                        break;

                    case 1:
                        sprintf(line5, "6");
                        break;

                    case 2:
                        sprintf(line5, "64");
                        break;

                    case 3:
                        sprintf(line5, "7");
                        break;

                    case 4:
                        sprintf(line5, "65");
                        break;

                    case 5:
                        sprintf(line5, "43");
                        break;

                    case 6:
                        sprintf(line5, "42");
                        break;
                    }
                    display_update(&disp, line1, line2, line3, line4, line5);
                }
            }
            else if (operating_mode == 3) // duet mode
            {
                // note1 is bass (an octave lower), note2 is soprano
                int note1 = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_ECHO_PIN, MAX_RANGE_1, 8);               
                int note2 = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_ECHO_PIN, MAX_RANGE_2, 8);

                // set lights
                if (note1 == 7)
                    setLeftMux(0);
                else 
                    setLeftMux(note1);
                
                setRightMux(note2);

                if (note1 == -1)
                {
                    if (prevDuetNote1 != 0)
                        midi_turn_off_note(prevDuetNote1);
                    prevDuetNote1 = 0;
                }

                if (note2 == -1)
                {
                    if (prevDuetNote2 != 0)
                        midi_turn_off_note(prevDuetNote2);
                    prevDuetNote2 = 0;
                }

                // no pitch offset here... one octave is enough
                if (major)
                {
                    if (note1 != -1)
                        note1 = melodic_major_scale[note1] + key - 12;
                    if (note2 != -1)
                        note2 = melodic_major_scale[note2] + key;
                }
                else
                {
                    if (note1 != -1)
                        note1 = playable_natural_minor_scale[note1] + key - 12;
                    if (note2 != -1)
                        note2 = playable_natural_minor_scale[note2] + key;
                }

                if (note1 != -1 && note1 != prevDuetNote1)
                {
                    if (prevDuetNote1 != 0)
                        midi_turn_off_note(prevDuetNote1);
                    midi_write_note(note1, 127);
                    prevDuetNote1 = note1;
                }

                if (note2 != -1 && note2 != prevDuetNote2)
                {
                    if (prevDuetNote2 != 0)
                        midi_turn_off_note(prevDuetNote2);
                    midi_write_note(note2, 127);
                    prevDuetNote2 = note2;
                }
           }

            tud_task(); // tinyusb device task
        }
    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

void midi_write_note(uint8_t note, uint8_t volume)
{
    uint8_t const cable_num = 0;

    uint8_t note_on[3] = { 0x90 | midi_channel, note, volume};
    tud_midi_stream_write(cable_num, note_on, 3);

}

void midi_turn_off_note(uint8_t note)
{ 
    uint8_t const cable_num = 0;

    uint8_t note_off[3] = { 0x80 | midi_channel, note, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
}

void midi_reset_all(){
    for (int i = 21; i < 128; i++)
    {
        midi_turn_off_note(i);
    }
}

void midi_play_chord(struct Chord previous, struct Chord next)
{
    for (int i = 0; i < 4; i++)
        {
            // if (previous.notes[i].pitch > 0 && previous.notes[i].pitch < 128)
            midi_turn_off_note(previous.notes[i].pitch);
        }

    for (int i = 0; i < 4; i++)
        {
            midi_write_note(next.notes[i].pitch, 127);
        }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms)
        return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}

void display_update(ssd1306_t *disp, char *line1, char *line2, char *line3, char *line4, char *line5)
{
    ssd1306_clear(disp);

    if (operating_mode == 0)
    {
        ssd1306_draw_string(disp, 0, 0, 1, "Melody");
    }
    else if (operating_mode == 1)
    {
        ssd1306_draw_string(disp, 0, 0, 1, "Chords");
    }
    else if (operating_mode == 2)
    {
        ssd1306_draw_string(disp, 0, 0, 1, "Fig B");
    }
    else if (operating_mode == 3)
    {
        ssd1306_draw_string(disp, 0, 0, 1, "Duet");
    }


    char channelName[8] = "";
    sprintf(channelName, "CHAN %d", midi_channel);
    ssd1306_draw_string(disp, 72, 0, 1, channelName);

    ssd1306_draw_string(disp, 0, 8, 1, "Key");
    char keyName[8] = "";
    get_note_name(key, keyName);

    ssd1306_draw_string(disp, 30, 8, 1, keyName);

    if (major)
        ssd1306_draw_string(disp, 84, 8, 1, "Major");
    else
        ssd1306_draw_string(disp, 84, 8, 1, "Minor");
    
    if (strlen(line1) != 0)
    {
        if (strlen(line1) <= 3)
            ssd1306_draw_string(disp, 56, 17, 1, line1);
        else
            ssd1306_draw_string(disp, 36, 17, 1, line1);
    }
    if (strlen(line2) != 0)
    {
        if (strlen(line2) <= 3)
            ssd1306_draw_string(disp, 56, 26, 1, line2);
        else
            ssd1306_draw_string(disp, 36, 26, 1, line2);
    }
    if (strlen(line3) != 0)
    {
        if (strlen(line3) <= 3)
            ssd1306_draw_string(disp, 56, 35, 1, line3);
        else
            ssd1306_draw_string(disp, 36, 35, 1, line3);
    }
    if (strlen(line4) != 0)
    {
        if (strlen(line4) <= 3)
            ssd1306_draw_string(disp, 56, 44, 1, line4);
        else
            ssd1306_draw_string(disp, 36, 44, 1, line4);
    }
    if (strlen(line5) != 0)
    {
        if (strlen(line5) <= 3)
            ssd1306_draw_string(disp, 56, 53, 1, line5);
        else
            ssd1306_draw_string(disp, 36, 53, 1, line5);
    }
    ssd1306_show(disp);
}

void setup_gpios()
{
    setup_ssd_gpios();

    gpio_init(ROTARY_1_CLK_PIN);
    gpio_init(ROTARY_1_DT_PIN);
    gpio_init(ROTARY_1_SW_PIN);
    gpio_init(ROTARY_2_CLK_PIN);
    gpio_init(ROTARY_2_DT_PIN);
    gpio_init(ROTARY_2_SW_PIN);
    gpio_init(ROTARY_3_CLK_PIN);
    gpio_init(ROTARY_3_DT_PIN);
    gpio_init(ROTARY_3_SW_PIN);

    gpio_set_dir(ROTARY_1_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_1_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_1_SW_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_SW_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_SW_PIN, GPIO_IN);

    init_sonar();

    gpio_init(MUX_1_A_PIN);
    gpio_init(MUX_1_B_PIN);
    gpio_init(MUX_1_C_PIN);
    gpio_init(MUX_2_A_PIN);
    gpio_init(MUX_2_B_PIN);
    gpio_init(MUX_2_C_PIN);
    gpio_init(MUX_2_DISABLE_PIN);

    gpio_set_dir(MUX_1_A_PIN, GPIO_OUT);
    gpio_set_dir(MUX_1_B_PIN, GPIO_OUT);
    gpio_set_dir(MUX_1_C_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_A_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_B_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_C_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_DISABLE_PIN, GPIO_OUT);

    gpio_init(FOOT_PEDAL_PIN);
    gpio_set_dir(FOOT_PEDAL_PIN, GPIO_IN);
    gpio_pull_up(FOOT_PEDAL_PIN);
}

void read_rotaries()
{
    // Keep track of if anything changed to update the display
    bool changed = false;
    // Read the first encoder
    bool currentStateCLK = gpio_get(ROTARY_1_CLK_PIN);

    if (currentStateCLK != prevStateCLK1 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_1_DT_PIN) == currentStateCLK)
        {
            // Counterclockwise
            key--;
            if (key < 0)
                key = 127;
        }
        else 
        {
            key++;
            if (key > 127)
                key = 0;
        }
        changed = true;
    }
    prevStateCLK1 = currentStateCLK;

    bool currentStateSW = gpio_get(ROTARY_1_SW_PIN);

    if (!currentStateSW && prevButtonState1)
    {
        changed = true;
        major = !major;
    }
    
    prevButtonState1 = currentStateSW;

    // Now read the second encoder
    currentStateCLK = gpio_get(ROTARY_2_CLK_PIN);

    if (currentStateCLK != prevStateCLK2 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_2_DT_PIN) == currentStateCLK)
        {
            // Counterclockwise
            operating_mode--;
            if (operating_mode < 0)
                operating_mode = 3;
        }
        else 
        {
            operating_mode++;
            if (operating_mode > 3)
                operating_mode = 0;
        }
        changed = true;
    }

    prevStateCLK2 = currentStateCLK;

    currentStateSW = gpio_get(ROTARY_2_SW_PIN);

    if (!currentStateSW && prevButtonState2)
        midi_reset_all();

    prevButtonState2 = currentStateSW;

    // Finally, read the third encoder
    currentStateCLK = gpio_get(ROTARY_3_CLK_PIN);

    if (currentStateCLK != prevStateCLK3 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_3_DT_PIN) == currentStateCLK)
        {
            // Counterclockwise
            midi_channel--;
            if (midi_channel < 0)
                midi_channel = 15;
        }
        else 
        {
            midi_channel++;
            if (midi_channel > 15)
                midi_channel = 0;
        }
        changed = true;
    }

    prevStateCLK3 = currentStateCLK;

    currentStateSW = gpio_get(ROTARY_3_SW_PIN);

    if (!currentStateSW && prevButtonState3)
        ; // TODO: currently has no use

    prevButtonState3 = currentStateSW;

    // update display
    if (changed)
        display_update(&disp, "", "", "", "", "");
}

void get_note_name(int pitch, char *name)
{
    strcpy(name, note_names[pitch % 12]);

    if (strlen(alt_note_names[pitch % 12]))
    {
        name[2] = pitch / 12 - 1 + '0';
        name[3] = '/';
        strcpy(name + 4 * sizeof(char), alt_note_names[pitch % 12]);
    }
    else
    {
        name[1] = pitch / 12 - 1 + '0';
    }
}

// Between 0 and 6. -1 or 7 will turn all off.
void setLeftMux (int light)
{
    switch (light)
    {
        // y0
        case 0:
        gpio_put(MUX_1_A_PIN, 0);
        gpio_put(MUX_1_B_PIN, 0);
        gpio_put(MUX_1_C_PIN, 0);
        break;

        // y6
        case 1:
        gpio_put(MUX_1_A_PIN, 0);
        gpio_put(MUX_1_B_PIN, 1);
        gpio_put(MUX_1_C_PIN, 1);
        break;

        // y5
        case 2:
        gpio_put(MUX_1_A_PIN, 1);
        gpio_put(MUX_1_B_PIN, 0);
        gpio_put(MUX_1_C_PIN, 1);
        break;

        // y4
        case 3:
        gpio_put(MUX_1_A_PIN, 0);
        gpio_put(MUX_1_B_PIN, 0);
        gpio_put(MUX_1_C_PIN, 1);
        break;

        // y3
        case 4:
        gpio_put(MUX_1_A_PIN, 1);
        gpio_put(MUX_1_B_PIN, 1);
        gpio_put(MUX_1_C_PIN, 0);
        break;

        // y7
        case 5:
        gpio_put(MUX_1_A_PIN, 1);
        gpio_put(MUX_1_B_PIN, 1);
        gpio_put(MUX_1_C_PIN, 1);
        break;

        // y1
        case 6:
        gpio_put(MUX_1_A_PIN, 1);
        gpio_put(MUX_1_B_PIN, 0);
        gpio_put(MUX_1_C_PIN, 0);
        break;

        // y2
        default:
        gpio_put(MUX_1_A_PIN, 0);
        gpio_put(MUX_1_B_PIN, 1);
        gpio_put(MUX_1_C_PIN, 0);
    }
}

// Between 0 and 7. -1 and 8+ will turn off

void setRightMux(int light)
{
    gpio_put(MUX_2_DISABLE_PIN, 1); // enables mux

    switch (light)
    {
        // y7
        case 0:
        gpio_put(MUX_2_A_PIN, 1);
        gpio_put(MUX_2_B_PIN, 1);
        gpio_put(MUX_2_C_PIN, 1);
        break;

        // y6
        case 1:
        gpio_put(MUX_2_A_PIN, 0);
        gpio_put(MUX_2_B_PIN, 1);
        gpio_put(MUX_2_C_PIN, 1);
        break;

        // y5
        case 2:
        gpio_put(MUX_2_A_PIN, 1);
        gpio_put(MUX_2_B_PIN, 0);
        gpio_put(MUX_2_C_PIN, 1);
        break;

        // y4
        case 3:
        gpio_put(MUX_2_A_PIN, 0);
        gpio_put(MUX_2_B_PIN, 0);
        gpio_put(MUX_2_C_PIN, 1);
        break;

        // y3
        case 4:
        gpio_put(MUX_2_A_PIN, 1);
        gpio_put(MUX_2_B_PIN, 1);
        gpio_put(MUX_2_C_PIN, 0);
        break;

        // y2
        case 5:
        gpio_put(MUX_2_A_PIN, 0);
        gpio_put(MUX_2_B_PIN, 1);
        gpio_put(MUX_2_C_PIN, 0);
        break;

        // y1
        case 6:
        gpio_put(MUX_2_A_PIN, 1);
        gpio_put(MUX_2_B_PIN, 0);
        gpio_put(MUX_2_C_PIN, 0);
        break;

        // y0
        case 7:
        gpio_put(MUX_2_A_PIN, 0);
        gpio_put(MUX_2_B_PIN, 0);
        gpio_put(MUX_2_C_PIN, 0);
        break;

        default:
        gpio_put(MUX_2_DISABLE_PIN, 0);
    }
}