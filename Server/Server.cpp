#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

boost::asio::io_context io;

const std::string SHARED_SECRET = "Yb93$plQf#1xN!v7RuTe0Z@mCgWk";

struct ClientInfo {
	udp::endpoint endpoint;
	std::string username;
	std::chrono::steady_clock::time_point last_seen;
};

std::unordered_map<std::string, std::vector<ClientInfo>> rooms;
std::unordered_map<std::string, std::unique_ptr<udp::socket>> udp_sockets;
std::unordered_map<std::string, uint16_t> room_ports;
std::unordered_map<std::string, std::string> room_passwords;

uint16_t base_port = 5000;
const int INACTIVITY_SECONDS = 6;

void start_udp_receive(const std::string& room);

void update_or_add_client(const std::string& room, const std::string& username, const udp::endpoint& sender) {
	auto& clients = rooms[room];
	bool replaced = false;

	for (auto& client : clients) {
		if (client.username == username) {
			if (client.endpoint != sender) {
				auto socket = udp_sockets[room].get();
				auto kick = std::make_shared<std::string>("KICKED");
				socket->async_send_to(boost::asio::buffer(*kick), client.endpoint,
					[kick](boost::system::error_code, std::size_t) {});
			}
			client.endpoint = sender;
			client.last_seen = std::chrono::steady_clock::now();
			replaced = true;
			break;
		}
	}

	if (!replaced) {
		clients.push_back({ sender, username, std::chrono::steady_clock::now() });
		std::cout << "[+] Cliente '" << username << "' se unió a '" << room << "' (" << sender << ")\n";
	}
}

void remove_inactive_clients() {
	auto now = std::chrono::steady_clock::now();
	for (auto& [room, clients] : rooms) {
		clients.erase(std::remove_if(clients.begin(), clients.end(),
			[&](const ClientInfo& client) {
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client.last_seen).count();
				if (elapsed > INACTIVITY_SECONDS) {
					std::cout << "[-] Cliente inactivo '" << client.username << "' eliminado de '" << room << "'\n";
					return true;
				}
				return false;
			}), clients.end());
	}
}

void start_inactivity_timer() {
	static boost::asio::steady_timer timer(io, std::chrono::seconds(3));
	timer.expires_after(std::chrono::seconds(3));
	timer.async_wait([](const boost::system::error_code&) {
		remove_inactive_clients();
		start_inactivity_timer();
		});
}

void handle_udp_message(const std::string& room, udp::socket* socket, char* data, size_t length, udp::endpoint sender) {
	std::string msg(data, length);
	auto first = msg.find(':');
	if (first == std::string::npos) return;
	auto second = msg.find(':', first + 1);
	if (second == std::string::npos) return;

	std::string username = msg.substr(0, first);
	std::string token = msg.substr(first + 1, second - first - 1);
	std::string content = msg.substr(second + 1);

	if (token != SHARED_SECRET) {
		std::cerr << "[X] Token inválido (UDP) desde " << sender << "\n";
		return;
	}

	update_or_add_client(room, username, sender);

	if (content == "__ping__") {
		start_udp_receive(room);
		return;
	}

	for (const auto& client : rooms[room]) {
		if (client.endpoint != sender) {
			auto response = std::make_shared<std::string>(username + ": " + content);
			socket->async_send_to(boost::asio::buffer(*response), client.endpoint,
				[response](boost::system::error_code, std::size_t) {});
		}
	}

	start_udp_receive(room);
}

void start_udp_receive(const std::string& room) {
	auto socket = udp_sockets[room].get();
	auto data = std::make_shared<std::vector<char>>(1024);
	auto sender = std::make_shared<udp::endpoint>();

	socket->async_receive_from(boost::asio::buffer(*data), *sender,
		[=](boost::system::error_code ec, size_t len) {
			if (!ec) {
				handle_udp_message(room, socket, data->data(), len, *sender);
			}
			else {
				std::cerr << "[UDP ERROR][" << room << "] " << ec.message() << std::endl;
				start_udp_receive(room);
			}
		});
}

void start_tcp_accept(tcp::acceptor& acceptor) {
	auto socket = std::make_shared<tcp::socket>(io);
	acceptor.async_accept(*socket, [socket, &acceptor](boost::system::error_code ec) {
		if (!ec) {
			auto buf = std::make_shared<boost::asio::streambuf>();
			boost::asio::async_read_until(*socket, *buf, "\n", [socket, buf](boost::system::error_code ec, size_t) {
				if (!ec) {
					std::istream is(buf.get());
					std::string username, room, password, token;
					std::getline(is, username, ',');
					std::getline(is, room, ',');
					std::getline(is, password, ',');
					std::getline(is, token);

					if (username.empty() || room.empty() || token != SHARED_SECRET) {
						std::cerr << "[X] Conexión TCP rechazada: datos inválidos\n";
						return;
					}

					if (room_ports.count(room)) {
						if (room_passwords[room] != password) {
							std::cerr << "[X] Contraseña incorrecta para la sala '" << room << "'\n";
							return;
						}
					}
					else {
						uint16_t port = base_port++;
						room_ports[room] = port;
						room_passwords[room] = password;

						auto udp_socket = std::make_unique<udp::socket>(io, udp::endpoint(udp::v4(), port));
						udp_sockets[room] = std::move(udp_socket);
						start_udp_receive(room);

						std::cout << "[+] Sala creada: '" << room << "' (puerto UDP " << port << ")\n";
					}

					uint16_t udp_port = room_ports[room];
					auto response = std::make_shared<std::string>(std::to_string(udp_port) + "\n");
					boost::asio::async_write(*socket, boost::asio::buffer(*response),
						[socket, response](boost::system::error_code, std::size_t) {});
				}
				});
		}

		start_tcp_accept(acceptor);
		});
}

int main() {
	try {
		tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 12345));
		start_tcp_accept(acceptor);
		start_inactivity_timer();
		std::cout << "Servidor iniciado en puerto TCP 12345\n";

		std::vector<std::thread> threads;
		for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i)
			threads.emplace_back([]() { io.run(); });

		for (auto& t : threads) t.join();
	}
	catch (const std::exception& e) {
		std::cerr << "[FATAL] " << e.what() << std::endl;
	}
}
