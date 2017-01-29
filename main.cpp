#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <regex>
#include <netinet/in.h>

#define BUFSIZE 2048
#define PORT_N 80


using namespace std;

/* Jednoducha funkcia na vytvorenie socketu
 */
int create_socket() {
	int client_socket;

	// Vytvorenie socketu
	if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("ERROR in socket");
        exit(EXIT_FAILURE);
	}
	return client_socket;
}

/* Funkcia, ktora spracovava zadanu adresu
 */
int parse_url(string url, string* hostname, string* filepath, string* filename) {

	string url_filepath;
	string url_hostname;
	string server_hostname;

	int port = 80;

	string medzera = "%20";
	string vlnovka = "%7E";

	filename->clear();
	*filename = url;
	while(filename->find('/') != url.npos) {
		string tmp_string = filename->substr(filename->find('/') + 1);
		*filename = tmp_string.c_str();
	}


	// Nahradenie medzier %20 a ~ $7E
	for (unsigned i = 0; i < url.size(); i++) {

		if (url.at(i) == ' ') {

			url = url.insert(i, medzera.c_str());
			i+=3;
		}

	}

	string tmp_string = url;
	url = "";
	for (unsigned i = 0; i < tmp_string.size(); i++) {

		if ((tmp_string.at(i) != ' ')) {

			url += tmp_string.at(i);
		}
	}

	// Skontrolujem, ci ma url validny format
	if(!regex_match(url, regex("http:\\/\\/.*"))) {
		if(!regex_match(url, regex("https:\\/\\/.*"))) {
			perror("invalid url");
			exit(EXIT_FAILURE);
		}
	}
	url = url.substr(url.find("//")+2);


	if(regex_match(url, regex("www\\..*"))) {
		url_hostname.append("www.");
		url = url.substr(4);
	}


	if (url.at(strlen(url.c_str())-1) == '/')
		*filename = "index.html";

	if (url.find('/') != url.npos) {
		url_filepath = url.substr(url.find('/'));
		server_hostname = url.substr(0, url.find('/'));

	}
	else {

		filename->clear();
		url_filepath = "/";
		server_hostname = url;
		*filename = "index.html";
	}

	// Cislo portu v adrese
	if (server_hostname.find(':') != server_hostname.npos) {
		tmp_string = server_hostname.substr(server_hostname.find(':') + 1);
		server_hostname = server_hostname.erase(server_hostname.find(':'));
		port = atoi(tmp_string.c_str());
	}

	if ((port > 70000) || (port < 0)) {
		perror("invalid port");
		exit(EXIT_FAILURE);
	}

	url_hostname.append(server_hostname);

	*hostname = url_hostname;
	*filepath = url_filepath;

	return port;
}

/* Funkcia, ktora z hlavicky vytahuje navratovy kod XXX
 */
int get_code(string head) {

	unsigned i =0;
	for(i = 0; i < head.size(); i++) {
		if ( head.at(i) == ' ') {
			break;
		}
	}
	string code = head.substr(i+1, 3);
	#ifdef DEBUG
		uts(code.c_str());
	#endif
	return atoi(code.c_str());
}

/* Spracovavanie adresy z prikazovej riadky
 */
string parse_args(int argc, char *argv[]) {

	string url;

	for (int i = 1;  i< argc; i++) {
		string tmp_string;
		tmp_string = argv[i];

		if ( (i+1 < argc) && (tmp_string.at(strlen(tmp_string.c_str()) - 1) != '\\')) {
			perror("invalid arguments");
			exit(EXIT_FAILURE);
		}
		else {
			if (argc != i +1) {
				url.append(tmp_string.erase(strlen(tmp_string.c_str()) - 1));
			}
			else
				url.append(tmp_string.c_str());
		}
	}

	return url;
}

/* V pripade, ze stiahnuty subor je v chunkoch, tato funkcia ho znormalizuje
 */
string remove_chunks(string code) {

	string size;
	string chunkless;
	size_t chunk_size = 1;

	chunkless = "";

	while (chunk_size > 0) {
		size = code.substr(0,code.find("\r\n"));

		chunk_size = strtol(size.c_str(), NULL, 16);

		chunkless.append(code.substr(code.find("\r\n") + 2, chunk_size ));

		code = code.substr(code.find("\r\n") + 4 + chunk_size);

	}

	return chunkless;
}

/* Main
 */
