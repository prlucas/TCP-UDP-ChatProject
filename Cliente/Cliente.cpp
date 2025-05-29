#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <windows.h> // Para SetConsoleOutputCP y SetConsoleCP

using namespace boost::asio;
using ip::tcp;
using ip::udp;
using namespace std;

io_context io;
string global_username;
udp::endpoint server_endpoint;
udp::socket* g_udp_socket = nullptr;

const std::string SHARED_SECRET = "Yb93$plQf#1xN!v7RuTe0Z@mCgWk";

void udp_receive(udp::socket& socket) {
	while (true) {
		char data[1024];
		udp::endpoint sender;
		boost::system::error_code ec;
		size_t len = socket.receive_from(buffer(data), sender, 0, ec);
		if (!ec) {
			std::string msg(data, len);
			if (msg == "KICKED") {
				cout << "[!] Fuiste expulsado: otro cliente se conectó con tu nombre.\n";
				exit(0);
			}
			cout << msg << endl;
		}
	}
}

void ping_loop() {
	while (true) {
		this_thread::sleep_for(std::chrono::seconds(3));
		string ping = global_username + ":" + SHARED_SECRET + ":__ping__";
		g_udp_socket->send_to(buffer(ping), server_endpoint);
	}
}

int main() {
	// 🛠️ Habilitar UTF-8 en consola de Windows
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	string room, password;
	cout << "Usuario: ";
	getline(cin, global_username);
	cout << "Sala: ";
	getline(cin, room);
	cout << "Contraseña (vacía si es pública): ";
	getline(cin, password);

	tcp::socket tcp_socket(io);
	tcp::resolver resolver(io);
	connect(tcp_socket, resolver.resolve("127.0.0.1", "12345"));

	string registration = global_username + "," + room + "," + password + "," + SHARED_SECRET + "\n";
	write(tcp_socket, buffer(registration));

	boost::asio::streambuf response;
	read_until(tcp_socket, response, "\n");
	istream is(&response);
	string udp_port_str;
	getline(is, udp_port_str);
	uint16_t udp_port = stoi(udp_port_str);
	cout << "Puerto UDP asignado: " << udp_port << endl;

	udp::socket udp_socket(io, udp::endpoint(udp::v4(), 0));
	server_endpoint = udp::endpoint(ip::make_address("127.0.0.1"), udp_port);
	g_udp_socket = &udp_socket;

	string hello = global_username + ":" + SHARED_SECRET + ":";
	udp_socket.send_to(buffer(hello), server_endpoint);

	thread recv_thread(udp_receive, ref(udp_socket));
	thread ping_thread(ping_loop);

	while (true) {
		string msg;
		getline(cin, msg);
		string full_msg = global_username + ":" + SHARED_SECRET + ":" + msg;
		udp_socket.send_to(buffer(full_msg), server_endpoint);
	}
}
