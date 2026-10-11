#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>

// typedef enum MessageType { ... } message_type_t;
// typedef struct __attribute__((packed)) Message { ... } message_t;
typedef struct Settings {
    struct sockaddr_in server;
    bool quiet;
    int socket_fd;
    bool running;
    char username[32];
} settings_t;

//create Mycord message type
typedef enum MessageType{
	MSGT_LOGIN = 0,
	MSGT_LOGOUT = 1,
	MSGT_SEND = 2,
	MSGT_MESSAGE_RECV = 10,
	MSGT_DISCONNECT = 12,
	MSGT_SYSTEM = 13,
}message_type_t;

//create Mycord message protocol
typedef struct __attribute__((packed)) Message{
	uint32_t type;
	uint32_t timestamp;
	char username[32];
	char message[1024];
}message_t;


static char* COLOR_RED = "\033[31m";
static char* COLOR_GRAY = "\033[90m";
static char* COLOR_RESET = "\033[0m";
static settings_t settings = {0};

//printing out the helper menu to the user
void print_help(){
	printf("usage: ./client [-h] [--port PORT] [--ip IP] [--domain DOMAIN] [--quiet]\n\n");
	printf("mycord client\n\n");
	printf("options:\n");
	printf("  --help                show this help message and exit\n");
	printf("  --port PORT           port to connect to (default: 8080)\n");
	printf("  --ip IP               IP to connect to (default: \"127.0.0.1\")\n");
	printf("  --domain DOMAIN       Domain name to connect to (if domain is specified, IP must not be)\n");
	printf("  --quiet               do not perform alerts or mention highlighting\n\n");
	printf("examples:\n");
	printf("  ./client --help (prints the above message)\n");
	printf("  ./client --port 1738 (connects to a mycord server at 127.0.0.1:1738)\n");
	printf("  ./client --domain example.com (connects to a mycord server at example.com:8080)\n");
}

//printing an error message to stderr
void print_error(const char* error){
	fprintf(stderr, "Error: %s\n", error);
}

//parase CLI argument
int process_args(int argc, char *argv[]) {
	//setting the default conneciton
	settings.server.sin_family = AF_INET;
	settings.server.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &settings.server.sin_addr);
	settings.quiet = false;

	//add bool so --ip and -- domain can't be used together
	bool ip_use = false;
	bool domain_use = false;

	//loop through the argument, start with 1 to skip the program name
	for(int i = 1; i < argc; i++){
		//parse arguments for ---help or -h
		if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0){
			print_help();
			exit(0);
		}
		//parse arguments for --port
		else if(strcmp(argv[i], "--port") == 0){
			//exceed argc means no argument after
			if(i + 1 >= argc){
				print_error("Missing argument after --port\n");
				return -1;
			}	
			//convert string to int and store as port
			settings.server.sin_port = htons(atoi(argv[++i]));
		}
		//parse arguments for --ip
		else if(strcmp(argv[i], "--ip") == 0){
			//exceed argc means no argument after
			if(i + 1 >= argc){
				print_error("Missing argument after --ip\n");
				return -1;
			}
			ip_use = true;
			//convert string to binary and store in setting
			if(inet_pton(AF_INET, argv[++i], &settings.server.sin_addr) != 1){
				print_error("Invalid IP address");
				return -1;
			}
                }
		//parse arguments for --domain
		else if(strcmp(argv[i], "--domain") == 0){
			//exceed argc means no argument after
			if(i + 1 >= argc){
				print_error("Missing argument after --domain");
				return -1;
			}
			domain_use = true;
			//attemp to resolve the hostname
			struct hostent* host_info = gethostbyname(argv[++i]);
			if(host_info == NULL ||
			   host_info -> h_addrtype != AF_INET ||
			   host_info -> h_addr_list[0] == NULL){

				print_error("Can not find a valid IPv4 address");
				return -1;
			}
			//extract returned IP and store in setting
			settings.server.sin_addr = *(struct in_addr*)(host_info -> h_addr_list[0]);
		}
		//parse arguments for --quiet
		else if(strcmp(argv[i], "--quiet") == 0){
			settings.quiet = true;
		}
		//else unknow input
		else{
			print_error("Unknown argument");
			return -1;
		}
	}

	//check if both --ip and --domain are in use
        if(ip_use == true && domain_use == true){
		print_error("Can't use --ip and --domain at the same time");
                return -1;
	}

	//success
	return 0;
}

