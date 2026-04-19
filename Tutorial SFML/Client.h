#pragma once
#include <SFML/Network.hpp>
#include <string>

class Client {
public:
    Client(sf::TcpSocket* socket);

    sf::TcpSocket* GetSocket() const;
    
    std::string GetGUID() const;
    std::string GetNickname() const;
    void SetNickname(const std::string& nickname);
    void SetRoomID(const std::string& roomID);
    std::string GetRoomID() const;    
    void SetP2PPort(unsigned short port) { _p2pPort = port; }
    unsigned short GetP2PPort() const { return _p2pPort; }

private:
    sf::TcpSocket* _socket;
    std::string _guid;
    std::string _nickname;
    std::string _roomID;
    unsigned short _p2pPort = 0;
};