
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// XDCtools Header files
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* TI-RTOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Idle.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/GPIO.h>
#include <ti/net/http/httpcli.h>

#include "Board.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#define HOSTNAME          "api.openweathermap.org"
#define REQUEST_URI       "/data/2.5/forecast/?id=315202&APPID=b9bdaf75a7b1e96362a172ec83cb9303"
#define USER_AGENT        "HTTPCli (ARM; TI-RTOS)"
#define SOCKETTEST_IP     "192.168.1.16"
#define TASKSTACKSIZE     4096
#define OUTGOING_PORT     5011
#define INCOMING_PORT     5030

extern Semaphore_Handle semaphore0;     // posted by httpTask and pended by clientTask
char   tempstr[20];                     // temperature string

/*
 *  ======== printError ========
 */
void printError(char *errString, int code)
{
    System_printf("Error! code = %d, desc = %s\n", code, errString);
    BIOS_exit(code);
}

bool sendData2Server(char *serverIP, int serverPort, char *data, int size)
{
    int sockfd, connStat, numSend;
    bool retval=false;
    struct sockaddr_in serverAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) {
        System_printf("Socket not created");
        close(sockfd);
        return false;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));  // clear serverAddr structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);     // convert port # to network order
    inet_pton(AF_INET, serverIP, &(serverAddr.sin_addr));

    connStat = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(connStat < 0) {
        System_printf("sendData2Server::Error while connecting to server\n");
    }
    else {
        numSend = send(sockfd, data, size, 0);       // send data to the server
        if(numSend < 0) {
            System_printf("sendData2Server::Error while sending data to server\n");
        }
        else {
            retval = true;      // we successfully sent the temperature string
        }
    }
    System_flush();
    close(sockfd);
    return retval;
}

Void clientSocketTask(UArg arg0, UArg arg1)
{
    while(1) {
        // wait for the semaphore that httpTask() will signal
        // when temperature string is retrieved from api.openweathermap.org site
        //
        Semaphore_pend(semaphore0, BIOS_WAIT_FOREVER);

        GPIO_write(Board_LED0, 1); // turn on the LED

        // connect to SocketTest program on the system with given IP/port
        // send hello message whihc has a length of 5.
        //
        if(sendData2Server(SOCKETTEST_IP, OUTGOING_PORT, tempstr, strlen(tempstr))) {
            System_printf("clientSocketTask:: Temperature is sent to the server\n");
            System_flush();
        }

        GPIO_write(Board_LED0, 0);  // turn off the LED
    }
}

void getTimeStr(char *str)
{
    // dummy get time as string function
    // you may need to replace the time by getting time from NTP servers
    //
    strcpy(str, "2021-01-07 12:34:56");
}

float getTemperature(void)
{
    // dummy return
    //
    return atof(tempstr);
}

