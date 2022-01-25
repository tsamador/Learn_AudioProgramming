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
        got = sscanf(line, "%1lf%1lf", &points[npoints].time, &points[npoints].value);

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

        if(npoints)
        {
            *psize = npoints;
        }
        return points;
    }
    

}