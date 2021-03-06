//
//  main.c
//  lab5
//
//  Created by 许鑫 on 2018/3/4.
//  Copyright © 2018年 osu. All rights reserved.
//

/**********************************************************/
/* This program is a 'pass and play' version of tictactoe */
/* Two users, player 1 and player 2, pass the game back   */
/* and forth, on a single computer                        */
/**********************************************************/

/* include files go here */
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
/* #define section, for now we will define the number of rows and columns */
#define ROWS  3
#define COLUMNS  3
#define MAXWAIT 5
#define MOVEBYTE 4
#define TIMEOUT_S 20

#define SERVER_PLAYER 1
#define CLIENT_PLAYER 2

#define MAX_CLIENTS 10
#define MAX_IVMV 5

#define MOVE 0
#define NEWGAME 1
#define ENDGAME 2

#define SLOT_FULL -1
#define SLOT_LIMIT MAX_CLIENTS
#define INIT_ALL_SLOT -1

/* struct definition */
struct message{
    int command;
    int choice;
    int slotNum;
};

struct status{
    int EGSEND; // ENDGAME SEND, 1 for sent, 0 for no send yet
    int IVMVNUM; // Invalid Move Number
};

static struct message SLOT_FULL_MSG = {NEWGAME, 0, SLOT_FULL};



/* C language requires that you predefine all the routines you are writing */
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS], int slotNum);
int tictactoe(char board[ROWS][COLUMNS], int choice, int player);
int initSharedState(char board[ROWS][COLUMNS]);
void initSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], int slot[SLOT_LIMIT], int slotNum);
int server(int port);
struct message findSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], int slot[SLOT_LIMIT]);
int inputChoice(char board[SLOT_LIMIT][ROWS][COLUMNS], struct message* msg, int player);

extern int errno;



int main(int argc, char *argv[])
{
    
    
    char * usage= "Usage:\ntictactoeServer <portNumber>\n";
    if (argc<2){
        printf("%s",usage );
        return 0;
    }
    int portNum;
    portNum = atoi(argv[1]);
    server(portNum);
    return 0;
}

