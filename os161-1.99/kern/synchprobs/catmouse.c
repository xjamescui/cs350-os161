/*
 * catmouse.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 * 26-11-2007: KMS : Modified to use cat_eat and mouse_eat
 * 21-04-2009: KMS : modified to use cat_sleep and mouse_sleep
 * 21-04-2009: KMS : added sem_destroy of CatMouseWait
 * 05-01-2012: TBB : added comments to try to clarify use/non use of volatile
 * 22-08-2013: TBB: made cat and mouse eating and sleeping time optional parameters
 *
 */

/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <synchprobs.h>

/*
 * 
 * cat,mouse,bowl simulation functions defined in bowls.c
 *
 * For Assignment 1, you should use these functions to
 *  make your cats and mice eat from the bowls.
 * 
 * You may *not* modify these functions in any way.
 * They are implemented in a separate file (bowls.c) to remind
 * you that you should not change them.
 *
 * For information about the behaviour and return values
 *  of these functions, see bowls.c
 *
 */

/* this must be called before any calls to cat_eat or mouse_eat */
extern int initialize_bowls(unsigned int bowlcount);

extern void cleanup_bowls(void);
extern void cat_eat(unsigned int bowlnumber, int eat_time);
extern void mouse_eat(unsigned int bowlnumber, int eat_time);
extern void cat_sleep(int sleep_time);
extern void mouse_sleep(int sleep_time);

/*
 *
 * Problem parameters
 *
 * Values for these parameters are set by the main driver
 *  function, catmouse(), based on the problem parameters
 *  that are passed in from the kernel menu command or
 *  kernel command line.
 *
 * Once they have been set, you probably shouldn't be
 *  changing them.
 *
 * These are only ever modified by one thread, at creation time,
 * so they do not need to be volatile.
 */
int NumBowls;  // number of food bowls
int NumCats;   // number of cats
int NumMice;   // number of mice
int NumLoops;  // number of times each cat and mouse should eat

/* 
 * Defaults here are as they were with the previous implementation
 * where these could not be changed.
 */
int CatEatTime = 1;      // length of time a cat spends eating
int CatSleepTime = 5;    // length of time a cat spends sleeping
int MouseEatTime = 3;    // length of time a mouse spends eating
int MouseSleepTime = 3;  // length of time a mouse spends sleeping

/*
 * Once the main driver function (catmouse()) has created the cat and mouse
 * simulation threads, it uses this semaphore to block until all of the
 * cat and mouse simulations are finished.
 */
struct semaphore *CatMouseWait;
#if OPT_A1
//struct semaphore *BowlsLimit;
struct semaphore *BowlsLimit, *CatsBowlsLimit, *MiceBowlsLimit;
struct semaphore *CatSendout;
struct cv *CatsTurn, *MiceTurn, *CatsLineUp, *MiceLineUp, *BackToCat,
		*BackToMice, *NextBatch, *NextNextBatch;

struct lock *EatLock, MiceLock;

const int CAT = 0;
const int MOUSE = 1;
const int NONE = -1;

volatile int* bowlsArray;
volatile int eatingAnimal = -1;
volatile bool nextBatchCalled = false;
volatile bool lastOneOut = false;
volatile int cat_queue_count;
volatile int mice_queue_count;

int capacity = 0; //how many cats/mice are in dining room eating at the moment

#endif

/*
 *
 * Function Definitions
 *
 */

#if OPT_A1

//Return a vacant bowl
static int findBowlToEat() {

	int bowl = 0; //default: 0 for no vacant bowl

	for (int i = 0; i < NumBowls; i++) {
		if (bowlsArray[i] == 0) {
			bowl = i + 1;
			break;
		}
	}

	return bowl;
}

static int getAnimalEatingTime(int animal) {
	return (animal == CAT) ? CatEatTime : MouseEatTime;
}

static void animal_eat(int animal, int bowl, int eat_time) {
	if (animal == CAT) {
		cat_eat(bowl, eat_time);
	} else if (animal == MOUSE) {
		mouse_eat(bowl, eat_time);
	}
}
static void toggleEatingAnimal() {
	if (eatingAnimal == CAT) {
		eatingAnimal = MOUSE;
	} else if (eatingAnimal == MOUSE) {
		eatingAnimal = CAT;
	}
}

