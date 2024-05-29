#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 100
#define MAX_CLIENTS 1024

int main(int argc, char *argv[]) {
	int serv_sock, clnt_sock, option;
	struct sockaddr_in serv_adr, clnt_adr;

	socklen_t	adr_sz, optlen;
	char	buf[BUFFER_SIZE];
	if (argc != 2) {
		std::cout << "Usage : " << argv[0] << " <port>" << std::endl;
		exit(1);
	}

	// 서버  소캣  생성
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1) {
		perror("socket");
		exit(1);
	}

	// 주소의 재할당 설정 
	optlen = sizeof(option);
	option = true;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void *)&option, optlen);

	// sockaddr_in 구조체 설정
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	// bind(), listen()
	if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1) {
		std::cout << "bind() error" << std::endl;
		exit(1);
	}
	if (listen(serv_sock, 10) == -1) {
		std::cout << "listen() error" << std::endl;
		exit(1);
	}

	// non-blocking 모드로 설정
	fcntl(serv_sock, F_SETFL, O_NONBLOCK);

	// kqueue 생성
	int kq = kqueue();
	if (kq == -1) {
		perror("kqueue");
		exit(1);
	}

	// kevent 설정
	struct kevent change;
	EV_SET(&change, serv_sock, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, NULL);

	// 서버 소켓에 대한 이벤트 등록
	if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
		perror("kevent");
		exit(1);
	}

	// 대기 시간 설정
	struct timespec timeout;
	timeout.tv_sec = 5;
	timeout.tv_nsec = 0;

	// 실행
	while (1) {
		struct kevent events[MAX_CLIENTS];
		int nev = kevent(kq, NULL, 0, events, MAX_CLIENTS, &timeout);
		if (nev == -1) {
			perror("kevent");
			break;
		}
		for (int i = 0; i < nev; i++) {
			if (events[i].flags & EV_ERROR) {
				std::cerr << "EV+ERROR: " << events[i].data << std::endl;
			}

			if (events[i].ident == serv_sock) {
				adr_sz = sizeof(clnt_adr);
				clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
				if (clnt_sock == -1) {
					perror("accept");
					continue;
				}
				fcntl(clnt_sock, F_SETFL, O_NONBLOCK);
				EV_SET(&change, clnt_sock, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, NULL);
				if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
					perror("kevent");
					close(clnt_sock);
					continue;
				}
				std::cout << "connected client: " << clnt_sock << std::endl;
			} else if (events[i].filter == EVFILT_READ) {
				int sockfd = events[i].ident;
				int str_len = recv(sockfd, buf, sizeof(buf), 0);
				if (str_len <= 0) {
					std::cout << "closed client: " << sockfd << std::endl;
					close(sockfd);
				} else {
					send(sockfd, buf, str_len, 0);
				}
			}
		}
	}
	close(serv_sock);
	return 0;
}