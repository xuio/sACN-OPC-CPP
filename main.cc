#include <err.h>
#include <e131.h>
#include <iostream>
#include "opc_client.h"
#include <vector>
#include <netdb.h>
#include <inttypes.h>
#include <numeric>

using namespace std;

OPCClient opc;

const uint16_t universe_count = 14;
const uint16_t leds_per_universe = 80;
const uint32_t bytes_per_universe = leds_per_universe * 3;

const uint32_t led_count = universe_count * leds_per_universe;
const uint32_t frame_bytes = led_count * 3;

vector<uint8_t> framebuffer;

vector<uint32_t> timer_v;

int main() {
	int sockfd;
	e131_packet_t packet;
	e131_error_t error;
	uint8_t last_seq = 0x00;

	// create a socket for E1.31
	if ((sockfd = e131_socket()) < 0)
		err(EXIT_FAILURE, "e131_socket");

	// bind the socket to the default E1.31 port and join multicast group for universe 1
	if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0)
		err(EXIT_FAILURE, "e131_bind");

	// initialize OPC
	framebuffer.resize(sizeof(OPCClient::Header) + frame_bytes);
	OPCClient::Header::view(framebuffer).init(0, opc.SET_PIXEL_COLORS, frame_bytes);

	// OPC Server address
	opc.resolve("localhost:7890");

	// try to connect to OPC server
	if (!opc.tryConnect())
		err(EXIT_FAILURE, "opc_connect");

	auto timestamp = chrono::steady_clock::now();

	uint32_t counter = 0;

	cout << "Server Started" << endl;

	while(1) {
		if (e131_recv(sockfd, &packet) < 0)
			err(EXIT_FAILURE, "e131_recv");

		if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
			fprintf(stderr, "e131_pkt_validate: %s\n", e131_strerror(error));
			continue;
		}

		// populate framebuffer
		uint16_t universe = ntohs(packet.frame.universe);
		uint32_t offset   = (universe - 1) * bytes_per_universe;
		uint8_t  *dest    = OPCClient::Header::view(framebuffer).data() + offset;

		for (int i = 0; i < bytes_per_universe; i++)
			*(dest++) = packet.dmp.prop_val[i + 1];

		//e131_pkt_dump(stderr, &packet);
		last_seq = packet.frame.seq_number;

		opc.write(framebuffer);

		auto timestamp_now = chrono::steady_clock::now();

		auto duration = chrono::duration_cast<chrono::microseconds> (timestamp_now - timestamp);

		timestamp = timestamp_now;

		timer_v.push_back(duration.count());

		if(timer_v.size() > 200)
			timer_v.erase(timer_v.begin());

		float avg_time_between_frames = (accumulate(timer_v.begin(), timer_v.end(), 0.0)/timer_v.size())*14;

		if(counter > 100){
			uint16_t FPS = 1/(avg_time_between_frames / 1000000);
			counter = 0;
			cout << "\r" "avg FPS: " << +FPS << " " << flush;
		}else
			counter++;
	}
}
