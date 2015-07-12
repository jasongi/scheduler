CPU/IO SCHEDULER by Jason Giancono 16065985

This is a simulated CPU/IO scheduler made for an Operating Systems assignment in my computer science undergraduate degree. It was created in 2013. The introduction to the assignment is below:

*The goal of this assignment is to give students experiences of using POSIX Pthreads library
functions for thread creations and synchronizations while simulating a simple process
scheduling. You are asked to write a program scheduler in C under Linux environment to
simulate the activities of n processes running on a system with one CPU and one I/O as
shown in the following figure.*

To compile:
type the following command
>make

To run:
put the executable 'scheduler' in the same folder as the job/PID files and run the command
>./scheduler <job>
where <job> is the filename of the job file.

I have also included the two test files given in the assignment spec under directory test and test2
