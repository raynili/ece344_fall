#include "common.h"

int fact(int num){
    if (num == 0)
        return 1;
    else if (num == 1)
        return 1;
    else
        return num * fact(num - 1);
}

int
main(int argc, char *argv[])
{
	
    
    int num;
    if (argc != 2){
        printf("Huh?");
        return -1;
    }
        
    num = atoi(argv[1]);
    double temp = atof(argv[1]);
    
    if (num != temp){   //The input is probably a double type
        printf("Huh?\n");
        return 0;
    }
    
    if (num > 12){
        printf("Overflow\n");
        return 0;
    }
    else if (num > 0){
        printf("%d\n", fact(num));
        return 0;
    }
    else
        printf("Huh?\n");
    return 0;  

}