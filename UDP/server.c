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
#include <fcntl.h>
#include <time.h>

/* #define section, for now we will define the number of rows and columns */
#define ROWS 3
#define COLUMNS 3
#define TIMEOUT_S 5
#define INTERVAL 1

#define SERVER_PLAYER 1
#define CLIENT_PLAYER 2

#define END 1
#define CONTINUE 0
#define ERROR -1
#define OK 1
/* struct definition */
struct message {
  int command;
  int choice;
  int slotNum;
};

struct slot {
  int used;
  clock_t last_time;
  struct sockaddr_in from_address;
  struct message last_sent;
  int retry_time;
};

#define NEWGAME 1
#define MOVE 0
#define ENDGAME 2
#define SLOT_FULL -1
#define SLOT_LIMIT 100
#define INIT_ALL_SLOT -1

/* C language requires that you predefine all the routines you are writing */
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS], int slotNum);
int tictactoe(char board[ROWS][COLUMNS], int choice, int player);
int initSharedState(char board[ROWS][COLUMNS]);
void initSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], struct slot *slots,
              int slotNum);

int server(int port);
struct message findSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS],
                        struct slot *slots, struct sockaddr_in from_address);
int inputChoice(char board[SLOT_LIMIT][ROWS][COLUMNS],
                struct message *msg, int player);
int resent (char boards[SLOT_LIMIT][ROWS][COLUMNS], struct slot *slots, int sd, socklen_t address_length, clock_t *last_check);
extern int errno;

int main(int argc, char *argv[]) {
  char *usage = "Usage:\ntictactoeServer <portNumber>\n";
  if (argc < 2) {
    printf("%s", usage);
    return 0;
  }
  int portNum;
  portNum = atoi(argv[1]);
  server(portNum);
  return OK;
}

int server(int port) {
  int sd;
  int rc;
  struct sockaddr_in server_address;
  // struct timeval tv;
  struct message msg;
  struct message rtmsg;
  int flags = 0;
  struct sockaddr_in from_address;
  socklen_t address_length = sizeof(struct sockaddr_in);

  // Initialize the 'game' board
  char boards[SLOT_LIMIT][ROWS][COLUMNS];
  struct slot slots[SLOT_LIMIT];
  int slotUsed = 0;
  initSlot(boards, slots, INIT_ALL_SLOT);
  clock_t last_check;

  // Initialize server address
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = INADDR_ANY;
  bind(sd, (struct sockaddr *)&server_address, sizeof(server_address));

  // Initialize timeout
  // tv.tv_sec = INTERVAL;
  // tv.tv_usec = 0;
  // if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
  //   perror("Error");
  //   exit(1);
  // }

  // Set Non-Blocking
  fcntl(sd,F_SETFL,O_NONBLOCK);

  last_check=clock();
  while (1) {
    rc = recvfrom(sd, &msg, sizeof(msg), flags,
                  (struct sockaddr *)&from_address, &address_length);
    resent(boards, slots, sd, address_length,&last_check);
    if (rc > 0) { // handle correct message
      msg.choice = ntohl(msg.choice);
      msg.command = ntohl(msg.command);
      msg.slotNum = ntohl(msg.slotNum);
      if (msg.command == NEWGAME) { // NEWGAME command
        rtmsg = findSlot(boards, slots, from_address);
        rtmsg.choice = htonl(rtmsg.choice);
        rtmsg.command = htonl(rtmsg.command);
        rtmsg.slotNum = htonl(rtmsg.slotNum);
        if ((rc = sendto(sd, &rtmsg, sizeof(rtmsg), flags,
                    (struct sockaddr *)&from_address, address_length))< -1) {
          perror("Error");
          exit(1);
        }
      } else if (msg.command == MOVE &&
                 slots[msg.slotNum].used == 1 &&
               from_address.sin_addr.s_addr==slots[msg.slotNum].from_address.sin_addr.s_addr &&
             from_address.sin_port==slots[msg.slotNum].from_address.sin_port) { // MOVE command
        rc = tictactoe(boards[msg.slotNum], msg.choice, CLIENT_PLAYER);
        if (rc == CONTINUE) {
          slots[msg.slotNum].last_time=clock();
          rtmsg.slotNum = msg.slotNum;
          rtmsg.command = MOVE;
          inputChoice(boards, &rtmsg, SERVER_PLAYER);

          rtmsg.choice = htonl(rtmsg.choice);
          rtmsg.command = htonl(rtmsg.command);
          rtmsg.slotNum = htonl(rtmsg.slotNum);

          slots[msg.slotNum].last_sent=rtmsg;

          if ((rc = sendto(sd, &rtmsg, sizeof(rtmsg), flags,
                      (struct sockaddr *)&from_address, address_length)) < -1) {
            perror("Error");
            exit(1);
          }
        } else if (rc==END){
          slots[msg.slotNum].last_time=clock();
          rtmsg.command = ENDGAME;
          rtmsg.slotNum = msg.slotNum;

          rtmsg.command = htonl(rtmsg.command);
          rtmsg.slotNum = htonl(rtmsg.slotNum);

          slots[msg.slotNum].last_sent=rtmsg;
          slots[msg.slotNum].used=ENDGAME;

          rc = sendto(sd, &rtmsg, sizeof(rtmsg), flags,
                      (struct sockaddr *)&from_address, address_length);
        }
      }else if (msg.command==ENDGAME){
        if(slots[msg.slotNum].used!=ENDGAME){
          rtmsg.command = ENDGAME;
          rtmsg.slotNum = msg.slotNum;
          rtmsg.command = htonl(rtmsg.command);
          rtmsg.slotNum = htonl(rtmsg.slotNum);
          rc = sendto(sd, &rtmsg, sizeof(rtmsg), flags,
                      (struct sockaddr *)&from_address, address_length);
        }
        printf("slot %d has finished.\n", msg.slotNum);
        initSlot(boards, slots, msg.slotNum);
        slotUsed -= 1;
      }
    }
  }
  return OK;
}


