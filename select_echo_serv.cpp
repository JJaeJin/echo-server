#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 5

int main(int argc, char *argv[]) {
	int serv_sock, clnt_sock, option;
	struct sockaddr_in serv_adr, clnt_adr;
	struct timeval	timeout;
	fd_set	reads, cpy_reads;

	socklen_t	adr_sz, optlen;
	int fd_max, str_len, fd_num;
	char	buf[BUFFER_SIZE];
	if (argc != 2) {
		std::cerr << "Usage : " << argv[0] << " <port>" << std::endl;
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
		std::cerr << "bind() error" << std::endl;
		exit(1);
	}
	if (listen(serv_sock, 10) == -1) {
		std::cerr << "listen() error" << std::endl;
		exit(1);
	}

	// non-blocking 모드로 설정
	fcntl(serv_sock, F_SETFL, O_NONBLOCK);

	// reads에 서버 소캣 fd 등록
	FD_ZERO(&reads);
	FD_SET(serv_sock, &reads);
	fd_max = serv_sock;

	// 대기 시간 설정
	timeout.tv_sec = 5;
	timeout.tv_usec = 5000;
	
	// 실행
	while (1) {
		cpy_reads = reads;

		fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout);
		if (fd_num == -1) {
			break;
		}
		if (fd_num == 0) {
			std::cout << "time out" << std::endl;
			continue;
		}
		for (int i = 0; i < fd_max + 1; i++) {
			if (FD_ISSET(i, &cpy_reads)) {
				if (i == serv_sock) {
					adr_sz = sizeof(clnt_adr);
					clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr, &adr_sz);
					fcntl(clnt_sock, F_SETFL, O_NONBLOCK);
					FD_SET(clnt_sock, &reads);
					if (fd_max < clnt_sock) {
						fd_max = clnt_sock;
					}
					std::cout << "connected client: " << clnt_sock << std::endl;
				} else {
					str_len = recv(i, buf, sizeof(buf), 0);
					if (str_len <= 0) {
						FD_CLR(i, &reads);
						close(i);
						std::cout << "closed client: " << i << std::endl;
					} else {
						send(i, buf, str_len, 0);
					}
				}
			}
		}
	}
	close(serv_sock);
	return 0;
}