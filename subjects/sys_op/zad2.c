/*
 * -------------------------------------------------------------------- 
 *  Zmodyfikować program calka1, który oblicza całkę wybranej funkcji na zadanym przedziale (pole powierzchni pod wykresem funkcji). Funkcja jest zapisana w kodzie programu. Program pobiera trzy argumenty określające granice przedziału zmienności funkcji oraz liczbę podprzedziałów. W każdym podprzedziale program przybliża całkę pojedynczym trapezem. Postać wywołania programu:
 *
 *      calka1  start  stop  N
 *
 *      1. Przekształcić program do wersji wielowątkowej. Wątek główny dzieli przedział zmienności na równe podprzedziały i uruchamia nowy wątek roboczy do obliczenia całki w każdym z nich. Wątek roboczy oblicza całkę w podprzedziale, następnie wypisuje swój identyfikator i uzyskany wynik. Wątek główny sumuje wyniki z poszczególnych wątków i wypisuje na stdout.
 *      2. Dodać wątek, który będzie w sposób synchroniczny (przy pomocy sigwait()) obsługiwał sygnały SIGINT przysyłane do procesu.  Pozostałe sygnały powinny być blokowane. Po każdym odebraniu sygnału SIGINT wątek powinien wypisać komunikat na stdout.
 *
 *      ---------------------------------------------------------------------
 *
 */

/* Build:
 * gcc zad2.c -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>


#define MAX 20

double zakres[MAX];
double calka[MAX];
double rozmiar;
void *trapez(int n); 	/* funkcja watku roboczego */
double fun(double arg);


struct integral_data {	/* integral context per each thread */
	int idx;			/* thread's index in tid table*/
	double from;		/* beginning of the integration range */
	double to;			/* the end of the integration range */
	double integral_estimate;	/* estimation of integral in given range, NOTE: no need to protect with mutex - only producer accesses this before the main thred controller accesses it to sum up the integration results from all the threads. In other words - access is synchronised between all threads. */
};

struct integral_arg {
	int thread_n;		/* number of threads to create */
	struct integral_data range;
};

void* calc_integral_task(void *integral_arg);		/* start threads (thread controller) */
void* calc_integral_in_range(void *intergal_ctx);	/* thread function (single producer) */

void* signal_handler_task(void *);					/* thread processing the system signals */


pthread_mutex_t	signals_mutex;
int	handle_signals = 1;


int
main(int argc, char *argv[])
{
	sigset_t	s, s_old;
	pthread_t integral_task_pid_t, signal_handler_task_pid_t;
	struct integral_arg integral_data;
	memset(&integral_data, 0, sizeof(struct integral_arg));

	if (argc < 4) {
		printf("Nieprawidlowa liczba argumentow\n");
		printf("Wywolanie: %s start stop N\n", argv[0]);
		exit(1);
	}

	int n = atoi(argv[3]);
	if (n > MAX) {
		printf("Nieprawidlowa liczba podprzedzialow\n");
		exit(1);
	}
	
	pthread_mutex_init(&signals_mutex, NULL);

	sigfillset(&s);
	pthread_sigmask(SIG_BLOCK, &s, &s_old);

	pthread_create(&signal_handler_task_pid_t, NULL, signal_handler_task, NULL);

	integral_data.range.from = atoi(argv[1]);
	integral_data.range.to = atoi(argv[2]);
	integral_data.thread_n = n;

	pthread_create(&integral_task_pid_t, NULL, calc_integral_task, &integral_data);
	pthread_join(integral_task_pid_t, NULL);

	pthread_mutex_lock(&signals_mutex);
	handle_signals = 0;
	pthread_mutex_unlock(&signals_mutex);
	pthread_kill(signal_handler_task_pid_t, SIGINT);

	pthread_join(signal_handler_task_pid_t, NULL);

	pthread_mutex_destroy(&signals_mutex);
	return 0;
}


double fun(double arg) {
	
	double result;
	
	result = 5 * arg * arg + 3;
	return(result);
}

void* calc_integral_task(void *integral_arg)
{
	pthread_t tid[MAX];
	int i = 0;
	sigset_t	s, s_old;

	double integral_result = 0.0;
	double dx = 0.0;

	if (!integral_arg)
		return;

	struct integral_arg * d = integral_arg;

	if (d->thread_n < 1)
		return;
	dx = (d->range.to - d->range.from) / d->thread_n;

	struct integral_data *integral_ctx = malloc(d->thread_n * sizeof(struct integral_data));
	if (!integral_ctx)
		return;

	fprintf(stdout, "Calculating integral on the range [%f, %f] using %d threads...\n", d->range.from, d->range.to, d->thread_n);

	sigfillset(&s);
	pthread_sigmask(SIG_BLOCK, &s, &s_old);

	i = 0;
	for (; i < d->thread_n; ++i)
	{
		/* init thread's integral data */
		integral_ctx[i].idx = i;
		integral_ctx[i].from = d->range.from + i * dx;
		integral_ctx[i].to =  integral_ctx[i].from + dx;

		/* and the thread itself */
		pthread_create(&tid[i], NULL, calc_integral_in_range, &integral_ctx[i]);
	}

	i = 0;
	for (; i < d->thread_n; ++i)
	{
		pthread_join(tid[i], NULL);
		integral_result += integral_ctx[i].integral_estimate;
	}

	fprintf(stdout, "Integral result: %f\n", integral_result);
	free(integral_ctx);
}

void* calc_integral_in_range(void *integral_ctx)
{
	double integral = 0.0;
	sigset_t	s, s_old;

	struct integral_data *d = integral_ctx;
	if (!d)
		return;

	sigfillset(&s);
	pthread_sigmask(SIG_BLOCK, &s, &s_old);

	integral = (fun(d->from) + fun(d->to)) / 2.0 * (d->to - d->from);

	d->integral_estimate = integral;

	fprintf(stdout, "thread [%d]: integral in range [%f, %f] is %f\n", d->idx, d->from, d->to, integral);
}

void* signal_handler_task(void* arg)
{
	sigset_t s, s_old;
	int sig = -1;

	pthread_mutex_lock(&signals_mutex);
	while(handle_signals)
	{
		pthread_mutex_unlock(&signals_mutex);
		sigemptyset(&s);
		sigaddset(&s, SIGINT);
		if (sigwait(&s, &sig) != 0) {
			fprintf(stdout, "Err, in sigwait\n");
			pthread_mutex_lock(&signals_mutex);
			continue;
		}

		if (sig != SIGINT) {
			fprintf(stdout, "Err, signalled signal not supported %d\n", sig);
			pthread_mutex_lock(&signals_mutex);
			continue;
		}

		pthread_mutex_lock(&signals_mutex);
		if (handle_signals)
			fprintf(stdout, "== SIGINT received\n");
		else
			fprintf(stdout, "== SIGINT received from the main thread... Job done.\n");
	}

	pthread_mutex_unlock(&signals_mutex);
}
