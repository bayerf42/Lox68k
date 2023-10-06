#ifndef clox_ffp_glue_h
#define clox_ffp_glue_h

Real fabs(Real x);
int  trunc(Real x);

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

Real add(Real x, Real y);
Real sub(Real x, Real y);
Real mul(Real x, Real y);
Real div(Real x, Real y);
int  less(Real x, Real y);

Real intToReal(int n);
Real strToReal(const char* str, char** endptr);
int  realToInt(Real x);
void realToStr(char* str, Real x);

extern char  errno;

#endif