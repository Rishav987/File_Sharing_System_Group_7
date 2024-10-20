
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
using namespace std;
#include <dirent.h>

#define CLIENT_DATA_PORT 55000
#define SERVER_CONTROL_PORT 50000
#define MAX 81

void parseFileName(char *buffer, char *filename, int i)
{
    strcpy(filename, buffer + i);
}

int getClientDataSocket()
{
    int socketFD;
    struct sockaddr_in server_addr;

    // socket creation
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        cout << "socket creation failed.." << endl;
        exit(0);
    }

    // allowing reuse of address and port
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&server_addr,0, sizeof(server_addr));

    // assigning ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CLIENT_DATA_PORT);

    // binding socket to ip
    if ((bind(socketFD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0))
    {
        cout << "Bind failed" << endl;
        exit(0);
    }

    return socketFD;
}

void checkStatusCode(char *response, int clientC_FD)
{
    int code = atoi(response);

    switch (code)
    {

    case 200:
    {
        cout << "\nsuccessful\n" << endl;
        break;
    }
    case 503:
    {
        cout << "\ninvalid server request\n" << endl;
        close(clientC_FD);
        exit(0);
    }
    case 550:
    {
        cout << "\ninvalid request\n" << endl;
        close(clientC_FD);
        exit(0);
    }
    case 201:
    {
        cout << "\n file exists and ready for transfer\n" << endl;
        break;
    }
    case 501:
    {
        cout << "\nerror changing directory\n" << endl;
        break;
    }
    }
    return;
}

void getFile(int clientC_FD, char *buffer)
{

    char response[MAX];
    memset(response, 0,MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0)
    {
        cout << "Data port listening error\n" << endl;
        return;
    }
    else
    {
        cout << "Data port started listening\n" << endl;
    }

    socklen_t len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0)
    {
        cout << "error accepting server data port\n" << endl;
        close(clientD_FD);
        return;
    }

    char filename[100];

    // parseFileName(buffer,filename,4);
    FILE *fp = fopen("received_from_server.txt", "wb"); // Open file for writing in binary mode
    while (1)
    {
        int bytes_recv = recv(serverD_FD, buffer, MAX, 0);
        // cout << "%d\n",bytes_recv);
        if (bytes_recv <= 0)
        {
            break;
        }

        char flag = buffer[0];
        short data_length = ntohs(*(short *)(buffer + 1));

        // write data into file
        fwrite(buffer + 3, sizeof(char), data_length, fp); // Write received data to file
        if (flag == 'L')
        {
            // Last block received, break out of loop
            break;
        }
    }

    fclose(fp);
    close(clientD_FD);
}

void sendFile(char *filename)
{
    char buffer[MAX];
    memset(buffer,0 ,MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0)
    {
        cout << "Data port listening error\n" << endl;
        return;
    }
    else
    {
        cout << "Data port started listening\n" << endl;
    }

    socklen_t len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0)
    {
        cout << "error accepting server data port\n"
             << endl;
        close(clientD_FD);
        return;
    }

    // send blocks of data
    FILE *fp = fopen(filename, "rb");
    int bytes_read;
    while ((bytes_read = fread(buffer + 3, sizeof(char), MAX, fp)) > 0)
    {
        // Set flag character based on remaining data
        buffer[0] = (feof(fp)) ? 'L' : '*'; // 'L' for last block, '*' for others

        // Convert data length (excluding header) to network byte order
        // | L/* | length | length | data | data...
        short data_length = htons(bytes_read);
        memcpy(buffer + 1, &data_length, sizeof(short));

        // Send the entire block (header + data)
        int bytes_sent = write(serverD_FD, buffer, MAX);
        if (bytes_sent < 0)
        {
            perror("send failed");
            break;
        }
    }
}

void handleClientConnection(int clientC_FD)
{
    char buffer[MAX];
    char response[MAX];

    memset(buffer,0, MAX);
    sprintf(buffer, "%d", CLIENT_DATA_PORT);

    // send port Y to server
    write(clientC_FD, buffer, sizeof(buffer));
    memset(response,0, MAX);

    recv(clientC_FD, response, sizeof(response), 0);
    checkStatusCode(response, clientC_FD);

    // options for the client
    while (1)
    {
        bool flag = false;
        int ch;
        cout << " 1. To change server directory\n 2. get file from server \n 3. send file to server\n 4. quit\n"
             << endl;
        scanf("%d", &ch);
        getchar();
        memset(buffer, 0,MAX);
        switch (ch)
        {
        case 1:
        {
            cout << "enter the command to change directory (like > cd path)\n"
                 << endl;
            fflush(stdin);
            fgets(buffer, sizeof(buffer), stdin);

            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0';

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // receive status code
            recv(clientC_FD, response, sizeof(response), 0);
            checkStatusCode(response, clientC_FD);

            break;
        }
        case 2:
        {
            // ask user for file name
            cout << "enter the command (like > get filename)\n"
                 << endl;
            fflush(stdin);
            // scanf("%[^\n]s",buffer);
            // read(stdin,buffer,sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);

            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0';

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // receive status code
            recv(clientC_FD, response, sizeof(response), 0);
            checkStatusCode(response, clientC_FD);

            // receive the file from the server
            getFile(clientC_FD, buffer);
            break;
        }
        case 3:
        {
            // ask user for file name
            cout << "enter the command (like > put filename)\n"
                 << endl;
            fflush(stdin);
            // scanf("%[^\n]s",buffer);
            // read(stdin,buffer,sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);

            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0';

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // parse filename
            char filename[20];
            parseFileName(buffer, filename, 4);

            // send file function
            sendFile(filename);
            break;
        }
        case 4:
        {
            flag = true;
            break;
        }
        }
        if (flag)
            break;
    }
    close(clientC_FD);
}

int main()
{
    int clientC_FD;
    struct sockaddr_in server_addr;

    clientC_FD = socket(AF_INET, SOCK_STREAM, 0);

    if (clientC_FD < 0)
    {
        cout << "failed to create client socket"
             << endl;
        exit(0);
    }

    memset(&server_addr, 0 ,sizeof(server_addr));

    // assign server ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    // connect client with server
    if (connect(clientC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cout << "error connecting with server..." << endl;
        exit(0);
    }
    else
        cout << "connected with server.." << endl;

    handleClientConnection(clientC_FD);
}