#include <ncurses.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define msleep(msec) usleep(msec*1000)
#define PORT 2232
#define MAX_LENGTH 256
#define Sadr (struct sockaddr *)


void * RecvMessage(void *arg);
void * SendMessage(void *arg);


WINDOW *chatBox;
WINDOW *messageBox;

int done = 0;
int TOPline;
int BOTline;
char MyUID[10];
int UID = 0;
int mode = 0;
int line = 0;
struct sockaddr_in Server;
socklen_t fromlen;


pthread_mutex_t chat_mutex;
pthread_mutex_t mutexsum;

int main(int argc, char *argv[])
{	
  ////////////////////////////
 //////	  Variables		/////
////////////////////////////
	
	int SocketFD;
	int * arg_r;
	int * arg_s;
	int option = 0;
	char nickname[20];
	pthread_t recvthread;
	pthread_t sendthread;
	
  ////////////////////////////
 //// 	Clean up		 ////
////////////////////////////
	memset(&Server, 0, sizeof(Server));
	memset(&MyUID, 0, sizeof(MyUID));
	memset(&nickname, 0, sizeof(nickname));
  ////////////////////////////
 //// 	Initialize		 ////
////////////////////////////	
	Server.sin_family = AF_INET;
	Server.sin_port = htons(PORT);
	fromlen = sizeof(Server);
	if(inet_pton(AF_INET, "127.0.0.1", &Server.sin_addr) <= 0)
	{
		perror("Convertation");
		shutdown(SocketFD, SHUT_RDWR);
		close(SocketFD);
		return 1;
	}
	
	if(pthread_mutex_init(&mutexsum, 0) != 0)
	{
		perror("\nSumm mutex error:init");
		shutdown(SocketFD, SHUT_RDWR);
		close(SocketFD);
		return 1;
	}
	if(pthread_mutex_init(&chat_mutex, 0) != 0)
	{
		perror("\nChat mutex error:init");
		shutdown(SocketFD, SHUT_RDWR);
		close(SocketFD);
		return 1;
	}
  ////////////////////////////
 //// 	Protocol		 ////
////////////////////////////
	if((SocketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Socket:create");
		shutdown(SocketFD, SHUT_RDWR);
		close(SocketFD);
		return 1;
	}
	if((connect(SocketFD, Sadr &Server, sizeof(Server))) == -1)
	{
		perror("Connection");
		shutdown(SocketFD, SHUT_RDWR);
		close(SocketFD);
		return 1;
	}
  ////////////////////////////
 //// 	Ask nickname	 ////
////////////////////////////
	do
	{
		printf("Enter your name without spaces: ");
		scanf("%s", nickname);
		if(strlen(nickname) > 20) printf("Sorry, you can't use this name (a lot of characters)\n");
	} while(strlen(nickname) > 20);

	send(SocketFD, nickname, 20, 0); //Send nickname
	recv(SocketFD, MyUID, 10, 0); //get UID from server

	UID = atoi(MyUID);
  /////////////////////////////
 ///////  Build windows  /////
/////////////////////////////
	initscr();			
	cbreak();
	
	start_color();			/* Start color 			*/
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_RED, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);

	attron(COLOR_PAIR(1));	

	TOPline = LINES / 1.3;
	BOTline = LINES - TOPline;
	
	refresh();
	
	chatBox = newwin(TOPline, COLS, 0, 0);
	wrefresh(chatBox);
	messageBox = newwin(BOTline, COLS, TOPline, 0);
	wrefresh(messageBox);

	scrollok(chatBox,TRUE);
	scrollok(messageBox,TRUE);
  ////////////////////////////////
 ///  Initialize recv thrd  /////
////////////////////////////////
	//arguments for receive function
	arg_r = malloc(sizeof(int));
	*arg_r = SocketFD;
	//arguments for send function
	arg_s = malloc(sizeof(int));
	*arg_s = SocketFD;
		
	pthread_create(&recvthread, 0, RecvMessage, arg_r);
	pthread_create(&sendthread, 0, SendMessage, arg_s);
	// Keep alive
	while(!done);
	//Clean up
	attroff(COLOR_PAIR(1));
	attroff(COLOR_PAIR(2));
	attroff(COLOR_PAIR(3));
	attroff(COLOR_PAIR(4));	
	pthread_mutex_destroy(&chat_mutex);
	pthread_mutex_destroy(&mutexsum);
	wborder(chatBox, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(chatBox);
	delwin(chatBox);
	wborder(messageBox, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(messageBox);
	delwin(messageBox);
	shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
	endwin();			/* End curses mode		  */
	return 0;
}

void * SendMessage(void *arg)
{
	int SocketFD;
	char msg[MAX_LENGTH];
	char buffer[290];
	char toUID[10];
	SocketFD = *((int *) arg);
	free(arg);
	pthread_detach(pthread_self());
	for(;;)
	{	
		wrefresh(chatBox);
		wrefresh(messageBox);
		memset(&buffer, 0, sizeof(buffer));
		memset(&msg, 0, sizeof(msg));
	
		//get original message
		mvwgetstr(messageBox,1,2,msg);
		if(strlen(msg) > MAX_LENGTH) 
		{	
			wclear(messageBox);
			pthread_mutex_lock(&chat_mutex);
			wattron(chatBox,COLOR_PAIR(4));
			wprintw(chatBox,"Sorry, you can't send the message (a lot of characters)");
			pthread_mutex_unlock(&chat_mutex);
			continue;
		}
		if(strlen(msg) > 0) 
		{	
			memset(&toUID, 0, sizeof(toUID));
			////////Parse message////////////
			/* if user entered a message and first three characters are "/pm" */
			if(msg[0] == '/' && msg[1] == 'p' && msg[2] == 'm')
			{
				int i = 3;
				int j = 0;
				char buf[290];
				memset(&buf, 0, sizeof(buf));
				while(msg[i] == ' ') i++;
				while(msg[i] != ' ')
				{
					toUID[j] = msg[i];
					i++;
					j++;
				}
				while(msg[i] == ' ') i++;
				j = 0;
				while(i < strlen(msg))
				{
					buf[j] = msg[i];
					j++;
					i++;
				}
				memset(&msg, 0, sizeof(msg));
				strcpy(msg, buf);
				//check correct message
				bool correct = true;
				for(i = 0; i < strlen(toUID); i++)
				{
					//the message is incorrect if it contains wrong characters
					if((int)(toUID[i]) < 48 || (int)(toUID[i]) > 57)
					{
						correct = false; //if incorrect
						break;
					}
				}
				//if no message
				if(strlen(msg) == 0)
				{
					wclear(messageBox);
					pthread_mutex_lock(&chat_mutex);
					wattron(chatBox,COLOR_PAIR(4));
					wprintw(chatBox,"No message, please type the message\n");
					pthread_mutex_unlock(&chat_mutex);
					continue;
				}
				if(!correct)
				{
					wclear(messageBox);
					pthread_mutex_lock(&chat_mutex);
					wattron(chatBox,COLOR_PAIR(4));
					wprintw(chatBox,"%s isn't UID\n", toUID);
					pthread_mutex_unlock(&chat_mutex);
					continue;
				}
			}	


			//Build the new message
			//MyUID;toUID;message
			//if message to all then toUID is "0"
			strcpy(buffer, MyUID);
			strcat(buffer, ";");
			strcat(buffer, toUID);
			strcat(buffer, ";");
			strcat(buffer, msg);
			if(strcmp(msg,"exit") == 0) 
			{
				//send msg if msg is "exit"
				done = 1;
				send(SocketFD, msg, sizeof(msg), 0); break;
			}
			else 
			{
				send(SocketFD, buffer, sizeof(buffer), 0); break;
			}
			wclear(messageBox);
		}	
	}
	return 0;
}

void * RecvMessage(void *arg)
{
	start_color();
	int SocketFD;
	char rmessage[290];
	int err;
	char md[10];
	SocketFD = *((int *) arg);
	free(arg);
	pthread_detach(pthread_self());
	for(;;msleep(100))
	{
		wrefresh(chatBox);
		wrefresh(messageBox);
		memset(&rmessage, 0, sizeof(rmessage));
		err = recv(SocketFD, rmessage, 290, 0); //get message

		recv(SocketFD, md, 10, 0); //get mode message(private or public)
			//parse message
			int i = 0;
			int uid = 0;
			char bfUID[10];
			int j = 0;
			memset(&bfUID, 0, sizeof(bfUID));
			while(rmessage[i] != '[') i++;
			i++;
			while(rmessage[i] != ']')
			{ 
				bfUID[j] = rmessage[i];
				i++;
				j++;
			}
			uid = atoi(bfUID);
		//Print
		//New comment
		//One more comment
		pthread_mutex_lock(&chat_mutex);
		if (strlen(rmessage) > 0) 
		{
			//if our message
			if(uid == UID) 
			{
				wattron(chatBox,COLOR_PAIR(3)); //green color in chat
				wprintw(chatBox, rmessage);
			}
			//if private message
			else if(strcmp(md, "priv") == 0) 
			{
				wattron(chatBox,COLOR_PAIR(2)); //red color in chat
				wprintw(chatBox, rmessage);
			}
			else 
			{	
				wattron(chatBox,COLOR_PAIR(1)); //original color
				wprintw(chatBox, rmessage);
			}

		}
		if(err == -1 || strlen(rmessage) <= 0)
		{
			done = 1;
			wattron(chatBox,COLOR_PAIR(4));
			wprintw(chatBox,"Connection has been lost");
			break;
		}
		pthread_mutex_unlock(&chat_mutex);
		
		//autoscroll
		pthread_mutex_lock (&mutexsum);
		if(line!=TOPline)
		{
			line++;
		}
		else
		{
			wscrl(chatBox, TOPline-line);
			line-=2;
		}
		pthread_mutex_unlock(&mutexsum);
	}
	return 0;
}
