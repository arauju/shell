#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER 512

bool saida = false, entrada = false, background = false, pip = false;

void setLabels (){
  saida = false;
  entrada = false;
  background = false;
  pip = false;
}

int main (int argc, char** argv){

  int fd[2]; /* File descriptors pro Pipe*/
  pid_t pid; /* Variável para armazenar o pid*/
  char* comando = (char*) calloc(BUFFER + 1, sizeof(char));
  char* token = NULL;
  char** tokens = NULL;
  char** tokens2 = NULL;
  int cont, contTokens, status;
  char arqSaida[50], arqEntrada[50];
  char ch;

  /* Checa se há arquivo de entrada e se ela é válida */
  if (argc > 2){
    perror("É aceito apenas um argumento de linha de comando.");
    exit(1);
  }
  if(argc > 1) {
    if(!access(argv[1], F_OK)){
    freopen(argv[1], "r", stdin);
  } else {
    perror("Arquivo de entrada não existe.");
    exit(1);
    }
  }

  pipe(fd);

  /* Lê o comando */
  while (!feof(stdin)) {
    setLabels();
    tokens = (char**) calloc (20, sizeof(char));

    if(argc <= 1)
      printf("$ ");
    fgets(comando, BUFFER, stdin);

    if(!strcmp(comando, "fim\n"))
        break;

    /* Trata a entrada */
    token = strtok(comando, " \t\n");
    tokens[0] = token;
    cont = 1;
    while (token = strtok(NULL, " \t\n")){
      if ((strcmp(token, "=>")) && (strcmp(token, "<="))
          && (strcmp(token, "&")) && (strcmp(token, "|"))){
          if (pip == false)
            tokens[cont] = token;
          else
            tokens2[cont] = token;
          cont ++;
      }
      else {
        /* Saida em arquivo */
        if (!(strcmp(token, "=>"))){
          saida = true;
          token = strtok(NULL, " \t\n");
          strcpy(arqSaida, token);
        }
        /* Arquivo de entrada */
        if (!(strcmp(token, "<="))){
          entrada = true;
          token = strtok(NULL, " \t\n");
          strcpy(arqEntrada, token);
        }
        /* Comando em background */
        if (!(strcmp(token, "&"))){
            background = true;
        }
        /* Se há pipe, salva o processo seguinte em outro token*/
        if (!(strcmp(token, "|"))){
          pip = true;
          contTokens = cont;
          cont = 0;
          tokens2 = (char**) calloc (20, sizeof(char));
        }
      }
    }

    /*Realoca os tokens e adiciona NULL ao final de cada um*/
    if (pip == true){
      tokens = (char**) realloc(tokens, (contTokens + 1) * sizeof(char*));
      tokens[contTokens] = NULL;
      tokens2 = (char**) realloc(tokens2, (cont + 2) * sizeof(char*));
      tokens2[cont] = arqEntrada;
      tokens2[cont + 1] = NULL;
    } else {
      tokens = (char**) realloc(tokens, (cont + 1) * sizeof(char*));
      tokens[cont] = NULL;
    }

    /*Executa o comando*/
    pid = getpid();
    switch(pid = fork()) {
        case -1:
            perror("fork");
            exit(1);
        case 0:
            if (saida == true && pip == false) //Saida é escrita em arquivo
                freopen(arqSaida, "w", stdout);
            if (entrada == true){ //Substitui a entrada pelo arquivo
              if(!access(arqEntrada, F_OK)){
                if(!access(arqEntrada, R_OK)){ //checa se é possível acessar o arq
                  freopen(arqEntrada, "r", stdin);
                }
              }
              else {
                puts("Arquivo não encontrado");
              }
            }
            /*se há pipe fecha a entrada duplica a saída*/
            if (pip == true){
              close(fd[0]);
              dup2(fd[1], 1);
            }
            execvp(tokens[0], tokens);
            perror("Comando nao encontrado");
            exit (1);
        default:
          if (!background){ //Se o processo não é background ele é esperado
            waitpid(pid, &status, WUNTRACED);
            if (pip == true)
              close(fd[1]);
          }
    }

    /* se existir mais de um processo (|), ele é executado*/
    if (pip == true){
      switch(pid = fork()) {
          case -1:
              perror("fork");
              exit(1);
          case 0:
              if (saida == true)
                freopen(arqSaida, "w", stdout);

              /*fecha a saida e duplica a entrada*/
              close(fd[1]);
              dup2(fd[0], 0);

              while(read(fd[0], &ch, 1) > 0)
                write(1, &ch, 1);
              execvp(tokens2[0], tokens2);
              perror("Comando nao encontrado");
              exit(1);
          default:
            if (!background){
              waitpid(pid, &status, WUNTRACED);
              close(fd[0]);
            }
      }
    }
    free(tokens);
    free(tokens2);
  }
}
