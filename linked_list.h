/*definition of a linked list node, has a pointer to element and
a pointer to the next node*/
typedef struct LinkedListNode{
void *element;
struct LinkedListNode *next; 
} LinkedListNode;

/*definition of linked list type, just two pointers a head and tail*/
typedef struct {
LinkedListNode *head;
LinkedListNode *tail; 
} LinkedList;

/*allocates memory for a list and returns a pointer to that memory*/
LinkedList *createList();

/*all of the following functions need to be passed a pointer to the
linked list you want to manipulate*/

/*Only frees the node, not the element it points to. Useful if you
want to remove from a queue and place into a different queue*/
void softRemoveFirst(LinkedList *lL);
/*creates a new node and points it to the element (void pointer) and
adds it to the end of the list*/
void insertLast(LinkedList *lL, void *);
/*frees the first node and the element it points to*/
void removeFirst(LinkedList *lL);
/*returns a pointer to the first element*/
void *returnFirstElement(LinkedList *lL);
/*nuke the list, free all elements and nodes*/
void freeList(LinkedList *lL);
