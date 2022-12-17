#ifndef clox_mathffp_h
#define clox_mathffp_h

#include "common.h"

Real fabs(Real x);
Real sqrt(Real x);
Real sin(Real x);
Real cos(Real x);
Real tan(Real x);
Real sinh(Real x);
Real cosh(Real x);
Real tanh(Real x);
Real atan(Real x);
Real exp(Real x);
Real log(Real x);
Real pow(Real x, Real y);

Real intToReal(int n);
Real scanReal(const char* str);
void printReal(char* str, Real x);

extern char errno;

#endif