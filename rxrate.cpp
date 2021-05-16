// compile with: g++ -O3 -static -o txrate txrate.cpp
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <iomanip>
#include <iostream>
#include <csignal>
#include <cassert>
#include <atomic>

std::atomic<bool> quit_flag;

const size_t buffer_size = 32 * 1024 * 1024;

int start_server(const char *bind_addr, int port) {
  int server_fd, sock, valread;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  std::vector<char> buffer(buffer_size);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket");
    return -1;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    return -1;
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
    perror("bind");
    return -1;
  }
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    return -1;
  }
  if ((sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
    perror("accept");
    return -1;
  }

  std::cout << "connected" << std::endl;

  auto t0 = std::chrono::high_resolution_clock::now();
  size_t read_size = 0;
  while (!quit_flag) {
    int valread = read(sock, buffer.data(), buffer.size());
    if (valread < 0) {
      perror("read");
      close(sock);
      return -1;
    } else if (valread == 0) {
      break;
    }
    read_size += valread;
  }
  auto t1 = std::chrono::high_resolution_clock::now();

  close(sock);

  auto seconds = std::chrono::duration<double>(t1 - t0).count();
  auto mbs = read_size / seconds / (1024 * 1024);
  std::cout << "rx rate " << std::fixed << std::setprecision(2) << mbs << "MiB/s" << std::endl;
  return 0;
}

int start_client(const char *server_addr, int port) {
  int sock = 0, valread;
  struct sockaddr_in serv_addr;
  std::vector<char> buffer(buffer_size);
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if(inet_pton(AF_INET, server_addr, &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect");
    return -1;
  }

  std::cout << "connected" << std::endl;

  auto t0 = std::chrono::high_resolution_clock::now();
  size_t write_size = 0;
  while (!quit_flag) {
    int n = write(sock, buffer.data(), buffer.size());
    if (n < 0) {
      perror("write");
      break;
    } else if (n == 0) {
      std::cerr << "write returns 0" << std::endl;
      break;
    }
    write_size += n;
  }
  auto t1 = std::chrono::high_resolution_clock::now();

  auto seconds = std::chrono::duration<double>(t1 - t0).count();
  auto mbs = write_size / seconds / (1024 * 1024);
  std::cout << "tx rate " << std::fixed << std::setprecision(2) << mbs << "MiB/s" << std::endl;
  return 0;
}

void stop(int signum) {
  assert(signum == SIGINT);
  quit_flag = true;
}

int main(int argc, char const *argv[]) {
  if (argc != 4) {
    printf("usage: %s <server|client> <bind_addr|server_addr> <port>\n", argv[0]);
    exit(1);
  }

  quit_flag = false;
  signal(SIGINT, stop);
  int port = atoi(argv[3]);
  if (strcmp(argv[1], "server") == 0) {
    start_server(argv[2], port);
  } else {
    start_client(argv[2], port);
  }
}
