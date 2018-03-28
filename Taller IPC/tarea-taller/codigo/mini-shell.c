#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))

static int
run(const char ***progs, size_t count)
{
	int pipes[2][2];
	pid_t *children;
	size_t i, j;
	pid_t cur;
	int r, status;

	if (!(children = malloc(sizeof(*children) * count))) {
		fputs("out of memory\n", stderr);
		exit(1);
	}

	// TODO: crear pipes ANTES de crear los procesos
	// Pensar cuantos pipes necesito.
	if (pipe(pipes[0]) == -1) {
		perror("pipe");
		exit(1);
	}
	if (pipe(pipes[1]) == -1) {
		perror("pipe");
		exit(1);
	}

	for (int i = 0; i < count; i++) {

		//TODO: Crea *count* procesos
		cur = fork();
		if (cur == -1){
			perror("fork");
			exit(1);
		}

		//TODO: Guardar el PID del proceso hijo en children[i]
		if (cur != 0)
			children[i] = cur;

		//TODO: Para cada proceso hijo:
			//1. Redireccionar los file descriptors adecuados al proceso
			//2- Ejecutar el programa correspondiente
		if (cur == 0){
			if (i != 0){ // redirijo el stdin de wc y awk
				if (dup2(pipes[i - 1][0], 0) == -1){
					perror("dup2");
					exit(1);
				}
			}
			if (i != 2){ // redirijo el stdout de ls y wc; awk lo tiene que escribir por pantalla
				if (dup2(pipes[i][1], 1) == -1){
					perror("dup2");
					exit(1);
				}
			}
			//Cierro exactamente los pipes que no uso
			if (i == 0){
				close(pipes[0][0]);
				close(pipes[1][0]);
				close(pipes[1][1]);
			}
			if (i == 1){
				close(pipes[0][1]);
				close(pipes[1][0]);
			}
			if (i == 2){
				close(pipes[0][0]);
				close(pipes[0][1]);
				close(pipes[1][1]);
			}
			if (execvp(progs[i][0], (char * const*) progs[i]) == -1){
				perror("execvp");
				exit(1);
			}
		}
	}
	//Si no cerraba los pipes en el proceso padre, se quedaba esperando
	close(pipes[0][0]);
	close(pipes[0][1]);
	close(pipes[1][0]);
	close(pipes[1][1]);

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

	printf("status: %d\n", run((const char ***) progs, NELEMS(progs)));
	fflush(stdout);
	fflush(stderr);

	return 0;
}
