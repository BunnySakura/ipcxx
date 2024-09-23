#pragma once

#include "basesocket.hpp"
#include <string>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>

template<uint16_t BUFFER_SIZE = AS_DEFAULT_BUFFER_SIZE>
class TCPSocket final : public BaseSocket, public std::enable_shared_from_this<TCPSocket<BUFFER_SIZE>> {
  public:
    // Event Listeners:
    std::function<void(std::string)> onMessageReceived;
    std::function<void(const char *, ssize_t)> onRawMessageReceived;
    std::function<void(int)> onSocketClosed;

    explicit TCPSocket(FDR_ON_ERROR, int socketId = -1) : BaseSocket(onError, TCP, socketId) {}

    // Send raw bytes
    ssize_t Send(const char *bytes, size_t byteslength) const { return send(this->sock, bytes, byteslength, 0); }

    // Send std::string
    ssize_t Send(const std::string &message) const { return this->Send(message.c_str(), message.length()); }

    // Connect to a TCP Server with `uint32_t ipv4` & `uint16_t port` values
    void Connect(uint32_t ipv4, uint16_t port, const std::function<void()> &onConnected = [] {}, FDR_ON_ERROR) {
      this->address.sin_family = AF_INET;
      this->address.sin_port = htons(port);
      this->address.sin_addr.s_addr = ipv4;

      this->setTimeout(5);

      // Try to connect.
      int status = connect(this->sock, reinterpret_cast<const sockaddr *>(&this->address), sizeof(sockaddr_in));
      if (status == -1) {
        onError(errno, "Connection failed to the host.");
        this->setTimeout(0);
        return;
      }

      this->setTimeout(0);

      // Connected to the server, fire the event.
      onConnected();

      // Start listening from server:
      this->Listen();
    }

    // Connect to a TCP Server with `const char* host` & `uint16_t port` values
    void Connect(const char *host, uint16_t port, std::function<void()> onConnected = [] {}, FDR_ON_ERROR) {
      addrinfo hints{}, *res;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;

      // Get address info from DNS
      int status = getaddrinfo(host, nullptr, &hints, &res);
      if (status != 0) {
        onError(errno, "Invalid address." + std::string(gai_strerror(status)));
        return;
      }

      for (addrinfo *it = res; it != nullptr; it = it->ai_next) {
        if (it->ai_family == AF_INET) { // IPv4
          memcpy(static_cast<void *>(&this->address), it->ai_addr, sizeof(sockaddr_in));
          break; // for now, just get the first ip (ipv4).
        }
      }

      freeaddrinfo(res);

      this->Connect(this->address.sin_addr.s_addr, port, onConnected, onError);
    }

    // Connect to a TCP Server with `const std::string& ipv4` & `uint16_t port` values
    void Connect(
      const std::string &host,
      uint16_t port,
      const std::function<void()> &onConnected = [] {},
      FDR_ON_ERROR
    ) {
      this->Connect(host.c_str(), port, onConnected, onError);
    }

    // Start another thread to listen the socket
    void Listen() {
      std::thread t(&TCPSocket::Receive, this, this->shared_from_this());
      t.detach();
    }

    void setAddressStruct(sockaddr_in addr) { this->address = addr; }

    [[nodiscard]] sockaddr_in getAddressStruct() const { return this->address; }

  private:
    void Receive(std::shared_ptr<TCPSocket> this_shared_ptr) {
      auto self = std::move(this_shared_ptr);
      char tempBuffer[BUFFER_SIZE + 1];
      ssize_t messageLength;

      while ((messageLength = recv(self->sock, tempBuffer, BUFFER_SIZE, 0)) > 0) {
        tempBuffer[messageLength] = '\0';
        if (self->onMessageReceived)
          self->onMessageReceived(std::string(tempBuffer, messageLength));

        if (self->onRawMessageReceived)
          self->onRawMessageReceived(tempBuffer, messageLength);
      }

      self->Close();
      if (self->onSocketClosed) { self->onSocketClosed(errno); }
    }

    void setTimeout(int seconds) {
      timeval tv{};
      tv.tv_sec = seconds;
      tv.tv_usec = 0;

      setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
      setsockopt(this->sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
    }
};