//getting the client username 
int get_username() {
    char buffer[32];
    buffer[0] = '\0';
    FILE *pfile = popen("whoami", "r");

    //get username from whoami
    if(pfile != NULL){
	//remove newline character and close the stream
    	if(fgets(buffer, sizeof(buffer), pfile) != NULL){
		//strip newline char
		buffer[strcspn(buffer, "\r\n")] = '\0';	
	}
	pclose(pfile);
    }

    //if failed getting username from whoami, try with $USER
    if(buffer[0] == '\0'){
	//get username with getenv
    	char *user = getenv("USER");
	//copy the username to buffer and ensure null termination
	if(user != NULL){
		strncpy(buffer, user, sizeof(buffer) - 1);
		buffer[sizeof(buffer) -1] = '\0';
	}
    }

    //if still failed getting username, return fail message
    if(buffer[0] == '\0'){
    	print_error("Fail to get username");
	return -1;
    }
    //check if username exceed 32 byte
    if(strlen(buffer) >= sizeof(settings.username)){
    	print_error("Username too long");
	return -1;
    }
    
    //loop through the username to validate 
    for(size_t i = 0; buffer[i] != '\0'; i++){
	//username should be non-empty printable ASCII / alphanumeric
    	if(isprint((unsigned char)buffer[i]) == false || isalnum((unsigned char)buffer[i]) == false){
		print_error("Username used for login must be non-empty, printable ASCII/alphanumeric");
		return -1;
	}
    }
    //copy into setting
    //-1 so don't exceed 32 byte with null
    strncpy(settings.username, buffer, sizeof(settings.username) - 1);
    //null terminate the last byte
    settings.username[sizeof(settings.username) - 1] = '\0';

    //success
    return 0;
}

//handling the signal 
void handle_signal(int signal) {
    //gracefully handle SIGINT/SIGTERM
    if(signal == SIGINT){
    	settings.running = false;
    }
    else if(signal == SIGTERM){
    	settings.running = false; 
    }
    //handling other signal
    else{
	settings.running = false;
    }
}

//read n bytes from the socket into buffer
ssize_t perform_full_read(void *buf, size_t n) {
    size_t readed = 0;
    char *byte = buf;

    //keep reading until it read all n byte
    while(readed < n){
    	ssize_t recvd = read(settings.socket_fd, byte + readed, n - readed);
	
	//error happen
	if(recvd < 0){
		//interrupted by signal
		if(errno == EINTR){
			continue;
		}        
		//actual read error
		print_error(strerror(errno));
		return -1;
	}
	//EOF, server close connection
	if(recvd == 0){
		return 0;
	}
	//add the total bytes read
	readed += (size_t)recvd;
    }
    //success
    return (ssize_t)readed;
}

