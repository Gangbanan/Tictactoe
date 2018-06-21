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
#define ROWS 3
#define COLUMNS 3
#define TIMEOUT_S 5
#define CONTINUE 0
#define ERROR -1
#define MOVE 0
#define END 1
#define ENDGAME 2
#define NEWGAME 1

/* C language requires that you predefine all the routines you are writing */
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int tictactoe(char board[ROWS][COLUMNS], int choice, int player);
int initSharedState(char board[ROWS][COLUMNS]);
int client(char board[ROWS][COLUMNS], char serverIP[29], int portNum, int sd);

extern int errno;

struct message{
  int command;
  int square;
  int gameNum;
};

int main(int argc, char *argv[]) {

  int sd=socket(AF_INET, SOCK_STREAM, 0);
  char board[ROWS][COLUMNS];
  char *usage = "Usage:\nclient <portNumber> <ip address>\n";
  if (argc < 3) {
    printf("%s", usage);
    return 0;
  }
  initSharedState(board); // Initialize the 'game' board
  int portNum;
  char serverIP[29];
  portNum = strtol(argv[1], NULL, 10);
  strcpy(serverIP, argv[2]);
  if (-1 == client(board, serverIP, portNum,sd)){
    perror("Error");
    close(sd);
  }
  return 0;
}

int client(char board[ROWS][COLUMNS], char serverIP[29], int portNum, int sd) {
  struct message m;
  struct timeval tv;
  struct sockaddr_in server_address;
  int rc;

  //Set timeout
  tv.tv_sec = TIMEOUT_S;
  tv.tv_usec = 0;
  if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
    return -1;
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(portNum);
  server_address.sin_addr.s_addr = inet_addr(serverIP);

  if(connect(sd,	(struct sockaddr *)&server_address,	sizeof(struct sockaddr_in))	<	0)	{
    return -1;
  }

  int gameEnd = 0;
  int turn = 1;
  int player;
  int res;
  print_board(board);

  m.command = htonl(1);
  m.square = htonl(0);
  m.gameNum = htonl(0);
  printf("Connecting to the server...\n");
  if ((rc = write(sd,&m,sizeof(m))) < 0) {
    return -1;
  }
    // printf("Sent: %d \n",rc);
    // printf("Sent: %d , %d , %d\n",ntohl(m.command),ntohl(m.square),ntohl(m.gameNum));
  if ((rc = read(sd,&m,sizeof(m))) <0) {
    return -1;
  }

  if (ntohl(m.gameNum)==-1){
    printf("No more slots\n" );
    return 0;
  }

  if (ntohl(m.command)!=NEWGAME){
    return -1;
  }

  int ec =0;
  while (!gameEnd) {
    if (turn % 2 == 0) { // waiting server
      printf("waiting for server entering choice.\n");
      if ((rc = read(sd,&m,sizeof(m))) <0) {
        return -1;
      }

      // printf("Sent: %d Bytes\n",rc);
      // printf("Received: %d , %d , %d\n",ntohl(m.command),ntohl(m.square),ntohl(m.gameNum));
      if (ntohl(m.command)!=MOVE) continue;

      m.square = ntohl(m.square);
      player = 1;
      res = tictactoe(board, m.square, player);



      if (res == ERROR){
        printf("Received invalid move\n");
        ec ++;
        if (ec==5) {
          printf("Received 5 error choices\n");
          close(sd);
          return 0;
        }
        continue;
      }
      ec = 0;
      if(res == END){
        gameEnd = 1;
        m.command= htonl(ENDGAME);
        if ((rc = write(sd, &m, sizeof(m)))<0){
          return -1;
        }
        while(1){
          if (read(sd, &m , sizeof(m))<0) return -1;
          if (ntohl(m.command)==ENDGAME) break;
        }
      }
    } else { // send choice
      printf("enter your choice:\n");
      scanf("%d", &m.square);
      player = 2;
      res = tictactoe(board, m.square, player);
      if (res == ERROR){
        printf("Invalid move\n");
        while (getchar() != '\n')
          ;
        continue;
      }
      m.square = htonl(m.square);
      m.command = htonl(0);
      if ((rc = write(sd,&m,sizeof(m))) < 0) {
        return -1;
      }
      if (res ==END){
        gameEnd=1;
        while(1){
          if (read(sd, &m , sizeof(m))<0) return -1;
          if (ntohl(m.command)==ENDGAME) break;
        }
        m.command = htonl(ENDGAME);
        if ((rc = write(sd, &m, sizeof(m)))<0){
          return -1;
        }
      }
        // printf("Sent: %d Bytes\n",rc);
        // printf("Sent: %d , %d , %d\n",ntohl(m.command),ntohl(m.square),ntohl(m.gameNum));
    }
    turn++;
  }
  printf("Game End Normally\n");
  close(sd);
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
    printf("==>\aPlayer %d wins\n ", player);
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