int server(int port) {
    printf("port: %d\n", port);
    int sd;
    int connected_sd;
    int rc, inrc;
    struct sockaddr_in server_address;
    struct sockaddr_in from_address;
    struct timeval tv;
    struct message msg;
    struct message rtmsg;
    socklen_t address_length = sizeof(from_address);
    
    //variables for socket set
    int clientSDList[MAX_CLIENTS] = {0};
    struct status clientStatus[MAX_CLIENTS];
    fd_set socketFDS;
    int maxSD = 0;
    bzero(clientStatus, SLOT_LIMIT*sizeof(struct status));
    
    
    // Initialize the 'game' board
    char boards[SLOT_LIMIT][ROWS][COLUMNS];
    int slot[SLOT_LIMIT];
    int slotUsed = 0;
    initSlot(boards, slot, INIT_ALL_SLOT);
    
    
    // Initialize server address
    sd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    
    
    // Initialize timeout
    tv.tv_sec = TIMEOUT_S;
    tv.tv_usec = 0;
    
    bind (sd, (struct sockaddr *)&server_address, sizeof(server_address));
    listen (sd, 5);
    maxSD = sd;
    for(;;) {
        // reset socket set every time
        FD_ZERO(&socketFDS);
        FD_SET(sd, &socketFDS);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clientSDList[i] > 0) {
                FD_SET(clientSDList[i], &socketFDS);
                if (clientSDList[i] > maxSD) maxSD = clientSDList[i];
            }
        }
        tv.tv_sec = TIMEOUT_S;
        tv.tv_usec = 0;
        rc = select (maxSD+1, &socketFDS, NULL, NULL, &tv);
        printf("----%d sockets gets message!----\n", rc);
        
        // handle timeout
        if (rc == 0) { // timeout
            // initialize all slot and close all socket
            initSlot(boards, slot, INIT_ALL_SLOT);
            bzero(clientStatus, SLOT_LIMIT*sizeof(struct status));
            slotUsed = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSDList[i] > 0) {
                    printf("time out, client %d has been closed\n", i);
                    close(clientSDList[i]);
                    clientSDList[i] = 0;
                }
            }
            continue; //
        }
        // test
        if (FD_ISSET(sd, &socketFDS)) {
            printf("----master socket msg, slot numer: %d----\n", slotUsed);
        }
        
        // handle slot full
        if (FD_ISSET(sd, &socketFDS) && slotUsed == SLOT_LIMIT) {
            connected_sd = accept(sd, (struct sockaddr *)&from_address, &address_length);
            rtmsg = SLOT_FULL_MSG;
            rtmsg.choice = htonl(rtmsg.choice); rtmsg.command = htonl(rtmsg.command); rtmsg.slotNum = htonl(rtmsg.slotNum);
            rc = write(connected_sd, &SLOT_FULL_MSG, sizeof(SLOT_FULL_MSG));
            close(connected_sd);
        }
        
        // handle new client
        if (FD_ISSET(sd, &socketFDS) && slotUsed != SLOT_LIMIT) {
            connected_sd = accept(sd, (struct sockaddr *)&from_address, &address_length);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSDList[i] == 0) {
                    printf("--------client %d has connected----\n", i);
                    clientSDList[i] = connected_sd;
                    slotUsed += 1;
                    // send newgame back
                    break;
                }
            }
        }
        
        //check for each socket
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(clientSDList[i], &socketFDS)) { // ith socket can be read.
                rc = read(clientSDList[i], &msg, sizeof(msg));
                if (rc < 0) { // error
                    printf("error while reading message in slot %d\n", i);
                    return -1;
                } else if (rc == 0) { // client disconnected
                    printf("----client %d has been closed----\n", i);
                    close(clientSDList[i]);
                    initSlot(boards, slot, i);
                    clientSDList[i] = 0;
                    bzero(&clientStatus[i], sizeof(struct status));
                    slotUsed -= 1;
                } else { // handle valid message
                    printf("----client %d receives correct messages----\n", i);
                    msg.command = ntohl(msg.command); msg.slotNum = ntohl(msg.slotNum); msg.choice = ntohl(msg.choice);
                    int rwrc, clientEnd = 0;
                    if (msg.command == NEWGAME) { // ***CASE*** NEWGAME command
                        printf("--------newgame command----\n");
                        rtmsg.command = NEWGAME; rtmsg.slotNum = i; rtmsg.choice = 0;
                        rtmsg.choice = htonl(rtmsg.choice); rtmsg.command = htonl(rtmsg.command); rtmsg.slotNum = htonl(rtmsg.slotNum);
                        rwrc = write(clientSDList[i], &rtmsg, sizeof(rtmsg));
                        if(rwrc < 0) { printf("error while sending choice"); return -1; }
                        
                    } else if (msg.command == MOVE && msg.slotNum == i) { // ***CASE*** MOVE command
                        printf("--------move command----\n");
                        int gamerc = tictactoe(boards[msg.slotNum], msg.choice, CLIENT_PLAYER);
                        print_board(boards[msg.slotNum], msg.slotNum);
                        if (gamerc == -1) {
                            clientStatus[i].IVMVNUM += 1;
                        } else if (gamerc == 0) { // game continues
                            rtmsg.slotNum = i;
                            rtmsg.command = MOVE;
                            inrc = inputChoice(boards, &rtmsg, SERVER_PLAYER);
                            rtmsg.choice = htonl(rtmsg.choice); rtmsg.command = htonl(rtmsg.command); rtmsg.slotNum = htonl(rtmsg.slotNum);
                            rwrc = write(clientSDList[i], &rtmsg, sizeof(rtmsg));
                            
                            if(rwrc < 0) { printf("error while sending choice"); return -1; }
                            
                        } else if (gamerc != SERVER_PLAYER){ // game end: client wins or game is draw.
                            clientEnd = 1;
                            // nothing to do
                        }
                        if (clientEnd == 1) {
                            printf("------------CLIENT END, server send ENDGAME command----\n");
                            rtmsg.command = ENDGAME; rtmsg.choice = 0; rtmsg.slotNum = i;
                            rtmsg.choice = htonl(rtmsg.choice); rtmsg.command = htonl(rtmsg.command); rtmsg.slotNum = htonl(rtmsg.slotNum);
                            rwrc = write(clientSDList[i], &rtmsg, sizeof(rtmsg));
                            if(rwrc < 0) { printf("error while sending choice"); return -1; }
                            clientStatus[i].EGSEND = 1;
                        }
                    } else if (msg.command == ENDGAME) { // ***CASE*** ENDGAME handshake
                        printf("--------ENDGAME command----\n");
                        if (clientStatus[i].EGSEND == 0) { // server needs to sent ENDGAME
                            rtmsg.command = ENDGAME; rtmsg.choice = 0; rtmsg.slotNum = i;
                            rtmsg.choice = htonl(rtmsg.choice); rtmsg.command = htonl(rtmsg.command); rtmsg.slotNum = htonl(rtmsg.slotNum);
                            rwrc = write(clientSDList[i], &rtmsg, sizeof(rtmsg));
                            if(rwrc < 0) { printf("error while sending choice"); return -1; }
                        }
                        printf("Slot %d has finished.\n", msg.slotNum);
                        close(clientSDList[i]);
                        initSlot(boards, slot, i);
                        clientSDList[i] = 0;
                        bzero(&clientStatus[i], sizeof(struct status));
                        slotUsed -= 1;
                    } else { // ***CASE*** invalid command
                        clientStatus[i].IVMVNUM += 1;
                    }
                }
                if (clientStatus[i].IVMVNUM >= MAX_IVMV) {
                    close(clientSDList[i]);
                    initSlot(boards, slot, i);
                    clientSDList[i] = 0;
                    bzero(&clientStatus[i], sizeof(struct status));
                    slotUsed -= 1;
                }
            }
        }
        printf("********now slot numer: %d********\n", slotUsed);
        
    }
    return 0;
}


