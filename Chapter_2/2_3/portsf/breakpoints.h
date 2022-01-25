#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include <stdio.h>
#include <stdlib.h>
typedef struct breakpoint {
    double time;
    double value;
} breakpoint;

breakpoint maxpoint(const breakpoint* points, long npoints);
breakpoint* get_breakpoints(FILE* fp, long* psize);
int inrange(const breakpoint* points, double minval, double maxval, unsigned long npoints);
double val_at_brktime(const breakpoint* points, unsigned long npoints, double time );

#endif