int main(int argc, char *argv[]) {

	string url = parse_args(argc, argv);

	string filepath;
    string host;
    string filename;

   	string response;
	struct hostent *server;

	int client_socket;

	struct sockaddr_in server_address;

	ssize_t bytesrx = 1;
	ssize_t bytestx;

	// navratovy kod head requestu
	int return_code = 0;

	// cislo portu
	int port;

	// kontrolna premenna pre redirecty
	int redirects = 0;

	string final_filename;

	// while, ktory sa skonci v pripade navratoveho kodu 200, ale bude pokracovat, pokial
	// dostaneme signal na redirect
	while(redirects <= 5) {

		// vytvorenie socketu a spracovanie adresy
		client_socket = create_socket();
		port = parse_url(url, &host, &filepath, &filename);

		if (redirects == 0) {
			final_filename = filename.c_str();
		}

		#ifdef DEBUG
			puts(host.c_str());
		#endif

		// ziskanie adresy servera
		if ((server = gethostbyname(host.c_str())) == NULL) {
			fprintf(stderr,"ERROR, no such host as %s\n", host.c_str());
	    	exit(EXIT_FAILURE);
		}

		bzero((char *) &server_address, sizeof(server_address));
		server_address.sin_family = AF_INET;
		bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
		server_address.sin_port = htons(port);

		// pripojenie
		if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
	                perror("ERROR: connect");
	                exit(EXIT_FAILURE);
	    }

	    // head request na server
	    string request = "HEAD " + filepath + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n";

	    #ifdef DEBUG
	    	puts(request.c_str());
	    #endif

	    bytestx = send(client_socket, request.c_str(), request.size(), 0);
	    if (bytestx < 0) {
	        perror("ERROR: sendto");
	  		exit(EXIT_FAILURE);
	  	}


	  	// cyklus, ktory nacita hlavicku
	  	char buff[BUFSIZE];
	  	response.clear();
	  	while ((bytesrx = recv(client_socket, buff, BUFSIZE, 0)) > 0) {


	   		response.append(buff);

	   		if (bytesrx < 0)  {
   		  		perror("ERROR: recvfrom");
   				exit(EXIT_FAILURE);
   			}

			bzero(buff,BUFSIZE);
		}


	   	// spracovanie hlavicky
	   	string tmp_string = response.substr(0,response.find('\n'));

	   	return_code = get_code(tmp_string);

	   	close(client_socket);


	   	// switch, ktory riesi navratove kody
	   	switch(return_code) {

	   		case(200):

	   			client_socket = create_socket();

				port = parse_url(url, &host, &filepath, &filename);

				if ((server = gethostbyname(host.c_str())) == NULL) {
					fprintf(stderr,"ERROR, no such host as %s\n", host.c_str());
			    	exit(EXIT_FAILURE);
				}

				bzero((char *) &server_address, sizeof(server_address));
				server_address.sin_family = AF_INET;
				bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
				server_address.sin_port = htons(port);

				if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
			                perror("ERROR: connect");
			                exit(EXIT_FAILURE);
			    }

			    // vytvorenie get requstu
	   			request = "GET " + filepath + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-agent: webclient " +"Connection: close \r\n\r\n";
	   			response.clear();


		   		bytestx = send(client_socket, request.c_str(), request.size(), 0);
		    	 if (bytestx < 0) {
			        perror("ERROR: sendto");
			  		exit(EXIT_FAILURE);
			  	}

		  		// cyklus, ktory nacita dany subor
		  		bytesrx = 1;
		  		bzero(buff,BUFSIZE);
		  		while ((bytesrx = recv(client_socket, buff, BUFSIZE, 0)) > 0) {

		   			response.append(buff,bytesrx);

		   			if (bytesrx < 0)  {
		   		  		perror("ERROR: recvfrom");
		   				exit(EXIT_FAILURE);
		   			}

		   			bzero(buff,BUFSIZE);
		   		}

		   		close(client_socket);


		   		bool chunks;
		   		chunks = 0;
		   		if (response.find("Transfer-Encoding: chunked") != response.npos)
		   			chunks = 1;

		   		response = response.substr(response.find("\r\n\r\n") + 4);

		   		if (chunks)
		   			response = remove_chunks(response);

		   		redirects = 7;
		   		break;

	   		case(301): ;

	   		case(302):

	   			redirects += 1;
	   			url = response.substr(response.find("Location:"));

	   			url = url.erase(url.find("\n")-1);

	   			url = url.substr(url.find("http"));

	   			break;

	   		default:
	   			fprintf(stderr, "Error %i\n", return_code );
	   			exit(EXIT_FAILURE);
	   	}

	} // koniec while

	if (redirects == 6) {
		return 0;
	}

	// ulozenie suboru
	ofstream out(final_filename);
	ostreambuf_iterator<char> iterator (out);
	copy(response.begin(), response.end(), iterator);


	return 0;
}
