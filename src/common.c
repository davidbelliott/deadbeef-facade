#include "common.h"

#include <math.h>

int _mod(int a, int b) {
    int r = a % b;
    return r < 0 ? r + b : r;
}

float _fmod(float a, float b) {
    float r = fmod(a, b);
    return r < 0.0f ? r + b : r;
}

int _sign(double x) {
	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 1;
	} else {
		return 0;
	}
}
