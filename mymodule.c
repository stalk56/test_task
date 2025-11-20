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

    while (!kthread_should_stop()) {
    	mutex_lock(&list_lock);
    	currentNode = (data->direction == 0) ? data->list->head : data->list->tail;
		if (currentNode == NULL) {
			mutex_unlock(&list_lock);
			break;
		}
		int value = currentNode->value;
		deleteNode(data->list, currentNode);
		mutex_unlock(&list_lock);

		data->count_nodes++;
		// Обработка value)
		if (!data->direction) value = ~value;
		for (int i = 0; i < sizeof(value) * 8; i++) {
			data->count_bits += ((value >> i) & 1);
		}
    }
    if(data->direction == 0)
    {
    	pr_info("Thread 1: %d bits=0 in %d nodes\n", data->count_bits, data->count_nodes);
    	thread[0] = NULL;
    }
    else
    {
    	pr_info("Thread 2: %d bits=1 in %d nodes\n", data->count_bits, data->count_nodes);
    	thread[1] = NULL;
    }
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
