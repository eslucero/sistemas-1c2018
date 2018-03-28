#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))

static int
run(const char ***progs, size_t count)
{
	int pipes[4];
	pid_t *children;
	size_t i, j;
	pid_t cur;
	int r, status;

	if (!(children = malloc(sizeof(*children) * count))) {
		fputs("out of memory\n", stderr);
		exit(1);
	}

	//if (!(pipes = malloc(2 * sizeof(int) * count))){
	//	fputs("out of memory\n", stderr);
	//	exit(1);
	//};

	//printf("Count: %d\n", count);
	for(int i = 0;i < 2*(count-1);i += 2)
	{
		if (pipe(&pipes[i]) == -1){
			perror("pipes");
			exit(1);
		};
		printf("Pipes: %d:%d\n", pipes[i], pipes[i+1]);
	}

	// TODO: crear pipes ANTES de crear los procesos
	// Pensar cuantos pipes necesito.

	for (i = 0; i < count; i++) {

		//TODO: Crea *count* procesos
		cur = fork();
		if (cur == -1){
			perror("fork");
			exit(1);
		}
		//TODO: Guardar el PID del proceso hijo en children[i]
		if (cur != 0){
			children[i] = cur;
			close(pipes[i]);
			close(pipes[i+1]);
		}
		//TODO: Para cada proceso hijo:
			//1. Redireccionar los file descriptors adecuados al proceso
			//2- Ejecutar el programa correspondiente
		if (cur == 0){

			if (dup2(pipes[i], 0) == -1){
				printf("%d\n", pipes[i]);
				perror("dup2");
				exit(1);
			};

			if (dup2(pipes[i+1], 1) == -1){
				printf("%d\n", pipes[i + 1]);
				perror("dup2");
				exit(1);
			};

			//printf("%s\n", progs[i][0]);
			if (execvp(progs[i][0], (char * const*)progs[i]) == -1){
				perror("execv");
				exit(1);
			};
		}
	}

	//El padre espera a que terminen todos los procesos hijos que ejecutan los programas
	for (i = 0; i < count; i++) {
		if (waitpid(children[i], &status, 0) == -1) {
			perror("waitpid");
			return -1;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "proceso %d no terminó correctamente [%d]: ",
			    (int)children[i], WIFSIGNALED(status));
			perror("");
			return -1;
		}
	}
	r = 0;

	free(children);
	//free(pipes);

	return r;
}

int
main(int argc, char **argv)
{
	char *lscmd[] = { "ls", "-al", NULL };
	char *wccmd[] = { "wc", NULL };
	char *awkcmd[] = { "awk", "{ print $2 }", NULL };
	char **progs[] = { lscmd, wccmd, awkcmd };

	printf("status: %d\n", run((const char ***)progs, NELEMS(progs)));
	fflush(stdout);
	fflush(stderr);

	return 0;
}
