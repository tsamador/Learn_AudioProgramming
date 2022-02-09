#ifndef OSCIL_TABLE_H
#define OSCIL_TABLE_H


#include <stdio.h>
#include <math.h>
#include <wave.h>

#define M_PI 3.1415926535897932
#define TWOPI (2.0 * M_PI)

typedef struct gTable {
    double *table;
    unsigned long length;
} g_table;

typedef struct trunc_table_oscil {
    OSCIL osc;
    const g_table* gtable;
    double dtablen;
    double size_over_srate;
} table_oscil;

g_table* new_sine_table(unsigned long length); 
void oscil_table_free(g_table** table);
table_oscil* new_oscil_trunc(double srate, const g_table* gtable, double phase);
double table_trunc_tick(table_oscil * tosc, double freq);


#endif