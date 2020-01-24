//
// Created by maciej on 18.01.2020.
//

#include <task.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>


// prywatna
// nie wraca (exec)
void runProgram(Proc *proc) {
    // todo przeniesc proces na foreground
    // todo zmienic na execvpe
    execvp(proc->argv[0], proc->argv);
    log_trace("Nie udało się uruchomić zadania %s", proc->argv[0]);
    exit(7);
}

void reset_rediractions() {
    log_trace("Wywołano reset_rediractions");
    if (before_redirection_stdin != -1) {
        dup2(before_redirection_stdin, STDIN_FILENO);
        close(before_redirection_stdin);
        before_redirection_stdin = -1;
    }
    if (before_redirection_stdout != -1) {
        dup2(before_redirection_stdout, STDOUT_FILENO);
        close(before_redirection_stdout);
        before_redirection_stdout = -1;
    }

}

void add_process_to_task(List_node *node) {
    Proc *p = (Proc *) malloc(sizeof(Proc));
    p->next = NULL;
    p->pid = -1;

    // tworzymy tablice argv
    int len = list_len(node);
    p->argv = (char **) malloc(sizeof(char *) * (len + 1));
    p->argv[len] = NULL;

    int i = 0;
    for (List_node *tmp = node; tmp != NULL; tmp = tmp->next) {
        p->argv[i] = (char *) malloc(sizeof(char) * (strlen(tmp->str) + 1));
        strcpy(p->argv[i++], tmp->str);
    }

    // dodaj na początek listy
    p->next = proc_head;
    proc_head = p;
}

bool run_builtin(Proc *proc) {
    if (strcmp(proc->argv[0], "pwd") == 0) {
        pwd_cmd();
        return true;
    } else if (strcmp(proc->argv[0], "cd") == 0) {
        cd_cmd(proc->argv[1]);
        return true;
    } else if (strcmp(proc->argv[0], "echo") == 0) {
        echo_cmd(proc->argv[1]);
        return true;
    }
    return false;
}

// rozpoczecie wykonywania zadania
void run_task() {


    char *log = proc_list_convert_to_str();
    log_trace("uruchamiam task: %s", log);
    free(log);

    int fd[2];
    int previous = -1; // wyjscie z poprzedniego procesu w tasku

    // czysczenie bufora wyniku
    // memset(result, '\0', 1024);

    for (Proc *tmp = proc_head; tmp != NULL; tmp = tmp->next) {
        // zapamietujemy sobie nasze aktualne stdin i stdout, potem je przywrocimy na macierzystym
        int our_stdin = dup(STDIN_FILENO);
        int our_stdout = dup(STDOUT_FILENO);


        // nie pierwszy
        if (tmp != proc_head) {
            // otwieramy jako stdin wyjscie z poprzedniego potoku
            dup2(previous, STDIN_FILENO);
            close(previous);
        }
        // nie ostatni
        if (tmp->next) {
            pipe(fd);
            // otwieramy stdout jako wejscie utworzonego przez nas potoku
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
        } // ostatni

        // zwroci false, jezeli to nie builtin
        if (!run_builtin(tmp)) {
            pid_t pid;
            // log_trace("Tworze nowy proces dla: %s", tmp->argv[0]);
            pid = fork();
            if (pid == 0) {
                // zamykamy wyjscie utworzonego przez nas potoku, bo z niego bedzie czytal nastepny proces
                close(fd[0]);
                // zamykamy deskryptory do zapamietaniu stanu macierzystego
                close(our_stdin);
                close(our_stdout);
                runProgram(tmp);
            }
            log_trace("Utworzono proces o pidzie %d", pid);
            tmp->pid = pid;
        }

        if (tmp->next != NULL) {
            // ustawiamy wejscie nastepnego procesu
            previous = fd[0];
        }

        dup2(our_stdout, STDOUT_FILENO);
        dup2(our_stdin, STDIN_FILENO);
        close(our_stdin);
        close(our_stdout);

//        if (!pgid)
//            pgid = pid;
//        setpgid(pid, pgid);



    }


//    while (read(pipefd[0], result, sizeof(result)) != 0)
//    {
//    }
//
//    printf("%s",result);

    for (Proc *tmp = proc_head; tmp != NULL; tmp = tmp->next) {
        // to byla komenda wbudowana
        if (tmp->pid == -1) {
            continue;
        }

        // WUNTRACED
        int status = 0;
        log_trace("Czekam na proces o pidzie %d", tmp->pid);

        pid_t x = waitpid(tmp->pid, &status, 0);
        if (x != tmp->pid) {
            log_error("Nie udal sie waitpid, zwrocil %d: %s", x, strerror(errno));
        }
        if (WIFEXITED(status)) {
            log_trace("Proces zakonczyl kodem: %d", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            log_trace("Proces zakonczyl sie przez sygnal %d", WTERMSIG(status));
        }
    }
}

// prywatna
void free_recursive(Proc *tmp) {
    if (tmp == NULL)
        return;

    free_recursive(tmp->next);

    for (int i = 0; tmp->argv[i] != NULL; i++) {
        free(tmp->argv[i]);
    }
    free(tmp->argv);
    free(tmp);
}

void free_process_list() {
    free_recursive(proc_head);
    proc_head = NULL;
}

char *proc_list_convert_to_str() {
    size_t size = 0;

    for (Proc *tmp = proc_head; tmp != NULL; tmp = tmp->next) {
        size += strlen(tmp->argv[0]) + 1;
    }

    char *result = malloc(sizeof(char) * (size + 1));
    result[0] = '\0';

    for (Proc *tmp = proc_head; tmp != NULL; tmp = tmp->next) {
        strcat(result, tmp->argv[0]);
        strcat(result, "|");
    }
    result[strlen(result) - 1] = '\0'; // usuwamy ostatni znak |, beda dwa \0 ale to nie szkodzi
    return result;
}
