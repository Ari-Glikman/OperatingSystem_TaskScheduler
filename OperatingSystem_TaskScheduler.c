//-----------------------------------------
// THIS IMPLEMENTS BOTH SHORTEST JOB FIRST (SJF) AND MULTI-LEVEL-FEEDBACK-QUEUE (MLFQ) SCHEDULING POLICIES
//-----------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

//times not specified are in microseconds
#define MAX_CHARS_PER_LINE 100
#define NANOS_PER_USEC 1000
#define USEC_PER_SEC 1000000
#define NANOS_PER_SEC 1000000000
#define TIME_SLICE 50 
#define TOTALTASKS 100
#define TIMED_WAIT_SECONDS 2
#define PRIORITY_BOOST_TIME 5000
struct timespec lastPBoost;
struct timespec arrive;
typedef enum bool
{
    false,
    true
} bool;

//'jobs' to 'run' will be stored as instances of this struct
typedef struct task
{
    char *task_name;
    int task_type;
    int task_length; //time left, updated after being run
    int odds_of_IO;
    bool firstrun;
    struct timespec firstrunTime;
    struct timespec end;
    struct timespec timeOfLastBoost;
    int time_in_curr_priority;
    int priority;
} task;

//priority 1 (also used as only queue for sjf)
task taskQueue[TOTALTASKS];
int numTasks = TOTALTASKS;

//priority 2
task secondPriority[TOTALTASKS];
int secondPriorityTasks;

//priority 3
task thirdPriority[TOTALTASKS];
int thirdPriorityTasks;

task completedTasks[TOTALTASKS];
int doneTasks = 0; //total, not per queue
//next task to run
task scheduledTask;

bool allTasksCompleted = false;
bool taskRequest = false;

int cpuAmount = 0; //number of threads

// locks
//note that to avoid deadlock "scheduledtaskaccess" should be held when holding queue access
//grab scheduled task access and only then can you grab queue access
pthread_mutex_t queueAccess;    //access to tasks that are on a queue and not yet finished
pthread_mutex_t scheduledTaskAccess;    //access to the global scheduledTask that is to be run next
pthread_mutex_t doneAccess;     //access to the done area 

//condition variables
pthread_cond_t getTask;  //cpu tells scheduler it is ready for a task
pthread_cond_t haveTask; //scheduler tells cpus there is a task available

char *policy;

//work to do for task
static void microsleep(unsigned int usecs)
{
    long seconds = usecs / USEC_PER_SEC;
    long nanos = (usecs % USEC_PER_SEC) * NANOS_PER_USEC;
    struct timespec t = {.tv_sec = seconds, .tv_nsec = nanos};
    int ret;
    do
    {
        ret = nanosleep(&t, &t);
        // need to loop, `nanosleep` might return before sleeping
        // for the complete time (see `man nanosleep` for details)
    } while (ret == -1 && (t.tv_sec || t.tv_nsec));
}


//------------------------------------------------------
// comparator
//
// PURPOSE: for sorting queue, used by scheduler and qsort
// compares two tasks and returns the difference between first argument and second argument's length (must be tasks)
// INPUT PARAMETERS:
//     Two tasks
// OUTPUT PARAMETERS:
//     Returns the difference in time length of task1 - task 2
//------------------------------------------------------

