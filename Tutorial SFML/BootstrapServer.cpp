#include "BootstrapServer.h"
#include <iostream>
#include <algorithm>

// configura el puerto y conecta con la base de datos

BootstrapServer::BootstrapServer(unsigned short port) 
{
    std::srand(std::time(nullptr));

    // abre el puerto

    if (_listener.listen(port) != sf::Socket::Status::Done) 
    {
        std::cerr << "[Server] Failed to bind port" << std::endl;
    }

    _selector.add(_listener);

    _db.ConnectDatabase();
}

// bucle principal del servidor

void BootstrapServer::Run() 
{
    while (true) 
    {
        if (_selector.wait(sf::seconds(1.f))) 
        {
            if (_selector.isReady(_listener))
            {
                AcceptNewConnection();
            }
            else
            {
                // uso índice porque StartMatch puede cambiar _clients durante el bucle
                for (size_t i = 0; i < _clients.size(); ++i)
                {
                    Client* c = _clients[i].get();
                    if (!c || !c->GetSocket()) continue;

                    if (_selector.isReady(*c->GetSocket()))
                    {
                        size_t sizeBefore = _clients.size();
                        ReceiveData(c);
                        // si el vector cambió de tamaño paro para no leer basura
                        if (_clients.size() < sizeBefore) break;
                    }
                }
            }
        }
        for (auto& [roomID, room] : _rooms)
        {
            if (room->waitingToStart && room->GetPlayers().size() >= MIN_PLAYERS_TO_START)
            {
                if (room->startTimer.getElapsedTime().asSeconds() >= MATCH_START_COUNTDOWN)
                {
                    std::cout << "[Server] Timer expired, starting match for room: " << roomID << std::endl;
                    StartMatch(room.get());
                    break;
                }
            }
        }
    }
}

// acepta un cliente nuevo y lo añade al selector

void BootstrapServer::AcceptNewConnection() {
    sf::TcpSocket* socket = new sf::TcpSocket();

    if (_listener.accept(*socket) == sf::Socket::Status::Done) 
    {
        socket->setBlocking(false);
        _selector.add(*socket);
        _clients.emplace_back(std::make_unique<Client>(socket));
        std::cout << "[Server] New client connected." << std::endl;
    }
    else 
    {
        delete socket;
    }
}

// recibe un paquete y lo procesa según el comando

void BootstrapServer::ReceiveData(Client* client) 
{
    sf::Packet packet;

    if (client->GetSocket()->receive(packet) == sf::Socket::Status::Done)
    {
        std::cout << "[Server] Packet received" << std::endl;

        std::string command;
        if (!(packet >> command))
        {
            std::cerr << "[Server] Failed to extract command" << std::endl;
            return;
        }

        std::cout << "[Server] Command: " << command << std::endl;

        if (command == "LOGIN" || command == "REGISTER")
        {
            std::string nick, pass;
            if (!(packet >> nick >> pass)) {
                std::cerr << "[Server] Failed to extract user & password" << std::endl;
                return;
            }

            HandleCommand(client, command, nick, pass);
        }
        else if (command == "GAME_RESULT" || command == "RANKING_REQUEST")
        {
            HandleRankingCommand(client, command, packet);
        }
        else
        {
            HandleCommand(client, command, packet);
        }
    }
}

// login y registro

void BootstrapServer::HandleCommand(Client* client, const std::string& command, const std::string& nick, const std::string& pass)
{
    sf::Packet response;

    if (command == "REGISTER")
    {
        bool success = _db.RegisterUser(nick, pass);
        response << (success ? "REGISTER_OK" : "REGISTER_FAIL");

        client->GetSocket()->send(response);
    }
    else if (command == "LOGIN")
    {
        bool success = _db.LoginUser(nick, pass);
        response << (success ? "LOGIN_OK" : "LOGIN_FAIL");

        client->GetSocket()->send(response);
    }
}

// crear o unirse a una sala

