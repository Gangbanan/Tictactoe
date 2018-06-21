/**********************************************************/
/* This program is a 'pass and play' version of tictactoe */
/* Two users, player 1 and player 2, pass the game back   */
/* and forth, on a single computer                        */
/**********************************************************/

/* include files go here */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* #define section, for now we will define the number of rows and columns */
#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"
#define CF_NAME "servers"

#define ROWS 3
#define COLUMNS 3
#define TIMEOUT_S 5

#define CONTINUE 0
#define ERROR -1
#define END 1

#define MOVE 0
#define RESUME 3
#define ENDGAME 2
#define NEWGAME 1

#define SERVER 0
#define CLIENT 1

/* C language requires that you predefine all the routines you are writing */
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int tictactoe(char board[ROWS][COLUMNS], int choice, int player);
int initSharedState(char board[ROWS][COLUMNS]);
int saveAddress(char ip[],int port);
struct sockaddr_in findServer();

extern int errno;

struct message{
  int command;
  int square;
  int gameNum;
  char board[3][3];
};

int main(int argc, char *argv[]) {
  struct sockaddr_in server_address;
  int sd=socket(AF_INET, SOCK_STREAM, 0);
  struct message m;
  struct timeval tv;
  int gameState=NEWGAME;
  int player;
  int res;
  int port;
  char ip[30];
  char board[3][3];
  int i,j;
  int retry_count=0;
  //Set timeout
  if(argc<3) server_address=findServer();
  else{
    int portNum;
    char serverIP[29];
    portNum = strtol(argv[1], NULL, 10);
    strcpy(serverIP, argv[2]);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portNum);
    server_address.sin_addr.s_addr = inet_addr(serverIP);
  }
  tv.tv_sec = TIMEOUT_S;
  tv.tv_usec = 0;
  if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
    return -1;
  }
  initSharedState(board);
  initSharedState(m.board);

  while(1){
    if(retry_count==5) break;
    if(gameState==NEWGAME){

      port=ntohs(server_address.sin_port);
      strcpy(ip,inet_ntoa(server_address.sin_addr));
      printf("Connecting to the following server...\n");
      printf("IP: %s\n", ip);
      printf("Port: %d\n",port);
      retry_count++;
      if(connect(sd,	(struct sockaddr *)&server_address,	sizeof(struct sockaddr_in))	<	0){
        perror("ERROR");
        continue;
      }
      m.command = htonl(NEWGAME);
      m.square = htonl(0);
      m.gameNum = htonl(0);
      if (write(sd,&m,sizeof(m)) <= 0){
        perror("ERROR");
        continue;
      }
      if (read(sd,&m,sizeof(m)) <=0){
        perror("ERROR");
        continue;
      }
      if(ntohl(m.gameNum)!=-1&&ntohl(m.command)==NEWGAME){
          gameState = MOVE;
          player = CLIENT;
      }
    }else if (gameState==RESUME){
      close(sd);
      sd=socket(AF_INET, SOCK_STREAM, 0);
      retry_count++;
      printf("Resuming Game...\n");

      server_address=findServer();
      if(connect(sd,	(struct sockaddr *)&server_address,	sizeof(struct sockaddr_in))	<	0){
        perror("ERROR");
        continue;
      }
      m.command = htonl(RESUME);
      for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
          m.board[i][j] = board[i][j];
        }
      if (write(sd,&m,sizeof(m)) <= 0){
        perror("ERROR");
        continue;
      }
      gameState = MOVE;
      player = SERVER;
    }else if (gameState==MOVE){
      if (player == SERVER) { // waiting server
        printf("Waiting for server entering choice.\n");
        if (read(sd,&m,sizeof(m))<=0) {
          perror("ERROR");
          gameState=RESUME;
          continue;
        }

        //if receive endgame reply endgame and end.
        if (ntohl(m.command)==ENDGAME){
          m.command = ENDGAME;
          if(write(sd, &m, sizeof(m)<=0)){
            perror("ERROR");
            gameState = RESUME;
            continue;
          }else{
            break;
          }
        }

        //if received the move
        m.square = ntohl(m.square);
        res = tictactoe(board, m.square, player);
        if (res==ERROR) {
          printf("Bad Move Received, Disconnected \n" );
          gameState = RESUME;
          continue;
        }
        if(res == END){
          //if receive the end move
          m.command= htonl(ENDGAME);
          gameState = ENDGAME;
          if (write(sd, &m, sizeof(m))<=0){
            perror("ERROR");
            gameState = RESUME;
            continue;
          }
        }
        player = CLIENT;
      } else if (player == CLIENT){ // send choice
        print_board(board);
        printf("Enter your choice:\n");
        scanf("%d", &m.square);
        res = tictactoe(board, m.square, player);
        if (res == ERROR){
          printf("Invalid move\n");
          while (getchar() != '\n') ;
          continue;
        }
        m.square = htonl(m.square);
        m.command = htonl(MOVE);
        for (i = 0; i < 3; i++)
          for (j = 0; j < 3; j++) {
            m.board[i][j] = board[i][j];
          }
        if (write(sd,&m,sizeof(m)) <= 0) {
          perror("ERROR");
          gameState=RESUME;
          continue;
        }
        player = SERVER;
      }
    }else if (gameState==ENDGAME){
      printf("Waiting for ENDGAME\n" );
      if(read(sd, &m, sizeof(m))<=0){
        perror("ERROR");
        gameState= RESUME;
        continue;
      }
      if(ntohl(m.command)==ENDGAME) break;
      else gameState=RESUME;
    }
  }
  printf("Game End\n");
  close(sd);
  return 0;
}

