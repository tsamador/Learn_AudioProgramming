#include "breakpoints.h"

breakpoint maxpoint(const breakpoint* points, long npoints)
{
    int i;
    breakpoint point;

    point.time = points[0].time;
    point.value = points[0].value;

    for(i = 0; i < npoints; i++)
    {
        if(point.value < points[i].value)
        {
            point.time = points[i].time;
            point.value = points[i].value;
        }
    }

    return point;
}

/* Exercise 2.3.3 
    Implement a minpoint function similarly to maxpoint
*/
breakpoint minpoint(const breakpoint* points, long npoints)
{
    int i;
    breakpoint point;

    point.time = points[0].time;
    point.value = points[0].value;

    for(i = 0; i < npoints; i++)
    {
        if(point.value > points[i].value)
        {
            point.time = points[i].time;
            point.value = points[i].value;
        }
    }

    return point;
}

breakpoint* get_breakpoints(FILE* fp, long* psize)
{
    int got;
    long npoints = 0, size = 64;
    double lasttime = 0.0;
    breakpoint* points = NULL;
    char line[80];

    if(fp == NULL)
    {
        return NULL;
    }
    points = (breakpoint*) malloc(sizeof(breakpoint) * size);

    if(!points)
    {
        return NULL;
    }

    while(fgets(line, 80, fp)) 
    {
        
        got = sscanf(line, "%lf %lf", &points[npoints].time, &points[npoints].value);
        
        if(got < 0)
        {
            continue; //Empty line
        }

        if(got == 0) {
            printf("Line %ld has nonnumeric data\n", npoints+1);
            break;
        }

        if( got == 1)
        {
            printf("Incomplete breapoint data at line %ld", npoints+1);
            break;
        }

        if(points[npoints].time < lasttime)
        {
            printf("data error at point %ld: time not increasing\n", npoints+1);
            break;
        }

        lasttime = points[npoints].time;

        if(++npoints == size)
        {
            breakpoint* temp;
            size += npoints;
            temp = (breakpoint*) realloc(points,sizeof(breakpoint) * size);

            if(!temp)
            {
                npoints = 0;
                free(points);
                points = NULL;
                break;
            }
            points = temp;
        }
    }

    if(npoints)
    {
        *psize = npoints;
    }
    return points;
    

}

/* Validates if our breakpoints are within a valid range, tpically between -1.0 and 1.0 */
int inrange(const breakpoint* points, double minval, double maxval, unsigned long npoints)
{
    unsigned long i;
    int range_OK = 1;

    for( i= 0; i < npoints; i++)
    {
        if(points[i].value < minval || points[i].value > maxval)
        {
            range_OK = 0;
            break;
        }
    }
    return range_OK;
}

breakpoint left, right;
unsigned long ileft, iright;

double val_at_brktime(const breakpoint* points, unsigned long npoints, double time )
{
    unsigned long i;
    double frac, val, width;

    /* scan until we find a span containing out time */
    for(i = 1; i < npoints; i++)
    {
        if(time <= points[i].time)
            break;
    }

    /*maintain final value if time eyond end of data */
    if(i == npoints)
    {
        return points[i-1].value;
        /* Exercise 2.3.1 What are we assuming by using i-1?
            assuming that the breakpoints are all in order by time, 
            and that the previous one is the near (lower) breakpoint
        */
    }
    left = points[i-1];
    right = points[i];

    /* check for instant jump - two points with same time */
    width = right.time - left.time;

    if(width == 0.0)
        return right.value;
    
    frac = (time - left.time)/width;
    val = left.value  + ((right.value - left.value) * frac);
    return val;
}