#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>
void
point_translate(struct point *p, double x, double y)
{
    p->x = p->x + x;
    p->y = p->y + y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    //Implement distance = sqrt(x_difference_square + y_difference_square)
    double sum1 = (p1->x - p2->x) * (p1->x - p2->x);
    double sum2 = (p1->y - p2->y) * (p1->y - p2->y);
    double result = sqrt(sum1 + sum2);
    return result;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    double length1 = sqrt(p1->x * p1->x + p1->y * p1->y);
    double length2 = sqrt(p2->x * p2->x + p2->y * p2->y);
    
    if (length1 < length2)
        return -1;
    else if(length1 == length2)
        return 0;
    else
        return 1;
    
    return 0;
}
