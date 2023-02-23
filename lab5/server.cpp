//
//  server.cpp - SEP 400 Lab 4
//  Created by:  Yiyuan Dong    2023-02-18
//

#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <queue>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

// #define DEBUG

using namespace std;

const char LOCALHOST[] = "127.0.0.1";
bool isRunning = true;
const int BUF_LEN = 4096;
const int NUMCLIENT = 3;

queue<string> messageQueue;
pthread_mutex_t lock_x;

int setupAddr(struct sockaddr_in &addr, int &sock, const char *port);
int setupSocket(int&);
void check(int, int &);
void *recv_func(void *arg);
void setupActionHandler();
static void signalHandler(int signum);

int main(int argc, char const *argv[]) {

    //  no port number provided, return -1
    if (argc < 2) {
        cout << "usage: client <port number>" << endl;
        return -1;
    }
#if defined(DEBUG)
    cout << "[" << getpid() <<"] Server running..."  << endl;   
#endif

    int ret;
    int connections = 0;
    int clients[NUMCLIENT];
    char buf[BUF_LEN];
    sockaddr_in cl_addrs[NUMCLIENT];
    struct sockaddr_in addr;
    pthread_t threads[NUMCLIENT];

    memset(&clients, 0, sizeof(clients));

    // sigaction 
    setupActionHandler();

    //  create a socket
    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    // check(setupSocket(master_socket), master_socket);

    //  setup addr (hint)
    check(setupAddr(addr, master_socket, argv[1]), master_socket);

    //  bind socket to addr
    check(bind(master_socket, (struct sockaddr *)&addr, sizeof(addr)),
          master_socket);

    //  mark the socket for listening in
    check(listen(master_socket, 5), master_socket);

    pthread_mutex_init(&lock_x, NULL);

    while (isRunning) {
#if defined(DEBUG)
            cout << "[Server] Waiting for connection... " << endl;
#endif
        if (connections < 3) {
            socklen_t cl_size = sizeof(cl_addrs[connections]);
            clients[connections] = accept(
                master_socket, (sockaddr *)&cl_addrs[connections], &cl_size);

            check(clients[connections], master_socket);

#if defined(DEBUG)
            cout << "[Accept] Client fd: " << clients[connections] << endl;
#endif

#if defined(DEBUG)
            cout << "[Recv] Creating thread for client fd " << clients[connections] << endl;
#endif

            check(pthread_create(&threads[connections], NULL, recv_func,
                                 &clients[connections]),
                  master_socket);
            connections += 1;
        }

        if (messageQueue.empty() == 0) {
            pthread_mutex_lock(&lock_x);
            cout << messageQueue.front() << endl;
            messageQueue.pop();
            pthread_mutex_unlock(&lock_x);
        }
        sleep(1);
    }

#if defined(DEBUG)
            cout << "[Server] Shutting down clients and closing sockets..." << endl;
#endif

    //  joining threads
    for(int i=0; i<NUMCLIENT; i++) 
        pthread_join(threads[i], NULL);

    // close sockets 
    for(int i=0; i<NUMCLIENT; i++) {
        ret = write(clients[i], "Quit", 4);
        if(ret == -1){
            cout << strerror(errno) << endl;
        } else if (ret < 4) {
            cout << "ERROR: Buffer partially write." << endl;
        }
        close(clients[i]);
    }
     close(master_socket); 

#if defined(DEBUG)
    cout << "[Server]: Process has shutdown sucessfully." << endl;
#endif

    return 0;
}

int setupSocket(int& sock) {

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

}

int setupAddr(struct sockaddr_in &addr, int &sock, const char *port) {
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port)); //  host to network short
    return inet_pton(
        AF_INET, LOCALHOST,
        &addr.sin_addr); //  inet_pton: internet command, convert pointer to a
                         //  string to a number, 0.0.0.0 to get any address
}

void setupActionHandler() {
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, NULL);
}

void check(int ret, int &sock) {
    
//     if (errno == EAGAIN || errno == EWOULDBLOCK) {
// #if defined(DEBUG)
//     cout << "[Accept]: Timeout occured." << endl;
// #endif
//         return;
//     }

    if (ret < 0) {
        cout << "[ERROR] (" << __FILE__ << ": " << __LINE__
             << ". Error: " << strerror(errno) << endl;
        close(sock);
        exit(ret);
    }
}

void *recv_func(void *arg) {
    int client_fd = *(int *)arg;
    char buf[BUF_LEN];

#if defined(DEBUG)
    cout << "[Recv] Rready reading data from client " << client_fd << endl;
#endif
    // setting read timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
               sizeof(tv));

    while (isRunning) {
        if (read(client_fd, buf, BUF_LEN) > 0) {
            pthread_mutex_lock(&lock_x);
            messageQueue.push(buf);
            pthread_mutex_unlock(&lock_x);
        } else {
            cout << strerror(errno) << endl;
        }
    }

#if defined(DEBUG)
    cout << "[Recv] Stopped receving data from client " << client_fd << endl;
#endif

    pthread_exit(NULL);
}

static void signalHandler(int signum) {

    switch (signum)
    {
    case SIGINT:
        /* code */
        cout << "[Interrupt] SIGINT received." << endl;
        isRunning = false;
        break;
    
    default:
        cout << "Handler not defined." << endl;
        break;
    }

}