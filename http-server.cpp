#include <iostream>
#include <thread>
#include <fstream>
#include <string>
#include <bits/stdc++.h>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>


/* headers para sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LENGTH 4096
#define MAX_THREADS 20

using namespace std;

sem_t sem_mutex;
ofstream ans;


string http = "HTTP/1.1 200 OK\n";
string server = "Server: Gabspache/1.0.0\n";
string acceptRanges = "Accept-Ranges: bytes\n";
string html = "Content-Type: text/html\n";
string css = "Content-Type: text/css\n";
string ico = "Content-Type: image/x-icon\n";
string jpg = "Content-Type: image/jpg\n";
string png = "Content-Type: image/png\n";
string notFoundHttp = "HTTP/1.1 404 NOT FOUND\n" + server + acceptRanges + html
 + "Content-Length: 207\nConnection: Closed\n";
string badReqHttp = "HTTP/1.1 400 NOT FOUND\n" + server + acceptRanges + html
 + "Content-Length: 317\nConnection: Closed\n";

/* This function is called by the thread which is analyzing the requisition */
bool arquivoExiste(string req){
    ifstream infile(req);
    return infile.good();
}

bool reqHandle(int cliente, char* buffer, char* clientIp){
    long int bytes = 0;
    string content; // in the case file is text
    string fileExtension;
    char binaryContent[LENGTH]; // in the case the file is binary

    /* Mensagem recebida */
    if((recv(cliente, buffer, LENGTH, 0)) > 0){
        cout << "Mensagem recebida!" << endl << endl;
        cout << buffer;
    }else{
        close(cliente);
    }

    /* Gets current date */
    std::locale::global(std::locale("en_US.utf8"));
    std::time_t t = std::time(nullptr);
    char date[100];
    if(std::strftime(date, sizeof(date), "%A %c", std::localtime(&t))){
        char* position = strchr(date, ' ');
        strcpy(date, position+1);
    }
    string strDate(date);
    strDate = strDate + "\n";

    /* Handles the received requisition */
    string receivedMsg(buffer);
    // Verifica se é um GET...
    size_t foundPos = receivedMsg.find("GET", 0);
    string answ;
    string fileName;
    fileName = receivedMsg;
    if(foundPos == string::npos){
        answ = badReqHttp + "Date: " + strDate + "\nHTTP BAD REQUEST.\n";
        strcpy(buffer, answ.c_str());
        send(cliente, buffer, strlen(buffer), 0);
    }else{

        // Pega o nome do arquivo, já com o caminho
        foundPos = receivedMsg.find(' ');
        size_t secondBlank = receivedMsg.find(' ', foundPos+1);
        fileName = receivedMsg.substr(foundPos+1, (secondBlank - foundPos)-1);
        fileName = "./files" + fileName;

        ifstream reqBinFile (fileName, ios::in | ios::binary); // in the case the file is binary
        ifstream reqFile; // in the case the file is text

        // Se nao existir, retorna NOT FOUND
        if(arquivoExiste(fileName) == false)
            answ = notFoundHttp + "Date : " + strDate + "\n404 NOT FOUND.\n";
        
        // se existir...
        else{
            foundPos = fileName.find('.', 2);
            fileExtension = fileName.substr(foundPos+1, fileName.length());

            if(fileExtension == "png" || fileExtension == "jpg"){
                reqBinFile.read(binaryContent, LENGTH);    
            }else{
                reqFile.open(fileName);
                if(reqFile.is_open()){
                    cout << "FILE OPENED\nGETTING CONTENT...\n" << endl;
                    
                    if(reqFile.good()){
                        // Arquivo aberto e OK! Pegar tamanho e adicionar à variavel dos bytes!
                        reqFile.seekg(0, ios_base::end);
                        bytes += reqFile.tellg(); 

                        /* Cria uma variavel ifstream que acessa o arquivo e coloca na string
                        content todo o conteudo do arquivo, seja ele qual for */
                        ifstream readContent(fileName);
                        content.assign( (istreambuf_iterator<char>(readContent) ),
                                        (istreambuf_iterator<char>()  ) );
                    }else
                        cout << "Malformed or corrupted file.\n" << endl;

                }else{
                    cout << "FILE COULD NOT BE OPENED." << endl;
                }      
            }
        }

        /* Avisa o tamanho de bytes e fecha o arquivo */
        cout << "Size: " << bytes << " Bytes." << endl;
        reqFile.close();
        reqBinFile.close();

        cout << "File Extension: " << fileExtension << endl;
        cout << binaryContent;
        /* Monta a resposta e envia pro cliente */
        answ = http + "Date: " + strDate + server + acceptRanges;
        
        if(fileExtension == "html" || fileExtension == "txt")
            answ = answ + html;
        else if(fileExtension == "css")
            answ = answ + css;
        else if(fileExtension == "ico")
            answ = answ + ico;
        else if(fileExtension == "jpg")
            answ = answ + jpg;
        else if(fileExtension == "png")
            answ = answ + png;
        
        answ = answ + "Content-Length: " + to_string(bytes) + "\nConnection: Closed\n\n";

        if(fileExtension == "jpg" || fileExtension == "png"){
            strcpy(buffer, answ.c_str());
            send(cliente, buffer, strlen(buffer), 0);
            send(cliente, binaryContent, strlen(binaryContent), 0);
        }
        else{
            answ = answ + content;
            strcpy(buffer, answ.c_str());
            send(cliente,buffer,strlen(buffer), 0);
        }
    }

    memset(buffer, 0x0, LENGTH);
    close(cliente);

    /* Semáforo para escrever em log.txt */
    sem_wait(&sem_mutex);
    cout << "\nEntrando na seção critica\n";
    
    ans << "Client IP: " << clientIp << endl;
    ans << receivedMsg << endl << endl;
    cout << "Log escrito." << endl;
    cout << "Saindo da seção critica\n" << endl;
    sem_post(&sem_mutex);

    cout << "Fim da Thread de handling HTTP." << endl;
    cout << "Esperando novas requisições." << endl << endl;
}



