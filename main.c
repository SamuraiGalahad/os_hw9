#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>

#define SEM_KEY 1234
#define MSG_MAX_LEN 1024
#define MSG_COUNT 10

void error(char *msg) {
  perror(msg);
  exit(1);
}

int create_semaphore() {
  int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
  if (semid < 0) {
    error("Error creating semaphore");
  }

  union semun sem_val;
  sem_val.val = 1;
  if (semctl(semid, 0, SETVAL, sem_val) < 0) {
    error("Error setting semaphore value");
  }

  return semid;
}

void remove_semaphore(int semid) {
  if (semctl(semid, 0, IPC_RMID) < 0) {
    error("Error removing semaphore");
  }
}

void wait_semaphore(int semid) {
  struct sembuf sem_op = {0, -1, SEM_UNDO};
  if (semop(semid, &sem_op, 1) < 0) {
    error("Error waiting semaphore");
  }
}

void signal_semaphore(int semid) {
  struct sembuf sem_op = {0, 1, SEM_UNDO};
  if (semop(semid, &sem_op, 1) < 0) {
    error("Error signaling semaphore");
  }
}

int main() {
  int semid = create_semaphore();

  int fd[2];
  if (pipe(fd) < 0) {
    error("Error creating pipe");
  }

  pid_t pid = fork();
  if (pid < 0) {
    error("Error creating child process");
  }

  if (pid == 0) {
    // процесс-ребенок
    close(fd[1]); // закрытие pipe со стороны писателя

    char msg[MSG_MAX_LEN];
    for (int i = 0; i < MSG_COUNT; i++) {
      wait_semaphore(semid);
      read(fd[0], msg, MSG_MAX_LEN);
      printf("Child process received message: %s\n", msg);
      sprintf(msg, "Message from child process %d", i);
      write(fd[0], msg, strlen(msg) + 1);
      signal_semaphore(semid);
    }

    close(fd[0]); // закрытие pipe со стороны читателя
    remove_semaphore(semid);

  } else {
    // родительский процесс
    close(fd[0]); // закрытие со стороны читателя

    char msg[MSG_MAX_LEN];
    for (int i = 0; i < MSG_COUNT; i++) {
      sprintf(msg, "Message from parent process %d", i);
      write(fd[1], msg, strlen(msg) + 1);
      wait_semaphore(semid);
      read(fd[1], msg, MSG_MAX_LEN);
      printf("Parent process received message: %s\n", msg);
      signal_semaphore(semid);
    }

    close(fd[1]); // закрытие со стороны писателя
    remove_semaphore(semid);
  }

  return 0;
}