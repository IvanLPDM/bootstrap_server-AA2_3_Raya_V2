#include "Client.h"
#include <sstream>

Client::Client(sf::TcpSocket* socket) : _socket(socket) {
    std::ostringstream ss;
    ss << "C" << std::rand();
    _guid = ss.str();
}

sf::TcpSocket* Client::GetSocket() const { return _socket; }

std::string Client::GetGUID() const { return _guid; }

std::string Client::GetNickname() const { return _nickname; }

void Client::SetNickname(const std::string& nickname) { _nickname = nickname; }

void Client::SetRoomID(const std::string& roomID) { _roomID = roomID; }

std::string Client::GetRoomID() const { return _roomID; }