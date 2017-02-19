#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include "string.h"
#include <ctype.h>

#define MAX_HASH_SIZE 50000


//struct for word count element
struct wc {
    char *word;             //the word itself
    int count;              //count the number of appearence of the word
    int dirty;              //in the linked list, if there is another item behind 
    struct wc *next;        //linked list next pointer  
};


struct wc *
wc_init(char *word_array, long size)
{
    struct wc *wc;

    wc = (struct wc *)malloc(sizeof(struct wc) * MAX_HASH_SIZE);
    assert(wc);
    
    //initialize the instances in the array with default value
    int de;
    for (de = 0; de < MAX_HASH_SIZE; de++){
        wc[de].count = -1;
        wc[de].dirty = 0;
        wc[de].next  = NULL;
        wc[de].word  = NULL;
    }
    
    //parse the word_array to get all the words
    int i;
    int start = 0;
    int end   = 0;
    for (i = 0; i < size; i++){
        
        /*
         * (int)word_array[i] != 0x20                  //special char space
                && (int)word_array[i] != 0x09           //special char tab
                && (int)word_array[i] != 0x0d           //special char enter
                && (int)word_array[i] != 0x7f           //special char delete
                && (int)word_array[i] != 0x0a           //special char newline
                && (int)word_array[i] != 0x00
         * ((int)word_array[i] >= 65 && (int)word_array[i] <= 90)
                || ((int)word_array[i] >= 97 && (int)word_array[i] <= 122)
         */
        //only scan in the word if it is not a special char
        if (!isspace(word_array[i])){         
            
            start = i;
            end = i;
            
            int j = i;              //find the next unavailable char 
            while (!isspace(word_array[j])){
                end = end + 1;
                j = j + 1;
            }
            
            //from index start to index end - 1 are valid characters
            char *temp_word = (char *)malloc(sizeof(char) * (end - start + 1));
            unsigned long long ha_val = 0;
            j = 0;
            for(j = 0; j < end - start; j++){
                temp_word[j] = word_array[start + j];
                ha_val = ha_val * 31 + (int)word_array[start + j];      //hash function found online
            }
            temp_word[j] = '\0';            //append the null character to the end
            
            //try to find the same word in the hash table
            struct wc *curr = wc[ha_val % MAX_HASH_SIZE].next;
            struct wc *prev = &wc[ha_val % MAX_HASH_SIZE];
            int found = 0;                  //indicator for result of searching element
            while(curr != NULL){
                if (strcmp(curr->word, temp_word) == 0){
                    free(temp_word);
                    found = 1;
                    curr->count++;          //increment number for appearances
                    break;
                }
                prev = curr;
                curr = curr->next;
            }
            //if it is not found in the hash table, create a new instance and then put in it
            if (found == 0){
                
                struct wc *new_ele;
                new_ele = (struct wc *)malloc(sizeof(struct wc));
                new_ele->count = 1;
                new_ele->dirty = 0;
                new_ele->next  = curr;
                new_ele->word  = temp_word;
                prev->next = new_ele;
            }
            
            i = end;
            
        }
        else{
            continue;
        }
        
    }
    
    

    return wc;
}

void
wc_output(struct wc *wc)
{
    //loop through the array of elements and print those that is not empty
    int i;
    for (i = 0; i < MAX_HASH_SIZE; i++){
        struct wc *curr = &wc[i];
        while (curr->next != NULL){
            //print the word in the needed format
            int j = 0;
            while (curr->next->word[j] != '\0'){
                printf("%c", curr->next->word[j]);
                j = j + 1;
            }
            printf(":");
            printf("%d\n", curr->next->count);
            
            curr = curr->next;
        }
    }
}

void
wc_destroy(struct wc *wc)
{
    int i;
    for (i = 0; i < MAX_HASH_SIZE; i++){
        struct wc *curr = wc[i].next;
        struct wc *prev = &wc[i];
        while (curr != NULL){
            prev->next = curr->next;
            curr->next = NULL;
            free(curr->word);
            free(curr);
            curr = prev->next;
        }
    }
    
    free(wc);
}