void BootstrapServer::HandleCommand(Client* client, const std::string& command, sf::Packet& packet)
{
    if (command.empty()) return;

    std::string roomID;
    unsigned short p2pPort;
    packet >> roomID >> p2pPort;

    client->SetP2PPort(p2pPort);

    if (command == "JOIN_ROOM")
    {
        std::cout << "[Server] Join room requested with ID: " << roomID << std::endl;

        JoinRoom(client, roomID);
    }
    else if(command == "CREATE_ROOM")
    {
        CreateRoom(client, roomID);
    }
    else
    {
        std::cout << "[Server] Unknown command: " << command << std::endl;
    }
}

// elimina el cliente del selector y del vector

void BootstrapServer::RemoveClient(Client* client) 
{
    _selector.remove(*client->GetSocket());

    _clients.erase(std::remove_if(_clients.begin(), _clients.end(), [client](const std::unique_ptr<Client>& c) { return c.get() == client; }), _clients.end());
}


#pragma region Join & Create Room

// crea la sala y mete al cliente

void BootstrapServer::CreateRoom(Client* client, const std::string& roomID)
{
    std::cout << "[Server] Creating a room...\n";

    std::string finalRoomID = roomID.empty() ? GenerateRandomRoomID() : roomID;

    // si ya existe la sala rechazo
    if (_rooms.find(finalRoomID) != _rooms.end())
    {
        std::cout << "[Server] Room ID already exists: " << finalRoomID << '\n';
        sf::Packet response;
        response << "CREATE_FAIL";
        client->GetSocket()->send(response);
        return;
    }

    _rooms[finalRoomID] = std::make_unique<Room>(finalRoomID, 4);
    _rooms[finalRoomID]->AddPlayer(client);
    client->SetRoomID(finalRoomID);

    // confirmo la creación y mando el ID
    sf::Packet response;
    response << "CREATE_OK" << finalRoomID;
    client->GetSocket()->send(response);

    // mando cuántos hay en la sala
    sf::Packet roomUpdate;
    roomUpdate << "ROOM_UPDATE" << (short)1 << _rooms[finalRoomID]->GetMaxPlayers();
    client->GetSocket()->send(roomUpdate);

    std::cout << "[Server] Room created with ID: " << finalRoomID << '\n';
}

// añade el cliente a la sala y avisa a los demás

void BootstrapServer::JoinRoom(Client* client, const std::string& roomID)
{
    std::cout << "[Server] Join room requested with ID: " << roomID << std::endl;

    if (!client || client->GetRoomID() == roomID)
    {
        std::cout << "[Server] Client already in room or invalid.\n";
        return;
    }

    auto it = _rooms.find(roomID);

    if (it != _rooms.end() && !it->second->IsFull())
    {
        Room* room = it->second.get();
        room->AddPlayer(client);
        client->SetRoomID(roomID);

        const auto& players = room->GetPlayers();

        // CAMBIO A MALLA
        sf::Packet response;

        short numExisting = 0;
        for (Client* p : players)
            if (p != client) ++numExisting;

        response << "JOIN_OK" << numExisting;
        for (Client* p : players)
        {
            if (p == client) continue;
            auto pIp = p->GetSocket()->getRemoteAddress();
            if (pIp.has_value())
                response << pIp.value().toString() << p->GetP2PPort();
        }
        client->GetSocket()->send(response);

        // Notifico a todos los existentes del nuevo peer
        auto newIp = client->GetSocket()->getRemoteAddress();
        if (newIp.has_value())
        {
            sf::Packet newPeerPacket;
            newPeerPacket << "NEW_PEER" << newIp.value().toString() << client->GetP2PPort();
            for (Client* p : players)
            {
                if (p != client)
                    p->GetSocket()->send(newPeerPacket);
            }
        }

        // aviso a todos del número actual de jugadores
        {
            short count = static_cast<short>(room->GetPlayers().size());
            short maxP  = room->GetMaxPlayers();
            for (Client* p : room->GetPlayers())
            {
                sf::Packet updatePacket;
                updatePacket << "ROOM_UPDATE" << count << maxP;
                p->GetSocket()->send(updatePacket);
            }
        }

        short numPlayers = static_cast<short>(players.size());

        if (numPlayers == MIN_PLAYERS_TO_START && !room->waitingToStart)
        {
            room->waitingToStart = true;
            room->startTimer.restart();
            std::cout << "[Server] Room has 2 players, starting 10s countdown..." << std::endl;
        }
        else if (numPlayers == MAX_ROOM_PLAYERS)
        {
            std::cout << "[Server] Room full, starting immediately." << std::endl;
            StartMatch(room);
        }
    }
    else
    {
        std::cout << "[Server] This room does not exist or is full: " << roomID << std::endl;
        sf::Packet errorPacket;
        errorPacket << "JOIN_FAIL";
        client->GetSocket()->send(errorPacket);
    }
}

