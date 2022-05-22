#include <stdlib.h>
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "pico/types.h"

#include "hc-sr04.h"

// for testing
#include "ssd1306.h"

#define SENSOR_1_TRIG_PIN 8
#define SENSOR_1_ECHO_PIN 9
#define SENSOR_2_TRIG_PIN 16
#define SENSOR_2_ECHO_PIN 17

#define MEDIAN_NUM_SAMPLES 5

// used for qsort
// TODO this loses precision on floats because it's casting to int...
static int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

void init_sonar()
{
    // setup the trigger pin in putput mode and echo pin to input mode
    gpio_init(SENSOR_1_TRIG_PIN);
    gpio_init(SENSOR_1_ECHO_PIN);
    gpio_set_dir(SENSOR_1_TRIG_PIN, GPIO_OUT);
    gpio_set_dir(SENSOR_1_ECHO_PIN, GPIO_IN);
    
    gpio_init(SENSOR_2_TRIG_PIN);
    gpio_init(SENSOR_2_ECHO_PIN);
    gpio_set_dir(SENSOR_2_TRIG_PIN, GPIO_OUT);
    gpio_set_dir(SENSOR_2_ECHO_PIN, GPIO_IN);
}

float measure_distance(int trigger_pin, int echo_pin, int max_range){
    float distance_cm = 0.0;

    // diagnostics
    //gpio_put(26, 0);

    gpio_set_input_enabled(echo_pin, 1 );
    gpio_put(trigger_pin, 0 ); // make sure that the pin is low
    busy_wait_ms(1);                // wait a bit to let things stabalize
    uint echo_result = gpio_get(echo_pin);
	// printf("\nPulsed the pin high echo pin is %d", echo_pin );
	
    gpio_put(trigger_pin, 1);  // go high
    busy_wait_ms(3);             // No need for 10 ms... only 10 us needed
    gpio_put(trigger_pin, 0);  // Switch the trigger low again to let the SR04 send the pulse
    
	/*
		The echo pin outputs a pulse between 150 micro-seconds and 25 mili-seconds, or if no object is found, it will send a 38 ms pulse. 
		speed of sound is 343 meters per second. This would depend upon the elevation and hummidity, but 
		sufficiently accurate for our application. 

		speed = distance travelled / time taken
		
		so, distance = (speed * time taken)/2 -- Divide by 2 because we are listening to the echo
		
		Now, speed of sound is 343 meters/second, whch is 3.43 centimeters per second, which is
		3.43/ 1000000 = 0.0343 cm/microsecond. 
		
		
	*/
    absolute_time_t  listen_start_time = get_absolute_time();
    //absolute_time_t  max_wait_time  = delayed_by_ms(listen_start_time, (int)(max_range * 2 / 34.3) + 5);  // No point waiting more than necessary.
	
    absolute_time_t  max_wait_time  = delayed_by_ms(listen_start_time, 19);  // No point waiting more than necessary. 38/2 = 19 should be enough (maybe?)

    while (true)
    {
        absolute_time_t  t_now = get_absolute_time();
        //if( t_now._private_us_since_boot > max_wait_time._private_us_since_boot )
        if (absolute_time_diff_us(max_wait_time, t_now) > 0)
        {
            //distance_cm = -69.0;
            break;
        }

        echo_result = gpio_get(echo_pin);
        if (echo_result != 0 ){   // We got an echo!
            //diagnostics
            //gpio_put(26, 1);

            absolute_time_t  first_echo_time = t_now;
            while (echo_result == 1 && absolute_time_diff_us(max_wait_time, t_now) < 0){
                echo_result = gpio_get(echo_pin );
                t_now = get_absolute_time();
            }
			if( echo_result == 1 ){
				break;	// will return 0 cm.
			}
            int64_t pulse_high_time = absolute_time_diff_us( first_echo_time, t_now );
			float pulse_high_time_float = (float) pulse_high_time;
            distance_cm = pulse_high_time_float * 0.01715; //pulse_high_time_float / 58.0 ;
            break;
        }
    }
    gpio_set_input_enabled(echo_pin, 0 );
    return distance_cm;
}

float measure_median_distance(int trigger_pin, int echo_pin, int max_range)
{
    float distances[MEDIAN_NUM_SAMPLES];

    for (int i = 0; i < MEDIAN_NUM_SAMPLES; i++)
    {
        distances[i] = measure_distance(trigger_pin, echo_pin, max_range);
    }

    qsort(distances, MEDIAN_NUM_SAMPLES, sizeof(float), cmpfunc);
    return distances[MEDIAN_NUM_SAMPLES / 2];
}

int make_interval(float distance, int divisions, int max_range)
{
    if (distance == 0.0)
        return -1;

    float gap_dist = (float)max_range / divisions;

    float interval = distance / gap_dist;

    // int interval = (int)distance; // make sure MAX_RANGE is a multiple of divisions
    //printf("gap dist %d and typecasted dist %d\n", gap_dist, interval);

    // interval /= gap_dist;

    //printf("now interval %d\n", interval);

    // low sol to high do is 11 divisions (sol, la, ti, do, re, mi, fa, sol, la, ti, do)
    // inversions are 7 (0, 6, 64, 7, 65, 43, 42)

    if (interval == divisions)
        return divisions - 1; // sensor is finnicky at the end so extend the last segment by one gap

    if (interval > divisions)
        return -1;

    return (int)interval;
}

int measure_median_interval(int trigger_pin, int echo_pin, int max_range, int divisions)
{
    int sorted_intervals[MEDIAN_NUM_SAMPLES];

    for (int i = 0; i < MEDIAN_NUM_SAMPLES; i++)
    {
        sorted_intervals[i] = make_interval(measure_distance(trigger_pin, echo_pin, max_range), divisions, max_range);
    }

    qsort(sorted_intervals, MEDIAN_NUM_SAMPLES, sizeof(float), cmpfunc);
    return sorted_intervals[MEDIAN_NUM_SAMPLES / 2];
}