Void serverSocketTask(UArg arg0, UArg arg1)
{
    int serverfd, new_socket, valread, len;
    struct sockaddr_in serverAddr, clientAddr;
    float temp;
    char buffer[30];
    char outstr[30], tmpstr[30];
    bool quit_protocol;

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverfd == -1) {
        System_printf("serverSocketTask::Socket not created.. quiting the task.\n");
        return;     // we just quit the tasks. nothing else to do.
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(INCOMING_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Attaching socket to the port
    //
    if (bind(serverfd, (struct sockaddr *)&serverAddr,  sizeof(serverAddr))<0) {
         System_printf("serverSocketTask::bind failed..\n");

         // nothing else to do, since without bind nothing else works
         // we need to terminate the task
         return;
    }
    if (listen(serverfd, 3) < 0) {

        System_printf("serverSocketTask::listen() failed\n");
        // nothing else to do, since without bind nothing else works
        // we need to terminate the task
        return;
    }

    while(1) {

        len = sizeof(clientAddr);
        if ((new_socket = accept(serverfd, (struct sockaddr *)&clientAddr, &len))<0) {
            System_printf("serverSocketTask::accept() failed\n");
            continue;               // get back to the beginning of the while loop
        }

        System_printf("Accepted connection\n"); // IP address is in clientAddr.sin_addr
        System_flush();

        // task while loop
        //
        quit_protocol = false;
        do {

            // let's receive data string
            if((valread = recv(new_socket, buffer, 10, 0))<0) {

                // there is an error. Let's terminate the connection and get out of the loop
                //
                close(new_socket);
                break;
            }

            // let's truncate the received string
            //
            buffer[10]=0;
            if(valread<10) buffer[valread]=0;

            System_printf("message received: %s\n", buffer);

            if(!strcmp(buffer, "HELLO")) {
                strcpy(outstr,"GREETINGS 200\n");
                send(new_socket , outstr , strlen(outstr) , 0);
                System_printf("Server <-- GREETINGS 200\n");
            }
            else if(!strcmp(buffer, "GETTIME")) {
                getTimeStr(tmpstr);
                strcpy(outstr, "OK ");
                strcat(outstr, tmpstr);
                strcat(outstr, "\n");
                send(new_socket , outstr , strlen(outstr) , 0);
            }
            else if(!strcmp(buffer, "GETTEMP")) {
                temp = getTemperature();
                sprintf(outstr, "OK %5.2f\n", temp);
                send(new_socket , outstr , strlen(outstr) , 0);
            }
            else if(!strcmp(buffer, "QUIT")) {
                quit_protocol = true;     // it will allow us to get out of while loop
                strcpy(outstr, "BYE 200");
                send(new_socket , outstr , strlen(outstr) , 0);
            }

        }
        while(!quit_protocol);

        System_flush();
        close(new_socket);
    }

    close(serverfd);
    return;
}

/*
 *  ======== httpTask ========
 *  Makes a HTTP GET request
 */
Void httpTask(UArg arg0, UArg arg1)
{
    bool moreFlag = false;
    char data[64], *s1, *s2;
    int ret, temp_received=0, len;
    struct sockaddr_in addr;

    HTTPCli_Struct cli;
    HTTPCli_Field fields[3] = {
        { HTTPStd_FIELD_NAME_HOST, HOSTNAME },
        { HTTPStd_FIELD_NAME_USER_AGENT, USER_AGENT },
        { NULL, NULL }
    };

    while(1) {

        System_printf("Sending a HTTP GET request to '%s'\n", HOSTNAME);
        System_flush();

        HTTPCli_construct(&cli);

        HTTPCli_setRequestFields(&cli, fields);

        ret = HTTPCli_initSockAddr((struct sockaddr *)&addr, HOSTNAME, 0);
        if (ret < 0) {
            HTTPCli_destruct(&cli);
            System_printf("httpTask: address resolution failed\n");
            continue;
        }

        ret = HTTPCli_connect(&cli, (struct sockaddr *)&addr, 0, NULL);
        if (ret < 0) {
            HTTPCli_destruct(&cli);
            System_printf("httpTask: connect failed\n");
            continue;
        }

        ret = HTTPCli_sendRequest(&cli, HTTPStd_GET, REQUEST_URI, false);
        if (ret < 0) {
            HTTPCli_disconnect(&cli);
            HTTPCli_destruct(&cli);
            System_printf("httpTask: send failed");
            continue;
        }

        ret = HTTPCli_getResponseStatus(&cli);
        if (ret != HTTPStd_OK) {
            HTTPCli_disconnect(&cli);
            HTTPCli_destruct(&cli);
            System_printf("httpTask: cannot get status");
            continue;
        }

        System_printf("HTTP Response Status Code: %d\n", ret);

        ret = HTTPCli_getResponseField(&cli, data, sizeof(data), &moreFlag);
        if (ret != HTTPCli_FIELD_ID_END) {
            HTTPCli_disconnect(&cli);
            HTTPCli_destruct(&cli);
            System_printf("httpTask: response field processing failed\n");
            continue;
        }

        len = 0;
        do {
            ret = HTTPCli_readResponseBody(&cli, data, sizeof(data), &moreFlag);
            if (ret < 0) {
                HTTPCli_disconnect(&cli);
                HTTPCli_destruct(&cli);
                System_printf("httpTask: response body processing failed\n");
                moreFlag = false;
            }
            else {
                // string is read correctly
                // find "temp:" string
                //
                s1=strstr(data, "temp");
                if(s1) {
                    if(temp_received) continue;     // temperature is retrieved before, continue
                    // is s1 is not null i.e. "temp" string is found
                    // search for comma
                    s2=strstr(s1, ",");
                    if(s2) {
                        *s2=0;                      // put end of string
                        strcpy(tempstr, s1+6);      // copy the string
                        temp_received = 1;
                    }
                }
            }

            len += ret;     // update the total string length received so far
        } while (moreFlag);

        System_printf("Received %d bytes of payload\n", len);
        System_printf("Temperature %s\n", tempstr);
        System_flush();                                         // write logs to console

        HTTPCli_disconnect(&cli);                               // disconnect from openweathermap
        HTTPCli_destruct(&cli);
        Semaphore_post(semaphore0);                             // activate socketTask

        Task_sleep(5000);                                       // sleep 5 seconds
    }
}

bool createTasks(void)
{
    static Task_Handle taskHandle1, taskHandle2, taskHandle3;
    Task_Params taskParams;
    Error_Block eb;

    Error_init(&eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle1 = Task_create((Task_FuncPtr)httpTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle2 = Task_create((Task_FuncPtr)clientSocketTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle3 = Task_create((Task_FuncPtr)serverSocketTask, &taskParams, &eb);

    if (taskHandle1 == NULL || taskHandle2 == NULL || taskHandle3 == NULL) {
        printError("netIPAddrHook: Failed to create HTTP, Socket and Server Tasks\n", -1);
        return false;
    }

    return true;
}

//  This function is called when IP Addr is added or deleted
//
void netIPAddrHook(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
    // Create a HTTP task when the IP address is added
    if (fAdd) {
        createTasks();
    }
}

int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initEMAC();

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    System_printf("Starting the HTTP GET example\nSystem provider is set to "
            "SysMin. Halt the target to view any SysMin contents in ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();


    /* Start BIOS */
    BIOS_start();

    return (0);
}