static void callNext(struct lock *lk) {
	dbflags = DB_CATMOUSE;
	if (eatingAnimal == CAT) {
		if (cat_queue_count > 0) {
			for (int i = 0; i < NumBowls; i++) {
				if (i < cat_queue_count) {
					cv_signal(CatsTurn, lk);
				} else {
					break;
				}
			}
		} else {
			DEBUG(DB_CATMOUSE,
					"cats queue is empty: make it mice turn again, eatingAnimal = %d\n",
					eatingAnimal);

			//if mice queue is not empty then make it mice's turn again
			if (mice_queue_count > 0) {
				eatingAnimal = MOUSE;
				cv_broadcast(MiceTurn, lk);

			}
		}

	} else if (eatingAnimal == MOUSE) {
		if (mice_queue_count > 0) {
			for (int i = 0; i < NumBowls; i++) {
				if (i < mice_queue_count) {

					cv_signal(MiceTurn, lk);
				} else {
					break;
				}
			}
		} else {
			DEBUG(DB_CATMOUSE,
					"mice queue is empty: make it cats turn again, eatingAnimal=%d\n",
					eatingAnimal);
			//if cats queue is not empty then make it cats' turn
			if (cat_queue_count > 0) {
				eatingAnimal = CAT;
				cv_broadcast(CatsTurn, lk);
			}
		}
	}
}
/**
 * Both mice and cats will visit the diningRoom to eat so
 * let's make synchronization happen here
 */
static void diningRoom(int animal) {

	dbflags = DB_CATMOUSE;
	unsigned int bowl;

	//bunch of cats and mouse entering the dining room
	//(assuming they cant see each other)

	lock_acquire(EatLock);

	//cat or mouse? first come first serve
	if (eatingAnimal == NONE) {
		eatingAnimal = animal;
	}

	//if you are not the eatingAnimal or if you are the eating animal but the capacity is full
	//then queue up
	while (animal != eatingAnimal
			|| (animal == eatingAnimal && BowlsLimit->sem_count == 0)) {
		if (animal == MOUSE) {
			mice_queue_count++;
			DEBUG(DB_CATMOUSE, "mice coming in\n");
			cv_wait(MiceTurn, EatLock);
			DEBUG(DB_CATMOUSE, "awaking mice\n");
			mice_queue_count--;
			break;
		}

		else if (animal == CAT) {
			cat_queue_count++;
			DEBUG(DB_CATMOUSE, "cat coming in\n");
			cv_wait(CatsTurn, EatLock);
			DEBUG(DB_CATMOUSE, "awaking cat\n");
			cat_queue_count--;

			break;
		}

	}

	//Checkpoint: you are the eatingAnimal------------------
	lock_release(EatLock);

	//Max Seats: NumBowls
	P(BowlsLimit);

	//Grab your plates
	lock_acquire(EatLock);
	bowl = findBowlToEat(); //find a bowl
	bowlsArray[bowl - 1] = 1; //mark this ball as being occupied
	lock_release(EatLock);

	//Eat
	animal_eat(animal, bowl, getAnimalEatingTime(animal));

	lock_acquire(EatLock);
	//Wash dishes and exit
	V(BowlsLimit);
	bowlsArray[bowl - 1] = 0;

	//call next batch if this is the last guy out
	if (BowlsLimit->sem_count == NumBowls) {
		DEBUG(DB_CATMOUSE, "sem_count is %d\n", BowlsLimit->sem_count);
		DEBUG(DB_CATMOUSE, "mice queue has : %d\n", mice_queue_count);
		DEBUG(DB_CATMOUSE, "cat queue has : %d\n", cat_queue_count);
		DEBUG(DB_CATMOUSE, "now calling next batch...\n");
		toggleEatingAnimal();
		callNext(EatLock);
	}

	lock_release(EatLock);

}
#endif

/*
 * cat_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds cat identifier from 0 to NumCats-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Each cat simulation thread runs this function.
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 */