int inputChoice(char boards[SLOT_LIMIT][ROWS][COLUMNS], struct message* msg, int player) {
    int choice = 1;
    int rc = -1;
    while (rc == -1) {
        rc = tictactoe(boards[(*msg).slotNum], choice, player);
        if (rc != -1) {
            (*msg).choice = choice;
            break;
        }
        choice++;
        if (choice > 9) return -1; // cannot find a valid choice
    }
    print_board(boards[(*msg).slotNum], (*msg).slotNum);
    if (rc == 0) { // game continues
        return 0;
    }
    return 1; // game ends
}

struct message findSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], int slot[SLOT_LIMIT]) {
    struct message msg;
    msg.command = MOVE;
    for (int i = 0; i < SLOT_LIMIT; i++) {
        if (slot[i] == 0) {
            slot[i] = 1;
            msg.slotNum = i;
            return msg;
        }
    }
    msg.slotNum = SLOT_FULL;
    return msg;
}

void initSlot(char boards[SLOT_LIMIT][ROWS][COLUMNS], int slot[SLOT_LIMIT], int slotNum) {
    if (slotNum == INIT_ALL_SLOT){
        for (int i = 0; i < SLOT_LIMIT; i++) {
            slot[i] = 0;
            initSharedState(boards[i]);
            printf ("slot %d has been initialized\n", i);
        }
    } else {
        slot[slotNum] = 0;
        initSharedState(boards[slotNum]);
        printf ("slot %d has been initialized\n", slotNum);
    }
}

int tictactoe(char board[ROWS][COLUMNS], int choice, int player)
{
    /* this is the meat of the game, you'll look here for how to change it up */
    int i;  // used for keeping track of choice user makes
    int row, column;
    char mark;      // either an 'x' or an 'o'
    
    if (choice < 1 || choice > ROWS*COLUMNS) return -1;
    
    mark = (player == 1) ? 'X' : 'O'; //depending on who the player is, either us x or o
    /******************************************************************/
    /** little math here. you know the squares are numbered 1-9, but  */
    /* the program is using 3 rows and 3 columns. We have to do some  */
    /* simple math to conver a 1-9 to the right row/column            */
    /******************************************************************/
    
    row = (int)((choice-1) / ROWS);
    column = (choice-1) % COLUMNS;
    
    /* first check to see if the row/column chosen is has a digit in it, if it */
    /* square 8 has and '8' then it is a valid choice                          */
    
    if (board[row][column] == (choice+'0'))
        board[row][column] = mark;
    else{
        return -1;
    }
    /* after a move, check to see if someone won! (or if there is a draw */
    i = checkwin(board);
    
    if (i ==  -1) return 0;
    else if (i == 1) {// means a player won!! congratulate them
        printf("==>\aPlayer %d wins\n ", player);
        return player;
    } else {
        printf("==>\aGame draw\n"); // ran out of squares, it is a draw
        return -2;
    }
    
}

int checkwin(char board[ROWS][COLUMNS])
{
    /************************************************************************/
    /* brute force check to see if someone won, or if there is a draw       */
    /* return a 0 if the game is 'over' and return -1 if game should go on  */
    /************************************************************************/
    if (board[0][0] == board[0][1] && board[0][1] == board[0][2] ) // row matches
        return 1;
    
    else if (board[1][0] == board[1][1] && board[1][1] == board[1][2] ) // row matches
        return 1;
    
    else if (board[2][0] == board[2][1] && board[2][1] == board[2][2] ) // row matches
        return 1;
    
    else if (board[0][0] == board[1][0] && board[1][0] == board[2][0] ) // column
        return 1;
    
    else if (board[0][1] == board[1][1] && board[1][1] == board[2][1] ) // column
        return 1;
    
    else if (board[0][2] == board[1][2] && board[1][2] == board[2][2] ) // column
        return 1;
    
    else if (board[0][0] == board[1][1] && board[1][1] == board[2][2] ) // diagonal
        return 1;
    
    else if (board[2][0] == board[1][1] && board[1][1] == board[0][2] ) // diagonal
        return 1;
    
    else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
             board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' &&
             board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')
        
        return 0; // Return of 0 means game over
    else
        return  - 1; // return of -1 means keep playing
}

void print_board(char board[ROWS][COLUMNS], int slotNum)
{
    /*****************************************************************/
    /* brute force print out the board and all the squares/values    */
    /*****************************************************************/
    return;
    
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

int initSharedState(char board[ROWS][COLUMNS]){
    /* this just initializing the shared state aka the board */
    int i, j, count = 1;
    for (i=0;i<3;i++)
        for (j=0;j<3;j++){
            board[i][j] = count + '0';
            count++;
        }
    
    
    return 0;
    
}