int resent (char boards[SLOT_LIMIT][ROWS][COLUMNS], struct slot *slots, int sd, socklen_t address_length, clock_t *last_check){
  clock_t current_time = clock();
  int diff;
  diff = (current_time-*last_check)*1000/CLOCKS_PER_SEC;
  if(diff<1000)
    return ERROR;
  for(int i=0;i<SLOT_LIMIT;i++){
    diff = (current_time-slots[i].last_time)/CLOCKS_PER_SEC;
    if(slots[i].used!=0){
      if (slots[i].retry_time>=5){
        printf("slot %d time out\n",i );
        initSlot(boards, slots, i);
      }
      else if(diff>=5){
        slots[i].last_time=clock();
        slots[i].retry_time++;
        sendto(sd, &(slots[i].last_sent), sizeof(struct message), 0,
                    (struct sockaddr *)&(slots[i].from_address), address_length);
      }
    }
  }
  *last_check=clock();
  return OK;
}

int inputChoice(char boards[SLOT_LIMIT][ROWS][COLUMNS],
                struct message *msg, int player) {
  int choice = 1;
  int rc = ERROR;
  while (1) {
    rc = tictactoe(boards[(*msg).slotNum], choice, player);
    if(rc!=ERROR){
      (*msg).choice=choice;
      return OK;
    }
    choice++;
  }
}

struct message findSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS],
                        struct slot *slots, struct sockaddr_in from_address) {
  struct message msg;
  msg.command = NEWGAME;
  for (int i = 0; i < SLOT_LIMIT; i++) {
    if (slots[i].used == 0) {
      slots[i].used = 1;
      slots[i].last_time=clock();
      slots[i].from_address=from_address;
      slots[i].retry_time=0;
      msg.slotNum = i;
      printf("slot %d is up\n", i);
      return msg;
    }
  }
  msg.slotNum = SLOT_FULL;
  return msg;
}

void initSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], struct slot *slots,
              int slotNum) {
  if (slotNum == INIT_ALL_SLOT) {
    for (int i = 0; i < SLOT_LIMIT; i++) {
      slots[i].used = 0;
      initSharedState(boards[i]);
      printf("slot %d has been initialized\n", i);
    }
  } else {
    slots[slotNum].used = 0;
    initSharedState(boards[slotNum]);
    printf("slot %d has been initialized\n", slotNum);
  }
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

  row = (int)((choice - 1) / ROWS);
  column = (choice - 1) % COLUMNS;

  /* first check to see if the row/column chosen is has a digit in it, if it */
  /* square 8 has and '8' then it is a valid choice                          */

  if (board[row][column] == (choice + '0'))
    board[row][column] = mark;
  else {
    return ERROR; // means the choice is not valid.
  }
  /* after a move, check to see if someone won! (or if there is a draw */
  i = checkwin(board);

  if (i == -1)
    return CONTINUE;        // means the choice is good and game goes on.
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

void print_board(char board[ROWS][COLUMNS], int slotNum) {
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\n\n\tCurrent TicTacToe Game Slot %d\n\n", slotNum);

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
