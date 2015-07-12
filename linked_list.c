/*a linked list file that offers FIFO functions. It creates a
singly linked list with a head and tail pointer*/

/*  
    Copyright (C) 2013 Jason Giancono (jasongiancono@gmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/


#include <stdio.h>
#include <stdlib.h>
#include "linked_list.h"
LinkedList *createList()
{
    /*allocates a new list*/
    LinkedList *newLL = (LinkedList*)malloc(sizeof(LinkedList));
    /*points both head and tail to null*/
    newLL->head = NULL;
    newLL->tail = NULL;
    return newLL;
}

void insertLast(LinkedList *lL, void *inElement)
{
    /*allocates a new node*/
    LinkedListNode *newLLN = (LinkedListNode*)malloc(sizeof(LinkedListNode));       
    newLLN->element = inElement;
    /*only when the list is empty*/        
    if (lL->head == NULL)                                                            
    {
        lL->head = newLLN;
        lL->tail = newLLN;
        newLLN->next = NULL;
    }
    /*when the list has one or more element*/
    else       
    {
        LinkedListNode *tempPtr = lL->tail;
        newLLN->next = NULL;
        lL->tail = newLLN;
        tempPtr->next = lL->tail;
    }
}
/*does not free the element... USE WITH CAUTION or get memory leaks*/
void softRemoveFirst(LinkedList *lL)
{
    LinkedListNode *tempPtr = lL->head;
    /*checks to see if list is empty*/
    if (tempPtr == NULL);
    else if (tempPtr == lL->tail)
    {
        free(tempPtr);
        lL->head = NULL;
        lL->tail = NULL;
    }
    else
    {
        lL->head = tempPtr->next;
        free(tempPtr);
    }
}

void removeFirst(LinkedList *lL)
{
    LinkedListNode *tempPtr = lL->head;
    /*checks to see if list is empty*/
    if (tempPtr == NULL);
    else if (tempPtr == lL->tail)
    {
        free(tempPtr->element);
        free(tempPtr);
        lL->head = NULL;
        lL->tail = NULL;
    }
    else
    {
        lL->head = tempPtr->next;
        free(tempPtr->element);
        free(tempPtr);
    }
}

void *returnFirstElement(LinkedList *lL)
{
    void *tempI;
    if (lL->head == NULL)
        tempI = NULL;
    else
        tempI = lL->head->element;
    return tempI;
}


void freeList(LinkedList *lL)
{
    LinkedListNode *tempPtr;
    tempPtr = lL->head;
    while (tempPtr != NULL);
    {
        removeFirst(lL);
        tempPtr= lL->head;
    }
    free(lL);                                                                        /*finally deallocates the list*/
}
