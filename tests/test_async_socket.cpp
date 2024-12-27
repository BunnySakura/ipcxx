#include "ipcxx/async_socket/tcpserver.hpp"

#include <iostream>
#include <list>

using namespace std;

int main() {
  // Initialize server socket..
  TCPServer<1024> tcpServer;
  list<TCPSocket<1024> *> client_list;

  // When a new client connected:
  tcpServer.onNewConnection = [&](TCPSocket<1024> *newClient) {
    cout << "New client: ["
        << newClient->remoteAddress() << ":" << newClient->remotePort()
        << "]" << endl;
    client_list.push_back(newClient);

    newClient->onMessageReceived = [newClient, &client_list](const string &message) {
      cout << newClient->remoteAddress() << ":" << newClient->remotePort() << " => " << message << endl;
      for (auto &client : client_list) {
        if (client == newClient) continue;
        FDR_UNUSED(client->Send(std::to_string(newClient->remotePort()) + "=>" + message));
      }
    };

    newClient->onSocketClosed = [newClient, &client_list](int errorCode) {
      cout << "Socket closed:"
          << newClient->remoteAddress() << ":" << newClient->remotePort()
          << " -> " << errorCode << endl << flush;
      client_list.remove(newClient);
    };
  };

  // Bind the server to a port.
  tcpServer.Bind(
    8888, [](int errorCode, string errorMessage) {
      // BINDING FAILED:
      cout << errorCode << " : " << errorMessage << endl;
    }
  );

  // Start Listening the server.
  tcpServer.Listen(
    [](int errorCode, string errorMessage) {
      // LISTENING FAILED:
      cout << errorCode << " : " << errorMessage << endl;
    }
  );

  // You should do an input loop, so the program won't terminate immediately
  string input;
  getline(cin, input);
  while (input != "exit") {
    getline(cin, input);
  }

  // Close the server before exiting the program.
  tcpServer.Close();

  return 0;
}
