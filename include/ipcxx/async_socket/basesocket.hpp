#pragma once

#if _WIN32
#include <winsock32.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#endif

#include <string>
#include <functional>
#include <cerrno>

#define FDR_UNUSED(expr){ (void)(expr); }
#define FDR_ON_ERROR const std::function<void(int, std::string)>    \
    &onError = [](int errorCode,const std::string &errorMessage){   \
        FDR_UNUSED(errorCode);                                      \
        FDR_UNUSED(errorMessage)                                    \
    }

#ifndef AS_DEFAULT_BUFFER_SIZE
#define AS_DEFAULT_BUFFER_SIZE 0x1000 /*4096 bytes*/
#endif

class BaseSocket {
  public:
    enum SocketType {
      TCP = SOCK_STREAM,
      UDP = SOCK_DGRAM
    };

    sockaddr_in address{};

    void Close() {
      shutdown(this->sock, SHUT_RDWR);
      close(this->sock);
    }

    [[nodiscard]] std::string remoteAddress() const { return ipToString(this->address); }

    [[nodiscard]] int remotePort() const { return ntohs(this->address.sin_port); }

    [[nodiscard]] int fileDescriptor() const { return this->sock; }

  protected:
    int sock = 0;

    // Get std::string value of the IP from a `sockaddr_in` address struct
    static std::string ipToString(const sockaddr_in &addr) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);

      return {ip};
    }

    explicit BaseSocket(FDR_ON_ERROR, SocketType sockType = TCP, int socketId = -1) {
      if (socketId == -1) {
        this->sock = socket(AF_INET, sockType, 0);

        if (this->sock == -1) {
          onError(errno, "Socket creating error.");
        }
      } else {
        this->sock = socketId;
      }
    }

    virtual ~BaseSocket() = default;
};
