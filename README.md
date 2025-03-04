# GC Library
Сборщик мусора, реализующий алгоритм Mark-and-Sweep, для С программ с поддержкой многопоточности.

## Оглавление  
- [Quickstart](#quickstart)  
  - [Установка](#установка)  
  - [Основное использование](#основное-использование)  
- [Core API](#core-api)  
  - [Создание GC](#создание-gc)  
  - [Выделение памяти](#выделение-памяти)  
  - [Освобождение памяти](#освобождение-памяти)  
  - [Помечание корней](#помечание-корней)  
  - [Запуск сборки мусора](#запуск-сборки-мусора)
    - [Пример с многопоточностью](#пример-с-многопоточностью)  
  - [Фоновая работа](#фоновая-работа)  
  - [Завершение работы](#завершение-работы)  
  - [Полезные макросы](#полезные-макросы)  
- [Важно](#важно)  
- [Концепция](#концепция)  

## Quickstart
### Установка
Установаите библиотику папку в директроию проекта.
```shell
git clone https://github.com/MrSior/gc-lib
```

В CMakeLists.txt проекта добвьте директорию библиотеки в какчестве поддиректории. И добавьте в пути дирректорию ```/include```.

```cmake
add_subdirectory(<"путь до библиотеки">/gc-lib)
include_directories(<"путь до библиотеки">/gc-lib/include)
```

```cmake
# Пример, если gc-lib находится рядом с CMakeLists.txt

include_directories(${CMAKE_SOURCE_DIR}/gc-lib/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/gc-lib)
```

### Основное использование
Пример кода ```main.c```, используюший библиотеку
```c
#include <stdio.h>
#include <stdlib.h>
#include "gc/gc.h"

int main(int argc, char* argv[]) {
    GC_CREATE();
    int* val;
    GC_MARK_ROOT(val);
    GC_MALLOC(val, 4);

    *val = 123;
    printf("%d\n", *val);
    val = NULL;
    
    GC_COLLECT(THREAD_LOCAL);
    GC_STOP();

    return 0;
}
```

## Core API
Описание API, находящегося в [gc.h](./include/gc/gc.h)

### Создание GC
Для объявления сборщика мусора для данного потока небходимо вызвать функцию ```gc_create(pthread_t)```, в которую необходимо при помощи ```pthread_self()``` передать id данного потока. Функция ```gc_create(pthread_t)``` возвращает обработичк типа ```gc_handler```, в котором лежат указатели на функции для дальнейшого взаимодейтсвия с gc.

### Выделение памяти
Для вывделения памяти реализован malloc, похожий на стандартный. В gc_handler есть указатель ```void(*gc_malloc)(pthread_t, void**, size_t)```, который принимает id данного потока, указатель на перемунню, в которую надо записать адрес блока выделенной памяти, размер необходимого блока.

### Освобождение памяти
Аналог free() **TBA**

### Помечание корней
Для сканирования памяти кучи сборщику мусора необходимо отметить "корневые вершины": переменные на стеке, которые хранят в седе адреса кучи. Их можно указать при помощи вызова функции через указатель ```void(*mark_root)(pthread_t, void*)``` в ```gc_handler```. Функция принимает id данного потока и адрес переменной стека.

Аналогично при помощи ```void(*unmark_root)(pthread_t, void*)``` снять отметку "коренвой вершины" со стековой переменной. Параметры вызова такие же.

### Запуск сборки мусора
Для запуска сборки мусора, необходимо через указатель ```void(*collect)(pthread_t, int)``` в ```gc_handler``` вызвать соотвутсвующую функцию, которая принимает id данного потока и флаг типа сборки.
Про флаг сборки:
Так как библиотека поддерживает localthread и multithread программы, очистку от мусора можно запустить в двух режимах:
- ```0 = THREAD_LOCAL```
    Отчистка мусора среди аллокаций, сделанных только данным потоком.
- ```1 = GLOBAL```
    Отчитска мусора происходит среди аллокаций, сделанных всеми потоками. При вызове происходит "stop the world", когда все потока остонавливаются и только после завершения сборки возобновляют работу.

#### Пример с многопоточностью
```c

// Поток, который "создаёт мусор". Каждые 2 сек. выделяет память и теряет указаетль.
void* first_thread_func(void* arg) {
    gc_handler* handler = gc_create(pthread_self());
    int* val;
    handler->mark_root(pthread_self(), &val);
    
    for (size_t i = 0; i < 3; i++)
    {
        handler->gc_malloc(pthread_self(), (void**)&val, 4);
        *val = 123;
        sleep(2);
    }
    
    printf("Thread with ID: %lu (WORKER) end\n", (unsigned long)pthread_self());
    gc_stop(pthread_self());
    return NULL;
}

// Поток, который вызовет глобальную сборку мусора.
void* second_thread_func(void* arg) {
    gc_handler* handler = gc_create(pthread_self());
    sleep(5);

    handler->collect(pthread_self(), GLOBAL);
    gc_stop(pthread_self());

    printf("Thread with ID: %lu (CALLER CLEANING) end\n", (unsigned long)pthread_self());
    return NULL;
}

int main(int argc, char* argv[]) {
    const int num_thread = 10;
    pthread_t threads[num_thread];

    for (size_t i = 0; i < num_thread - 1; i++)
    {
        if (pthread_create(&threads[i], NULL, first_thread_func, NULL) != 0)
        {
            perror("pthread_create failed");
            return EXIT_FAILURE;
        }
    }

    if (pthread_create(&threads[9], NULL, second_thread_func, NULL) != 0)
        {
            perror("pthread_create failed");
            return EXIT_FAILURE;
        }


    for (size_t i = 0; i < num_thread; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("pthread_join failed");
            return EXIT_FAILURE;
        }   
    }
    
    printf("All threads have finished.\n");
    return 0;
}
```

За 5 секунд, которые ждёт Caller Cleaning Thread, каждый из 9 потоков успевает выделить 3 раза по 4 байта, при этом утеряв 2 раза указетль на выделенную пямать. Если выставть ```LOG_LVL = LOG_LEVEL::INFO```, то в логах мы увидем 18 найденых мусорных блоков памяти, которые были освобождены.

### Фоновая работа
**TBA**

### Завершение работы
В конце работы потока необходимо вызвать функцию ```gc_stop(pthread_t tid)```, которая завершит работу gc и освободит всю выделенную через него память на куче.

### Полезные макросы
| Макрос | Код |
|--------|-----|
| ```GC_CREATE()``` | ```gc_handler* __handler = gc_create(pthread_self());``` |
| ```GC_MALLOC(val, size) ``` | ```__handler->gc_malloc(pthread_self(), (void**)(val), (size));``` |
| ```GC_MARK_ROOT(val)``` | ```__handler->mark_root(pthread_self(), (void*)(&(val)));``` |
| ```GC_COLLECT(flag)``` | ```__handler->collect(pthread_self(), (flag));``` |
| ```GC_STOP()``` | ```gc_stop(pthread_self());``` |

## Важно
При созданнии сборщика мусора к потоку также привязывается обработчик сигнала ```SIGUSR1```, необходимый для механизма "stop the world". Если потоко использует gc, то **НЕ** переопределяется обработчик сигнала ```SIGUSR1```.


## Концепция
Основной принцип библиотеки - это 1 поток, 1 сборщик мусора. Ядром библиотеки является thread-pool, через который проходят все операции с памятью от всех потоков. Поэтому для использования необходимо создать сборщик для данного потока и получить api-структуру с методами для работы с памятью и сборщиком мусора.


