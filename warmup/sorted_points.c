#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "point.h"
#include "sorted_points.h"


struct sorted_points {
    struct point *ptptr;
    struct sorted_points *next;
};

struct sorted_points *
sp_init()
{
	struct sorted_points *sp;

	sp = (struct sorted_points *)malloc(sizeof(struct sorted_points));
	assert(sp);
        
        sp->ptptr = NULL;
        sp->next = NULL;

	return sp;
}

void
sp_destroy(struct sorted_points *sp)
{
    struct sorted_points *head = sp->next;
    
    
    struct sorted_points *curr = head;
    while (curr != NULL){
        head = curr->next;
        curr->next = NULL;
        free(curr->ptptr);
        free(curr);
        curr = head;
    }
    free(sp);
    return;
    
    

}

int
sp_add_point(struct sorted_points *sp, double x, double y)
{

    struct point *data;
    data = (struct point *)malloc(sizeof(struct point));
    data->x = x;
    data->y = y;
    
    struct sorted_points *head = sp->next;
    struct sorted_points *curr = head;
    struct sorted_points *prev = NULL;
    
    struct sorted_points *insert;
    insert = (struct sorted_points *)malloc(sizeof(struct sorted_points));
    insert->ptptr = data;
    insert->next = NULL;
    
    //if the list is empty, directly add the item
    if (head == NULL){
        insert->ptptr = data;
        sp->next = insert;
        return 1;
    }
        
    //if the list is not empty, then find the corrected place for insertion
    while(curr != NULL){
        
        if (point_compare(data, curr->ptptr) != 1)//when curr is equal to / greater than data
            break;
        
        prev = curr;
        curr = curr->next;
    }
    
    if (curr == NULL){
        prev->next = insert;
        return 1;
    }
        
    
    if (point_compare(data, curr->ptptr) == -1){//smaller than curr, insert between prev and curr
        
        //curr is the first element
        if (prev == NULL){
            insert->next = curr;
            sp->next = insert;
            return 1;
        }
        else{
            insert->next = curr;
            prev->next = insert;
            return 1;
        }
    }
    
    
    while(curr != NULL 
            && point_compare(data, curr->ptptr) == 0 
            && (data->x > curr->ptptr->x 
            || (data->x == curr->ptptr->x && data->y > curr->ptptr->y))){
        prev = curr;
        curr = curr->next;
    }
    
    if (curr == NULL){
        prev->next = insert;
        return 1;
    }
    
    if (prev == NULL){
        sp->next = insert;
        insert->next = curr;
        return 1;
    }
    
    prev->next = insert;
    insert->next = curr;
    return 1;
    
    
    return 0;
    
}



int
sp_remove_first(struct sorted_points *sp, struct point *ret)
{
    
    struct sorted_points *head = sp->next;
    
    
    if (head == NULL)  //empty list
        return 0;
    else{
        
        struct sorted_points *first = head;
        ret = point_set(ret, first->ptptr->x, first->ptptr->y);
        sp->next = first->next;
        first->next = NULL;
        free(first->ptptr);
        free(first);
        return 1;
    }
    
    
}

int
sp_remove_last(struct sorted_points *sp, struct point *ret)
{
    
    struct sorted_points *head = sp->next;
    
    struct sorted_points *curr = head;
    struct sorted_points *prev = NULL;
    
    //test for empty list
    if (head == NULL)
        return 0;
    
    //when it is not an empty list, find the last element
    while (curr->next != NULL){
        prev = curr;
        curr = curr->next;
    }
    
    //if there is only one element in the list;
    if (prev == NULL){
        ret = point_set(ret, curr->ptptr->x, curr->ptptr->y);
        sp->next = NULL;
        free(curr->ptptr);
        free(curr);
        return 1;
    }
    else{
        ret = point_set(ret, curr->ptptr->x, curr->ptptr->y);
        prev->next = NULL;
        free(curr->ptptr);
        free(curr);
        return 1;
    }
    
}

int
sp_remove_by_index(struct sorted_points *sp, int index, struct point *ret)
{

        

    struct sorted_points *curr = sp;
    int counter = 0;
    
    
    
    while (counter != index && curr->next != NULL){
        counter++;
        curr = curr->next;
    }
    
    if (counter != index || curr->next == NULL){
        ret = NULL;
        return 0;
    }
    
    struct sorted_points *temp = curr->next;
    curr->next = curr->next->next;
    temp->next = NULL;
    ret = point_set(ret, temp->ptptr->x, temp->ptptr->y);
    free(temp->ptptr);
    free(temp);
    return 1;
    
}

int
sp_delete_duplicates(struct sorted_points *sp)
{

    struct sorted_points *head = sp->next;
    
    struct sorted_points *curr = head;
    struct sorted_points *prev = NULL;
    
    int deletion = 0;
    
    while(curr != NULL){
        if (prev == NULL){
            prev = curr;
            curr = curr->next;
        }
        
        //test for duplicates
        if (prev->ptptr->x == curr->ptptr->x && prev->ptptr->y == curr->ptptr->y){
            //the case of duplicate element
            //delete curr
            deletion++;
            prev->next = curr->next;
            curr->next = NULL;
            free(curr->ptptr);
            free(curr);
            curr = prev->next;
        }
        else{
            prev = curr;
            curr = curr->next;
        }
        
    }
    
    
    
    
    return deletion;
    
}