// genera un ID aleatorio si el cliente no manda uno

std::string BootstrapServer::GenerateRandomRoomID()
{
    const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string roomId;
    for (short i = 0; i < ROOM_ID_LENGTH; ++i)
    {
        roomId += charset[rand() % charset.size()];
    }
    return roomId;
}

#pragma endregion

// arranca la partida, avisa a los jugadores y cierra sus conexiones

void BootstrapServer::StartMatch(Room* room)
{
    std::cout << "[Server] Match started for room: " << room->GetID() << std::endl;

    auto playersCopy = room->GetPlayers();

    for (short i = 0; i < static_cast<short>(playersCopy.size()); ++i)
    {
        sf::Packet packet;
        packet << "START_P2P";

        short playerIndex = i;
        short numPlayers  = static_cast<short>(playersCopy.size());

        packet << playerIndex << numPlayers;

        playersCopy[i]->GetSocket()->send(packet);
    }

    for (auto* player : playersCopy)
    {
        _selector.remove(*player->GetSocket());
        player->GetSocket()->disconnect();
        RemoveClient(player);
    }

    _rooms.erase(room->GetID());
}

// genera una clave única con los resultados para comparar entre clientes

std::string BootstrapServer::BuildResultKey(const std::vector<std::pair<std::string, short>>& results) const
{
    auto sorted = results;
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    std::string key;
    for (const auto& [nick, pos] : sorted)
        key += std::to_string(pos) + ":" + nick + ",";
    return key;
}

// gestiona resultados de partida y peticiones de ranking

void BootstrapServer::HandleRankingCommand(Client* client, const std::string& command, sf::Packet& packet)
{
    if (command == "GAME_RESULT")
    {
        short numPlayers;
        packet >> numPlayers;

        std::vector<std::pair<std::string, short>> results;
        for (short i = 0; i < numPlayers; ++i)
        {
            std::string nick;
            short pos;
            packet >> nick >> pos;
            results.push_back({nick, static_cast<short>(pos)});
        }

        std::string key = BuildResultKey(results);
        _pendingResults[key]++;

        std::cout << "[Server] GAME_RESULT received. Key count: " << _pendingResults[key] << '\n';

        // At least 2 matching reports: update ranking
        if (_pendingResults[key] == 2)
        {
            _db.UpdateRanking(results, numPlayers);
            std::cout << "[Server] Ranking updated after peer validation.\n";
        }
    }
    else if (command == "RANKING_REQUEST")
    {
        std::string requestingNick;
        packet >> requestingNick;

        auto topTen = _db.GetTopTenPlayers();

        // Check if requesting player is already in the top 10
        bool inTop10 = false;
        for (const auto& e : topTen)
            if (e.nickname == requestingNick) 
            { 
                inTop10 = true; 
                break; 
            }

        sf::Packet response;
        response << "RANKING_DATA" << static_cast<short>(topTen.size());
        for (const auto& e : topTen)
            response << (short)e.rank << e.nickname << (short)e.points << (short)e.wins << (short)e.losses;

        if (!inTop10)
        {
            short rank, pts, wins, losses;
            if (_db.GetPlayerRanking(requestingNick, rank, pts, wins, losses))
            {
                response << (short)1 << (short)rank << requestingNick << (short)pts << (short)wins << (short)losses;
            }
            else
            {
                response << (short)0;
            }
        }
        else
        {
            response << (short)0;
        }

        client->GetSocket()->send(response);
        std::cout << "[Server] RANKING_DATA sent to " << requestingNick << '\n';
    }
}

