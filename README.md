# OS Project 2 README

I'll list all the files that I've submitted along with a one sentence explanation in this README file.

```c
.
├── Prj2README.md /* Current README file */
├── Report /* Report folder */
│   └── OsPrj2-519021911033.pdf /* The project report */
├── Results
│   ├── cpu_output.txt /* CPU-Bound throughput results */
│   ├── io_output.txt /* IO-Bound throughput results */
│   ├── latency_100.txt /* 100 CPU-Bound processes latency results */
│   ├── latency_20.txt /* 20 CPU-Bound processes latency results */
│   ├── latency_50.txt /* 50 CPU-Bound processes latency results */
│   └── mixed_output.txt /* CPU/IO mixed throughput results */
├── Screenshots /* Test Screenshots folder */
│   ├── cpu_bound_benchmark.png /* CPU benchmark screenshot */
│   ├── io_bound_benchmark.png /* IO benchmark screenshot */
│   ├── latency_benchmark.png /* latency benchmark screenshot */
│   ├── mixed_benchmark.png /* CPU, IO mixed benchmark screenshot */
│   ├── sched_rr_info.png /* Processtest.apk under SCHED_RR screenshot */
│   ├── sched_wrr_back_info.png /* Processtest.apk under SCHED_WRR background screenshot */
│   ├── sched_wrr_fore_info.png /* Processtest.apk under SCHED_WRR foreground screenshot */
│   └── test_processtest.png /* Test WRR Scheduler (based on RMLFQ) output screenshot */
└── Source Code /* Source code folder */
    ├── Basic /* Basic part of the project */
    │   ├── src
    │   │   ├── kernel
    │   │   │   ├── Makefile /* The modified Sched Makefile */
    │   │   │   ├── core.c /* The core file for Linux Scheduler */
    │   │   │   ├── rt.c  /* The modified RT Scheduler source file */
    │   │   │   ├── sched.h /* Modified /kernel/sched/sched.h */
    │   │   │   └── wrr_basic.c /* Our basic WRR Scheduler source file */
    │   │   └── linux
    │   │       └── sched.h /* Modified /include/linux/sched.h */
    │   └── test
    │       ├── jni
    │       │   ├── Android.mk /* Android Makefile */
    │       │   └── testfile.c /* Our source file for basic test */
    │       ├── libs
    │       │   └── armeabi
    │       │       └── testARM /* Android Executable File */
    │       └── obj
    │           └── local
    │               └── armeabi
    │                   ├── objs
    │                   │   └── testARM
    │                   │       ├── testfile.o
    │                   │       └── testfile.o.d
    │                   └── testARM
    └── Bonus /* Extra bonus of the project */
        ├── RevisedWRR
        │   ├── wrr_RMLFQ.c /* Revised WRR Scheduler based on Random Multilevel feedback queue */
        │   └── wrr_group.c /* WRR Scheduler based on Group Scheduling */
        ├── benchmark
        │   ├── jni
        │   │   ├── Android.mk
        │   │   └── benchmark.c /* The driven file to acquire benchmark ouput */
        │   ├── libs
        │   │   └── armeabi
        │   │       └── benchmark
        │   └── obj
        │       └── local
        │           └── armeabi
        │               ├── benchmark
        │               └── objs
        │                   └── benchmark
        │                       ├── benchmark.o
        │                       └── benchmark.o.d
        ├── benchmark_cpubound
        │   ├── jni
        │   │   ├── Android.mk
        │   │   └── cpu_bound.c /* The CPU bound throughput benchmark source file */
        │   ├── libs
        │   │   └── armeabi
        │   │       └── test_cpubound
        │   └── obj
        │       └── local
        │           └── armeabi
        │               ├── objs
        │               │   └── test_cpubound
        │               │       ├── cpu-bound.o
        │               │       └── cpu-bound.o.d
        │               └── test_cpubound
        ├── benchmark_cpulatency
        │   ├── jni
        │   │   ├── Android.mk
        │   │   └── cpu_latency.c /* The CPU bound latency benchmark source file */
        │   ├── libs
        │   │   └── armeabi
        │   │       └── test_cpulatency 
        │   └── obj
        │       └── local
        │           └── armeabi
        │               ├── objs
        │               │   └── test_cpulatency
        │               │       ├── cpu-latency.o
        │               │       └── cpu-latency.o.d
        │               └── test_cpulatency
        ├── benchmark_iobound
        │   ├── data_in /* The file to be read for I/O */ 
        │   ├── jni
        │   │   ├── Android.mk
        │   │   └── io_bound.c /* The IO bound throughput benchmark source file */
        │   ├── libs
        │   │   └── armeabi
        │   │       └── test_iobound
        │   └── obj
        │       └── local
        │           └── armeabi
        │               ├── objs
        │               │   └── test_iobound
        │               │       ├── io-bound.o
        │               │       └── io-bound.o.d
        │               └── test_iobound
        └── benchmark_mixed
            ├── jni
            │   ├── Android.mk
            │   └── mixed.c /* The IO/CPU mixed throughput benchmark source file */
            ├── libs
            │   └── armeabi
            │       └── test_mixed
            └── obj
                └── local
                    └── armeabi
                        ├── objs
                        │   └── test_mixed
                        │       ├── mixed.o
                        │       └── mixed.o.d
                        └── test_mixed

64 directories, 61 files
```
