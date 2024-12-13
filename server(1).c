#include "commands.h"

int clientCount = 0;
int exitCommand = 0; 
clientInfo* clientList[MAX_NUM_OF_CLIENTS]; 
char latestMessagesFromTo[MAX_NUM_OF_CLIENTS][MAX_NUM_OF_CLIENTS][BUFFER_LENGTH]; 
int availableIds[MAX_NUM_OF_CLIENTS];  

int getAvailableId(){ 
    int i;
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(availableIds[i]){
            return i;
        }
    }
    return -1;
}

void sendJoinedChatMessage(int id,char name[]){ 
    int i;
    
    char buffer[MESSAGE_LENGTH];
    sprintf(buffer, "%s joined to chat", name);
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(id != clientList[i]->id){
            write(clientList[i]->socketId, buffer, strlen(buffer));
        }
    }
}

void sendLeftChatMessage(int id,char name[]){ 
    int i;
    char buffer[MESSAGE_LENGTH];
    sprintf(buffer, "%s left the chat", name);
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(id != clientList[i]->id){
            write(clientList[i]->socketId, buffer, strlen(buffer));
        }
    }
}
void getClientNameList(int id, char buffer[]){ 
    if(clientCount == 1){
        strcpy(buffer,"You are the first person on the chat!");
        return;
    }
    strcpy(buffer, "---USER LIST---\n");
    int i;
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(clientList[i]->id == id) continue;
        buffer = strcat(buffer, " - ");
        buffer = strcat(buffer, clientList[i]->name);
        buffer = strcat(buffer, "\n");
    }
}
void *ServiceClient(void* client){   
    char buffer[BUFFER_LENGTH];
    srand(time(NULL));
    
    clientInfo* client_info = (clientInfo*)client;
    if(recv(client_info->socketId, buffer, BUFFER_LENGTH, 0) > 0){ 
        CONN* conn = (CONN*)malloc(sizeof(CONN));
        string_to_connection(buffer, conn);
        strcpy(client_info->name, conn->name);

        char joinMessage[MESSAGE_LENGTH];
        sprintf(joinMessage, "%s joined to chat", client_info->name);
        
        sendJoinedChatMessage(client_info->id, client_info->name); 
        char clientListMessage[BUFFER_LENGTH];
		memset(clientListMessage, 0, BUFFER_LENGTH);

        getClientNameList(client_info->id, clientListMessage); 
        if(write(client_info->socketId, clientListMessage, sizeof(clientListMessage)) < 0){ 
            perror("ERROR: write to descriptor failed");
        }
    }
    
    int toId;
    while(!exitCommand){ 
        memset(buffer, 0, BUFFER_LENGTH);
        if(recv(client_info->socketId, buffer, BUFFER_LENGTH, 0) > 0){ 
            if(strcmp(buffer, "quit") == 0){ 
                break;
            }
            else{ 
                char copy[BUFFER_LENGTH];
                strcpy(copy, buffer);
                char* command = strtok(copy,"|"); 
                if (strcmp(command, "MESG") == 0){
                    MESG* message = (MESG*)malloc(sizeof(MESG));
                    string_to_message(buffer, message);
                    if(rand()%2){ 
                        fprintf(stdout, "Generating noise\n");
                        
                        int charCount = rand()%(sizeof(message->message)/3);
                        int i;
                        for(i = 0; i < charCount; i++){ 
                            int index = rand()%sizeof(message->message);
                            char newChar = 'a' + rand()%26;
                            message->message[index] = newChar;
                        }
                    }
                    
                    for(toId = 0; toId < clientCount; toId++){ 
                        if(strcmp(message->to, clientList[toId]->name) == 0) break;
                    }
                    strcpy(latestMessagesFromTo[client_info->id][toId],buffer); 
                    char sendMessage[BUFFER_LENGTH];
                    message_to_string(message, sendMessage);
                    write(clientList[toId]->socketId, sendMessage, sizeof(sendMessage)); 
                }
                else if(strcmp(command, "MERR") == 0){ 
                    MERR* merr = (MERR*) malloc(sizeof(MERR));
                    string_to_message_error(buffer, merr);
                    for(toId = 0; toId < clientCount; toId++){ 
                        if(strcmp(merr->to, clientList[toId]->name) == 0) break;
                    }
                    int fromId;
                    for(fromId = 0; fromId < clientCount; fromId++){ 
                        if(strcmp(merr->from, clientList[fromId]->name) == 0) break;
                    }

                    fprintf(stdout, "MESSAGE ERROR DETECTED\nMessage: %s\n",latestMessagesFromTo[fromId][toId]);
                    write(clientList[toId]->socketId, latestMessagesFromTo[fromId][toId], sizeof(latestMessagesFromTo[fromId][toId])); 
                    memset(latestMessagesFromTo[fromId][toId], 0, BUFFER_LENGTH);
                }
                else if(strcmp(command, "GONE") == 0){ 
                    sendLeftChatMessage(client_info->id, client_info->name); 
                    break;
                }
            }
        }
        else{
            break;
        }
    }
    
    clientCount--;
    availableIds[client_info->id] = 1;
    clientList[client_info->id] = NULL;
    close(client_info->socketId);
    free(client_info);
    pthread_detach(pthread_self()); 
    return NULL;
}

void ctrl_c_and_exit(int sig) {
    exitCommand = 1;
}
int main(){
    int index;
    for(index = 0; index < MAX_NUM_OF_CLIENTS; index++){ 
        availableIds[index] = 1;
        clientList[index] = NULL; 
    }
   
    int listenId = 0;
    int connectionId = 0;
    memset(clientList, 0, MAX_NUM_OF_CLIENTS);
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    pthread_t threadId;

    listenId = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(listenId, (struct sockaddr*) &server_addr, sizeof(server_addr))){
        perror("ERROR: Socket binding failed");
        return -1;
    }

    if(listen(listenId, MAX_NUM_OF_CLIENTS)< 0){
         perror("ERROR: Socket listening failed");
        return -1;
	}


    
    while(!exitCommand){ //gelen clientları handlelar
        socklen_t size = sizeof(client_addr);
        connectionId = accept(listenId, (struct sockaddr*)&client_addr, &size);

        clientInfo* client_info = (clientInfo*)malloc(sizeof(clientInfo));
        client_info->socketId = connectionId;
        client_info->address = client_addr;
        client_info->id = getAvailableId();
        availableIds[client_info->id] = 0;
        clientList[clientCount] = client_info;
        clientCount++;
        
        pthread_create(&threadId, NULL, &ServiceClient, (void*)client_info);
        sleep(1);
    }
    

}
