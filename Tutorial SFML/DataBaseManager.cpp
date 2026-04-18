#include "DataBaseManager.h"
#include "bcrypt.h"
#include <iostream>
#include <algorithm>

#define SERVER "127.0.0.1"
#define USERNAME "Ivan"
#define PASSWORD "1234"

static constexpr int RANKING_POINTS_1ST		= 4;
static constexpr int RANKING_POINTS_2ND		= 2;
static constexpr int RANKING_POINTS_3RD		= 1;
static constexpr int RANKING_POINTS_LAST	= 0;

DataBaseManager::DataBaseManager()
{
	_driver = nullptr;
	_con = nullptr;
}

DataBaseManager::~DataBaseManager()
{
	if (_con) {
		_con->close();
		delete _con;
	}
}

// conecta con la base de datos

void DataBaseManager::ConnectDatabase()
{
	try {
		_driver = get_driver_instance();
		_con = _driver->connect(SERVER, USERNAME, PASSWORD);
		_con->setSchema("users");
		std::cout << "[Server] Connection with database done" << std::endl;

		// añado las columnas de ranking si no existen
		try { std::unique_ptr<sql::PreparedStatement> s(_con->prepareStatement(
			"ALTER TABLE users ADD COLUMN points INT NOT NULL DEFAULT 0")); s->execute(); } catch (...) {}
		try { std::unique_ptr<sql::PreparedStatement> s(_con->prepareStatement(
			"ALTER TABLE users ADD COLUMN wins INT NOT NULL DEFAULT 0")); s->execute(); } catch (...) {}
		try { std::unique_ptr<sql::PreparedStatement> s(_con->prepareStatement(
			"ALTER TABLE users ADD COLUMN losses INT NOT NULL DEFAULT 0")); s->execute(); } catch (...) {}

	}
	catch (sql::SQLException e) {
		std::cerr << "[DB] Error al conectar: " << e.what()
			<< " (MySQL error code: " << e.getErrorCode()
			<< ", SQLState: " << e.getSQLState() << ")" << std::endl;

	}
}

// registra un usuario con la contraseña hasheada

bool DataBaseManager::RegisterUser(const std::string& nickname, const std::string& password)
{
	if (nickname.empty() || password.empty()) {
		std::cerr << "[Server] Nickname or password empty on field \n";
		return false;
	}

	try { //Registrar contraseña y nickname
		std::string query = "INSERT INTO users(nickname, password) VALUES (?,?)";
		std::string hashedPassword = bcrypt::generateHash(password);
		std::unique_ptr<sql::PreparedStatement> stmt(_con->prepareStatement(query));

		stmt->setString(1, nickname);
		stmt->setString(2, hashedPassword);
		stmt->execute();		

		std::cout << "[Server] User registered correctly" << std::endl;
		return true;
	}
	catch(sql::SQLException& e){
		std::cerr<<"[Server] Error while trying to register user" << e.what() << std::endl;
		return false;
	}
	
}

// comprueba las credenciales del usuario

bool DataBaseManager::LoginUser(const std::string& nickname, const std::string& password)
{
	if (nickname.empty() || password.empty()) {
		std::cerr << "[Server] Nickname or password empty on field \n";
		return false;
	}
	try {
		std::string query = "SELECT password FROM users WHERE nickname = ?";
		std::unique_ptr<sql::PreparedStatement> stmt(_con->prepareStatement(query));
		stmt->setString(1, nickname);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		if (res->next()) {
			std::string storedHash = res->getString("password"); 
			if (bcrypt::validatePassword(password, storedHash)) { //Si la contraseña ya traducida coincide, inicia sesion
				std::cout << "[Server] Correct login for user: " << nickname << std::endl;
				return true;
			}
		}

		std::cout << "[Server] Login failed for user: " << nickname << std::endl;
		return false;
	}
	catch (sql::SQLException& e) {
		std::cerr << "[Server] Login error" << e.what() << std::endl;
		return false;
	}
}

// actualiza puntos, victorias y derrotas según la posición final

void DataBaseManager::UpdateRanking(const std::vector<std::pair<std::string, int>>& results, int numPlayers)
{
	static constexpr int POINTS_TABLE[] = { RANKING_POINTS_1ST, RANKING_POINTS_2ND, RANKING_POINTS_3RD, RANKING_POINTS_LAST };

	for (const auto& [nickname, finishPos] : results)
	{
		int idx    = std::min(finishPos - 1, 3);
		int pts    = (finishPos == numPlayers) ? RANKING_POINTS_LAST : POINTS_TABLE[idx];
		int isWin  = (finishPos == 1) ? 1 : 0;
		int isLoss = (finishPos == numPlayers) ? 1 : 0;

		try {
			std::unique_ptr<sql::PreparedStatement> stmt(_con->prepareStatement(
				"UPDATE users SET points = points + ?, wins = wins + ?, losses = losses + ? WHERE nickname = ?"));
			stmt->setInt(1, pts);
			stmt->setInt(2, isWin);
			stmt->setInt(3, isLoss);
			stmt->setString(4, nickname);
			stmt->execute();
			std::cout << "[DB] Updated ranking for " << nickname
				<< " pos=" << finishPos << " pts+" << pts << '\n';
		}
		catch (sql::SQLException& e) {
			std::cerr << "[DB] UpdateRanking error: " << e.what() << '\n';
		}
	}
}

// devuelve los 10 primeros ordenados por puntos

std::vector<RankingEntry> DataBaseManager::GetTopTenPlayers()
{
	std::vector<RankingEntry> entries;
	try {
		std::unique_ptr<sql::PreparedStatement> stmt(_con->prepareStatement(
			"SELECT nickname, points, wins, losses FROM users ORDER BY points DESC LIMIT 10"));
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		int rank = 1;
		while (res->next())
		{
			RankingEntry e;
			e.rank = rank++;
			e.nickname = res->getString("nickname");
			e.points = res->getInt("points");
			e.wins = res->getInt("wins");
			e.losses = res->getInt("losses");
			entries.push_back(e);
		}
	}
	catch (sql::SQLException& e) {
		std::cerr << "[DB] GetTopTenPlayers error: " << e.what() << '\n';
	}
	return entries;
}

// devuelve el ranking y stats de un jugador concreto

bool DataBaseManager::GetPlayerRanking(const std::string& nickname, int& outRank, int& outPoints, int& outWins, int& outLosses)
{
	try {
		// rango = jugadores con más puntos + 1
		std::unique_ptr<sql::PreparedStatement> stmt(_con->prepareStatement(
			"SELECT u.points, u.wins, u.losses, "
			"(SELECT COUNT(*) + 1 FROM users WHERE points > u.points) AS rank "
			"FROM users u WHERE u.nickname = ?"));
		stmt->setString(1, nickname);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		if (res->next())
		{
			outRank = res->getInt("rank");
			outPoints = res->getInt("points");
			outWins = res->getInt("wins");
			outLosses = res->getInt("losses");
			return true;
		}
	}
	catch (sql::SQLException& e) {
		std::cerr << "[DB] GetPlayerRanking error: " << e.what() << '\n';
	}
	return false;
}
