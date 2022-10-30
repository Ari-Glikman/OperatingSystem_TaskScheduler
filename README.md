# OperatingSystem_TaskScheduler

## BY ARIEL GLIKMAN


This program simulates an OS scheduler by implementing two policies: Shortest Job First (SJF) and Multi-Level Feedback Queue (MLFQ). 
To run the program on the command line enter 'make' which will compile the program and output an executable called.
Decide between running the program with 1,2,4, or 8 CPUS and the policy: 'sjf' or 'mlfq' (note that they should be entered in lowercase). Once you have made a decision run the program by entering the following on the command line:
'./a3 <number of cpus> <policy>'.

Below is a table broken down by policy, number of CPUs, type of task, turnaround time, and response time.
Each was run five times under each condition and the results are averaged below.



| POLICY | CPUS | TASK TYPE | TURNAROUND TIME (usec) | RESPONSE TIME (usec) |
| SJF    | 1    | 0         | 15580                  | 13150                |
| SJF    | 1    | 1         | 87764                  | 80565                |
| SJF    | 1    | 2         | 532230                 | 478784               |
| SJF    | 1    | 3         | 576111                 | 524135               |
| SJF    | 2    | 0         | 8628                   | 6883                 |
| SJF    | 2    | 1         | 44414                  | 39146                |
| SJF    | 2    | 2         | 271341                 | 231438               |
| SJF    | 2    | 3         | 293328                 | 254366               |
| SJF    | 4    | 0         | 5461                   | 3645                 |
| SJF    | 4    | 1         | 23868                  | 19504                |
| SJF    | 4    | 2         | 143359                 | 110067               |
| SJF    | 4    | 3         | 154028                 | 121757               |
| SJF    | 8    | 0         | 3794                   | 2523                 |
| SJF    | 8    | 1         | 13600                  | 9682                 |
| SJF    | 8    | 2         | 79723                  | 50120                |
| SJF    | 8    | 3         | 84635                  | 55423                |
| MLFQ   | 1    | 0         | 246500                 | 5475                 |
| MLFQ   | 1    | 1         | 651495                 | 4709                 |
| MLFQ   | 1    | 2         | 2408999                | 5434                 |
| MLFQ   | 1    | 3         | 2374978                | 6270                 |
| MLFQ   | 2    | 0         | 125861                 | 3376                 |
| MLFQ   | 2    | 1         | 341548                 | 2966                 |
| MLFQ   | 2    | 2         | 1248962                | 3380                 |
| MLFQ   | 2    | 3         | 1223190                | 3833                 |
| MLFQ   | 4    | 0         | 70933                  | 2201                 |
| MLFQ   | 4    | 1         | 189416                 | 1993                 |
| MLFQ   | 4    | 2         | 695415                 | 2290                 |
| MLFQ   | 4    | 3         | 667485                 | 2455                 |
| MLFQ   | 8    | 0         | 36168                  | 2195                 |
| MLFQ   | 8    | 1         | 88585                  | 1982                 |
| MLFQ   | 8    | 2         | 315790                 | 2196                 |
| MLFQ   | 8    | 3         | 299854                 | 2427                 |

These results are what was to be expected when taking into account both policies and CPUs available. We see that for turnaround time SJF is much quicker because there is much less context switching. The context switching takes time and by forcing tasks to do round robin (MLFQ) as opposed to just run when it is their turn (SJF) we lose efficiency (ratio of time spent on a task as opposed to context switching goes down). As for response time MLFQ is advantageous because of this very trait. It tries to start tasks as soon as possible as opposed to waiting to take the longest task last as SJF does. Thus for turnaround and response time the expected results are seen when comparing SJF and MLFQ.

When looking instead at the CPUs used between the same policy we see that for SJF and MLFQ both the turnaround and response time decrease proportionally with respect to the CPUs utilized. This is trivial as with more CPUs executing the same amount of tasks, in the same way they will do the same work but faster, explaining why the response time is highly correlated to the amount of CPUs in use. In the same amount of time they will also reach a proportional amount of more tasks explaining why there is high correlation between CPUs and response time within the same policy.