void* receive_messages_thread(void* arg) {
    (void)arg;
    // while some condition(s) are true
    while(settings.running == true){
        // read message from the server (ensure no short reads)
	message_t recv;
	ssize_t readed = perform_full_read(&recv, sizeof(recv));

	//nothing will print if readed == 0, server closed connection gracefully
	//print socket read error if readed < 0
	if(readed <= 0){
		settings.running = false;
		break;	
	}

	uint32_t type = ntohl(recv.type);
	uint32_t timestamp = ntohl(recv.timestamp);	
        // check the message type
	if(type == MSGT_MESSAGE_RECV){
            // for message types, print the message and do highlight parsing (if not quiet)
	    time_t get_time = (time_t)timestamp;
	    struct tm *time_info = localtime(&get_time);
	    char time_buffer[32];

	    //if fail to get current time, use the epoch time
	    if(time_info == NULL){
	    	strcpy(time_buffer, "1970-01-01 00:00:00");
	    }
	    //if success then formate time to current time
	    else{
	    	strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
	    }
	 
	    //print as the require formate's prefix
	    printf("[%s] %s: ", time_buffer, recv.username);

	    //determine whether mention highlight or not based on quiet
	    if(settings.quiet == true){
	    	printf("%s\n", recv.message);
		fflush(stdout);
	    }
	    else{
	    	char mention[33];
		char *ptr = recv.message;
		char *mentioned;

		//print the taged username
		snprintf(mention, sizeof(mention), "@%s", settings.username);
		
		//loop to deal with multiple mention in 1 message
		while((mentioned = strstr(ptr, mention)) != NULL){
			fwrite(ptr, 1, (size_t)(mentioned - ptr), stdout);
			//using ascii bel to send notification and highlight in red
			printf("\a%s%s%s", COLOR_RED, mention, COLOR_RESET);

			//update the loop
			ptr = mentioned + strlen(mention);
		}
		//print the rest of the message
		printf("%s\n", ptr);
		fflush(stdout);
	    }
	}
	else if(type == MSGT_SYSTEM){
	    // for system types, print the message in gray with username SYSTEM
	    printf("%s[SYSTEM] %s%s\n", COLOR_GRAY, recv.message, COLOR_RESET);
	    fflush(stdout);
	}
	else if(type == MSGT_DISCONNECT){
	    // for disconnect types, print the reason in red with username DISCONNECT and exit
	    fprintf(stderr, "%s[DISCONNECT] %s%s\n", COLOR_RED, recv.message, COLOR_RESET);
	    settings.running = false;
	    fflush(stderr);
	    break;
	}   
	// for anything else, print an error
	else{
		print_error("Unknown message type");
	}
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // setup sigactions (ill-advised to use signal for this project, use sigaction with default (0) flags instead)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGINT, &sa, NULL) != 0){
    	print_error("Failed setting SIGINT handler");
	return 1;
    }
    if(sigaction(SIGTERM, &sa, NULL) != 0){
    	print_error("Failed setting SIGTERM handler");
	return 1;
    }

    // parse arguments
    if(process_args(argc, argv) != 0){
    	return 1;
    }

    // get username
    if(get_username() != 0){
    	return 1;
    }

    settings.running = true;

    // create socket
    settings.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(settings.socket_fd < 0){
    	print_error(strerror(errno));
	return 1;
    }

    // connect to server
    if(connect(settings.socket_fd, (struct sockaddr*)&settings.server, sizeof(settings.server)) != 0){
    	print_error(strerror(errno));
	close(settings.socket_fd);
	return 1;
    }

    // create and send login message
    //initialize login struct
    message_t login_message;
    memset(&login_message, 0, sizeof(login_message));

    //setting login_message and copy the username to the struct
    login_message.type = htonl(MSGT_LOGIN);
    strncpy(login_message.username, settings.username, sizeof(login_message.username) - 1);
    login_message.username[sizeof(login_message.username) -1] = '\0';

    //loop to avoid short write
    size_t login_send = 0;
    char *login_ptr = (char *)&login_message;

    while(login_send < sizeof(login_message)){
    	ssize_t writed = write(settings.socket_fd, login_ptr + login_send, sizeof(login_message) - login_send);
    
    	//check for status
    	if(writed < 0){
		//interrupted by signal
		if(errno == EINTR){
			continue;
		}
		//login message error
		print_error("Failed to send LOGIN message");
		close(settings.socket_fd);
		return 1;
    	}
	if(writed == 0){
		print_error("Socket closed while sending message");
		close(settings.socket_fd);
		return 1;
	}
    	
	login_send += (size_t)writed; 
    }
    // create and start receive messages thread
    pthread_t recv_thread;
    
    if(pthread_create(&recv_thread, NULL, receive_messages_thread, NULL) != 0){
    	print_error("Failed to create receive message thread");
	close(settings.socket_fd);
	return 1;
    }

    // while some condition(s) are true
    char *request = NULL;
    size_t size = 0;

    while(settings.running == true){
        // read a line from STDIN
	ssize_t number_read = getline(&request, &size, stdin);

        // do some error checking (handle EOF, EINTR, etc.)
        if(number_read == -1){
		//handle EOF
		if(feof(stdin) == true){
			break;
		}
		//interrupt by signal
		if(errno == EINTR){
			continue;
		}
		print_error("Failed to read from stdin");
		settings.running = false;
		break;
	}

	//strip newline to avoid getting disconnected
	while(number_read > 0 && (request[number_read - 1] == '\n' || request[number_read - 1] == '\r')){
		request[--number_read] = '\0';
	}

	//validation for non_empty message
	if(number_read == 0){
		print_error("Empty message");
		continue;
	}
	//validation for max 1023 length message
	if(number_read > 1023){
		print_error("Message too long, max 1023 characters");
		continue;
	}
	//validation for printable ASCII message
	bool valid = true;
	for(ssize_t i = 0; i < number_read; i++){
		if(isprint((unsigned char)request[i]) == false){
			print_error("Message contain non printable character");
			valid = false;
			break;
		}
	}
	//only send if message is valid
	if(valid == false){
		continue;
	}

	// send message to the server
	//initialize send struct
	message_t send_message;
	memset(&send_message, 0, sizeof(send_message));

	//setting message type to send
	send_message.type = htonl(MSGT_SEND);
	strncpy(send_message.message, request, sizeof(send_message.message) -1);
	send_message.message[sizeof(send_message.message) -1] = '\0';

	//loop to avoid short write
	size_t message_send = 0;
	char *message_ptr = (char *)&send_message;

	while(message_send < sizeof(send_message)){
		ssize_t writed = write(settings.socket_fd, message_ptr + message_send, sizeof(send_message) - message_send);
		
		if(writed < 0){
			//interrupted by signal
			if(errno == EINTR){
				continue;
			}
			//socket write error
			print_error("Socket write error");
			settings.running = false;
			break;
    		}
		if(writed == 0){
			print_error("Socket closed while sending message");
			settings.running = false;
			break;
		}
		message_send += (size_t)writed;
	}

	if(settings.running == false){
		break;
	}

    }

    // wait for the thread / clean up
    settings.running = false;
    
    //initialize logout struct
    message_t logout_message;
    memset(&logout_message, 0, sizeof(logout_message));
    
    //setting loutout type
    logout_message.type = htonl(MSGT_LOGOUT);

    //loop to avoid short write
    size_t logout_send = 0;
    char *logout_ptr = (char *)&logout_message;

    while(logout_send < sizeof(logout_message)){
    	ssize_t writed = write(settings.socket_fd, logout_ptr + logout_send, sizeof(logout_message) - logout_send);

	//no need to send any error message after disconnect
	if(writed < 0){
		//interrupted by signal
                if(errno == EINTR){
			continue;
		}
		break;
	}
        if(writed == 0){
		break;
        }
        logout_send += (size_t)writed;
    }

    // cleanup and return
    if(request != NULL){
        free(request);
        request = NULL;
    }
    close(settings.socket_fd);
    settings.socket_fd = -1;
    //avoid blocking
    pthread_join(recv_thread, NULL);

    return 0;
}
