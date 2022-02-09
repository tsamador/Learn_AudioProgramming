
#include "gtable.h"



g_table* new_sine_table(unsigned long length)
{
    unsigned long i;
    double step;
    g_table *table = NULL;

    if (length == 0)
    {
        return NULL;
    }

    table = (g_table*)malloc(sizeof(g_table));

    if(!table)
    {
        return NULL;
    }

    table->table = (double*)malloc((length + 1) * sizeof(double));

    if(!table->table)
    {
        free(table);
        return NULL;
    }

    table->length = length;

    step = TWOPI/length; /* make a sine wave */

    for( i = 0; i < length; i++)
    {
        table->table[i] = sin(step * i);
    }
    table->table[i] = table->table[0]; /*gaurd point */
    return table;

}


void oscil_table_free(g_table** table)
{
    if(table && *table && (*table)->table)
    {
        free((*table)->table);
        free(*table);
        *table = NULL;
    }
}

table_oscil* new_oscil_trunc(double srate, const g_table* gtable, double phase)
{
    table_oscil  *osc;
    // /* Ensure we have a good gtable */
    if(!gtable || !gtable->table ||gtable->length == 0 )
    {
        return NULL;
    }    
    osc = (table_oscil*)malloc(sizeof(table_oscil));
    if(osc == NULL)
    {
        return NULL;
    }

    /* initialize the oscillator */

    osc->osc.current_frequency = 0.0;
    osc->osc.current_phase = gtable->length * phase;
    osc->osc.incr = 0.0;

    /* then Gtable specific things */
    osc->gtable = gtable;
    osc->dtablen = (double) gtable->length;
    osc->size_over_srate = osc->dtablen / srate;
    return osc;
}


double table_trunc_tick(table_oscil *t_osc, double freq)
{
    int index = (int)t_osc->osc.current_phase;
    double val;
    double dtablen = t_osc->dtablen;
    double curphase = t_osc->osc.current_phase;
    double* table = t_osc->gtable->table;

    if(t_osc->osc.current_frequency != freq)
    {
        t_osc->osc.current_frequency = freq;
        t_osc->osc.incr = t_osc->size_over_srate * t_osc->osc.current_frequency;
    }

    curphase += t_osc->osc.incr;
    while(curphase >= dtablen)
    {
        curphase -= dtablen;
    }
    while(curphase < 0.0)
    {
        curphase += dtablen;
    }

    t_osc->osc.current_phase = curphase;
    return table[index];
}

double table_inter_tick(table_oscil * tosc, double freq)
{
    int base_index = (int)tosc->osc.current_phase; // Grab our current phase location
    unsigned long next_index = base_index + 1; 
    double slope, frac, val;

    double dtablen = tosc->dtablen;
    double curphase = tosc->osc.current_phase;
    double* table = tosc->gtable->table;

    if(tosc->osc.current_frequency != freq)
    {
        tosc->osc.current_frequency = freq;
        tosc->osc.incr = tosc->size_over_srate * tosc->osc.current_frequency;
    }

    frac = curphase - base_index; // find decimal difference between phase and index
    val = table[base_index];
    slope = table[next_index] - val;
    val += (frac * slope);

    curphase += tosc->osc.incr;
    while(curphase >= dtablen)
    {
        curphase -= dtablen;
    }
    while(curphase < 0.0)
    {
        curphase += dtablen;
    }

    tosc->osc.current_phase = curphase;
    return val;
}