int comparator(const void *task1_ptr, const void *task2_ptr)
{
    task *task1 = (task *)task1_ptr;
    task *task2 = (task *)task2_ptr;
    int task1_time = task1->task_length;
    int task2_time = task2->task_length;
    return (task1_time - task2_time);
}
//------------------------------------------------------
// runDiagnostics
//
// PURPOSE: once the program is done analyze the response time and turnaround time for each type of task
// uses lock: doneAccess
//------------------------------------------------------
void runDiagnostics()
{
    int type0Count = 0;
    int type1Count = 0;
    int type2Count = 0;
    int type3Count = 0;
    int type0SumTurnaround = 0;
    int type1SumTurnaround = 0;
    int type2SumTurnaround = 0;
    int type3SumTurnaround = 0;
    int type0SumResponse = 0;
    int type1SumResponse = 0;
    int type2SumResponse = 0;
    int type3SumResponse = 0;

    pthread_mutex_lock(&doneAccess);
    for (int i = 0; i < TOTALTASKS; i++)
    {

        task myTask = completedTasks[i];

        if (myTask.task_type == 0)
        {
            type0Count++;
            type0SumTurnaround += (myTask.end.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.end.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
            type0SumResponse += (myTask.firstrunTime.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.firstrunTime.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
        }
        else if (myTask.task_type == 1)
        {
            type1Count++;
            type1SumTurnaround += (myTask.end.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.end.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
            type1SumResponse += (myTask.firstrunTime.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.firstrunTime.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
        }
        if (myTask.task_type == 2)
        {
            type2Count++;
            type2SumTurnaround += (myTask.end.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.end.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
            type2SumResponse += (myTask.firstrunTime.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.firstrunTime.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
        }
        if (myTask.task_type == 3)
        {
            type3Count++;
            type3SumTurnaround += (myTask.end.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.end.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
            type3SumResponse += (myTask.firstrunTime.tv_nsec - arrive.tv_nsec) / NANOS_PER_USEC + (myTask.firstrunTime.tv_sec - arrive.tv_sec) * USEC_PER_SEC;
        }
    }
    pthread_mutex_unlock(&doneAccess);
    if (strcmp(policy, "sjf") == 0)
        printf("Using SJF with %d CPUs.\n", cpuAmount);

    else
        printf("Using MLFQ with %d CPUs.\n", cpuAmount);

    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            printf("Average turnaround time per type:\n");
        }
        else
        {
            printf("Average response time per type:\n");
        }
        if (type0Count > 0)
        {
            if (i == 0)
                printf("Type 0: %d usec\n", type0SumTurnaround / type0Count);
            else
            {
                printf("Type 0: %d usec\n", type0SumResponse / type0Count);
            }
        }
        else
        {
            printf("Type 0: 0 usec\n");
        }

        if (type1Count > 0)
        {
            if (i == 0)
                printf("Type 1: %d usec\n", type1SumTurnaround / type1Count);
            else
                printf("Type 1: %d usec\n", type1SumResponse / type1Count);
        }
        else
        {
            printf("Type 1: 0 usec\n");
        }
        if (type2Count > 0)
        {
            if (i == 0)
                printf("Type 2: %d usec\n", type2SumTurnaround / type2Count);
            else
                printf("Type 2: %d usec\n", type2SumResponse / type2Count);
        }
        else
        {
            printf("Type 2: 0 usec\n");
        }

        if (type3Count > 0)
        {
            if (i == 0)
                printf("Type 3: %d usec\n", type3SumTurnaround / type3Count);
            else
                printf("Type 3: %d usec\n", type3SumResponse / type3Count);
        }
        else
        {
            printf("Type 3: 0 usec\n");
        }
    }
}


//------------------------------------------------------
// priorityBoost
//
// PURPOSE: must hold queue access lock to call this function
// checks for priority boost and if appropriate (time is right) implements it (moves all non top priority tasks to top priority queue)
// note that priority boost is meant to give lower priority tasks a fair chance at running so there is no priority boost/reset for tasks that are already in the top priority
// tasks that are currently on a cpu will not be reached by this boost so the cpu has its own boost mechanism
//------------------------------------------------------
void priorityBoost()
{
    struct timespec currTime;
    clock_gettime(CLOCK_REALTIME, &currTime);
    if (((currTime.tv_nsec - lastPBoost.tv_nsec) / NANOS_PER_USEC) + (currTime.tv_sec - lastPBoost.tv_sec) * USEC_PER_SEC >= PRIORITY_BOOST_TIME)
    {
        //DO PRIORITY BOOST
        for (int i = 0; i < secondPriorityTasks; i++)
        {
            secondPriority[i].priority = 1;
            secondPriority[i].time_in_curr_priority = 0;
            secondPriority[i].timeOfLastBoost = currTime;
            taskQueue[numTasks] = secondPriority[i];
            numTasks++;
        }
        //no need to reset second priority queue, just overwrite by resetting counter to 0
        secondPriorityTasks = 0;

        for (int i = 0; i < thirdPriorityTasks; i++)
        {
            thirdPriority[i].priority = 1;
            thirdPriority[i].time_in_curr_priority = 0;
            thirdPriority[i].timeOfLastBoost = currTime;
            taskQueue[numTasks] = thirdPriority[i];
            numTasks++;
        }
        //don't have to reset third priority queue, instead just overwrite by resetting counter to 0
        thirdPriorityTasks = 0;
        clock_gettime(CLOCK_REALTIME, &lastPBoost);
    }
}

//------------------------------------------------------
// scheduler
//
// PURPOSE: 
// this thread chooses what task is to be scheduled next (taking into consideration policy)
// it then removes the task from the queue it is in and schedules it by telling cpus that a new task is available in the global variable scheduledTask
// if there is a timeout (2 seconds) we assume there is no more tasks to run
// uses locks: scheduledTaskAccess, queueAccess
//------------------------------------------------------

void *scheduler()
{
    task nextTask;   //task to become scheduledTask
    bool taskExists; //check that task is valid
    struct timespec maxWait;
    int timedOut = 0;
    while (!allTasksCompleted) //work to do!
    {
        taskExists = false;
        pthread_mutex_lock(&scheduledTaskAccess);
        while (taskRequest)
        { //a different thread requested a task, wait
            clock_gettime(CLOCK_REALTIME, &maxWait);
            maxWait.tv_sec += TIMED_WAIT_SECONDS;
            timedOut = pthread_cond_timedwait(&getTask, &scheduledTaskAccess, &maxWait);

            if (timedOut)
            {
                taskExists = false;
                allTasksCompleted = true;
                taskRequest = false; //break out of while
            }
        }
        if (!timedOut)
        {
            pthread_mutex_lock(&queueAccess);
            if (strcmp(policy, "sjf") == 0)
            {
                assert(numTasks >= 0);
                if (numTasks > 0)
                {
                    taskExists = true;
                    qsort(taskQueue, numTasks, sizeof(task), comparator);
                    nextTask = taskQueue[0];
                    for (int i = 0; i < numTasks; i++)
                    {
                        taskQueue[i] = taskQueue[i + 1];
                    }
                    numTasks--;
                }
            }
            else
            { //MLFQ
                //CHECK PRIORITY BOOST
                priorityBoost();

                if (numTasks > 0)
                {
                    taskExists = true;
                    nextTask = taskQueue[0];
                    for (int i = 0; i < numTasks; i++)
                    {
                        taskQueue[i] = taskQueue[i + 1];
                    }
                    numTasks--;
                }
                else if (secondPriorityTasks > 0)
                {
                    taskExists = true;
                    nextTask = secondPriority[0];
                    for (int i = 0; i < secondPriorityTasks; i++)
                    {
                        secondPriority[i] = secondPriority[i + 1];
                    }
                    secondPriorityTasks--;
                }
                else if (thirdPriorityTasks > 0)
                {
                    taskExists = true;
                    nextTask = thirdPriority[0];
                    for (int i = 0; i < thirdPriorityTasks; i++)
                    {
                        thirdPriority[i] = thirdPriority[i + 1];
                    }
                    thirdPriorityTasks--;
                }
            } //else (MLFQ)

            if (taskExists)
            {
                scheduledTask = nextTask;
                if (scheduledTask.firstrun)
                {
                    scheduledTask.firstrun = false;
                    clock_gettime(CLOCK_REALTIME, &scheduledTask.firstrunTime);
                }
            }
            pthread_mutex_unlock(&queueAccess);

            //now the signal has arrived that a task is requested and we have the lock to access the scheduled task variable w/out worrying it is in use by a different thread
            if (doneTasks == TOTALTASKS) //will not be scheduled again
            {
                allTasksCompleted = true;
            }
            taskRequest = true; //task has been submitted to cpus, ready for next tsak
        }
        pthread_cond_signal(&haveTask); //wake cpus searching for task
        pthread_mutex_unlock(&scheduledTaskAccess);
    }
    taskRequest = false;
    return NULL;
}


//------------------------------------------------------
// cpu
//
// PURPOSE: 
// this is the thread(s) that act as cpu(s)
// it will communicate via condition variables with the scheduler as to when it wants a task and when one is available
// if a task is available it succeeeds in grabbing the lock it will get task via the global variable scheduledTask
// if a task it not done and it is needed to be put back into a queue it will check if appropriate to boost it as it may have been on cpu while boost from scheduler happened
// uses locks: scheduledTaskAccess, queueAccess, doneAccess
//------------------------------------------------------
void *cpu()
{
    task myTask;
    int executeTime;
    int randNum;
    bool myTaskExists;
    struct timespec maxWait;
    struct timespec timeNow;
    int timedOut = 0;
    while (!allTasksCompleted) //work to do!
    {
        myTaskExists = true;
        pthread_mutex_lock(&scheduledTaskAccess);
        while (!taskRequest)
        { //a different thread requested a task, wait
            clock_gettime(CLOCK_REALTIME, &maxWait);
            maxWait.tv_sec += TIMED_WAIT_SECONDS;
            timedOut = pthread_cond_timedwait(&haveTask, &scheduledTaskAccess, &maxWait);
            if (timedOut) //timed out
            {
                allTasksCompleted = true;
                taskRequest = true; //to break out of loop
            }
        }
        if (!timedOut)
        {
            if (!allTasksCompleted)
            {
                myTask = scheduledTask;
            }
            if (myTask.task_length <= 0)
            {
                allTasksCompleted = true;
                myTaskExists = false;
            }
            if (myTaskExists)
            {
                if (strcmp(policy, "sjf") == 0)
                    executeTime = myTask.task_length; //it's the shortest job so run entire time (unless i/o happens, in which case yield)
                else
                    executeTime = TIME_SLICE;

                randNum = (rand() % 100) + 1; //random number 1-100 (inclusive)

                if (randNum <= myTask.odds_of_IO)
                {                              //does io
                    executeTime = rand() % 51; //0 to 50 us of execution time before io
                    if (executeTime > myTask.task_length)
                        executeTime = myTask.task_length;
                }
                if (strcmp(policy, "mlfq") == 0)
                {
                    if (executeTime + myTask.time_in_curr_priority > 200 && myTask.priority < 3)
                    {
                        executeTime = 200 - myTask.time_in_curr_priority;
                    }
                }
                myTask.task_length -= executeTime;
                myTask.time_in_curr_priority += executeTime;
                if (myTask.task_length > 0) //task is not done, put back into queue
                {
                    pthread_mutex_lock(&queueAccess);
                    if (strcmp(policy, "sjf") == 0)
                    {
                        taskQueue[numTasks] = myTask;
                        numTasks++;
                    }
                    else //MLFQ
                    {
                        clock_gettime(CLOCK_REALTIME, &timeNow);
                        if (myTask.time_in_curr_priority >= 200 && myTask.priority < 3)
                        {
                            myTask.priority += 1;
                            myTask.time_in_curr_priority = 0;
                        }
                        if (myTask.priority == 1)
                        {
                            taskQueue[numTasks] = myTask;
                            numTasks++;
                        }
                        else if (myTask.priority == 2)
                        {
                            if ((timeNow.tv_nsec - myTask.timeOfLastBoost.tv_nsec) / NANOS_PER_USEC + (timeNow.tv_sec - myTask.timeOfLastBoost.tv_sec) * USEC_PER_SEC >= PRIORITY_BOOST_TIME)
                            { //boost
                                myTask.timeOfLastBoost = timeNow;
                                myTask.time_in_curr_priority = 0;
                                myTask.priority = 1;
                                taskQueue[numTasks] = myTask;
                                numTasks++;
                            }
                            else
                            { //no boost
                                secondPriority[secondPriorityTasks] = myTask;
                                secondPriorityTasks++;
                            }
                        }
                        else //priority 3
                        {
                            if ((timeNow.tv_nsec - myTask.timeOfLastBoost.tv_nsec) / NANOS_PER_USEC + (timeNow.tv_sec - myTask.timeOfLastBoost.tv_sec) * USEC_PER_SEC >= PRIORITY_BOOST_TIME)
                            { //boost
                                myTask.timeOfLastBoost = timeNow;
                                myTask.time_in_curr_priority = 0;
                                myTask.priority = 2;
                                taskQueue[numTasks] = myTask;
                                numTasks++;
                            }
                            else
                            {   //no boost
                                thirdPriority[thirdPriorityTasks] = myTask;
                                thirdPriorityTasks++;
                            }
                        }
                    }
                    pthread_mutex_unlock(&queueAccess);
                }
                //task is done
            }
        } //not timed out
        if (!allTasksCompleted)
            taskRequest = false; //this cpu requests a task
        pthread_cond_signal(&getTask);
        pthread_mutex_unlock(&scheduledTaskAccess);

        //execute task
        if (myTaskExists)
        {
            microsleep(executeTime);
            if (myTask.task_length <= 0)
            {
                pthread_mutex_lock(&doneAccess);
                clock_gettime(CLOCK_REALTIME, &myTask.end);
                completedTasks[doneTasks] = myTask;
                doneTasks++;
                if (doneTasks == 100)
                {
                    allTasksCompleted = true;
                }
                pthread_mutex_unlock(&doneAccess);
            }
        }
    }
    return NULL;
}

//populate task array (priority 1) at appropiate index
void populateTasks(char *taskLine, int idx)
{
    char *myLine = strdup(taskLine);
    taskQueue[idx].task_name = strtok(myLine, " ");
    taskQueue[idx].task_type = atoi(strtok(NULL, " "));
    taskQueue[idx].task_length = atoi(strtok(NULL, " "));
    taskQueue[idx].odds_of_IO = atoi(strtok(NULL, " "));
    taskQueue[idx].firstrun = true;
    taskQueue[idx].priority = 1;
    taskQueue[idx].time_in_curr_priority = 0;
    clock_gettime(CLOCK_REALTIME, &taskQueue[idx].timeOfLastBoost);
}

bool validLine(char *line)
{
    bool valid = false;
    if (line[0] == 's' || line[0] == 'm' || line[0] == 'l' || line[0] == 'i')
        valid = true;
    return valid;
}

//------------------------------------------------------
// createPool
//
// PURPOSE: create and join cpu threads and 1 scheduler thread
// INPUT PARAMETERS:
//     How many CPUs should be created/used
//------------------------------------------------------

void createPool(int cpuCount)
{
    //make thread/cpu pool
    pthread_t cpus[cpuCount]; //make cpus
    pthread_t schedule_thread;
    for (int i = 0; i < cpuCount; i++)
    {
        pthread_create(&cpus[i], NULL, &cpu, NULL);
    }
    pthread_create(&schedule_thread, NULL, &scheduler, NULL);

    for (int i = 0; i < cpuCount; i++)
    {
        pthread_join(cpus[i], NULL);
    }
    pthread_join(schedule_thread, NULL);
}

int main(int argc, char *argv[])
{
    char *cpuString = argv[1];
    cpuAmount = atoi(cpuString);
    policy = argv[2];
    char line[MAX_CHARS_PER_LINE];
    int idx = 0;
    FILE *fp = fopen("tasks.txt", "r");
    pthread_mutex_init(&queueAccess, NULL);
    pthread_mutex_init(&scheduledTaskAccess, NULL);
    pthread_mutex_init(&doneAccess, NULL);
    pthread_cond_init(&haveTask, NULL);
    pthread_cond_init(&getTask, NULL);
    srand(time(0)); //seed for random numbers (get diff numbers each run)

    if (argc != 3)
    {
        printf("Please enter following format: ./<executable> CPUs POLICY.\n");
    }
    else
    {
        while (fgets(line, MAX_CHARS_PER_LINE, fp) != NULL) //read line
        {
            if (validLine(line)) //assure we do not read empty lines into list of tasks
            {
                populateTasks(line, idx);
            }
            idx++;
        }

        clock_gettime(CLOCK_REALTIME, &arrive); //tasks arrive now (time is counted from this point)
        clock_gettime(CLOCK_REALTIME, &lastPBoost); //call this the first priority boost when comparing times
        createPool(cpuAmount);
        fclose(fp);
    }
    runDiagnostics();
    printf("End of Processing.\nProgrammed by Ariel Glikman\n");
    return EXIT_SUCCESS;
}