static
void cat_simulation(void * unusedpointer, unsigned long catnumber) {
	int i;
//	unsigned int bowl;

	/* avoid unused variable warnings. */
	(void) unusedpointer;
	(void) catnumber;

	/* your simulated cat must iterate NumLoops times,
	 *  sleeping (by calling cat_sleep() and eating
	 *  (by calling cat_eat()) on each iteration */
	for (i = 0; i < NumLoops; i++) {

		/* do not synchronize calls to cat_sleep().
		 Any number of cats (and mice) should be able
		 sleep at the same time. */
		cat_sleep(CatSleepTime);

		/* for now, this cat chooses a random bowl from
		 * which to eat, and it is not synchronized with
		 * other cats and mice.
		 *
		 * you will probably want to control which bowl this
		 * cat eats from, and you will need to provide
		 * synchronization so that the cat does not violate
		 * the rules when it eats */

#if OPT_A1
		diningRoom(CAT);

#else
		/* legal bowl numbers range from 1 to NumBowls */
		bowl = ((unsigned int)random() % NumBowls) + 1;
		cat_eat(bowl, CatEatTime);
#endif
	}

	/* indicate that this cat simulation is finished */
	V(CatMouseWait);
}

/*
 * mouse_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds mouse identifier from 0 to NumMice-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      each mouse simulation thread runs this function
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 *
 */

static
void mouse_simulation(void * unusedpointer, unsigned long mousenumber) {
	int i;
//	unsigned int bowl;

	/* Avoid unused variable warnings. */
	(void) unusedpointer;
	(void) mousenumber;

	/* your simulated mouse must iterate NumLoops times,
	 *  sleeping (by calling mouse_sleep()) and eating
	 *  (by calling mouse_eat()) on each iteration */
	for (i = 0; i < NumLoops; i++) {

		/* do not synchronize calls to mouse_sleep().
		 Any number of mice (and cats) should be able
		 sleep at the same time. */
		mouse_sleep(MouseSleepTime);

		/* for now, this mouse chooses a random bowl from
		 * which to eat, and it is not synchronized with
		 * other cats and mice.
		 *
		 * you will probably want to control which bowl this
		 * mouse eats from, and you will need to provide
		 * synchronization so that the mouse does not violate
		 * the rules when it eats */

#if OPT_A1

		diningRoom(MOUSE);

#else
		/* legal bowl numbers range from 1 to NumBowls */
		bowl = ((unsigned int) random() % NumBowls) + 1;
		mouse_eat(bowl, MouseEatTime);
#endif
	}

	/* indicate that this mouse is finished */
	V(CatMouseWait);
}

/*
 * catmouse()
 *
 * Arguments:
 *      int nargs: should be 5 or 9
 *      char ** args: args[1] = number of food bowls
 *                    args[2] = number of cats
 *                    args[3] = number of mice
 *                    args[4] = number of loops
 * Optional parameters
 *                    args[5] = cat eating time
 *                    args[6] = cat sleeping time
 *                    args[7] = mouse eating time
 *                    args[8] = mouse sleeping time
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up cat_simulation() and
 *      mouse_simulation() threads.
 *      You may need to modify this function, e.g., to
 *      initialize synchronization primitives used
 *      by the cat and mouse threads.
 *
 *      However, you should should ensure that this function
 *      continues to create the appropriate numbers of
 *      cat and mouse threads, to initialize the simulation,
 *      and to wait for all cats and mice to finish.
 */