struct sockaddr_in findServer(){
  //Define the multicast address.
  struct sockaddr_in mc_addr;
  mc_addr.sin_family = AF_INET;
  mc_addr.sin_port = htons(MC_PORT);
  socklen_t mc_length = sizeof(mc_addr);
  mc_addr.sin_addr.s_addr = inet_addr(MC_GROUP);
  char mc_message = '1';

  //multicast information 1 and wait for response
  int mc_sd=socket(AF_INET, SOCK_DGRAM, 0);
  sendto(mc_sd, &mc_message, sizeof(mc_message), 0, (struct sockaddr *)&mc_addr, mc_length);
  struct timeval tv;
  int rc;
  tv.tv_sec = TIMEOUT_S;
  tv.tv_usec = 0;
  setsockopt(mc_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t server_length= sizeof(server_addr);
  int port_message=-1;
  rc =recvfrom(mc_sd, &port_message, sizeof(port_message), 0, (struct sockaddr *)&server_addr , &server_length);

  char ip[30];
  int port=-1;
  //write config file or read server from config file
  if(rc>0){
    port_message=ntohl(port_message);
    server_addr.sin_port=htons(port_message);
  }else{
    // read from config file
    printf("Got no response from multicast...\n" );
    FILE* cfgFile;
    cfgFile = fopen(CF_NAME,"r");
    if (cfgFile==NULL){
      // config file do not exist
      printf("The config file does not exist...\nPlease input the server ip and port\n");
      printf("IP: ");
      scanf("%s", ip);
      printf("Port: ");
      scanf("%d", &port);
    }else{
      fscanf(cfgFile,"%s",ip);
      fscanf(cfgFile,"%d",&port);
      fclose(cfgFile);
    }
    server_addr.sin_addr.s_addr=inet_addr(ip);
    server_addr.sin_port=htons(port);
  }
  close(mc_sd);
  return server_addr;
}

int saveAddress(char ip[],int port){
  FILE* file;
  file = fopen(CF_NAME,"w");
  fprintf(file, "%s\n%d\n",ip,port);
  fclose(file);
  return 0;
}

int tictactoe(char board[ROWS][COLUMNS], int choice, int player) {
  /* this is the meat of the game, you'll look here for how to change it up */
  int i; // used for keeping track of choice user makes
  int row, column;
  char mark; // either an 'x' or an 'o'

  mark = (player == 1)
             ? 'X'
             : 'O'; // depending on who the player is, either us x or o
  /******************************************************************/
  /** little math here. you know the squares are numbered 1-9, but  */
  /* the program is using 3 rows and 3 columns. We have to do some  */
  /* simple math to conver a 1-9 to the right row/column            */
  /******************************************************************/
  if (choice >= 1 && choice <= 9)
    ;
  else {
    return ERROR;
  }

  row = (int)((choice - 1) / ROWS);
  column = (choice - 1) % COLUMNS;

  /* first check to see if the row/column chosen is has a digit in it, if it */
  /* square 8 has and '8' then it is a valid choice                          */

  if (board[row][column] == (choice + '0'))
    board[row][column] = mark;
  else {
    return ERROR;
  }
  /* after a move, check to see if someone won! (or if there is a draw */
  i = checkwin(board);

  /* print out the board again */
  print_board(board);
  if (i == -1)
    return CONTINUE;
  else if (i == 1) { // means a player won!! congratulate them
    printf("==>\aPlayer %d wins\n", player);
    return END;
  } else {
    printf("==>\aGame draw\n"); // ran out of squares, it is a draw
    return END;
  }
}

int checkwin(char board[ROWS][COLUMNS]) {
  /************************************************************************/
  /* brute force check to see if someone won, or if there is a draw       */
  /* return a 0 if the game is 'over' and return -1 if game should go on  */
  /************************************************************************/
  if (board[0][0] == board[0][1] && board[0][1] == board[0][2]) // row matches
    return 1;

  else if (board[1][0] == board[1][1] &&
           board[1][1] == board[1][2]) // row matches
    return 1;

  else if (board[2][0] == board[2][1] &&
           board[2][1] == board[2][2]) // row matches
    return 1;

  else if (board[0][0] == board[1][0] && board[1][0] == board[2][0]) // column
    return 1;

  else if (board[0][1] == board[1][1] && board[1][1] == board[2][1]) // column
    return 1;

  else if (board[0][2] == board[1][2] && board[1][2] == board[2][2]) // column
    return 1;

  else if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) // diagonal
    return 1;

  else if (board[2][0] == board[1][1] && board[1][1] == board[0][2]) // diagonal
    return 1;

  else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
           board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' &&
           board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')

    return 0; // Return of 0 means game over
  else
    return -1; // return of -1 means keep playing
}

void print_board(char board[ROWS][COLUMNS]) {
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\n\n\tCurrent TicTacToe Game\n\n");

  printf("Player 1 (X)  -  Player 2 (O)\n\n\n");

  printf("     |     |     \n");
  printf("  %c  |  %c  |  %c \n", board[0][0], board[0][1], board[0][2]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[1][0], board[1][1], board[1][2]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[2][0], board[2][1], board[2][2]);

  printf("     |     |     \n\n");
}

int initSharedState(char board[ROWS][COLUMNS]) {
  /* this just initializing the shared state aka the board */
  int i, j, count = 1;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++) {
      board[i][j] = count + '0';
      count++;
    }

  return 0;
}
