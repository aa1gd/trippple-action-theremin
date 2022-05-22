#pragma once

void init_sonar();
float measure_distance(int trigger_pin, int echo_pin, int max_range);
float measure_median_distance(int trigger_pin, int echo_pin, int max_range);
int make_interval(float distance, int divisions, int max_range);
int measure_median_interval(int trigger_pin, int echo_pin, int max_range, int divisions);