int catmouse(int nargs, char ** args) {
	int index, error;
	int i;

	/* check and process command line arguments */
	if ((nargs != 9) && (nargs != 5)) {
		kprintf("Usage: <command> NUM_BOWLS NUM_CATS NUM_MICE NUM_LOOPS\n");
		kprintf("or\n");
		kprintf("Usage: <command> NUM_BOWLS NUM_CATS NUM_MICE NUM_LOOPS ");
		kprintf(
				"CAT_EATING_TIME CAT_SLEEPING_TIME MOUSE_EATING_TIME MOUSE_SLEEPING_TIME\n");
		return 1;  // return failure indication
	}

	/* check the problem parameters, and set the global variables */
	NumBowls = atoi(args[1]);
	if (NumBowls <= 0) {
		kprintf("catmouse: invalid number of bowls: %d\n", NumBowls);
		return 1;
	}
	NumCats = atoi(args[2]);
	if (NumCats < 0) {
		kprintf("catmouse: invalid number of cats: %d\n", NumCats);
		return 1;
	}
	NumMice = atoi(args[3]);
	if (NumMice < 0) {
		kprintf("catmouse: invalid number of mice: %d\n", NumMice);
		return 1;
	}
	NumLoops = atoi(args[4]);
	if (NumLoops <= 0) {
		kprintf("catmouse: invalid number of loops: %d\n", NumLoops);
		return 1;
	}

	if (nargs == 9) {
		CatEatTime = atoi(args[5]);
		if (NumLoops <= 0) {
			kprintf("catmouse: invalid cat eating time: %d\n", CatEatTime);
			return 1;
		}

		CatSleepTime = atoi(args[6]);
		if (NumLoops <= 0) {
			kprintf("catmouse: invalid cat sleeping time: %d\n", CatSleepTime);
			return 1;
		}

		MouseEatTime = atoi(args[7]);
		if (NumLoops <= 0) {
			kprintf("catmouse: invalid mouse eating time: %d\n", MouseEatTime);
			return 1;
		}

		MouseSleepTime = atoi(args[8]);
		if (NumLoops <= 0) {
			kprintf("catmouse: invalid mouse sleeping time: %d\n",
					MouseSleepTime);
			return 1;
		}
	}

	kprintf("Using %d bowls, %d cats, and %d mice. Looping %d times.\n",
			NumBowls, NumCats, NumMice, NumLoops);
	kprintf("Using cat eating time %d, cat sleeping time %d\n", CatEatTime,
			CatSleepTime);
	kprintf("Using mouse eating time %d, mouse sleeping time %d\n",
			MouseEatTime, MouseSleepTime);

	bowlsArray = kmalloc(sizeof(int) * NumBowls);

	for (int i = 0; i < NumBowls; i++) {
		bowlsArray[i] = 0;
	}

	/* create the semaphore that is used to make the main thread
	 wait for all of the cats and mice to finish */
	CatMouseWait = sem_create("CatMouseWait", 0);
	if (CatMouseWait == NULL) {
		panic("catmouse: could not create semaphore\n");
	}

	BowlsLimit = sem_create("BowlsLimit", NumBowls);
	CatsTurn = cv_create("CatsTurn");
	MiceTurn = cv_create("MiceTurn");
	EatLock = lock_create("EatLock");

	/*
	 * initialize the bowls
	 */
	if (initialize_bowls(NumBowls)) {
		panic("catmouse: error initializing bowls.\n");
	}

	/*
	 * Start NumCats cat_simulation() threads.
	 */
	for (index = 0; index < NumCats; index++) {
		error = thread_fork("cat_simulation thread", NULL, cat_simulation,
		NULL, index);
		if (error) {
			panic("cat_simulation: thread_fork failed: %s\n", strerror(error));
		}
	}

	/*
	 * Start NumMice mouse_simulation() threads.
	 */
	for (index = 0; index < NumMice; index++) {
		error = thread_fork("mouse_simulation thread", NULL, mouse_simulation,
		NULL, index);
		if (error) {
			panic("mouse_simulation: thread_fork failed: %s\n",
					strerror(error));
		}
	}

	/* wait for all of the cats and mice to finish before
	 terminating */
	for (i = 0; i < (NumCats + NumMice); i++) {
		P(CatMouseWait);
	}

	/* clean up the semaphore that we created */
	sem_destroy(CatMouseWait);
	sem_destroy(BowlsLimit);
	cv_destroy(MiceTurn);
	cv_destroy(CatsTurn);

	lock_destroy(EatLock);

	/* clean up resources used for tracking bowl use */
	cleanup_bowls();

	kfree((void *) bowlsArray);
	bowlsArray = NULL;

	return 0;
}

/*
 * End of catmouse.c
 */
