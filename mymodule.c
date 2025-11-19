#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/threads.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolai");
MODULE_DESCRIPTION("Linux module");

static DEFINE_MUTEX(list_lock);

#ifndef NULL
#define NULL   ((void *) 0)
#endif

typedef struct Node {
    int value;
    struct Node* prev;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
} DoublyLinkedList;

typedef struct {
    DoublyLinkedList* list;
    int direction;
    int count_bits;
    int count_nodes;
} ThreadData;

static struct task_struct* thread[2];
static ThreadData data[2];
static DoublyLinkedList list;

static void appendNode(DoublyLinkedList* list) {
    Node* newNode = (Node*)kmalloc(sizeof(Node), GFP_KERNEL);
    if (newNode == NULL)
        return;

    get_random_bytes(&newNode->value, sizeof(int));
    newNode->prev = NULL;
    newNode->next = NULL;
    if (list->head == NULL) {
        list->head = newNode;
        list->tail = newNode;
    } else {
        newNode->prev = list->tail;
        list->tail->next = newNode;
        list->tail = newNode;
    }
}

static void deleteNode(DoublyLinkedList* list, Node* node) {
    if (node == NULL) return;

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    kfree(node); // Освобождаем память
}

static int processList(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Node* currentNode;

    // Thread 1 processes from head (counts bits=0)
    if (data->direction == 0) {
    	do
    	{
    		mutex_lock(&list_lock);
    		currentNode = data->list->head;
    		int value = 0;
    		if(currentNode != NULL)
    		{
    			value = currentNode->value;
    			deleteNode(data->list, currentNode);
    		}
    		else
    		{
    			mutex_unlock(&list_lock);
    			break;
    		}
    		mutex_unlock(&list_lock);
    		data->count_nodes++;
//    		//вариант подсчета всех нулей
//    		for (int i = 0; i < sizeof(value) * 8; i++) {
//				if (((value >> i) & 1) == 0) {
//					data->count_bits++;
//				}
//			}
    		//вариант подсчета только значащих нулей
    		if(value == 0)
    			data->count_bits++;
    		else
    		{
				int startBit = 0;
				for (int i = sizeof(value) * 8 - 1; i >= 0; i--) {
					if( ((1<<i) & value) != 0 )
					{
						startBit = i;
						break;
					}
				}
				for(int i = 0; i < startBit; i++)
				{
					if( ((1<<i) & value) == 0 ) {
						data->count_bits++;
					}
				}
			}
    	}
    	while((data->list->head != NULL) && !kthread_should_stop());
    	pr_info("Thread 1: %d bits=0 in %d nodes\n", data->count_bits, data->count_nodes);
    }
    // Thread 2 processes from tail (counts bits=1)
    else {
    	do
		{
    		mutex_lock(&list_lock);
			currentNode = data->list->tail;
			int value = 0;
			if(currentNode != NULL)
			{
				value = currentNode->value;
				deleteNode(data->list, currentNode);
			}
			else
			{
				mutex_unlock(&list_lock);
				break;
			}
			mutex_unlock(&list_lock);
			data->count_nodes++;
			for (int i = 0; i < sizeof(value) * 8; i++) {
				if (((value >> i) & 1) == 1) {
					data->count_bits++;
				}
			}
		}
		while((data->list->tail != NULL) && (!kthread_should_stop()));
    	pr_info("Thread 2: %d bits=1 in %d nodes\n", data->count_bits, data->count_nodes);
    }
    thread[data->direction==0?0:1] = NULL;
    return 0;
}

static int __init hello_init(void) {
	pr_info("Start module!\n");
    int numElements = 1000; // размер списка

	list.head = NULL;
	list.tail = NULL;

	for (int i = 0; i < numElements; i++) {
		appendNode(&list);
	}

	data[0].list = &list;
	data[0].direction = 0;
	data[0].count_bits = 0;
	data[0].count_nodes = 0;

	data[1].list = &list;
	data[1].direction = 1;
	data[1].count_bits = 0;
	data[1].count_nodes = 0;

	thread[0] = kthread_run(&processList, &data[0], "direction1");
	thread[1] = kthread_run(&processList, &data[1], "direction2");
	for(int i = 0;  i<sizeof(thread)/sizeof(thread[0]); i++)
	{
		if(IS_ERR(thread[i]))
		{
			pr_err("Error starting thread%d\n",i);
			thread[i] = NULL;
		}
	}
    return 0; // Возврат 0 означает успешную загрузку модуля
}

static void __exit hello_exit(void) {
	for(int i = 0;  i<sizeof(thread)/sizeof(thread[0]); i++)
	{
		if (thread[i] != NULL)
			if (thread[i]->__state != TASK_DEAD)
			{
				int s = kthread_stop(thread[i]);
				pr_info("Stop the Thread%d status=%d\n", i,s);
			}
	}
	mutex_lock(&list_lock);
	while(list.head != NULL)
	{
		deleteNode(&list, list.head);
	}
	mutex_unlock(&list_lock);
	pr_info("Count of zeros %d in %d nodes\n", data[0].count_bits, data[0].count_nodes);
	pr_info("Count of ones %d in %d nodes\n", data[1].count_bits, data[1].count_nodes);
	pr_info("Goodbye!\n");
}

module_init(hello_init); // Указывает функцию инициализации
module_exit(hello_exit); // Указывает функцию выгрузки
