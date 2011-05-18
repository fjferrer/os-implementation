/*
 * Common user mode functions
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.50 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/errno.h>
#include <geekos/ktypes.h>
#include <geekos/kassert.h>
#include <geekos/int.h>
#include <geekos/mem.h>
#include <geekos/malloc.h>
#include <geekos/kthread.h>
#include <geekos/vfs.h>
#include <geekos/tss.h>
#include <geekos/user.h>

/*
 * This module contains common functions for implementation of user
 * mode processes.
 */

/*
 * Associate the given user context with a kernel thread.
 * This makes the thread a user process.
 */
void Attach_User_Context(struct Kernel_Thread* kthread, struct User_Context* context)
{
    KASSERT(context != 0);
    kthread->userContext = context;

    Disable_Interrupts();

    /*
     * We don't actually allow multiple threads
     * to share a user context (yet)
     */
    KASSERT(context->refCount == 0);

    ++context->refCount;
    Enable_Interrupts();
}

/*
 * If the given thread has a user context, detach it
 * and destroy it.  This is called when a thread is
 * being destroyed.
 */
void Detach_User_Context(struct Kernel_Thread* kthread)
{
    struct User_Context* old = kthread->userContext;

    kthread->userContext = 0;

    if (old != 0) {
	int refCount;

	Disable_Interrupts();
        --old->refCount;
	refCount = old->refCount;
	Enable_Interrupts();

	/*Print("User context refcount == %d\n", refCount);*/
        if (refCount == 0)
            Destroy_User_Context(old);
    }
}

/*
 * Spawn a user process.
 * Params:
 *   program - the full path of the program executable file
 *   command - the command, including name of program and arguments
 *   pThread - reference to Kernel_Thread pointer where a pointer to
 *     the newly created user mode thread (process) should be
 *     stored
 * Returns:
 *   The process id (pid) of the new process, or an error code
 *   if the process couldn't be created.  Note that this function
 *   should return ENOTFOUND if the reason for failure is that
 *   the executable file doesn't exist.
 */
int Spawn(const char *program, const char *command, struct Kernel_Thread **pThread)
{
    /*
     * Hints:
     * - Call Read_Fully() to load the entire executable into a memory buffer
     * - Call Parse_ELF_Executable() to verify that the executable is
     *   valid, and to populate an Exe_Format data structure describing
     *   how the executable should be loaded
     * - Call Load_User_Program() to create a User_Context with the loaded
     *   program
     * - Call Start_User_Thread() with the new User_Context
     *
     * If all goes well, store the pointer to the new thread in
     * pThread and return 0.  Otherwise, return an error code.
     */

    int iErrCod = 0;

    char *exeFileData = 0;
    struct Exe_Format exeFormat;
    ulong_t exeFileLength = 0;
    struct User_Context *pUserContext = NULL;
    bool detached = false;

    struct Kernel_Thread *process = NULL; /* No entiendo pq crea esta var */

    iErrCod = Read_Fully(program, (void**) &exeFileData, &exeFileLength);
    if (iErrCod != 0) { 
	iErrCod = ENOTFOUND;
	goto error;
      }

    iErrCod =  Parse_ELF_Executable( exeFileData, exeFileLength, &exeFormat);
    if (iErrCod != 0) {
	iErrCod = ENOEXEC;
	goto error;
      }

    iErrCod = Load_User_Program( exeFileData, exeFileLength, &exeFormat,
				    command, &pUserContext);
    if (iErrCod != 0) { 
	iErrCod = -1;
	goto error;
      }

    /* *pThread = Start_User_Thread( pUserContext, detached); */
    process = Start_User_Thread( pUserContext, detached);
    if (process == NULL) {
	iErrCod = -1;
	goto error;
    }

    *pThread = process; /* No entiendo xq generar la doble variable process */

    iErrCod = (*pThread)->pid;

error:
    if (exeFileData)
        Free(exeFileData);
    exeFileData = 0;

    return iErrCod;
  }


/*
 * If the given thread has a User_Context,
 * switch to its memory space.
 *
 * Params:
 *   kthread - the thread that is about to execute
 *   state - saved processor registers describing the state when
 *      the thread was interrupted
 */
void Switch_To_User_Context(struct Kernel_Thread* kthread, struct Interrupt_State* state)
  {
    /*
     * Hint: Before executing in user mode, you will need to call
     * the Set_Kernel_Stack_Pointer() and Switch_To_Address_Space()
     * functions.
     */

    // check if the thread has usercontext or not and switch
    if (kthread->userContext != NULL){
        Switch_To_Address_Space(kthread->userContext);
        /*
	 * This line cause an exception breaking all the stuff
	 * Set_Kernel_Stack_Pointer(kthread->esp);
	 *
	*/ 
        Set_Kernel_Stack_Pointer(((ulong_t) kthread->stackPage) + PAGE_SIZE);
    }
}