int main(void){

    sem_init(&sem_mutex, 0, 1);
    ans.open("./files/log.txt");

    int portOption, port, count = 0; 
    thread protocol[MAX_THREADS];

    /* Port Selection */
    while(portOption != 1 && portOption != 2){
        cout << "Which port do you want to use?" << endl << endl;
        cout << "1- My own port" << endl;
        cout << "2- The standard port 8080" << endl << endl;
        cout << "Select an option: "; cin >> portOption;
        
        if(portOption != 1 && portOption != 2)
            cout << endl << "Please select only the available options." << endl << endl;
    }

    if(portOption == 1){
        cout << "Write the port that will be used: ";
        cin >> port;
    }else
        port = 8080;

    /* Creating sockets */

    struct sockaddr_in local;
    struct sockaddr_in remoto;
    int socketd, addr_client, cliente, slen;
    char buffer[4096];

    socketd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketd == -1){
        perror("\nSocketd error");
        return 1;
    }else
        printf("\nSocket criado com sucesso.\n");

    local.sin_family = AF_INET; // padrão
    local.sin_port = htons(port); // porta que havia sido selecionada antes
    memset(local.sin_zero, 0x0, 8);

    /* Binding: stablish connection between IP and Port */
    if(bind(socketd, (struct sockaddr*)&local, sizeof(local)) == -1){
        perror("\nBind error. Maybe the port does not exists or it's unavailable.\n");
        return 1;
    }else
        cout << "Binding criado!" << endl;

    /* Listening up requisitions */
    listen(socketd, 15);

    while(1){
        addr_client = sizeof(remoto);

        /* Waits for requisition */
        // When requisition happens, the accept function is called and thread is initiated

        if((cliente = accept(socketd, (struct sockaddr *)&remoto, (socklen_t *)&addr_client)) == -1){
            perror("\nAccept error");
            exit(1);
        }else
            cout << "Cliente aceito!" << endl;

        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&remoto;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, ip, INET_ADDRSTRLEN);
        /* Start thread with the requisition */
        protocol[count] = thread(reqHandle, cliente, buffer, ip);
        count++;
    }

    printf("\nServidor encerrado!");
    close(socketd);
    sem_destroy(&sem_mutex);
    ans.close();

    return 0;
}