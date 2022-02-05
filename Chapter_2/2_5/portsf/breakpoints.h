#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include <stdio.h>
#include <stdlib.h>
typedef struct breakpoint {
    double time;
    double value;
} breakpoint;

typedef struct breakpoint_stream {
    breakpoint* points;
    breakpoint leftpoint, rightpoint;
    unsigned long npoints;
    double curpos;
    double incr;
    double width;
    double height;
    unsigned long ileft, iright;
    int more_points;

} break_stream;

break_stream* new_breakpoint_stream(FILE * fp, unsigned long srate, unsigned long* size);
double breakpoints_stream_tick(break_stream* stream);
void bps_freepoints(break_stream* stream);
int bps_getminmax(break_stream* stream, double *minval, double *maxval);

/* functions for a single breakpoint */
breakpoint maxpoint(const breakpoint* points, long npoints);
breakpoint* get_breakpoints(FILE* fp, long* psize);
int inrange(const breakpoint* points, double minval, double maxval, unsigned long npoints);
double val_at_brktime(const breakpoint* points, unsigned long npoints, double time );
breakpoint maxpoint(const breakpoint* points, long npoints);
breakpoint minpoint(const breakpoint* points, long npoints);
#endif