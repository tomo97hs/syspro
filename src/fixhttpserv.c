/*
  nantyatte http server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

void http_server(int portno);
int read_wav(FILE * out, int key);
int http_receive_request(FILE * in);
void http_send_reply(FILE * out);
void http_send_reply_bad_request(FILE * out);
char *chomp(char *str);
int tcp_acc_port(int portno);
int fdopen_sock(int sock, FILE ** inp, FILE ** outp);

int main(int argc, char *argv[])
{
	int portno;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s portno\n", argv[0]);
		exit(1);
	}
	portno = strtol(argv[1], 0, 10);
	http_server(portno);
}

void http_server(int portno)
{
	int acc, com, byte;
	acc = tcp_acc_port(portno);
	if (acc < 0)
		exit(-1);

	while (1) {
		FILE *in, *out;
		printf("Accepting incoming connections...\n");
		if ((com = accept(acc, 0, 0)) < 0) {
			perror("accept");
			exit(-1);
		}

		if (fdopen_sock(com, &in, &out) < 0) {
			perror("fdooen()");
			return;
		}
		if (http_receive_request(in)) {
			//printf("bbb\n");
			http_send_reply(out);
		} else {
			http_send_reply_bad_request(out);
		}

		byte = http_receive_request(in);
		
		read_wav(out, 172);
		printf("Replied\n");
		fclose(in);
		fclose(out);
	}
}

int read_wav(FILE * out, int key)
{
	FILE *fpR;
	int data;

	if ((fpR = fopen("quote2017.wav", "rb")) == NULL) {
		perror("fopen");
		return -1;
	}
	fseek(fpR, 78, SEEK_SET);
	while ((data = fgetc(fpR)) != EOF) {
		fputc(data ^ key, out);
	}
// 	int fd, rc;
// 	FILE * file;
// 	char buff[48001];
// 	char result[48001];

// 	fd = open("quote2017.wav", O_RDONLY);
// 	if (fd < 0) {
//         perror("open");
//         return -1;
//     }
// 	lseek(fd, 78, SEEK_SET);
/*
	if (fstat(fd, &stbuf) != 0)
        perror("fstat");

    file_size = stbuf.st_size;
*/
// int i;
// 	rc = read(fd, buff, 48000);
//         if (rc == -1) {
//                 printf("ファイル読み込みエラー\n");
//         }
// 		printf("%d\n", rc);
// 		for (i=0; i<rc; i++) {
// 			result[i] = buff[i] ^ key;
// 			unsigned int u;

// 			/* 16進数 */
// 			sscanf(result[i], "%x", &u);
// 			fprintf(out, "%x", u);
// 			//printf("%c\n", (char)(buff[i] ^ key));
// 		}

		
/*
	while (read(src, buff, 1) > 0) {
		buff ^ key;
		write(out, buff, 1);
	}
*/




	// FILE *fp;
	// int size;
	// unsigned char buf[48079];
	// fp = fopen("quote2017.wav", "rb"); /* rb バイナリ読込み */
  	// if (fp == NULL) { /* ファイルがない場合のエラー処理 */
    //     printf("error");
    //     return -1;
  	// } else {
    //     /* 読込み及びデータ数の取得 */
    //     size = fread(buf, sizeof(unsigned char), 48079, fp);
	// 	printf("%d\n", size);
    //     for(int i=0; i<size; i++) /* 表示 */
    //        fprintf(out, "%c", (int)(buf[i] ^ key));
    // }
  	// fclose(fp);
	return 0;
}

#define BUFFERSIZE      1024

int http_receive_request(FILE * in)
{
	char requestline[BUFFERSIZE];
	char rheader[BUFFERSIZE];

	if (fgets(requestline, BUFFERSIZE, in) <= 0) {
		printf("No request line.\n");
		return 0;
	}
	chomp(requestline);	/* remove \r\n */
	printf("requestline is [%s]\n", requestline);

/*
	while (fgets(rheader, BUFFERSIZE, in)) {
		chomp(rheader);
		if (strcmp(rheader, "") == 0) {
			break;
		}
		printf("Ignored: %s\n", rheader);
	}
*/
	if (strchr(requestline, '<') || strstr(requestline, "..")) {
		printf("Dangerous request line found.\n");
		return 0;
	}
	return -1;
}

void http_send_reply(FILE * out)
{
	fprintf(out, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
	fprintf(out, "<html><head></head><body>I have a pen. I have an apple. Uh..." "</body></html>\n");
}

void http_send_reply_bad_request(FILE * out)
{
	fprintf(out, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\n\r\n");
	fprintf(out, "<html><head></head><body>400 Bad Request</body></html>\n");
}

char *chomp(char *str)
{
	int len;

	len = strlen(str);
	if (len >= 2 && str[len - 2] == '\r' && str[len - 1] == '\n') {
		str[len - 2] = str[len - 1] = 0;
	} else if (len >= 1 && (str[len - 1] == '\r' || str[len - 1] == '\n')) {
		str[len - 1] = 0;
	}
	return str;
}

int fdopen_sock(int sock, FILE ** inp, FILE ** outp)
{
	int sock2;

	if ((sock2 = dup(sock)) < 0) {
		return -1;
	}

	if ((*inp = fdopen(sock2, "r")) == NULL) {
		close(sock2);
		return -1;
	}

	if ((*outp = fdopen(sock, "w")) == NULL) {
		fclose(*inp);
		*inp = 0;
		return -1;
	}
	setvbuf(*outp, (char *)NULL, _IONBF, 0);

	return 0;
}

#define PORTNO_BUFSIZE		16
int tcp_acc_port(int portno)
{
	struct addrinfo hints, *ai;
	char portno_str[PORTNO_BUFSIZE];
	int err, s, on, pf;

	pf = PF_INET;

	snprintf(portno_str, sizeof(portno_str), "%d", portno);
	memset(&hints, 0, sizeof(hints));
	ai = NULL;
	hints.ai_family = pf;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	if ((err = getaddrinfo(NULL, portno_str, &hints, &ai))) {
		fprintf(stderr, "bad portno %d? (%s)\n", portno,
			gai_strerror(err));
		goto error0;
	}
	if ((s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0) {
		perror("socket");
		goto error1;
	}
	if (bind(s, ai->ai_addr, ai->ai_addrlen) < 0) {
		perror("bind");
		fprintf(stderr, "Port number %d\n", portno);
		goto error2;
	}
	if (listen(s, 5) < 0) {
		perror("listen");
		goto error2;
	}
	freeaddrinfo(ai);
	return s;

error2:
	close(s);
error1:
	freeaddrinfo(ai);
error0:
	return -1;
}
