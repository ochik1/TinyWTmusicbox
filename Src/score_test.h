/*
 * score_test_b20.h
 *
 * Created: 2025/09/04 15:27:24
 *  Author: ochik
 */ 


#ifndef SCORE_TEST_B20_H_
#define SCORE_TEST_B20_H_

#define RATE 2
const uint16_t score_tick[] = {
	 100 * RATE , 200 * RATE , 300 * RATE , 400 * RATE , 500 * RATE , 600 * RATE , 700 * RATE ,
	 800 * RATE , 900 * RATE ,1000 * RATE ,1100 * RATE ,1200 * RATE ,1300 * RATE ,1400 * RATE ,
	1500 * RATE ,1600 * RATE ,1700 * RATE ,1800 * RATE ,1900 * RATE ,2000 * RATE ,2100 * RATE ,
	2200 * RATE ,2300 * RATE ,2400 * RATE ,2500 * RATE ,2600 * RATE ,2700 * RATE ,2800 * RATE ,
	2900 * RATE ,3000 * RATE ,3100 * RATE ,3200 * RATE ,3300 * RATE ,3400 * RATE ,3500 * RATE ,
	3600 * RATE ,3700 * RATE ,3900 * RATE 
};

const uint8_t score_note[] = {
	0,	2,  4,  5,  7,  9, 11,
	12, 14, 16, 17, 19, 21, 23,
	24, 26, 28, 29, 31, 33, 35,
	36, 38, 40, 41, 43, 45, 47,
	48, 50, 52, 53, 55, 57, 59,
	60, 62, 255
};



#endif /* SCORE_TEST_B20_H_ */